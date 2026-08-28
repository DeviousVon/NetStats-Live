// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#pragma once

#include <QClipboard>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QPointer>
#include <QTimer>

namespace nsl {

// Watches clipboard URLs and emits their host as the remote target.
class ClipCap : public QObject {
    Q_OBJECT
public:
    explicit ClipCap(QObject* parent = nullptr);
    void setEnabled(bool enabled);
    bool klipperAvailable() const;

Q_SIGNALS:
    void urlCaptured(const QString& host);

private Q_SLOTS:
    void clipboardChanged();
    void pollKlipper();

private:
    void considerText(const QString& text);
    QString extractHost(const QString& text) const;

    bool enabled_ = false;
    bool klipperAvailable_ = false;
    quint64 requestGeneration_ = 0;
    QString lastSeenUrl_;
    QDBusServiceWatcher klipperWatcher_;
    QPointer<QDBusPendingCallWatcher> klipperCall_;
    QTimer klipperTimer_;
};

} // namespace nsl
