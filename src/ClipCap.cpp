// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "ClipCap.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QRegularExpression>
#include <QUrl>

namespace nsl {

ClipCap::ClipCap(QObject* parent)
    : QObject(parent),
      klipperWatcher_(QStringLiteral("org.kde.klipper"),
                      QDBusConnection::sessionBus(),
                      QDBusServiceWatcher::WatchForOwnerChange,
                      this) {
    if (auto* clipboard = QApplication::clipboard()) {
        connect(clipboard, &QClipboard::dataChanged, this, &ClipCap::clipboardChanged);
    }
    connect(&klipperWatcher_, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString&, const QString& oldOwner, const QString& newOwner) {
        if (oldOwner == newOwner) {
            return;
        }
        ++requestGeneration_;
        klipperAvailable_ = !newOwner.isEmpty();
        if (enabled_ && klipperAvailable_) {
            klipperTimer_.start();
            pollKlipper();
        } else {
            klipperTimer_.stop();
        }
    });
    // KDE Wayland blocks arbitrary background clipboard reads; Klipper exposes
    // the current clipboard over DBus, so poll it only when that service exists.
    auto* iface = QDBusConnection::sessionBus().interface();
    if (iface != nullptr) {
        const QDBusReply<bool> registered = iface->isServiceRegistered(QStringLiteral("org.kde.klipper"));
        klipperAvailable_ = registered.isValid() && registered.value();
    }
    klipperTimer_.setInterval(2000);
    connect(&klipperTimer_, &QTimer::timeout, this, &ClipCap::pollKlipper);
}

void ClipCap::setEnabled(bool enabled) {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    ++requestGeneration_;
    if (enabled_) {
        clipboardChanged();
        if (klipperAvailable_) {
            klipperTimer_.start();
            pollKlipper();
        }
    } else {
        klipperTimer_.stop();
    }
}

bool ClipCap::klipperAvailable() const {
    return klipperAvailable_;
}

void ClipCap::clipboardChanged() {
    if (!enabled_) {
        return;
    }
    if (auto* clipboard = QApplication::clipboard()) {
        considerText(clipboard->text(QClipboard::Clipboard));
    }
}

void ClipCap::pollKlipper() {
    if (!enabled_ || !klipperAvailable_ || !klipperCall_.isNull()) {
        return;
    }
    QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.klipper"),
        QStringLiteral("/klipper"),
        QStringLiteral("org.kde.klipper.klipper"),
        QStringLiteral("getClipboardContents"));
    const quint64 generation = requestGeneration_;
    auto* watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(request, 1500), this);
    klipperCall_ = watcher;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation](QDBusPendingCallWatcher*) {
                const bool currentRequest = klipperCall_ == watcher;
                if (currentRequest) {
                    klipperCall_.clear();
                }
                const QDBusPendingReply<QString> reply = *watcher;
                if (currentRequest && generation == requestGeneration_ && enabled_ && klipperAvailable_ && !reply.isError()) {
                    considerText(reply.value());
                }
                watcher->deleteLater();
            });
}

void ClipCap::considerText(const QString& text) {
    const QString host = extractHost(text);
    if (host.isEmpty()) {
        return;
    }
    const QString normalized = text.trimmed();
    if (normalized == lastSeenUrl_) {
        return;
    }
    lastSeenUrl_ = normalized;
    Q_EMIT urlCaptured(host);
}

QString ClipCap::extractHost(const QString& text) const {
    const QString trimmed = text.trimmed();
    if (!trimmed.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) &&
        !trimmed.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        return {};
    }
    const QUrl url(trimmed);
    return url.host();
}

} // namespace nsl
