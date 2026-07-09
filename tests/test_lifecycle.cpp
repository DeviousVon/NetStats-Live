// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 NAME

#include "Lifecycle.h"
#include "Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>

#include <cstdint>
#include <iostream>

namespace {

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

QString readAll(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

int main(int argc, char** argv) {
    QTemporaryDir tempConfig;
    qputenv("XDG_CONFIG_HOME", tempConfig.path().toUtf8());
    qputenv("NSL_FAKE_DATE", "2026-06-30");

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("nsl-linux-test"));
    QCoreApplication::setOrganizationName(QStringLiteral("NSL-Linux-Test"));

    using namespace nsl;

    expectEqual(AppSettings::currentMonthKey(), QStringLiteral("2026-06"), "NSL_FAKE_DATE yyyy-MM-dd controls month key");
    qputenv("NSL_FAKE_DATE", "2026-07");
    expectEqual(AppSettings::currentMonthKey(), QStringLiteral("2026-07"), "NSL_FAKE_DATE yyyy-MM controls month key");

    AppSettings settings;
    qputenv("NSL_FAKE_DATE", "2026-06-30");
    settings.saveMonthlyTotals(QStringLiteral("2026-06"), 12345, 67890);

    qputenv("NSL_FAKE_DATE", "2026-07-01");
    AppConfig rolled = settings.load();
    expectEqual(rolled.monthKey, QStringLiteral("2026-07"), "load rolls to fake current month");
    expectEqual(rolled.rxMonth, std::uint64_t{0}, "new month rx starts at zero");
    expectEqual(rolled.txMonth, std::uint64_t{0}, "new month tx starts at zero");
    expectEqual(rolled.lastRxMonth, std::uint64_t{12345}, "previous archived rx is available for Last Month display");
    expectEqual(rolled.lastTxMonth, std::uint64_t{67890}, "previous archived tx is available for Last Month display");

    QSettings raw(settings.configPath(), QSettings::IniFormat);
    expectEqual(raw.value(QStringLiteral("history/2026-06/rxMonth")).toULongLong(), qulonglong{12345}, "old rx archived under history month bucket");
    expectEqual(raw.value(QStringLiteral("history/2026-06/txMonth")).toULongLong(), qulonglong{67890}, "old tx archived under history month bucket");

    const QString buildPath = QDir(tempConfig.path()).filePath(QStringLiteral("build dir/nsl-linux"));
    expectTrue(settings.setAutoStart(true, buildPath), "autostart create succeeds");
    const QString autostartPath = settings.autoStartPath();
    expectTrue(QFile::exists(autostartPath), "autostart desktop file exists");
    const QString desktop = readAll(autostartPath);
    expectTrue(desktop.contains(QStringLiteral("X-KDE-autostart-after=panel\n")), "autostart waits until KDE panel/SNI host exists");
    expectTrue(desktop.contains(QStringLiteral("Icon=nsl-linux\n")), "autostart uses packaged nsl-linux icon name");
    expectTrue(desktop.contains(QStringLiteral("Exec=\"") + buildPath + QStringLiteral("\" --minimized\n")), "autostart quotes build-dir Exec path and starts minimized");
    expectTrue(settings.setAutoStart(false, buildPath), "autostart remove succeeds");
    expectTrue(!QFile::exists(autostartPath), "autostart desktop file removed");

    expectTrue(shouldShowMainWindow(false, false, true), "normal startup shows window");
    expectTrue(!shouldShowMainWindow(true, false, true), "--minimized hides window when tray exists");
    expectTrue(!shouldShowMainWindow(false, true, true), "Auto Minimize hides window when tray exists");
    expectTrue(shouldShowMainWindow(true, false, false), "--minimized is ignored without tray");
    expectTrue(shouldShowMainWindow(false, true, false), "Auto Minimize is ignored without tray");
    expectTrue(shouldHideToTray(true), "Minimize hides to tray when tray exists");
    expectTrue(!shouldHideToTray(false), "Minimize stays reachable without tray");
    expectTrue(!singleInstanceServiceName().isEmpty(), "single-instance service name present");
    expectTrue(!singleInstanceObjectPath().isEmpty(), "single-instance object path present");

    QTemporaryDir signalConfig;
    const QString binaryPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("nsl-linux"));
    expectTrue(QFile::exists(binaryPath), "nsl-linux binary exists beside lifecycle test");
    QProcess process;
    QProcessEnvironment processEnv = QProcessEnvironment::systemEnvironment();
    processEnv.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    processEnv.insert(QStringLiteral("XDG_CONFIG_HOME"), signalConfig.path());
    processEnv.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("unix:path=%1/no-session-bus").arg(signalConfig.path()));
    process.setProcessEnvironment(processEnv);
    process.setProgram(binaryPath);
    process.setArguments({QStringLiteral("--simulate"), QStringLiteral("--minimized")});
    process.start();
    expectTrue(process.waitForStarted(5000), "simulated app starts for SIGTERM persistence test");
    if (process.state() != QProcess::NotRunning) {
        QThread::sleep(4);
        process.terminate();
        expectTrue(process.waitForFinished(10000), "SIGTERM exits app cleanly");
        const QString signalConfigPath = QDir(signalConfig.path()).filePath(QStringLiteral("nsl-linux/nsl-linux.conf"));
        expectTrue(QFile::exists(signalConfigPath), "SIGTERM writes config before exit");
        QSettings signalSettings(signalConfigPath, QSettings::IniFormat);
        const qulonglong rx = signalSettings.value(QStringLiteral("totals/rxMonth")).toULongLong();
        const qulonglong tx = signalSettings.value(QStringLiteral("totals/txMonth")).toULongLong();
        expectTrue(rx > 0 || tx > 0, "SIGTERM saves non-zero simulated totals");
    }

    if (failures != 0) {
        std::cerr << failures << " lifecycle test failure(s)\n";
        return 1;
    }
    return 0;
}
