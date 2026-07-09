// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 NAME

#pragma once

#include <QClipboard>
#include <QObject>
#include <QTimer>

namespace nsl {

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
    QString lastSeenUrl_;
    QTimer klipperTimer_;
};

} // namespace nsl
