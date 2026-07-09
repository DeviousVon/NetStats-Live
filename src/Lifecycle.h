// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 NAME

#pragma once

#include <QString>

namespace nsl {

bool shouldShowMainWindow(bool startMinimizedOption, bool autoMinimizeEnabled);
QString singleInstanceServiceName();
QString singleInstanceObjectPath();
QString singleInstanceInterfaceName();

} // namespace nsl
