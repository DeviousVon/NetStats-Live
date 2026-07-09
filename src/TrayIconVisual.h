// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 NAME

#pragma once

#include <QColor>
#include <QIcon>
#include <QImage>
#include <QSize>
#include <QSystemTrayIcon>
#include <QtGlobal>

#include <cstdint>
#include <optional>

namespace nsl {

enum class TrayActivityBucket {
    Green,
    Yellow,
    Red,
};

struct TrayVisualState {
    bool txActive = false;
    bool rxActive = false;
    TrayActivityBucket activity = TrayActivityBucket::Red;

    friend bool operator==(const TrayVisualState& lhs, const TrayVisualState& rhs) = default;
};

TrayActivityBucket trayActivityBucket(qint64 lastActivityAgeMs);
TrayVisualState makeTrayVisualState(std::uint64_t txDelta, std::uint64_t rxDelta, qint64 lastActivityAgeMs);
QColor trayActivityColor(TrayActivityBucket bucket);
QImage renderTrayIconImage(const TrayVisualState& state, QSize size);
bool trayActivationTogglesWindow(QSystemTrayIcon::ActivationReason reason);

class TrayIconRenderer {
public:
    bool update(const TrayVisualState& state);
    QIcon icon() const;
    int regenerationCount() const;
    std::optional<TrayVisualState> state() const;

private:
    std::optional<TrayVisualState> state_;
    QIcon icon_;
    int regenerationCount_ = 0;
};

} // namespace nsl
