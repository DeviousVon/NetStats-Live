// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "Lifecycle.h"

namespace nsl {

bool shouldShowMainWindow(bool startMinimizedOption, bool autoMinimizeEnabled, bool trayAvailable) {
    if (!trayAvailable) {
        return true;
    }
    return !startMinimizedOption && !autoMinimizeEnabled;
}

bool shouldHideToTray(bool trayAvailable) {
    return trayAvailable;
}

QString singleInstanceServiceName() {
    return QStringLiteral("io.github.DeviousVon.NetStatsLive");
}

QString singleInstanceObjectPath() {
    return QStringLiteral("/io/github/DeviousVon/NetStatsLive/MainWindow");
}

QString singleInstanceInterfaceName() {
    return QStringLiteral("io.github.DeviousVon.NetStatsLive");
}

} // namespace nsl
