// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#pragma once

#include "Collector.h"
#include "DBusMenu.h"
#include "StatusNotifierItem.h"
#include "TrayIconVisual.h"

#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>

#include <memory>

namespace nsl {

// Bridges Collector snapshots into a QSystemTrayIcon/StatusNotifierItem.
class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(QObject* parent = nullptr);
    void setContextMenu(QMenu* menu);
    void updateFromSnapshot(const CollectorSnapshot& snapshot);
    bool isAvailable() const;
    int iconRegenerationCount() const;

Q_SIGNALS:
    void toggleRequested(const QString& activationToken);

private:
    void synchronizeSystemTrayFallback();

    StatusNotifierItem statusNotifier_;
    DBusMenu dbusMenu_;
    std::unique_ptr<QSystemTrayIcon> systemTrayFallback_;
    QPointer<QMenu> contextMenu_;
    TrayIconRenderer renderer_;
    QString tooltip_ = QStringLiteral("NetStats-Live");
};

} // namespace nsl
