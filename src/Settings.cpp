// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "Settings.h"

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

namespace nsl {
namespace {

QString configBaseDir() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
}

QString configDir() {
    return QDir(configBaseDir()).filePath(QStringLiteral("netstats-live"));
}

QString legacyConfigPath() {
    return QDir(QDir(configBaseDir()).filePath(QStringLiteral("nsl-linux"))).filePath(QStringLiteral("nsl-linux.conf"));
}

QString autostartDir() {
    return QDir(configBaseDir()).filePath(QStringLiteral("autostart"));
}

QString newAutoStartPath() {
    return QDir(autostartDir()).filePath(QStringLiteral("netstats-live.desktop"));
}

QString legacyAutoStartPath() {
    return QDir(autostartDir()).filePath(QStringLiteral("nsl-linux.desktop"));
}

void migrateLegacyConfigIfNeeded(const QString& settingsPath) {
    if (QFile::exists(settingsPath)) {
        return;
    }
    const QString legacyPath = legacyConfigPath();
    if (QFile::exists(legacyPath)) {
        QFile legacyFile(legacyPath);
        if (!legacyFile.copy(settingsPath)) {
            qWarning().noquote() << "Unable to migrate legacy config" << legacyPath << "to" << settingsPath << ":" << legacyFile.errorString();
        }
    }
}

void migrateLegacyAutoStartIfNeeded() {
    const QString newPath = newAutoStartPath();
    const QString legacyPath = legacyAutoStartPath();
    if (QFile::exists(newPath)) {
        if (QFile::exists(legacyPath) && !QFile::remove(legacyPath)) {
            qWarning().noquote() << "Could not remove legacy autostart entry" << legacyPath << "after finding" << newPath;
        }
        return;
    }

    if (!QFile::exists(legacyPath)) {
        return;
    }

    QFile legacyFile(legacyPath);
    if (!legacyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning().noquote() << "Unable to read legacy autostart entry" << legacyPath << ":" << legacyFile.errorString();
        return;
    }

    QString migrated = QString::fromUtf8(legacyFile.readAll());
    migrated.replace(QStringLiteral("AnalogX NetStat Live style network monitor for Linux"), QStringLiteral("Live network throughput and CPU monitor widget for Linux"));
    migrated.replace(QStringLiteral("NSL-Linux"), QStringLiteral("NetStats-Live"));
    migrated.replace(QStringLiteral("nsl-linux"), QStringLiteral("netstats-live"));

    if (!QDir().mkpath(QFileInfo(newPath).absolutePath())) {
        qWarning().noquote() << "Unable to create autostart directory for" << newPath;
        return;
    }

    QSaveFile file(newPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning().noquote() << "Unable to create migrated autostart entry" << newPath << ":" << file.errorString();
        return;
    }
    QTextStream stream(&file);
    stream << migrated;
    if (!file.commit()) {
        qWarning().noquote() << "Unable to commit migrated autostart entry" << newPath << ":" << file.errorString();
        return;
    }

    if (!QFile::remove(legacyPath)) {
        qWarning().noquote() << "Migrated autostart entry to" << newPath << "but could not remove legacy entry" << legacyPath;
    }
}

QString boolKey(PaneId id) {
    return QStringLiteral("panes/%1").arg(paneConfigKey(id));
}

void archiveMonthlyTotals(QSettings& settings, const QString& monthKey, std::uint64_t rxBytes, std::uint64_t txBytes) {
    if (monthKey.isEmpty()) {
        return;
    }
    settings.setValue(QStringLiteral("history/%1/rxMonth").arg(monthKey), QVariant::fromValue<qulonglong>(rxBytes));
    settings.setValue(QStringLiteral("history/%1/txMonth").arg(monthKey), QVariant::fromValue<qulonglong>(txBytes));
}

// "Last Month" is derived from archived history so a calendar rollover can
// reset active totals without losing the just-finished month in the UI.
QString previousMonthKey(const QString& monthKey) {
    const QDate firstOfMonth = QDate::fromString(monthKey + QStringLiteral("-01"), QStringLiteral("yyyy-MM-dd"));
    return firstOfMonth.isValid() ? firstOfMonth.addMonths(-1).toString(QStringLiteral("yyyy-MM")) : QString();
}

std::uint64_t archivedMonthTotal(QSettings& settings, const QString& monthKey, const QString& direction) {
    if (monthKey.isEmpty()) {
        return 0;
    }
    return settings.value(QStringLiteral("history/%1/%2Month").arg(monthKey, direction), 0).toULongLong();
}

QString resolvedExecutablePath(const QString& executablePath) {
    const QFileInfo info(executablePath);
    if (info.isAbsolute()) {
        return info.absoluteFilePath();
    }
    const QString found = QStandardPaths::findExecutable(executablePath);
    return found.isEmpty() ? executablePath : found;
}

QString quoteExecArgument(QString argument) {
    argument.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    argument.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(argument);
}

} // namespace

