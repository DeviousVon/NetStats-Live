// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "TrayIcon.h"

#include <QGuiApplication>
#include <QMenu>

namespace nsl {

TrayIcon::TrayIcon(QObject* parent)
    : QObject(parent) {
    renderer_.update(makeTrayVisualState(0, 0, -1));
    statusNotifier_.setIcon(renderer_.icon());
    connect(&statusNotifier_, &StatusNotifierItem::activateRequested,
            this, [this](const QPoint&, const QString& activationToken) {
                Q_EMIT toggleRequested(activationToken);
            });
    connect(&statusNotifier_, &StatusNotifierItem::contextMenuRequested,
            this, [this](const QPoint& point) {
                if (contextMenu_) {
                    contextMenu_->popup(point);
                }
            });
    connect(&statusNotifier_, &StatusNotifierItem::availabilityChanged,
            this, [this](bool) { synchronizeSystemTrayFallback(); });
    synchronizeSystemTrayFallback();
}

void TrayIcon::setContextMenu(QMenu* menu) {
    contextMenu_ = menu;
    dbusMenu_.setMenu(menu);
    statusNotifier_.setMenuPath(QDBusObjectPath(
        menu ? QStringLiteral("/MenuBar") : QStringLiteral("/NO_DBUSMENU")));
    if (systemTrayFallback_) {
        systemTrayFallback_->setContextMenu(menu);
    }
}

bool TrayIcon::isAvailable() const {
    return statusNotifier_.isAvailable() ||
        (systemTrayFallback_ && QSystemTrayIcon::isSystemTrayAvailable());
}

int TrayIcon::iconRegenerationCount() const {
    return renderer_.regenerationCount();
}

void TrayIcon::updateFromSnapshot(const CollectorSnapshot& snapshot) {
    const TrayVisualState state = makeTrayVisualState(snapshot.txDelta, snapshot.rxDelta, snapshot.lastActivityAgeMs);
    if (renderer_.update(state)) {
        statusNotifier_.setIcon(renderer_.icon());
        if (systemTrayFallback_) {
            systemTrayFallback_->setIcon(renderer_.icon());
        }
    }

    const QString activity = snapshot.lastActivityAgeMs < 0
        ? QStringLiteral("no activity yet")
        : QStringLiteral("last activity %1s ago").arg(snapshot.lastActivityAgeMs / 1000);
    tooltip_ = QStringLiteral("NetStats-Live\nDown %1  Up %2\n%3")
                   .arg(QString::fromStdString(formatRate(snapshot.rxRate, UnitMode::Bytes)),
                        QString::fromStdString(formatRate(snapshot.txRate, UnitMode::Bytes)),
                        activity);
    statusNotifier_.setToolTip(tooltip_);
    if (systemTrayFallback_) {
        systemTrayFallback_->setToolTip(tooltip_);
    }
}

void TrayIcon::synchronizeSystemTrayFallback() {
    const bool useFallback = QGuiApplication::platformName() == QStringLiteral("xcb") &&
        !statusNotifier_.isAvailable();
    if (!useFallback) {
        systemTrayFallback_.reset();
        return;
    }
    if (systemTrayFallback_) {
        return;
    }

    systemTrayFallback_ = std::make_unique<QSystemTrayIcon>();
    systemTrayFallback_->setToolTip(tooltip_);
    systemTrayFallback_->setIcon(renderer_.icon());
    systemTrayFallback_->setContextMenu(contextMenu_);
    connect(systemTrayFallback_.get(), &QSystemTrayIcon::activated,
            this, [this](QSystemTrayIcon::ActivationReason reason) {
                if (trayActivationTogglesWindow(reason)) {
                    Q_EMIT toggleRequested({});
                }
            });
    systemTrayFallback_->show();
}

} // namespace nsl
