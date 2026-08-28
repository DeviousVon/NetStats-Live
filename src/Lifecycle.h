// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#pragma once

#include <QList>
#include <QDBusConnection>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

#include <functional>

namespace nsl {

enum class TrayToggleAction { Minimize, Restore, PlatformRestorePending };
enum class InstanceActivationAction { Activate, NativeRestore, Remap };

struct SingleInstanceEndpoint {
    QString serviceName;
    QString objectPath;
    QString interfaceName;
};

bool shouldShowMainWindow(bool startMinimizedOption, bool autoMinimizeEnabled, bool trayAvailable);
bool shouldRestoreForTrayLoss(bool trayAvailable,
                              bool windowVisible,
                              bool windowMinimized,
                              bool windowExposed);
TrayToggleAction trayToggleAction(bool visible, bool minimized, bool exposed);
InstanceActivationAction instanceActivationAction(bool exposed, bool nativeActivationAvailable, bool hasActivationToken);
void withActivationToken(const QString& activationToken, const std::function<void()>& operation);
QPoint visibleWindowPosition(const QPoint& desiredPosition,
                             const QSize& windowSize,
                             const QList<QRect>& availableScreenGeometries);
QString singleInstanceServiceName();
QString singleInstanceObjectPath();
QString singleInstanceInterfaceName();
SingleInstanceEndpoint singleInstanceEndpoint(const QString& profileDirectory);
bool registerSingleInstanceObject(QDBusConnection& sessionBus,
                                  const SingleInstanceEndpoint& endpoint,
                                  QObject* object);
bool registerSingleInstanceObjectBeforeInitialization(
    QDBusConnection& sessionBus,
    const SingleInstanceEndpoint& endpoint,
    QObject* object,
    const std::function<void()>& initialize);

} // namespace nsl