QString paneConfigKey(PaneId id) {
    switch (id) {
    case PaneId::LocalMachine: return QStringLiteral("localMachine");
    case PaneId::RemoteMachine: return QStringLiteral("remoteMachine");
    case PaneId::IncomingTotals: return QStringLiteral("incomingTotals");
    case PaneId::Incoming: return QStringLiteral("incoming");
    case PaneId::OutgoingTotals: return QStringLiteral("outgoingTotals");
    case PaneId::Outgoing: return QStringLiteral("outgoing");
    case PaneId::Threads: return QStringLiteral("threads");
    case PaneId::Cpu: return QStringLiteral("cpu");
    case PaneId::Count: break;
    }
    return QStringLiteral("unknown");
}

QString paneDisplayName(PaneId id) {
    switch (id) {
    case PaneId::LocalMachine: return QStringLiteral("Local Machine");
    case PaneId::RemoteMachine: return QStringLiteral("Remote Machine");
    case PaneId::IncomingTotals: return QStringLiteral("Incoming Totals");
    case PaneId::Incoming: return QStringLiteral("Incoming");
    case PaneId::OutgoingTotals: return QStringLiteral("Outgoing Totals");
    case PaneId::Outgoing: return QStringLiteral("Outgoing");
    case PaneId::Threads: return QStringLiteral("Threads");
    case PaneId::Cpu: return QStringLiteral("CPU");
    case PaneId::Count: break;
    }
    return QStringLiteral("Unknown");
}

AppSettings::AppSettings()
    : settingsPath_(QDir(configDir()).filePath(QStringLiteral("netstats-live.conf"))) {
    if (!QDir().mkpath(configDir())) {
        qWarning().noquote() << "Unable to create config directory" << configDir();
    }
    migrateLegacyConfigIfNeeded(settingsPath_);
    migrateLegacyAutoStartIfNeeded();
}

QString AppSettings::currentMonthKey() {
    const QString fakeDate = QString::fromLocal8Bit(qgetenv("NSL_FAKE_DATE")).trimmed();
    if (!fakeDate.isEmpty()) {
        if (fakeDate.size() == 7) {
            const QDate month = QDate::fromString(fakeDate + QStringLiteral("-01"), QStringLiteral("yyyy-MM-dd"));
            if (month.isValid()) {
                return month.toString(QStringLiteral("yyyy-MM"));
            }
        }
        const QDate date = QDate::fromString(fakeDate, QStringLiteral("yyyy-MM-dd"));
        if (date.isValid()) {
            return date.toString(QStringLiteral("yyyy-MM"));
        }
    }
    return QDate::currentDate().toString(QStringLiteral("yyyy-MM"));
}

AppConfig AppSettings::load() const {
    AppConfig config;
    config.panes.fill(true);

    QSettings settings(settingsPath_, QSettings::IniFormat);
    for (int i = 0; i < PaneCount; ++i) {
        const auto id = static_cast<PaneId>(i);
        config.panes[static_cast<std::size_t>(i)] = settings.value(boolKey(id), true).toBool();
    }
    config.autoMinimize = settings.value(QStringLiteral("config/autoMinimize"), false).toBool();
    config.autoStart = settings.value(QStringLiteral("config/autoStart"), false).toBool();
    config.urlClipCap = settings.value(QStringLiteral("config/urlClipCap"), false).toBool();
    config.alwaysOnTop = settings.value(QStringLiteral("config/alwaysOnTop"), false).toBool();
    config.unitMode = settings.value(QStringLiteral("config/unitMode"), QStringLiteral("bytes")).toString() == QStringLiteral("bits")
        ? UnitMode::Bits
        : UnitMode::Bytes;
    config.selectedInterface = settings.value(QStringLiteral("config/interface"), QStringLiteral("ALL")).toString();
    if (config.selectedInterface.isEmpty()) {
        config.selectedInterface = QStringLiteral("ALL");
    }
    config.remoteTarget = settings.value(QStringLiteral("remote/target")).toString();
    config.windowPos = settings.value(QStringLiteral("window/pos"), QPoint()).toPoint();

    const QString currentMonth = currentMonthKey();
    const QString previousMonth = previousMonthKey(currentMonth);
    const QString storedMonth = settings.value(QStringLiteral("totals/monthKey"), currentMonth).toString();
    const std::uint64_t storedRxMonth = settings.value(QStringLiteral("totals/rxMonth"), 0).toULongLong();
    const std::uint64_t storedTxMonth = settings.value(QStringLiteral("totals/txMonth"), 0).toULongLong();
    config.monthKey = currentMonth;
    config.lastRxMonth = archivedMonthTotal(settings, previousMonth, QStringLiteral("rx"));
    config.lastTxMonth = archivedMonthTotal(settings, previousMonth, QStringLiteral("tx"));
    if (storedMonth == currentMonth) {
        config.rxMonth = storedRxMonth;
        config.txMonth = storedTxMonth;
    } else {
        archiveMonthlyTotals(settings, storedMonth, storedRxMonth, storedTxMonth);
        if (storedMonth == previousMonth) {
            config.lastRxMonth = storedRxMonth;
            config.lastTxMonth = storedTxMonth;
        }
        settings.setValue(QStringLiteral("totals/monthKey"), currentMonth);
        settings.setValue(QStringLiteral("totals/rxMonth"), QVariant::fromValue<qulonglong>(0));
        settings.setValue(QStringLiteral("totals/txMonth"), QVariant::fromValue<qulonglong>(0));
        settings.sync();
    }
    return config;
}

