// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 NAME

#include "ClipCap.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QRegularExpression>
#include <QUrl>

namespace nsl {

ClipCap::ClipCap(QObject* parent)
    : QObject(parent) {
    if (auto* clipboard = QApplication::clipboard()) {
        connect(clipboard, &QClipboard::dataChanged, this, &ClipCap::clipboardChanged);
    }
    // KDE Wayland blocks arbitrary background clipboard reads; Klipper exposes
    // the current clipboard over DBus, so poll it only when that service exists.
    auto* iface = QDBusConnection::sessionBus().interface();
    klipperAvailable_ = iface != nullptr && iface->isServiceRegistered(QStringLiteral("org.kde.klipper"));
    klipperTimer_.setInterval(2000);
    connect(&klipperTimer_, &QTimer::timeout, this, &ClipCap::pollKlipper);
}

void ClipCap::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (enabled_) {
        clipboardChanged();
        if (klipperAvailable_) {
            klipperTimer_.start();
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
    if (!enabled_ || !klipperAvailable_) {
        return;
    }
    QDBusInterface klipper(QStringLiteral("org.kde.klipper"),
                           QStringLiteral("/klipper"),
                           QStringLiteral("org.kde.klipper.klipper"),
                           QDBusConnection::sessionBus());
    if (!klipper.isValid()) {
        return;
    }
    const QDBusReply<QString> reply = klipper.call(QStringLiteral("getClipboardContents"));
    if (reply.isValid()) {
        considerText(reply.value());
    }
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
