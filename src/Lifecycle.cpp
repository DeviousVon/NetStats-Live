// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "Lifecycle.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <limits>

namespace nsl {

bool shouldShowMainWindow(bool startMinimizedOption, bool autoMinimizeEnabled, bool trayAvailable) {
    if (!trayAvailable) {
        return true;
    }
    return !startMinimizedOption && !autoMinimizeEnabled;
}

bool shouldRestoreForTrayLoss(bool trayAvailable,
                              bool windowVisible,
                              bool windowMinimized,
                              bool windowExposed) {
    return !trayAvailable && (!windowVisible || windowMinimized || !windowExposed);
}

TrayToggleAction trayToggleAction(bool visible, bool minimized, bool exposed) {
    if (!visible || minimized) {
        return TrayToggleAction::Restore;
    }
    if (!exposed) {
        return TrayToggleAction::PlatformRestorePending;
    }
    return TrayToggleAction::Minimize;
}

InstanceActivationAction instanceActivationAction(bool exposed, bool nativeActivationAvailable, bool hasActivationToken) {
    if (nativeActivationAvailable && hasActivationToken) {
        return InstanceActivationAction::NativeRestore;
    }
    if (exposed) {
        return InstanceActivationAction::Activate;
    }
    return InstanceActivationAction::Remap;
}

void withActivationToken(const QString& activationToken, const std::function<void()>& operation) {
    qunsetenv("XDG_ACTIVATION_TOKEN");
    if (!activationToken.isEmpty()) {
        qputenv("XDG_ACTIVATION_TOKEN", activationToken.toUtf8());
    }
    try {
        operation();
    } catch (...) {
        qunsetenv("XDG_ACTIVATION_TOKEN");
        throw;
    }
    qunsetenv("XDG_ACTIVATION_TOKEN");
}

QPoint visibleWindowPosition(const QPoint& desiredPosition,
                             const QSize& windowSize,
                             const QList<QRect>& availableScreenGeometries) {
    if (!windowSize.isValid() || availableScreenGeometries.isEmpty()) {
        return desiredPosition;
    }

    const QRect desiredWindow(desiredPosition, windowSize);
    const int minimumVisibleWidth = std::min(48, windowSize.width());
    const int minimumVisibleHeight = std::min(18, windowSize.height());
    for (const QRect& screen : availableScreenGeometries) {
        const QRect visible = desiredWindow.intersected(screen);
        if (visible.width() >= minimumVisibleWidth && visible.height() >= minimumVisibleHeight) {
            return desiredPosition;
        }
    }

    QPoint nearest = desiredPosition;
    qint64 nearestDistance = std::numeric_limits<qint64>::max();
    for (const QRect& screen : availableScreenGeometries) {
        if (!screen.isValid()) {
            continue;
        }
        const int maximumX = std::max(screen.left(), screen.right() - windowSize.width() + 1);
        const int maximumY = std::max(screen.top(), screen.bottom() - windowSize.height() + 1);
        const int x = std::clamp(desiredPosition.x(), screen.left(), maximumX);
        const int y = std::clamp(desiredPosition.y(), screen.top(), maximumY);
        const qint64 distance = std::abs(static_cast<qint64>(desiredPosition.x()) - x) +
                                std::abs(static_cast<qint64>(desiredPosition.y()) - y);
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = QPoint(x, y);
        }
    }
    return nearest;
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

SingleInstanceEndpoint singleInstanceEndpoint(const QString& profileDirectory) {
    const auto canonicalPath = [](const QString& path) {
        const QFileInfo info(path);
        const QString canonical = info.canonicalFilePath();
        return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical;
    };

    const QString profilePath = canonicalPath(profileDirectory);
    const QString defaultProfilePath = canonicalPath(
        QDir(QDir::homePath()).filePath(QStringLiteral(".config/netstats-live")));
    if (profilePath == defaultProfilePath) {
        return {singleInstanceServiceName(), singleInstanceObjectPath(), singleInstanceInterfaceName()};
    }

    const QString profileComponent = QStringLiteral("p") + QString::fromLatin1(
        QCryptographicHash::hash(profilePath.toUtf8(), QCryptographicHash::Sha256).toHex());
    return {
        singleInstanceServiceName() + QLatin1Char('.') + profileComponent,
        QStringLiteral("/io/github/DeviousVon/NetStatsLive/Profiles/") + profileComponent + QStringLiteral("/MainWindow"),
        singleInstanceInterfaceName(),
    };
}

bool registerSingleInstanceObject(QDBusConnection& sessionBus,
                                  const SingleInstanceEndpoint& endpoint,
                                  QObject* object) {
    return sessionBus.registerObject(endpoint.objectPath,
                                     endpoint.interfaceName,
                                     object,
                                     QDBusConnection::ExportScriptableSlots);
}

bool registerSingleInstanceObjectBeforeInitialization(
    QDBusConnection& sessionBus,
    const SingleInstanceEndpoint& endpoint,
    QObject* object,
    const std::function<void()>& initialize) {
    if (!registerSingleInstanceObject(sessionBus, endpoint, object)) {
        return false;
    }
    initialize();
    return true;
}

} // namespace nsl
