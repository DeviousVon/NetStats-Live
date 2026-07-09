// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 NAME

#include "Lifecycle.h"

namespace nsl {

bool shouldShowMainWindow(bool startMinimizedOption, bool autoMinimizeEnabled) {
    return !startMinimizedOption && !autoMinimizeEnabled;
}

QString singleInstanceServiceName() {
    return QStringLiteral("org.nsl_linux.NSL");
}

QString singleInstanceObjectPath() {
    return QStringLiteral("/org/nsl_linux/MainWindow");
}

QString singleInstanceInterfaceName() {
    return QStringLiteral("org.nsl_linux.NSL");
}

} // namespace nsl
