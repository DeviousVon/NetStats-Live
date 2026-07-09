// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 NAME

#include "TrayIcon.h"

#include <QMenu>

namespace nsl {

TrayIcon::TrayIcon(QObject* parent)
    : QObject(parent) {
    tray_.setToolTip(QStringLiteral("NSL-Linux"));
    renderer_.update(makeTrayVisualState(0, 0, -1));
    tray_.setIcon(renderer_.icon());
    connect(&tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (trayActivationTogglesWindow(reason)) {
            Q_EMIT toggleRequested();
        }
    });
    tray_.show();
}

void TrayIcon::setContextMenu(QMenu* menu) {
    tray_.setContextMenu(menu);
}

bool TrayIcon::isAvailable() const {
    return QSystemTrayIcon::isSystemTrayAvailable();
}

int TrayIcon::iconRegenerationCount() const {
    return renderer_.regenerationCount();
}

void TrayIcon::updateFromSnapshot(const CollectorSnapshot& snapshot) {
    const TrayVisualState state = makeTrayVisualState(snapshot.txDelta, snapshot.rxDelta, snapshot.lastActivityAgeMs);
    if (renderer_.update(state)) {
        tray_.setIcon(renderer_.icon());
    }

    const QString activity = snapshot.lastActivityAgeMs < 0
        ? QStringLiteral("no activity yet")
        : QStringLiteral("last activity %1s ago").arg(snapshot.lastActivityAgeMs / 1000);
    tray_.setToolTip(QStringLiteral("NSL-Linux\nDown %1  Up %2\n%3")
                         .arg(QString::fromStdString(formatRate(snapshot.rxRate, UnitMode::Bytes)),
                              QString::fromStdString(formatRate(snapshot.txRate, UnitMode::Bytes)),
                              activity));
}

} // namespace nsl
