// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#pragma once

#include <QString>

namespace nsl {

bool shouldShowMainWindow(bool startMinimizedOption, bool autoMinimizeEnabled, bool trayAvailable);
bool shouldHideToTray(bool trayAvailable);
QString singleInstanceServiceName();
QString singleInstanceObjectPath();
QString singleInstanceInterfaceName();

} // namespace nsl
