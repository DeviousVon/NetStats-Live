#include "Settings.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

namespace nsl {
namespace {

QString configDir() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return QDir(base).filePath(QStringLiteral("nsl-linux"));
}

QString boolKey(PaneId id) {
    return QStringLiteral("panes/%1").arg(paneConfigKey(id));
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
    : settingsPath_(QDir(configDir()).filePath(QStringLiteral("nsl-linux.conf"))) {
    QDir().mkpath(configDir());
}

QString AppSettings::currentMonthKey() {
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
    const QString storedMonth = settings.value(QStringLiteral("totals/monthKey"), currentMonth).toString();
    config.monthKey = currentMonth;
    if (storedMonth == currentMonth) {
        config.rxMonth = settings.value(QStringLiteral("totals/rxMonth"), 0).toULongLong();
        config.txMonth = settings.value(QStringLiteral("totals/txMonth"), 0).toULongLong();
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
    const QString autostart = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).filePath(QStringLiteral("autostart"));
    return QDir(autostart).filePath(QStringLiteral("nsl-linux.desktop"));
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
    QTextStream stream(&file);
    stream << "[Desktop Entry]\n"
           << "Type=Application\n"
           << "Name=NSL-Linux\n"
           << "Comment=AnalogX NetStat Live style network monitor for Linux\n"
           << "Exec=" << executablePath << " --minimized\n"
           << "Icon=network-workgroup\n"
           << "Terminal=false\n"
           << "Categories=Network;Monitor;Qt;\n"
           << "StartupWMClass=nsl-linux\n";
    return file.commit();
}

} // namespace nsl
