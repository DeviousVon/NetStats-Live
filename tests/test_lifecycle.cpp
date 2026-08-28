// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "Lifecycle.h"
#include "Settings.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRect>
#include <QSettings>
#include <QSize>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>

#include <cstdint>
#include <cstring>
#include <iostream>

namespace {

int failures = 0;
QStringList capturedWarnings;

void captureWarnings(QtMsgType type, const QMessageLogContext&, const QString& message) {
    if (type == QtWarningMsg) {
        capturedWarnings.append(message);
    }
}

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

bool writeText(const QString& path, const QString& content) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(content.toUtf8());
    return file.flush();
}

bool containsWarning(const QString& needle) {
    for (const QString& warning : capturedWarnings) {
        if (warning.contains(needle)) {
            return true;
        }
    }
    return false;
}

void printCapturedWarnings() {
    for (const QString& warning : capturedWarnings) {
        std::cerr << "captured warning: " << warning.toStdString() << "\n";
    }
}

int runMigrationCopyFailureLoggingCheck(int argc, char** argv) {
    QTemporaryDir tempConfig;
    qputenv("XDG_CONFIG_HOME", tempConfig.path().toUtf8());
    QCoreApplication app(argc, argv);

    const QString legacyDir = QDir(tempConfig.path()).filePath(QStringLiteral("nsl-linux"));
    expectTrue(QDir().mkpath(legacyDir), "legacy config directory created for copy-failure logging check");
    const QString legacyConfigPath = QDir(legacyDir).filePath(QStringLiteral("nsl-linux.conf"));
    {
        QSettings legacy(legacyConfigPath, QSettings::IniFormat);
        legacy.setValue(QStringLiteral("remote/target"), QStringLiteral("legacy.example"));
        legacy.sync();
    }

    const QString blockingPath = QDir(tempConfig.path()).filePath(QStringLiteral("netstats-live"));
    expectTrue(writeText(blockingPath, QStringLiteral("not a directory\n")), "file blocks netstats-live config directory creation");

    capturedWarnings.clear();
    qInstallMessageHandler(captureWarnings);
    nsl::AppSettings settings;
    qInstallMessageHandler(nullptr);

    const QString expectedWarning = QStringLiteral("Unable to migrate legacy config");
    expectTrue(containsWarning(expectedWarning), "legacy config copy failure is logged");
    expectTrue(!settings.initializationResult().ok && settings.initializationResult().error.contains(expectedWarning),
               "legacy config copy failure is returned as structured initialization status");
    expectTrue(!settings.storageAuthorityResult().ok,
               "legacy config copy failure revokes settings authority for the session");
    if (!containsWarning(expectedWarning)) {
        printCapturedWarnings();
    }

    return failures == 0 ? 0 : 1;
}

int runAutoStartSymlinkMigrationCheck(int argc, char** argv) {
    QTemporaryDir tempConfig;
    qputenv("XDG_CONFIG_HOME", tempConfig.path().toUtf8());
    QCoreApplication app(argc, argv);

    const QString autoStartDir = QDir(tempConfig.path()).filePath(QStringLiteral("autostart"));
    const QString legacyPath = QDir(autoStartDir).filePath(QStringLiteral("nsl-linux.desktop"));
    const QString newPath = QDir(autoStartDir).filePath(QStringLiteral("netstats-live.desktop"));
    const QString externalPath = QDir(tempConfig.path()).filePath(QStringLiteral("external.desktop"));
    const QString externalContents = QStringLiteral("[Desktop Entry]\nName=External\n");
    expectTrue(QDir().mkpath(autoStartDir), "autostart directory is created for symlink migration checks");
    expectTrue(writeText(externalPath, externalContents), "external autostart target is created");
    expectTrue(QFile::link(externalPath, legacyPath), "legacy autostart symlink is created");

    nsl::AppSettings legacySymlinkSettings;
    expectTrue(!legacySymlinkSettings.initializationResult().ok,
               "legacy autostart symlink makes initialization untrusted");
    expectTrue(legacySymlinkSettings.storageAuthorityResult().ok,
               "legacy autostart symlink does not revoke unrelated settings authority");
    expectTrue(QFileInfo(legacyPath).isSymLink(), "legacy autostart symlink is preserved");
    expectTrue(!QFile::exists(newPath), "legacy autostart symlink cannot publish a migrated entry");
    expectEqual(readAll(externalPath), externalContents, "legacy symlink target is not changed or disclosed into migration");

    expectTrue(QFile::remove(legacyPath), "legacy autostart symlink is removed for new-path test");
    expectTrue(writeText(legacyPath, QStringLiteral("[Desktop Entry]\nName=NSL-Linux\n")),
               "regular legacy autostart entry is created");
    expectTrue(QFile::link(externalPath, newPath), "new autostart symlink is created");

    nsl::AppSettings newSymlinkSettings;
    expectTrue(!newSymlinkSettings.initializationResult().ok,
               "new autostart symlink makes initialization untrusted");
    expectTrue(newSymlinkSettings.storageAuthorityResult().ok,
               "new autostart symlink does not revoke unrelated settings authority");
    expectTrue(QFileInfo(newPath).isSymLink(), "new autostart symlink is preserved");
    expectTrue(QFile::exists(legacyPath), "regular legacy entry is not deleted behind a new-path symlink");
    expectEqual(readAll(externalPath), externalContents, "new symlink target is unchanged");

    return failures == 0 ? 0 : 1;
}