void AppSettings::saveConfig(const AppConfig& config) const {
    QSettings settings(settingsPath_, QSettings::IniFormat);
    for (int i = 0; i < PaneCount; ++i) {
        const auto id = static_cast<PaneId>(i);
        settings.setValue(boolKey(id), config.panes[static_cast<std::size_t>(i)]);
    }
    settings.setValue(QStringLiteral("config/autoMinimize"), config.autoMinimize);
    settings.setValue(QStringLiteral("config/autoStart"), config.autoStart);
    settings.setValue(QStringLiteral("config/urlClipCap"), config.urlClipCap);
    settings.setValue(QStringLiteral("config/alwaysOnTop"), config.alwaysOnTop);
    settings.setValue(QStringLiteral("config/unitMode"), config.unitMode == UnitMode::Bits ? QStringLiteral("bits") : QStringLiteral("bytes"));
    settings.setValue(QStringLiteral("config/interface"), config.selectedInterface);
    settings.setValue(QStringLiteral("remote/target"), config.remoteTarget);
    settings.setValue(QStringLiteral("window/pos"), config.windowPos);
    settings.setValue(QStringLiteral("totals/monthKey"), config.monthKey);
    settings.setValue(QStringLiteral("totals/rxMonth"), QVariant::fromValue<qulonglong>(config.rxMonth));
    settings.setValue(QStringLiteral("totals/txMonth"), QVariant::fromValue<qulonglong>(config.txMonth));
    settings.sync();
}

void AppSettings::saveMonthlyTotals(const QString& monthKey, std::uint64_t rxBytes, std::uint64_t txBytes) const {
    QSettings settings(settingsPath_, QSettings::IniFormat);
    archiveMonthlyTotals(settings, monthKey, rxBytes, txBytes);
    settings.setValue(QStringLiteral("totals/monthKey"), monthKey);
    settings.setValue(QStringLiteral("totals/rxMonth"), QVariant::fromValue<qulonglong>(rxBytes));
    settings.setValue(QStringLiteral("totals/txMonth"), QVariant::fromValue<qulonglong>(txBytes));
    settings.sync();
}

void AppSettings::saveWindowPosition(const QPoint& pos) const {
    QSettings settings(settingsPath_, QSettings::IniFormat);
    settings.setValue(QStringLiteral("window/pos"), pos);
    settings.sync();
}

QString AppSettings::configPath() const {
    return settingsPath_;
}

QString AppSettings::autoStartPath() const {
    return newAutoStartPath();
}

bool AppSettings::setAutoStart(bool enabled, const QString& executablePath) const {
    const QString path = autoStartPath();
    if (!enabled) {
        if (!QFile::exists(path)) {
            return true;
        }
        return QFile::remove(path);
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    const QString execPath = resolvedExecutablePath(executablePath);
    QTextStream stream(&file);
    stream << "[Desktop Entry]\n"
           << "Type=Application\n"
           << "Name=NetStats-Live\n"
           << "Comment=Live network throughput and CPU monitor widget for Linux\n"
           << "Exec=" << quoteExecArgument(execPath) << " --minimized\n"
           << "Icon=netstats-live\n"
           << "Terminal=false\n"
           << "Categories=Network;Monitor;Qt;\n"
           << "StartupWMClass=netstats-live\n"
           << "X-KDE-autostart-after=panel\n";
    return file.commit();
}

} // namespace nsl
