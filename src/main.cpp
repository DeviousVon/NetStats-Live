#include "MainWindow.h"
#include "Lifecycle.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QIcon>
#include <QSocketNotifier>
#include <QTimer>

#include <csignal>
#include <fcntl.h>
#include <unistd.h>

namespace {

int signalPipeFds[2] = {-1, -1};

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

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("nsl-linux"));
    QCoreApplication::setOrganizationName(QStringLiteral("NSL-Linux"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("nsl-linux"), QIcon::fromTheme(QStringLiteral("network-workgroup"))));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("AnalogX NetStat Live style network monitor for Linux"));
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
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!screenshotMode && sessionBus.isConnected()) {
        if (!sessionBus.registerService(nsl::singleInstanceServiceName())) {
            QDBusInterface existing(nsl::singleInstanceServiceName(),
                                    nsl::singleInstanceObjectPath(),
                                    nsl::singleInstanceInterfaceName(),
                                    sessionBus);
            existing.call(QStringLiteral("activateFromInstanceRequest"));
            return 0;
        }
    }

    nsl::MainWindow window(parser.isSet(simulateOption) || screenshotMode);
    QSocketNotifier* signalNotifier = nullptr;
    if (!screenshotMode && installUnixSignalHandlers()) {
        signalNotifier = new QSocketNotifier(signalPipeFds[0], QSocketNotifier::Read, &app);
        QObject::connect(signalNotifier, &QSocketNotifier::activated, &window, [&window, signalNotifier]() {
            signalNotifier->setEnabled(false);
            drainSignalPipe();
            window.shutdownForSignal();
        });
    }
    if (!screenshotMode && sessionBus.isConnected()) {
        sessionBus.registerObject(nsl::singleInstanceObjectPath(),
                                  nsl::singleInstanceInterfaceName(),
                                  &window,
                                  QDBusConnection::ExportScriptableSlots);
    }

    if (screenshotMode) {
        const QString outputPath = parser.value(screenshotOption);
        window.setPersistenceEnabled(false);
        window.populateScreenshotDemoData();
        window.show();
        QTimer::singleShot(150, &window, [&window, outputPath]() {
            const bool saved = !outputPath.isEmpty() && window.grab().save(outputPath, "PNG");
            QCoreApplication::exit(saved ? 0 : 2);
        });
        return QApplication::exec();
    }

    if (nsl::shouldShowMainWindow(parser.isSet(minimizedOption), window.autoMinimizeEnabled())) {
        window.show();
    }
    return QApplication::exec();
}