int runDbusFailureSemanticsCheck(int argc, char** argv) {
    QTemporaryDir tempConfig;
    qputenv("XDG_CONFIG_HOME", tempConfig.path().toUtf8());
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("netstats-live-dbus-test"));

    const QString binaryPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("netstats-live"));
    expectTrue(QFile::exists(binaryPath), "netstats-live binary exists for isolated DBus test");

    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    expectTrue(sessionBus.isConnected(), "dbus-run-session provides an isolated session bus");
    const nsl::SingleInstanceEndpoint instanceEndpoint = nsl::singleInstanceEndpoint(
        QDir(tempConfig.path()).filePath(QStringLiteral("netstats-live")));
    const bool conflictingServiceRegistered = sessionBus.registerService(instanceEndpoint.serviceName);
    expectTrue(conflictingServiceRegistered, "test process reserves the NetStats-Live DBus service without an object");

    QProcessEnvironment processEnv = QProcessEnvironment::systemEnvironment();
    processEnv.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    processEnv.insert(QStringLiteral("XDG_CONFIG_HOME"), tempConfig.path());

    QProcess failedActivation;
    failedActivation.setProcessEnvironment(processEnv);
    failedActivation.setProgram(binaryPath);
    failedActivation.setArguments({QStringLiteral("--minimized")});
    failedActivation.start();
    expectTrue(failedActivation.waitForStarted(5000), "instance starts against a conflicting DBus service");
    const bool failedActivationFinished = failedActivation.waitForFinished(5000);
    if (!failedActivationFinished) {
        failedActivation.terminate();
        failedActivation.waitForFinished(10000);
    }
    const QString failedActivationError = QString::fromUtf8(failedActivation.readAllStandardError());
    expectTrue(failedActivationFinished && failedActivation.exitStatus() == QProcess::NormalExit &&
                   failedActivation.exitCode() != 0,
               "failed token-aware and legacy activation exits nonzero");
    expectTrue(failedActivationError.contains(QStringLiteral("DBus"), Qt::CaseInsensitive),
               "failed activation reports a clear DBus diagnostic");

    if (conflictingServiceRegistered) {
        expectTrue(sessionBus.unregisterService(instanceEndpoint.serviceName),
                   "test process releases the conflicting DBus service");
    }

    QObject occupiedObject;
    expectTrue(sessionBus.registerObject(instanceEndpoint.objectPath,
                                         instanceEndpoint.interfaceName,
                                         &occupiedObject,
                                         QDBusConnection::ExportScriptableSlots),
               "test process occupies the activation object path");
    QObject rejectedObject;
    bool rejectedInitializationRan = false;
    expectTrue(!nsl::registerSingleInstanceObjectBeforeInitialization(
                   sessionBus, instanceEndpoint, &rejectedObject, [&rejectedInitializationRan]() {
                       rejectedInitializationRan = true;
                   }),
               "production startup rejects an occupied activation path");
    expectTrue(!rejectedInitializationRan,
               "persistent initialization cannot run before activation-object authority");
    sessionBus.unregisterObject(instanceEndpoint.objectPath);
    bool acceptedInitializationRan = false;
    expectTrue(nsl::registerSingleInstanceObjectBeforeInitialization(
                   sessionBus, instanceEndpoint, &rejectedObject, [&acceptedInitializationRan]() {
                       acceptedInitializationRan = true;
                   }),
               "production startup registers after the path is released");
    expectTrue(acceptedInitializationRan,
               "persistent initialization runs only after activation-object authority");
    sessionBus.unregisterObject(instanceEndpoint.objectPath);

    QProcess owningInstance;
    owningInstance.setProcessEnvironment(processEnv);
    owningInstance.setProgram(binaryPath);
    owningInstance.setArguments({QStringLiteral("--minimized")});
    owningInstance.start();
    expectTrue(owningInstance.waitForStarted(5000), "real owning instance starts on the isolated DBus session");

    bool serviceReady = false;
    if (sessionBus.interface() != nullptr) {
        for (int attempt = 0; attempt < 50 && !serviceReady; ++attempt) {
            const QDBusReply<bool> registered = sessionBus.interface()->isServiceRegistered(instanceEndpoint.serviceName);
            serviceReady = registered.isValid() && registered.value();
            if (!serviceReady) {
                QThread::msleep(100);
            }
        }
    }
    expectTrue(serviceReady, "real owning instance registers its DBus service");

    QProcess successfulActivation;
    successfulActivation.setProcessEnvironment(processEnv);
    successfulActivation.setProgram(binaryPath);
    successfulActivation.setArguments({QStringLiteral("--minimized")});
    successfulActivation.start();
    expectTrue(successfulActivation.waitForStarted(5000), "second instance starts for successful DBus activation");
    expectTrue(successfulActivation.waitForFinished(5000) &&
                   successfulActivation.exitStatus() == QProcess::NormalExit && successfulActivation.exitCode() == 0,
               "successful activation exits zero only after the existing instance responds");

    QTemporaryDir alternateProfile;
    const nsl::SingleInstanceEndpoint alternateEndpoint = nsl::singleInstanceEndpoint(
        QDir(alternateProfile.path()).filePath(QStringLiteral("netstats-live")));
    QProcessEnvironment alternateProfileEnv = processEnv;
    alternateProfileEnv.insert(QStringLiteral("XDG_CONFIG_HOME"), alternateProfile.path());
    QProcess alternateProfileActivation;
    alternateProfileActivation.setProcessEnvironment(alternateProfileEnv);
    alternateProfileActivation.setProgram(binaryPath);
    alternateProfileActivation.setArguments({QStringLiteral("--minimized")});
    alternateProfileActivation.start();
    expectTrue(alternateProfileActivation.waitForStarted(5000),
               "alternate-profile instance starts while a real DBus owner exists");
    const bool alternateProfileExited = alternateProfileActivation.waitForFinished(5500);
    expectTrue(!alternateProfileExited && alternateProfileActivation.state() == QProcess::Running,
               "alternate profile remains authoritative instead of activating another profile");
    if (sessionBus.interface() != nullptr) {
        const QDBusReply<bool> alternateServiceRegistered =
            sessionBus.interface()->isServiceRegistered(alternateEndpoint.serviceName);
        expectTrue(alternateServiceRegistered.isValid() && alternateServiceRegistered.value(),
                   "alternate profile registers a distinct DBus service");
    }
    if (alternateProfileActivation.state() != QProcess::NotRunning) {
        alternateProfileActivation.terminate();
        expectTrue(alternateProfileActivation.waitForFinished(10000), "alternate-profile owner exits cleanly");
    }

    if (owningInstance.state() != QProcess::NotRunning) {
        owningInstance.terminate();
        expectTrue(owningInstance.waitForFinished(10000), "isolated DBus owner exits cleanly");
    }

    QTemporaryDir aliasedProfiles;
    const QString physicalConfigRoot = QDir(aliasedProfiles.path()).filePath(QStringLiteral("physical-config"));
    const QString firstConfigAlias = QDir(aliasedProfiles.path()).filePath(QStringLiteral("config-alias-a"));
    const QString secondConfigAlias = QDir(aliasedProfiles.path()).filePath(QStringLiteral("config-alias-b"));
    expectTrue(QDir().mkpath(physicalConfigRoot), "physical aliased config root is created");
    expectTrue(QFile::link(physicalConfigRoot, firstConfigAlias), "first config-root symlink is created");
    expectTrue(QFile::link(physicalConfigRoot, secondConfigAlias), "second config-root symlink is created");

    QProcessEnvironment firstAliasEnv = processEnv;
    firstAliasEnv.insert(QStringLiteral("XDG_CONFIG_HOME"), firstConfigAlias);
    QProcess firstAliasOwner;
    firstAliasOwner.setProcessEnvironment(firstAliasEnv);
    firstAliasOwner.setProgram(binaryPath);
    firstAliasOwner.setArguments({QStringLiteral("--minimized")});
    firstAliasOwner.start();
    expectTrue(firstAliasOwner.waitForStarted(5000), "first aliased-profile owner starts");

    const nsl::SingleInstanceEndpoint canonicalAliasEndpoint = nsl::singleInstanceEndpoint(
        QDir(physicalConfigRoot).filePath(QStringLiteral("netstats-live")));
    bool aliasedServiceReady = false;
    if (sessionBus.interface() != nullptr) {
        for (int attempt = 0; attempt < 50 && !aliasedServiceReady; ++attempt) {
            const QDBusReply<bool> registered =
                sessionBus.interface()->isServiceRegistered(canonicalAliasEndpoint.serviceName);
            aliasedServiceReady = registered.isValid() && registered.value();
            if (!aliasedServiceReady) {
                QThread::msleep(100);
            }
        }
    }
    expectTrue(aliasedServiceReady, "aliased owner registers the canonical physical-profile DBus service");

    QProcessEnvironment secondAliasEnv = processEnv;
    secondAliasEnv.insert(QStringLiteral("XDG_CONFIG_HOME"), secondConfigAlias);
    QProcess secondAliasActivation;
    secondAliasActivation.setProcessEnvironment(secondAliasEnv);
    secondAliasActivation.setProgram(binaryPath);
    secondAliasActivation.setArguments({QStringLiteral("--minimized")});
    secondAliasActivation.start();
    expectTrue(secondAliasActivation.waitForStarted(5000), "second alias starts for canonical-profile activation");
    expectTrue(secondAliasActivation.waitForFinished(5000) &&
                   secondAliasActivation.exitStatus() == QProcess::NormalExit && secondAliasActivation.exitCode() == 0,
               "two aliases of one physical profile activate one authority");

    if (firstAliasOwner.state() != QProcess::NotRunning) {
        firstAliasOwner.terminate();
        expectTrue(firstAliasOwner.waitForFinished(10000), "aliased-profile owner exits cleanly");
    }

    return failures == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--migration-copy-failure-logging-check") == 0) {
        return runMigrationCopyFailureLoggingCheck(argc, argv);
    }
    if (argc > 1 && std::strcmp(argv[1], "--dbus-failure-semantics-check") == 0) {
        return runDbusFailureSemanticsCheck(argc, argv);
    }
    if (argc > 1 && std::strcmp(argv[1], "--autostart-symlink-migration-check") == 0) {
        return runAutoStartSymlinkMigrationCheck(argc, argv);
    }

    QTemporaryDir tempConfig;
    qputenv("XDG_CONFIG_HOME", tempConfig.path().toUtf8());
    qputenv("NSL_FAKE_DATE", "2026-06-30");

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("netstats-live-test"));
    QCoreApplication::setOrganizationName(QStringLiteral("NetStats-Live-Test"));

    using namespace nsl;

    expectEqual(AppSettings::currentMonthKey(), QStringLiteral("2026-06"), "NSL_FAKE_DATE yyyy-MM-dd controls month key");
    qputenv("NSL_FAKE_DATE", "2026-07");
    expectEqual(AppSettings::currentMonthKey(), QStringLiteral("2026-07"), "NSL_FAKE_DATE yyyy-MM controls month key");

    const QString legacyDir = QDir(tempConfig.path()).filePath(QStringLiteral("nsl-linux"));
    const QString newDir = QDir(tempConfig.path()).filePath(QStringLiteral("netstats-live"));
    expectTrue(QDir().mkpath(legacyDir), "legacy config directory created");
    expectTrue(QDir().mkpath(newDir), "new config directory created");
    const QString legacyConfigPath = QDir(legacyDir).filePath(QStringLiteral("nsl-linux.conf"));
    const QString newConfigPath = QDir(newDir).filePath(QStringLiteral("netstats-live.conf"));
    {
        QSettings legacy(legacyConfigPath, QSettings::IniFormat);
        legacy.setValue(QStringLiteral("config/autoMinimize"), true);
        legacy.setValue(QStringLiteral("remote/target"), QStringLiteral("legacy.example"));
        legacy.sync();
    }
    {
        QSettings existing(newConfigPath, QSettings::IniFormat);
        existing.setValue(QStringLiteral("remote/target"), QStringLiteral("existing.example"));
        existing.sync();
    }

    const QString legacyAutostartPath = QDir(QDir(tempConfig.path()).filePath(QStringLiteral("autostart"))).filePath(QStringLiteral("nsl-linux.desktop"));
    expectTrue(writeText(legacyAutostartPath,
                   QStringLiteral("[Desktop Entry]\n"
                                  "Type=Application\n"
                                  "Name=NSL-Linux\n"
                                  "Comment=AnalogX NetStat Live style network monitor for Linux\n"
                                  "Exec=\"/usr/bin/nsl-linux\" --minimized\n"
                                  "Icon=nsl-linux\n"
                                  "StartupWMClass=nsl-linux\n")),
        "legacy autostart desktop file created");

    AppSettings preexistingSettings;
    const AppConfigLoadResult preexistingLoad = preexistingSettings.load();
    AppConfig preexisting = preexistingLoad.config;
    expectTrue(preexistingLoad.storage.ok, "preexisting settings load succeeds");
    expectEqual(preexisting.remoteTarget, QStringLiteral("existing.example"), "existing netstats-live config is not overwritten by legacy migration");
    expectEqual(preexistingSettings.configPath(), newConfigPath, "config path uses netstats-live identity");
    const QString migratedAutostart = readAll(preexistingSettings.autoStartPath());
    expectTrue(QFile::exists(preexistingSettings.autoStartPath()), "legacy autostart entry migrates to netstats-live path");
    expectTrue(!QFile::exists(legacyAutostartPath), "legacy autostart entry is removed after migration");
    const QDir migratedAutoStartDirectory(QFileInfo(legacyAutostartPath).absolutePath());
    const QStringList retainedMigrationEntries =
        migratedAutoStartDirectory.entryList({QStringLiteral("*.nsl-rollback-*")}, QDir::Files | QDir::Hidden);
    bool legacyMigrationBytesRetained = false;
    for (const QString& retainedEntry : retainedMigrationEntries) {
        if (readAll(migratedAutoStartDirectory.filePath(retainedEntry)).contains(QStringLiteral("Name=NSL-Linux\n"))) {
            legacyMigrationBytesRetained = true;
            break;
        }
    }
    expectTrue(legacyMigrationBytesRetained, "migration retains hidden recovery bytes for the retired legacy entry");
    expectTrue(migratedAutostart.contains(QStringLiteral("Name=NetStats-Live\n")), "migrated autostart uses product name");
    expectTrue(migratedAutostart.contains(QStringLiteral("Exec=\"/usr/bin/netstats-live\" --minimized\n")), "migrated autostart updates Exec binary");
    expectTrue(migratedAutostart.contains(QStringLiteral("Icon=netstats-live\n")), "migrated autostart updates icon name");
    expectTrue(!migratedAutostart.contains(QStringLiteral("nsl-linux")), "migrated autostart has no legacy product identifier");
    expectTrue(writeText(legacyAutostartPath, QStringLiteral("[Desktop Entry]\nName=NSL-Linux\nExec=nsl-linux --minimized\n")), "duplicate legacy autostart desktop file recreated");
    AppSettings duplicateAutostartSettings;
    expectTrue(QFile::exists(duplicateAutostartSettings.autoStartPath()), "existing netstats-live autostart entry is preserved");
    expectTrue(!QFile::exists(legacyAutostartPath), "duplicate legacy autostart entry is removed when new entry exists");
    expectTrue(QFile::remove(newConfigPath), "remove preexisting config so copy migration can be tested");

    AppSettings settings;
    expectEqual(settings.configPath(), newConfigPath, "config path uses netstats-live identity");
    expectTrue(QFile::exists(settings.configPath()), "legacy nsl-linux config is copied to netstats-live path");
    const AppConfigLoadResult migratedLoad = settings.load();
    AppConfig migrated = migratedLoad.config;
    expectTrue(migratedLoad.storage.ok, "migrated settings load succeeds");
    expectTrue(migrated.autoMinimize, "legacy autoMinimize setting migrates");
    expectEqual(migrated.remoteTarget, QStringLiteral("legacy.example"), "legacy remote target migrates");

    qputenv("NSL_FAKE_DATE", "2026-06-30");
    settings.saveMonthlyTotals(QStringLiteral("2026-06"), 12345, 67890);

    qputenv("NSL_FAKE_DATE", "2026-07-01");
    AppConfig transitionedRollover;
    transitionedRollover.monthKey = QStringLiteral("2026-07");
    const AppConfig preservedRollover = preserveMonthOnFailedRollover(
        transitionedRollover,
        {false, QStringLiteral("forced rollover persistence failure")},
        QStringLiteral("2026-06"),
        12345,
        67890);
    expectEqual(preservedRollover.monthKey, QStringLiteral("2026-06"),
                "failed month rollover keeps the old month authoritative");
    expectEqual(preservedRollover.rxMonth, std::uint64_t{12345},
                "failed month rollover preserves old incoming totals");
    expectEqual(preservedRollover.txMonth, std::uint64_t{67890},
                "failed month rollover preserves old outgoing totals");
    const AppConfigLoadResult rolledLoad = settings.load();
    AppConfig rolled = rolledLoad.config;
    expectTrue(rolledLoad.storage.ok, "month rollover persistence succeeds");
    expectEqual(rolled.monthKey, QStringLiteral("2026-07"), "load rolls to fake current month");
    expectEqual(rolled.rxMonth, std::uint64_t{0}, "new month rx starts at zero");
    expectEqual(rolled.txMonth, std::uint64_t{0}, "new month tx starts at zero");
    expectEqual(rolled.lastRxMonth, std::uint64_t{12345}, "previous archived rx is available for Last Month display");
    expectEqual(rolled.lastTxMonth, std::uint64_t{67890}, "previous archived tx is available for Last Month display");

    QSettings raw(settings.configPath(), QSettings::IniFormat);
    expectEqual(raw.value(QStringLiteral("history/2026-06/rxMonth")).toULongLong(), qulonglong{12345}, "old rx archived under history month bucket");
    expectEqual(raw.value(QStringLiteral("history/2026-06/txMonth")).toULongLong(), qulonglong{67890}, "old tx archived under history month bucket");

    const QString buildPath = QDir(tempConfig.path()).filePath(QStringLiteral("build %F dir/netstats-live"));
    expectTrue(settings.setAutoStart(true, buildPath).ok, "autostart create succeeds");
    const QString autostartPath = settings.autoStartPath();
    expectTrue(QFile::exists(autostartPath), "autostart desktop file exists");
    const QString desktop = readAll(autostartPath);
    expectTrue(desktop.contains(QStringLiteral("X-KDE-autostart-after=panel\n")), "autostart waits until KDE panel/SNI host exists");
    expectTrue(desktop.contains(QStringLiteral("Name=NetStats-Live\n")), "autostart uses product name");
    expectTrue(desktop.contains(QStringLiteral("Icon=netstats-live\n")), "autostart uses packaged netstats-live icon name");
    QString escapedBuildPath = buildPath;
    escapedBuildPath.replace(QLatin1Char('%'), QStringLiteral("%%"));
    expectTrue(desktop.contains(QStringLiteral("Exec=\"") + escapedBuildPath + QStringLiteral("\" --minimized\n")),
               "autostart quotes build-dir Exec path, escapes field codes, and starts minimized");
    expectTrue(settings.setAutoStart(false, buildPath).ok, "autostart remove succeeds");
    expectTrue(!QFile::exists(autostartPath), "autostart desktop file removed");
    expectTrue(QDir().mkpath(autostartPath), "directory blocks autostart file commit and removal");
    const StorageResult autostartCommitFailure = settings.setAutoStart(true, buildPath);
    expectTrue(!autostartCommitFailure.ok && !autostartCommitFailure.error.isEmpty(),
               "autostart file commit failure returns structured error information");
    expectTrue(QFileInfo(autostartPath).isDir(), "failed autostart commit preserves the blocking directory");
    const StorageResult autostartRemoveFailure = settings.setAutoStart(false, buildPath);
    expectTrue(!autostartRemoveFailure.ok && !autostartRemoveFailure.error.isEmpty(),
               "autostart removal failure returns structured error information");
    expectTrue(QFileInfo(autostartPath).isDir(), "failed autostart removal preserves the blocking directory");
    expectTrue(QDir(autostartPath).removeRecursively(), "remove autostart blocking directory after failure checks");
    const QString missingAutoStartTarget = QDir(tempConfig.path()).filePath(QStringLiteral("missing-autostart-target"));
    expectTrue(QFile::link(missingAutoStartTarget, autostartPath), "dangling autostart symlink is created");
    AutoStartSnapshot danglingSnapshot;
    const StorageResult danglingSnapshotFailure = settings.snapshotAutoStart(danglingSnapshot);
    expectTrue(!danglingSnapshotFailure.ok && QFileInfo(autostartPath).isSymLink(),
               "dangling autostart symlink is rejected without mutation during snapshot");
    const StorageResult danglingDisableFailure = settings.setAutoStart(false, buildPath);
    expectTrue(!danglingDisableFailure.ok && QFileInfo(autostartPath).isSymLink(),
               "dangling autostart symlink is rejected without mutation during disable");
    expectTrue(QFile::remove(autostartPath), "dangling autostart symlink is removed after failure checks");
    expectTrue(writeText(autostartPath, QStringLiteral("[Desktop Entry]\nName=Snapshot\n")),
               "regular autostart entry is created for rollback symlink test");
    AutoStartSnapshot regularSnapshot;
    expectTrue(settings.snapshotAutoStart(regularSnapshot).ok, "regular autostart entry snapshot succeeds");
    expectTrue(QFile::remove(autostartPath), "snapshotted autostart file is removed before replacement");
    expectTrue(QFile::link(missingAutoStartTarget, autostartPath),
               "dangling symlink replaces the snapshotted autostart file");
    const StorageResult danglingRestoreFailure = settings.restoreAutoStart(regularSnapshot);
    expectTrue(!danglingRestoreFailure.ok && QFileInfo(autostartPath).isSymLink(),
               "rollback rejects replacement dangling symlink without mutation");
    expectTrue(QFile::remove(autostartPath), "replacement dangling symlink is removed after rollback test");

    const QString originalContents = QStringLiteral("[Desktop Entry]\nName=Original\n");
    const QString substituteContents = QStringLiteral("[Desktop Entry]\nName=Concurrent Substitute\n");
    expectTrue(writeText(autostartPath, originalContents), "original autostart entry is created for identity tests");
    AutoStartSnapshot disableTransaction;
    expectTrue(settings.snapshotAutoStart(disableTransaction).ok, "disable transaction snapshots original identity");
    expectTrue(QFile::remove(autostartPath), "original entry is removed to simulate substitution");
    expectTrue(writeText(autostartPath, substituteContents), "concurrent substitute is installed before disable");
    AutoStartSnapshot substituteIdentity;
    expectTrue(settings.snapshotAutoStart(substituteIdentity).ok, "substitute identity is observed for inode-reuse simulation");
    disableTransaction.device = substituteIdentity.device;
    disableTransaction.inode = substituteIdentity.inode;
    const StorageResult substitutedDisable = settings.setAutoStart(false, buildPath, disableTransaction);
    expectTrue(!substitutedDisable.ok, "disable rejects a regular file substituted after snapshot");
    expectEqual(readAll(autostartPath), substituteContents, "rejected disable preserves the substituted regular file");

    expectTrue(QFile::remove(autostartPath), "substitute is removed for no-replace enable test");
    AutoStartSnapshot absentEnableTransaction;
    expectTrue(settings.snapshotAutoStart(absentEnableTransaction).ok && !absentEnableTransaction.existed,
               "enable transaction snapshots absence");
    expectTrue(writeText(autostartPath, substituteContents), "concurrent file is installed before enable publication");
    const StorageResult substitutedEnable = settings.setAutoStart(true, buildPath, absentEnableTransaction);
    expectTrue(!substitutedEnable.ok, "enable refuses to overwrite a file created after absence snapshot");
    expectEqual(readAll(autostartPath), substituteContents, "rejected enable preserves the concurrent regular file");

    expectTrue(QFile::remove(autostartPath), "concurrent file is removed for parent-authority test");
    AutoStartSnapshot parentAuthorityTransaction;
    expectTrue(settings.snapshotAutoStart(parentAuthorityTransaction).ok && !parentAuthorityTransaction.existed,
               "parent-authority transaction snapshots absence");
    const QString autoStartDirectoryPath = QFileInfo(autostartPath).absolutePath();
    const QString movedAutoStartDirectoryPath = autoStartDirectoryPath + QStringLiteral("-authority-original");
    expectTrue(QDir().rename(autoStartDirectoryPath, movedAutoStartDirectoryPath),
               "original autostart directory is moved after snapshot");
    expectTrue(QDir().mkpath(autoStartDirectoryPath), "replacement autostart directory is created at public path");
    const StorageResult replacedParentPublication =
        settings.setAutoStart(true, buildPath, parentAuthorityTransaction);
    expectTrue(!replacedParentPublication.ok,
               "publication rejects a parent directory replaced after snapshot");
    expectTrue(!QFile::exists(autostartPath), "replacement parent receives no generated entry");
    expectTrue(!QFile::exists(QDir(movedAutoStartDirectoryPath).filePath(QStringLiteral("netstats-live.desktop"))),
               "moved original parent receives no generated entry after authority loss");
    expectTrue(QDir(autoStartDirectoryPath).removeRecursively(), "replacement autostart directory is removed");
    expectTrue(QDir().rename(movedAutoStartDirectoryPath, autoStartDirectoryPath),
               "original autostart directory is restored after authority test");

    AutoStartSnapshot createdCommitTransaction;
    expectTrue(settings.snapshotAutoStart(createdCommitTransaction).ok && !createdCommitTransaction.existed,
               "created-entry commit transaction snapshots absence");
    expectTrue(settings.setAutoStart(true, buildPath, createdCommitTransaction).ok,
               "created-entry commit transaction publishes generated content");
    expectTrue(QFile::remove(autostartPath), "generated created entry is removed before commit substitution");
    expectTrue(writeText(autostartPath, substituteContents), "concurrent destination replaces created entry before commit");
    const StorageResult substitutedCreatedCommit = settings.commitAutoStart(createdCommitTransaction);
    expectTrue(!substitutedCreatedCommit.ok, "created-entry commit validates the published destination token");
    expectEqual(readAll(autostartPath), substituteContents, "failed created-entry commit preserves the substitute");
    expectTrue(QFile::remove(autostartPath), "created-entry substitute is removed before rollback retry");
    expectTrue(settings.restoreAutoStart(createdCommitTransaction).ok,
               "created-entry rollback accepts already-restored absence after conflict clears");

    AutoStartSnapshot absentDisableTransaction;
    expectTrue(settings.snapshotAutoStart(absentDisableTransaction).ok && !absentDisableTransaction.existed,
               "disable-from-absence transaction snapshots absence");
    expectTrue(settings.setAutoStart(false, buildPath, absentDisableTransaction).ok,
               "disable-from-absence transaction begins successfully");
    expectTrue(writeText(autostartPath, substituteContents),
               "concurrent destination appears before disable-from-absence commit");
    const StorageResult substitutedAbsentDisableCommit = settings.commitAutoStart(absentDisableTransaction);
    expectTrue(!substitutedAbsentDisableCommit.ok,
               "disable-from-absence commit revalidates absence after config persistence");
    expectEqual(readAll(autostartPath), substituteContents,
                "failed disable-from-absence commit preserves the concurrent destination");
    expectTrue(QFile::remove(autostartPath), "disable-from-absence substitute is removed before rollback retry");
    expectTrue(settings.restoreAutoStart(absentDisableTransaction).ok,
               "disable-from-absence rollback succeeds after the conflict clears");

    expectTrue(writeText(autostartPath, originalContents), "original entry is recreated for rollback publication test");
    AutoStartSnapshot rollbackTransaction;
    expectTrue(settings.snapshotAutoStart(rollbackTransaction).ok, "rollback transaction snapshots original entry");
    expectTrue(settings.setAutoStart(false, buildPath, rollbackTransaction).ok,
               "transactional disable stages the original entry without destroying it");
    expectTrue(!QFile::exists(autostartPath), "transactional disable removes the published path");
    expectTrue(writeText(autostartPath, substituteContents), "concurrent file is installed before rollback");
    const StorageResult substitutedRollback = settings.restoreAutoStart(rollbackTransaction);
    expectTrue(!substitutedRollback.ok, "rollback refuses to overwrite a concurrent regular file");
    expectEqual(readAll(autostartPath), substituteContents, "failed rollback preserves the concurrent regular file");
    expectTrue(QFile::remove(autostartPath), "concurrent file is removed before committing the staged disable");
    expectTrue(settings.commitAutoStart(rollbackTransaction).ok,
               "staged original is retained only after the conflicting destination clears");

    expectTrue(writeText(autostartPath, originalContents), "custom entry is recreated for enable preservation test");
    AutoStartSnapshot existingEnableTransaction;
    expectTrue(settings.snapshotAutoStart(existingEnableTransaction).ok, "existing enable transaction snapshots custom entry");
    expectTrue(settings.setAutoStart(true, buildPath, existingEnableTransaction).ok,
               "enabling safely replaces the exact snapshotted regular entry");
    expectTrue(readAll(autostartPath).contains(QStringLiteral("Name=NetStats-Live\n")),
               "identity-bound enable publishes the generated entry");
    expectTrue(QFile::remove(autostartPath), "generated entry is removed to simulate pre-commit substitution");
    expectTrue(writeText(autostartPath, substituteContents), "concurrent substitute is installed before commit");
    const StorageResult substitutedCommit = settings.commitAutoStart(existingEnableTransaction);
    expectTrue(!substitutedCommit.ok, "commit refuses to discard the original behind a substituted destination");
    expectEqual(readAll(autostartPath), substituteContents, "failed commit preserves the concurrent destination");
    expectTrue(QFile::remove(autostartPath), "concurrent destination is removed before rollback retry");
    expectTrue(settings.restoreAutoStart(existingEnableTransaction).ok,
               "rollback restores the retained original after the conflicting destination clears");
    expectEqual(readAll(autostartPath), originalContents, "rollback recovers the exact original after failed commit");
    expectTrue(settings.setAutoStart(true, buildPath, existingEnableTransaction).ok,
               "identity-bound replacement can be retried after rollback");
    expectTrue(settings.commitAutoStart(existingEnableTransaction).ok, "existing-entry enable transaction commits");
    expectTrue(QFile::remove(autostartPath), "custom entry is removed after identity tests");
    const QDir autoStartDirectory(QFileInfo(autostartPath).absolutePath());
    const QStringList retainedEntries =
        autoStartDirectory.entryList({QStringLiteral("*.nsl-rollback-*")}, QDir::Files | QDir::Hidden);
    bool originalBytesRetained = false;
    for (const QString& retainedEntry : retainedEntries) {
        if (readAll(autoStartDirectory.filePath(retainedEntry)) == originalContents) {
            originalBytesRetained = true;
            break;
        }
    }
    expectTrue(originalBytesRetained, "successful replacement retains the original bytes as hidden recovery data");
    expectTrue(autoStartDirectory.entryList({QStringLiteral(".netstats-live-autostart-*")},
                                            QDir::Files | QDir::Hidden)
                   .isEmpty(),
               "unnamed publication leaves no visible temporary artifact");

    expectTrue(QDir(newDir).removeRecursively(), "settings directory removed for write-failure checks");
    expectTrue(writeText(newDir, QStringLiteral("not a directory\n")), "file blocks settings writes");
    AppConfig unwritableConfig = rolled;
    const StorageResult configFailure = settings.saveConfig(unwritableConfig);
    expectTrue(!configFailure.ok && !configFailure.error.isEmpty(),
               "config write failure returns structured error information");
    const StorageResult totalsFailure = settings.saveMonthlyTotals(QStringLiteral("2026-07"), 1, 2);
    expectTrue(!totalsFailure.ok && !totalsFailure.error.isEmpty(),
               "totals write failure returns structured error information");
    const StorageResult positionFailure = settings.saveWindowPosition(QPoint(10, 20));
    expectTrue(!positionFailure.ok && !positionFailure.error.isEmpty(),
               "window-position write failure returns structured error information");
    const AppConfigLoadResult loadFailure = settings.load();
    expectTrue(!loadFailure.storage.ok && !loadFailure.storage.error.isEmpty(),
               "settings read or rollover failure returns structured error information");

    expectTrue(shouldShowMainWindow(false, false, true), "normal startup shows window");
    expectTrue(!shouldShowMainWindow(true, false, true), "--minimized hides window when tray exists");
    expectTrue(!shouldShowMainWindow(false, true, true), "Auto Minimize hides window when tray exists");
    expectTrue(shouldShowMainWindow(true, false, false), "--minimized is ignored without tray");
    expectTrue(shouldShowMainWindow(false, true, false), "Auto Minimize is ignored without tray");
    expectTrue(shouldRestoreForTrayLoss(false, false, false, false),
               "hidden window restores when the tray host disappears");
    expectTrue(!shouldRestoreForTrayLoss(true, false, false, false),
               "hidden window stays hidden while the tray host is available");
    expectTrue(!shouldRestoreForTrayLoss(false, true, false, true),
               "visible exposed window needs no tray-loss recovery");
    expectTrue(shouldRestoreForTrayLoss(false, true, true, false),
               "minimized window restores when the tray host disappears even though Qt reports it visible");
    expectTrue(shouldRestoreForTrayLoss(false, true, false, false),
               "unexposed Wayland surface restores when the tray host disappears");
    expectEqual(trayToggleAction(true, false, true), TrayToggleAction::Minimize,
                "an exposed normal window minimizes on tray activation");
    expectEqual(trayToggleAction(true, true, false), TrayToggleAction::Restore,
                "a minimized window explicitly restores on tray activation");
    expectEqual(trayToggleAction(true, false, false), TrayToggleAction::PlatformRestorePending,
                "a Wayland pre-activated but unexposed window uses the native-or-remap restore path");
    expectEqual(trayToggleAction(false, false, false), TrayToggleAction::Restore,
                "a startup-hidden window explicitly restores on tray activation");
    expectEqual(instanceActivationAction(true, true, false), InstanceActivationAction::Activate,
                "an exposed existing window only needs activation");
    expectEqual(instanceActivationAction(true, true, true), InstanceActivationAction::NativeRestore,
                "a forwarded token is used even when the existing surface is already exposed");
    expectEqual(instanceActivationAction(false, true, true), InstanceActivationAction::NativeRestore,
                "a token and native integration preserve an unexposed Wayland surface");
    expectEqual(instanceActivationAction(false, true, false), InstanceActivationAction::Remap,
                "native activation without a forwarded token uses the reliable remap fallback");
    expectEqual(instanceActivationAction(false, false, true), InstanceActivationAction::Remap,
                "a generic build remaps an unexposed surface even when a token was forwarded");
    bool tokenObservedDuringActivation = false;
    qputenv("XDG_ACTIVATION_TOKEN", "stale-token");
    withActivationToken(QStringLiteral("forwarded-token"), [&]() {
        tokenObservedDuringActivation = qEnvironmentVariable("XDG_ACTIVATION_TOKEN") == QStringLiteral("forwarded-token");
    });
    expectTrue(tokenObservedDuringActivation, "forwarded activation token is installed during the activation callback");
    expectTrue(!qEnvironmentVariableIsSet("XDG_ACTIVATION_TOKEN"), "forwarded activation token is removed after the callback");
    bool tokenlessCallbackWasClean = false;
    qputenv("XDG_ACTIVATION_TOKEN", "another-stale-token");
    withActivationToken(QString(), [&]() {
        tokenlessCallbackWasClean = !qEnvironmentVariableIsSet("XDG_ACTIVATION_TOKEN");
    });
    expectTrue(tokenlessCallbackWasClean, "tokenless activation callback cannot inherit a stale token");
    expectTrue(!qEnvironmentVariableIsSet("XDG_ACTIVATION_TOKEN"), "tokenless activation leaves no environment token");
    const QList<QRect> screens = {QRect(0, 0, 1920, 1080), QRect(-1280, 0, 1280, 1024)};
    const QSize windowSize(238, 523);
    expectEqual(visibleWindowPosition(QPoint(100, 100), windowSize, screens), QPoint(100, 100),
                "onscreen window position is preserved");
    expectEqual(visibleWindowPosition(QPoint(-1200, 80), windowSize, screens), QPoint(-1200, 80),
                "valid negative position on a left monitor is preserved");
    expectEqual(visibleWindowPosition(QPoint(5000, 5000), windowSize, screens), QPoint(1682, 557),
                "fully offscreen position is clamped to the nearest screen");
    expectEqual(visibleWindowPosition(QPoint(1900, 100), windowSize, screens), QPoint(1682, 100),
                "position with an unusably narrow visible sliver is clamped");
    expectTrue(!singleInstanceServiceName().isEmpty(), "single-instance service name present");
    expectTrue(!singleInstanceObjectPath().isEmpty(), "single-instance object path present");

    QTemporaryDir signalConfig;
    expectEqual(singleInstanceServiceName(), QStringLiteral("io.github.DeviousVon.NetStatsLive"), "single-instance DBus service uses product identity");
    expectEqual(singleInstanceObjectPath(), QStringLiteral("/io/github/DeviousVon/NetStatsLive/MainWindow"), "single-instance DBus object path uses product identity");
    expectEqual(singleInstanceInterfaceName(), QStringLiteral("io.github.DeviousVon.NetStatsLive"), "single-instance DBus interface uses product identity");
    const SingleInstanceEndpoint defaultProfileEndpoint = singleInstanceEndpoint(
        QDir(QDir::homePath()).filePath(QStringLiteral(".config/netstats-live")));
    expectEqual(defaultProfileEndpoint.serviceName, singleInstanceServiceName(),
                "default profile preserves the historical DBus service name");
    expectEqual(defaultProfileEndpoint.objectPath, singleInstanceObjectPath(),
                "default profile preserves the historical DBus object path");
    const SingleInstanceEndpoint isolatedProfileEndpoint = singleInstanceEndpoint(newDir);
    expectTrue(isolatedProfileEndpoint.serviceName != singleInstanceServiceName(),
               "non-default profile receives a distinct DBus service name");
    expectTrue(isolatedProfileEndpoint.objectPath != singleInstanceObjectPath(),
               "non-default profile receives a distinct DBus object path");
    expectEqual(isolatedProfileEndpoint.interfaceName, singleInstanceInterfaceName(),
                "all profiles preserve the public DBus interface contract");

    QTemporaryDir nonPersistentSimulationConfig;
    QProcess nonPersistentSimulation;
    QProcessEnvironment simulationEnv = QProcessEnvironment::systemEnvironment();
    simulationEnv.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    simulationEnv.insert(QStringLiteral("XDG_CONFIG_HOME"), nonPersistentSimulationConfig.path());
    simulationEnv.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("unix:path=%1/no-session-bus").arg(nonPersistentSimulationConfig.path()));
    nonPersistentSimulation.setProcessEnvironment(simulationEnv);
    const QString binaryPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("netstats-live"));
    expectTrue(QFile::exists(binaryPath), "netstats-live binary exists beside lifecycle test");
    nonPersistentSimulation.setProgram(binaryPath);
    nonPersistentSimulation.setArguments({QStringLiteral("--simulate"), QStringLiteral("--minimized")});
    nonPersistentSimulation.start();
    expectTrue(nonPersistentSimulation.waitForStarted(5000), "non-persistent simulation starts");
    if (nonPersistentSimulation.state() != QProcess::NotRunning) {
        QThread::sleep(1);
        nonPersistentSimulation.terminate();
        expectTrue(nonPersistentSimulation.waitForFinished(10000), "non-persistent simulation exits on SIGTERM");
    }
    const QString nonPersistentConfigPath = QDir(nonPersistentSimulationConfig.path()).filePath(QStringLiteral("netstats-live/netstats-live.conf"));
    expectTrue(!QFile::exists(nonPersistentConfigPath), "plain --simulate does not write synthetic totals to user config");
    const QString nonPersistentLockPath = QDir(nonPersistentSimulationConfig.path()).filePath(QStringLiteral("netstats-live/netstats-live.lock"));
    expectTrue(!QFile::exists(nonPersistentLockPath), "plain --simulate does not claim the persistent profile lock");

    QTemporaryDir forbiddenSimulationConfig;
    QProcess forbiddenSimulation;
    QProcessEnvironment forbiddenSimulationEnv = QProcessEnvironment::systemEnvironment();
    forbiddenSimulationEnv.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    forbiddenSimulationEnv.insert(QStringLiteral("XDG_CONFIG_HOME"), forbiddenSimulationConfig.path());
    forbiddenSimulationEnv.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("unix:path=%1/no-session-bus").arg(forbiddenSimulationConfig.path()));
    forbiddenSimulation.setProcessEnvironment(forbiddenSimulationEnv);
    forbiddenSimulation.setProgram(binaryPath);
    forbiddenSimulation.setArguments({QStringLiteral("--simulate"), QStringLiteral("--test-persist-simulation")});
    forbiddenSimulation.start();
    const bool forbiddenFlagRejected = forbiddenSimulation.waitForFinished(2000);
    if (!forbiddenFlagRejected) {
        forbiddenSimulation.terminate();
        forbiddenSimulation.waitForFinished(5000);
    }
    expectTrue(forbiddenFlagRejected && forbiddenSimulation.exitCode() != 0,
               "production binary rejects the test-only simulation persistence flag");
    const QString forbiddenConfigPath = QDir(forbiddenSimulationConfig.path()).filePath(QStringLiteral("netstats-live/netstats-live.conf"));
    expectTrue(!QFile::exists(forbiddenConfigPath), "rejected test-only flag cannot write synthetic user config");

    QTemporaryDir singleInstanceConfig;
    QProcessEnvironment singleInstanceEnv = QProcessEnvironment::systemEnvironment();
    singleInstanceEnv.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    singleInstanceEnv.insert(QStringLiteral("XDG_CONFIG_HOME"), singleInstanceConfig.path());
    singleInstanceEnv.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                             QStringLiteral("unix:path=%1/no-session-bus").arg(singleInstanceConfig.path()));

    QProcess firstPersistentInstance;
    firstPersistentInstance.setProcessEnvironment(singleInstanceEnv);
    firstPersistentInstance.setProgram(binaryPath);
    firstPersistentInstance.setArguments({QStringLiteral("--minimized")});
    firstPersistentInstance.start();
    expectTrue(firstPersistentInstance.waitForStarted(5000), "first persistent instance starts without session DBus");
    QThread::msleep(500);
    expectTrue(firstPersistentInstance.state() == QProcess::Running,
               "first persistent instance remains active without session DBus");
    const QString sharedSettingsPath = QDir(singleInstanceConfig.path()).filePath(QStringLiteral("netstats-live/netstats-live.conf"));
    const QString settingsBeforeCompetingLaunch = readAll(sharedSettingsPath);

    QProcess competingPersistentInstance;
    competingPersistentInstance.setProcessEnvironment(singleInstanceEnv);
    competingPersistentInstance.setProgram(binaryPath);
    competingPersistentInstance.setArguments({QStringLiteral("--minimized")});
    competingPersistentInstance.start();
    expectTrue(competingPersistentInstance.waitForStarted(5000), "competing persistent instance process starts");
    const bool competingInstanceRejected = competingPersistentInstance.waitForFinished(2000);
    if (!competingInstanceRejected) {
        competingPersistentInstance.terminate();
        competingPersistentInstance.waitForFinished(10000);
    }
    expectTrue(competingInstanceRejected && competingPersistentInstance.exitStatus() == QProcess::NormalExit &&
                   competingPersistentInstance.exitCode() != 0,
               "competing persistent instance is rejected without session DBus");
    expectTrue(QString::fromUtf8(competingPersistentInstance.readAllStandardError()).contains(QStringLiteral("already running"),
                                                                                                Qt::CaseInsensitive),
               "competing persistent instance reports the lock owner clearly");
    expectEqual(readAll(sharedSettingsPath), settingsBeforeCompetingLaunch,
                "rejected competing instance cannot modify the owner's settings bytes");

    if (firstPersistentInstance.state() != QProcess::NotRunning) {
        firstPersistentInstance.terminate();
        expectTrue(firstPersistentInstance.waitForFinished(10000), "first persistent instance exits cleanly");
    }

    QProcess replacementPersistentInstance;
    replacementPersistentInstance.setProcessEnvironment(singleInstanceEnv);
    replacementPersistentInstance.setProgram(binaryPath);
    replacementPersistentInstance.setArguments({QStringLiteral("--minimized")});
    replacementPersistentInstance.start();
    expectTrue(replacementPersistentInstance.waitForStarted(5000), "replacement persistent instance starts after lock owner exits");
    QThread::msleep(500);
    expectTrue(replacementPersistentInstance.state() == QProcess::Running,
               "replacement persistent instance owns the released profile lock");
    if (replacementPersistentInstance.state() != QProcess::NotRunning) {
        replacementPersistentInstance.kill();
        expectTrue(replacementPersistentInstance.waitForFinished(10000), "replacement persistent instance is killed for stale-lock recovery");
    }
    const QString staleLockPath = QDir(singleInstanceConfig.path()).filePath(QStringLiteral("netstats-live/netstats-live.lock"));
    expectTrue(QFile::exists(staleLockPath), "SIGKILL leaves the prior owner's lock file for stale detection");

    QProcess staleLockRecoveryInstance;
    staleLockRecoveryInstance.setProcessEnvironment(singleInstanceEnv);
    staleLockRecoveryInstance.setProgram(binaryPath);
    staleLockRecoveryInstance.setArguments({QStringLiteral("--minimized")});
    staleLockRecoveryInstance.start();
    expectTrue(staleLockRecoveryInstance.waitForStarted(5000), "stale-lock recovery process starts");
    QThread::msleep(500);
    expectTrue(staleLockRecoveryInstance.state() == QProcess::Running,
               "dead owner's stale lock is recovered without manual deletion");
    if (staleLockRecoveryInstance.state() != QProcess::NotRunning) {
        staleLockRecoveryInstance.terminate();
        expectTrue(staleLockRecoveryInstance.waitForFinished(10000), "stale-lock recovery instance exits cleanly");
    }

    QProcess process;
    QProcessEnvironment processEnv = QProcessEnvironment::systemEnvironment();
    processEnv.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    processEnv.insert(QStringLiteral("XDG_CONFIG_HOME"), signalConfig.path());
    processEnv.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("unix:path=%1/no-session-bus").arg(signalConfig.path()));
    const QString signalConfigPath = QDir(signalConfig.path()).filePath(QStringLiteral("netstats-live/netstats-live.conf"));
    expectTrue(QDir().mkpath(QFileInfo(signalConfigPath).absolutePath()), "SIGTERM fixture config directory created");
    {
        QSettings seededSignalSettings(signalConfigPath, QSettings::IniFormat);
        seededSignalSettings.setValue(QStringLiteral("totals/monthKey"), AppSettings::currentMonthKey());
        seededSignalSettings.setValue(QStringLiteral("totals/rxMonth"), qulonglong{12345});
        seededSignalSettings.setValue(QStringLiteral("totals/txMonth"), qulonglong{67890});
        seededSignalSettings.sync();
    }
    process.setProcessEnvironment(processEnv);
    process.setProgram(binaryPath);
    process.setArguments({QStringLiteral("--minimized")});
    process.start();
    expectTrue(process.waitForStarted(5000), "app starts for SIGTERM persistence test");
    if (process.state() != QProcess::NotRunning) {
        QThread::sleep(4);
        process.terminate();
        expectTrue(process.waitForFinished(10000), "SIGTERM exits app cleanly");
        QSettings signalSettings(signalConfigPath, QSettings::IniFormat);
        const qulonglong rx = signalSettings.value(QStringLiteral("totals/rxMonth")).toULongLong();
        const qulonglong tx = signalSettings.value(QStringLiteral("totals/txMonth")).toULongLong();
        expectTrue(rx >= 12345 && tx >= 67890, "SIGTERM preserves and saves non-zero monthly totals");
        expectTrue(signalSettings.contains(QStringLiteral("config/autoMinimize")),
                   "SIGTERM writes normal config fields before exit");
    }

    if (failures != 0) {
        std::cerr << failures << " lifecycle test failure(s)\n";
        return 1;
    }
    return 0;
}
