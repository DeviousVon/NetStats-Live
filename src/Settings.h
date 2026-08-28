// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#pragma once

#include "Core.h"

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QPoint>
#include <QSettings>
#include <QSharedPointer>
#include <QString>

#include <array>
#include <cstdint>

namespace nsl {

enum class PaneId : int {
    LocalMachine = 0,
    RemoteMachine,
    IncomingTotals,
    Incoming,
    OutgoingTotals,
    Outgoing,
    Threads,
    Cpu,
    Count
};

constexpr int PaneCount = static_cast<int>(PaneId::Count);

QString paneConfigKey(PaneId id);
QString paneDisplayName(PaneId id);

struct StorageResult {
    bool ok = true;
    QString error;
};

// Persisted user preferences plus active and previous-month transfer totals.
struct AppConfig {
    std::array<bool, PaneCount> panes{};
    bool autoMinimize = false;
    bool autoStart = false;
    bool urlClipCap = false;
    bool alwaysOnTop = false;
    UnitMode unitMode = UnitMode::Bytes;
    QString selectedInterface = QStringLiteral("ALL");
    QString remoteTarget;
    QPoint windowPos;
    QString monthKey;
    std::uint64_t rxMonth = 0;
    std::uint64_t txMonth = 0;
    std::uint64_t lastRxMonth = 0;
    std::uint64_t lastTxMonth = 0;
};

struct AppConfigLoadResult {
    AppConfig config;
    StorageResult storage;
};

AppConfig preserveMonthOnFailedRollover(AppConfig transitioned,
                                        const StorageResult& persistence,
                                        const QString& storedMonth,
                                        std::uint64_t storedRxMonth,
                                        std::uint64_t storedTxMonth);

struct AutoStartSnapshot {
    QSharedPointer<int> directoryDescriptor;
    QString directoryPath;
    quint64 directoryDevice = 0;
    quint64 directoryInode = 0;
    bool existed = false;
    QByteArray contents;
    QFile::Permissions permissions{};
    QDateTime accessTime;
    QDateTime modificationTime;
    quint64 device = 0;
    quint64 inode = 0;
    qint64 fileSize = -1;
    qint64 accessTimeNanoseconds = -1;
    qint64 modificationTimeNanoseconds = -1;
    enum class Mutation {
        None,
        DisabledAbsent,
        Created,
        StagedRemoval,
        Replaced
    } mutation = Mutation::None;
    QString backupPath;
    quint64 publishedDevice = 0;
    quint64 publishedInode = 0;
    qint64 publishedFileSize = -1;
    QByteArray publishedContentHash;
    QFile::Permissions publishedPermissions{};
};

// Owns QSettings persistence and autostart .desktop file generation.
class AppSettings {
public:
    explicit AppSettings(bool initializeStorage = true);

    AppConfigLoadResult load() const;
    const StorageResult& initializationResult() const;
    const StorageResult& storageAuthorityResult() const;
    StorageResult saveConfig(const AppConfig& config) const;
    StorageResult saveMonthlyTotals(const QString& monthKey, std::uint64_t rxBytes, std::uint64_t txBytes) const;
    StorageResult saveWindowPosition(const QPoint& pos) const;
    QString configPath() const;
    QString autoStartPath() const;
    StorageResult snapshotAutoStart(AutoStartSnapshot& snapshot) const;
    StorageResult restoreAutoStart(AutoStartSnapshot& snapshot) const;
    StorageResult commitAutoStart(AutoStartSnapshot& snapshot) const;
    StorageResult setAutoStart(bool enabled, const QString& executablePath) const;
    StorageResult setAutoStart(bool enabled,
                               const QString& executablePath,
                               AutoStartSnapshot& transaction) const;

    static QString currentMonthKey();

private:
    QString settingsPath_;
    StorageResult initializationResult_;
    StorageResult storageAuthorityResult_;
};

} // namespace nsl
