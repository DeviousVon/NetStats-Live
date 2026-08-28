// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "MainWindow.h"
#include "Lifecycle.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QLockFile>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTimer>

#include <csignal>
#include <fcntl.h>
#include <memory>
#include <unistd.h>

namespace {

int signalPipeFds[2] = {-1, -1};

class InstanceActivationProxy final : public QObject {
    Q_OBJECT

public:
    void setTarget(nsl::MainWindow* target) {
        target_ = target;
        if (pendingTokenRequest_) {
            pendingTokenRequest_ = false;
            target_->activateFromInstanceRequestWithToken(pendingToken_);
            pendingToken_.clear();
            pendingLegacyRequest_ = false;
        } else if (pendingLegacyRequest_) {
            pendingLegacyRequest_ = false;
            target_->activateFromInstanceRequest();
        }
    }

public Q_SLOTS:
    Q_SCRIPTABLE void activateFromInstanceRequest() {
        if (target_ != nullptr) {
            target_->activateFromInstanceRequest();
        } else {
            pendingLegacyRequest_ = true;
        }
    }

    Q_SCRIPTABLE void activateFromInstanceRequestWithToken(const QString& activationToken) {
        if (target_ != nullptr) {
            target_->activateFromInstanceRequestWithToken(activationToken);
        } else {
            pendingTokenRequest_ = true;
            pendingToken_ = activationToken;
        }
    }

private:
    nsl::MainWindow* target_ = nullptr;
    bool pendingLegacyRequest_ = false;
    bool pendingTokenRequest_ = false;
    QString pendingToken_;
};

// Keep Unix signal handling async-signal-safe: write one byte here, then let
// QSocketNotifier do the real shutdown/persistence work inside the Qt loop.
void handleUnixSignal(int) {
    const char byte = 'q';
    if (signalPipeFds[1] != -1) {
        const ssize_t ignored = ::write(signalPipeFds[1], &byte, sizeof(byte));
        Q_UNUSED(ignored)
    }
}

void makeCloseOnExec(int fd) {
    const int flags = ::fcntl(fd, F_GETFD, 0);
    if (flags != -1) {
        ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }
}

void makeNonBlocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

bool installUnixSignalHandlers() {
    if (signalPipeFds[0] != -1) {
        return true;
    }
    if (::pipe(signalPipeFds) != 0) {
        signalPipeFds[0] = -1;
        signalPipeFds[1] = -1;
        return false;
    }
    makeCloseOnExec(signalPipeFds[0]);
    makeCloseOnExec(signalPipeFds[1]);
    makeNonBlocking(signalPipeFds[0]);
    makeNonBlocking(signalPipeFds[1]);

    struct sigaction action;
    action.sa_handler = handleUnixSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    return ::sigaction(SIGTERM, &action, nullptr) == 0 && ::sigaction(SIGINT, &action, nullptr) == 0;
}

void drainSignalPipe() {
    char buffer[32];
    while (::read(signalPipeFds[0], buffer, sizeof(buffer)) > 0) {
    }
}

bool activateExistingInstance(const QDBusConnection& sessionBus,
                              const nsl::SingleInstanceEndpoint& endpoint) {
    constexpr int ActivationTimeoutMs = 2000;
    QDBusMessage tokenRequest = QDBusMessage::createMethodCall(
        endpoint.serviceName,
        endpoint.objectPath,
        endpoint.interfaceName,
        QStringLiteral("activateFromInstanceRequestWithToken"));
    tokenRequest << qEnvironmentVariable("XDG_ACTIVATION_TOKEN");
    const QDBusMessage tokenResponse = sessionBus.call(tokenRequest, QDBus::Block, ActivationTimeoutMs);
    if (tokenResponse.type() == QDBusMessage::ReplyMessage) {
        return true;
    }

    // Preserve compatibility with an older running instance, but only report
    // success if the legacy activation call actually receives a reply.
    const QDBusMessage legacyRequest = QDBusMessage::createMethodCall(
        endpoint.serviceName,
        endpoint.objectPath,
        endpoint.interfaceName,
        QStringLiteral("activateFromInstanceRequest"));
    const QDBusMessage legacyResponse = sessionBus.call(legacyRequest, QDBus::Block, ActivationTimeoutMs);
    return legacyResponse.type() == QDBusMessage::ReplyMessage;
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("netstats-live"));
    QCoreApplication::setOrganizationName(QStringLiteral("NetStats-Live"));
    QGuiApplication::setDesktopFileName(QStringLiteral("netstats-live"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("netstats-live"), QIcon::fromTheme(QStringLiteral("network-workgroup"))));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Live network throughput and CPU monitor widget for Linux"));
    parser.addHelpOption();
    const QCommandLineOption minimizedOption(QStringLiteral("minimized"), QStringLiteral("Start hidden in the system tray"));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                              QStringLiteral("Render a deterministic visual-fidelity PNG and exit"),
                                              QStringLiteral("path"));
    QCommandLineOption simulateOption(QStringLiteral("simulate"), QStringLiteral("Run deterministic synthetic tray traffic"));
    simulateOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(minimizedOption);
    parser.addOption(screenshotOption);
    parser.addOption(simulateOption);
    parser.process(app);

    const bool screenshotMode = parser.isSet(screenshotOption);
    const bool simulationMode = parser.isSet(simulateOption);
    const bool persistenceEnabled = !screenshotMode && !simulationMode;
    QDBusConnection sessionBus = QDBusConnection::sessionBus();

    const QString lockDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
                                      .filePath(QStringLiteral("netstats-live"));
    if (persistenceEnabled && !QDir().mkpath(lockDirectory)) {
        qCritical("Unable to create the NetStats-Live configuration directory for single-instance locking.");
        return 3;
    }
    const nsl::SingleInstanceEndpoint instanceEndpoint = nsl::singleInstanceEndpoint(lockDirectory);
    QLockFile instanceLock(QDir(lockDirectory).filePath(QStringLiteral("netstats-live.lock")));
    instanceLock.setStaleLockTime(0);
    if (persistenceEnabled) {
        if (!instanceLock.tryLock(0)) {
            if (instanceLock.error() == QLockFile::LockFailedError) {
                if (sessionBus.isConnected() && activateExistingInstance(sessionBus, instanceEndpoint)) {
                    return 0;
                }
                qCritical("NetStats-Live is already running for this user profile, but DBus activation failed or is unavailable.");
            } else {
                qCritical("Unable to acquire the NetStats-Live single-instance lock.");
            }
            return 3;
        }
    }

    bool dbusServiceRegistered = false;
    if (!screenshotMode && sessionBus.isConnected()) {
        if (!sessionBus.registerService(instanceEndpoint.serviceName)) {
            if (activateExistingInstance(sessionBus, instanceEndpoint)) {
                return 0;
            }
            qCritical("Unable to register the NetStats-Live DBus service; refusing to run without reliable single-instance activation.");
            return 3;
        }
        dbusServiceRegistered = true;
    }

    InstanceActivationProxy activationProxy;
    std::unique_ptr<nsl::MainWindow> window;
    const auto initializeWindow = [&]() {
        window = std::make_unique<nsl::MainWindow>(simulationMode || screenshotMode, nullptr, persistenceEnabled);
    };
    if (dbusServiceRegistered) {
        if (!nsl::registerSingleInstanceObjectBeforeInitialization(
                sessionBus, instanceEndpoint, &activationProxy, initializeWindow)) {
            sessionBus.unregisterService(instanceEndpoint.serviceName);
            qCritical("Unable to register the NetStats-Live DBus activation object; refusing to initialize persistent state.");
            return 3;
        }
    } else {
        initializeWindow();
    }
    activationProxy.setTarget(window.get());

    QSocketNotifier* signalNotifier = nullptr;
    if (!screenshotMode && installUnixSignalHandlers()) {
        signalNotifier = new QSocketNotifier(signalPipeFds[0], QSocketNotifier::Read, &app);
        QObject::connect(signalNotifier, &QSocketNotifier::activated, window.get(), [window = window.get(), signalNotifier]() {
            signalNotifier->setEnabled(false);
            drainSignalPipe();
            window->shutdownForSignal();
            QCoreApplication::quit();
        });
    }

    if (screenshotMode) {
        const QString outputPath = parser.value(screenshotOption);
        window->populateScreenshotDemoData();
        window->show();
        QTimer::singleShot(150, window.get(), [window = window.get(), outputPath]() {
            const bool saved = !outputPath.isEmpty() && window->grab().save(outputPath, "PNG");
            QCoreApplication::exit(saved ? 0 : 2);
        });
        return QApplication::exec();
    }

    const bool trayAvailable = window->trayAvailable();
    if (!trayAvailable) {
        qWarning("System tray is not available; starting with the main window visible.");
    }
    if (nsl::shouldShowMainWindow(parser.isSet(minimizedOption), window->autoMinimizeEnabled(), trayAvailable)) {
        window->show();
    }
    return QApplication::exec();
}

#include "main.moc"
