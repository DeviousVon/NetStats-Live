// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 NAME

#pragma once

#include "Core.h"

#include <QPoint>
#include <QSettings>
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

class AppSettings {
public:
    AppSettings();

    AppConfig load() const;
    void saveConfig(const AppConfig& config) const;
    void saveMonthlyTotals(const QString& monthKey, std::uint64_t rxBytes, std::uint64_t txBytes) const;
    void saveWindowPosition(const QPoint& pos) const;
    QString configPath() const;
    QString autoStartPath() const;
    bool setAutoStart(bool enabled, const QString& executablePath) const;

    static QString currentMonthKey();

private:
    QString settingsPath_;
};

} // namespace nsl
