// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "ClipCap.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTextStream>
#include <QThread>

#include <functional>
#include <iostream>

namespace {

constexpr auto KlipperService = "org.kde.klipper";
constexpr auto KlipperPath = "/klipper";
constexpr auto KlipperInterface = "org.kde.klipper.klipper";

int failures = 0;

void expectTrue(bool value, const char* expression) {
    if (!value) {
        std::cerr << "FAIL: " << expression << "\n";
        ++failures;
    }
}

template <typename L, typename R>
void expectEqual(const L& left, const R& right, const char* expression) {
    if (!(left == right)) {
        std::cerr << "FAIL: " << expression << "\n";
        ++failures;
    }
}

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(10);
    }
    QCoreApplication::processEvents();
    return predicate();
}

class FakeKlipper final : public QObject {
    Q_OBJECT

public:
    FakeKlipper(int delayMs, QString prefix)
        : delayMs_(delayMs), prefix_(std::move(prefix)) {}

public Q_SLOTS:
    Q_SCRIPTABLE QString getClipboardContents() {
        ++callCount_;
        QThread::msleep(static_cast<unsigned long>(delayMs_));
        return QStringLiteral("https://%1%2.test/path").arg(prefix_).arg(callCount_);
    }

    Q_SCRIPTABLE int getCallCount() const {
        return callCount_;
    }

private:
    int delayMs_ = 0;
    QString prefix_;
    int callCount_ = 0;
};

int runFakeKlipper(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const int delayMs = qEnvironmentVariableIntValue("NSL_FAKE_KLIPPER_DELAY_MS");
    const QString prefix = qEnvironmentVariable("NSL_FAKE_KLIPPER_PREFIX", QStringLiteral("call"));
    FakeKlipper klipper(delayMs, prefix);
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.registerService(QString::fromLatin1(KlipperService)) ||
        !bus.registerObject(QString::fromLatin1(KlipperPath),
                            QString::fromLatin1(KlipperInterface),
                            &klipper,
                            QDBusConnection::ExportScriptableSlots)) {
        return 2;
    }
    QTextStream(stdout) << "READY\n" << Qt::flush;
    return QCoreApplication::exec();
}

bool startFakeKlipper(QProcess& process, const QString& binaryPath, int delayMs, const QString& prefix) {
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("NSL_FAKE_KLIPPER_DELAY_MS"), QString::number(delayMs));
    environment.insert(QStringLiteral("NSL_FAKE_KLIPPER_PREFIX"), prefix);
    process.setProcessEnvironment(environment);
    process.setProgram(binaryPath);
    process.setArguments({QStringLiteral("--fake-klipper")});
    process.start();
    if (!process.waitForStarted(5000) || !process.waitForReadyRead(5000)) {
        return false;
    }
    return process.readLine().trimmed() == QByteArrayLiteral("READY");
}

bool stopProcess(QProcess& process) {
    if (process.state() == QProcess::NotRunning) {
        return true;
    }
    process.terminate();
    if (process.waitForFinished(5000)) {
        return true;
    }
    process.kill();
    return process.waitForFinished(5000);
}

int fakeCallCount() {
    QDBusInterface klipper(QString::fromLatin1(KlipperService),
                           QString::fromLatin1(KlipperPath),
                           QString::fromLatin1(KlipperInterface),
                           QDBusConnection::sessionBus());
    const QDBusReply<int> reply = klipper.call(QStringLiteral("getCallCount"));
    return reply.isValid() ? reply.value() : -1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && QByteArray(argv[1]) == QByteArrayLiteral("--fake-klipper")) {
        return runFakeKlipper(argc, argv);
    }

    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QApplication::clipboard()->setText(QStringLiteral("not a URL"));
    const QString binaryPath = QCoreApplication::applicationFilePath();

    QProcess firstService;
    expectTrue(startFakeKlipper(firstService, binaryPath, 700, QStringLiteral("call")),
               "delayed fake Klipper service starts");

    nsl::ClipCap clipCap;
    expectTrue(clipCap.klipperAvailable(), "ClipCap detects the running Klipper service");
    QStringList capturedHosts;
    QObject::connect(&clipCap, &nsl::ClipCap::urlCaptured, &app, [&capturedHosts](const QString& host) {
        capturedHosts.append(host);
    });

    clipCap.setEnabled(true);
    QElapsedTimer invocationTimer;
    invocationTimer.start();
    expectTrue(QMetaObject::invokeMethod(&clipCap, "pollKlipper", Qt::DirectConnection),
               "first Klipper poll is invokable");
    expectTrue(QMetaObject::invokeMethod(&clipCap, "pollKlipper", Qt::DirectConnection),
               "overlapping Klipper poll attempt is invokable");
    expectTrue(invocationTimer.elapsed() < 100, "Klipper polling never blocks the GUI thread");
    expectTrue(waitUntil([&capturedHosts]() { return !capturedHosts.isEmpty(); }, 3000),
               "first asynchronous Klipper reply arrives");
    expectEqual(capturedHosts.size(), 1, "overlapping poll attempts produce one captured URL");
    expectEqual(capturedHosts.value(0), QStringLiteral("call1.test"), "first asynchronous reply preserves URL host extraction");
    expectEqual(fakeCallCount(), 1, "only one Klipper request is in flight");

    expectTrue(QMetaObject::invokeMethod(&clipCap, "pollKlipper", Qt::DirectConnection),
               "second asynchronous poll starts");
    clipCap.setEnabled(false);
    expectTrue(waitUntil([]() { return fakeCallCount() >= 2; }, 3000), "delayed second Klipper call completes remotely");
    QCoreApplication::processEvents();
    expectEqual(capturedHosts.size(), 1, "late reply after disable is ignored");

    clipCap.setEnabled(true);
    expectTrue(QMetaObject::invokeMethod(&clipCap, "pollKlipper", Qt::DirectConnection),
               "poll remains invokable before service-loss test");
    expectTrue(stopProcess(firstService), "first fake Klipper service stops cleanly");
    expectTrue(waitUntil([&clipCap]() { return !clipCap.klipperAvailable(); }, 3000),
               "ClipCap detects Klipper service loss");
    QCoreApplication::processEvents();
    expectEqual(capturedHosts.size(), 1, "reply from a lost Klipper owner is ignored");

    QProcess resumedService;
    expectTrue(startFakeKlipper(resumedService, binaryPath, 50, QStringLiteral("resumed")),
               "replacement fake Klipper service starts");
    expectTrue(waitUntil([&clipCap]() { return clipCap.klipperAvailable(); }, 3000),
               "ClipCap detects Klipper service return");
    expectTrue(waitUntil([&capturedHosts]() { return capturedHosts.size() >= 2; }, 3000),
               "ClipCap resumes asynchronous polling after service return");
    expectEqual(capturedHosts.value(1), QStringLiteral("resumed1.test"),
                "resumed service reply is captured exactly once");
    clipCap.setEnabled(false);
    expectTrue(stopProcess(resumedService), "replacement fake Klipper service stops cleanly");

    if (failures != 0) {
        std::cerr << failures << " ClipCap test failure(s)\n";
        return 1;
    }
    return 0;
}

#include "test_clipcap.moc"
