// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "TrayIconVisual.h"

#include "Theme.h"

#include <QPainter>
#include <QPixmap>
#include <QPolygonF>

#include <algorithm>

namespace nsl {

TrayActivityBucket trayActivityBucket(qint64 lastActivityAgeMs) {
    if (lastActivityAgeMs >= 0 && lastActivityAgeMs < 60000) {
        return TrayActivityBucket::Green;
    }
    if (lastActivityAgeMs >= 0 && lastActivityAgeMs < 120000) {
        return TrayActivityBucket::Yellow;
    }
    return TrayActivityBucket::Red;
}

TrayVisualState makeTrayVisualState(std::uint64_t txDelta, std::uint64_t rxDelta, qint64 lastActivityAgeMs) {
    return TrayVisualState{txDelta > 0, rxDelta > 0, trayActivityBucket(lastActivityAgeMs)};
}

QColor trayActivityColor(TrayActivityBucket bucket) {
    switch (bucket) {
    case TrayActivityBucket::Green:
        return Theme::GraphFill;
    case TrayActivityBucket::Yellow:
        return Theme::TrayWarning;
    case TrayActivityBucket::Red:
        return Theme::TrayAlert;
    }
    return Theme::TrayAlert;
}

QImage renderTrayIconImage(const TrayVisualState& state, QSize size) {
    const int edge = std::max(8, std::min(size.width(), size.height()));
    QImage image(QSize(edge, edge), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(static_cast<double>(edge) / 32.0, static_cast<double>(edge) / 32.0);

    const QRectF body(4, 3, 24, 22);
    const QColor dark = Theme::PanelBackground;
    const QColor inactive = Theme::TrayInactive;
    const QColor border = Theme::GraphFill;

    painter.setPen(QPen(border, 1.25));
    painter.setBrush(dark);
    painter.drawRoundedRect(body, 4, 4);

    painter.setPen(Qt::NoPen);
    painter.setBrush(state.txActive ? Qt::white : inactive);
    painter.drawRect(QRectF(5, 4, 11, 20));
    painter.setBrush(state.rxActive ? Qt::white : inactive);
    painter.drawRect(QRectF(16, 4, 11, 20));

    painter.setPen(QPen(border, 1));
    painter.drawLine(QPointF(16, 5), QPointF(16, 24));

    QPolygonF tri;
    tri << QPointF(11, 25) << QPointF(21, 25) << QPointF(16, 31);
    painter.setBrush(trayActivityColor(state.activity));
    painter.setPen(QPen(QColor(0, 0, 0), 1));
    painter.drawPolygon(tri);

    return image;
}

bool trayActivationTogglesWindow(QSystemTrayIcon::ActivationReason reason) {
    return reason == QSystemTrayIcon::Trigger;
}

bool TrayIconRenderer::update(const TrayVisualState& state) {
    if (state_.has_value() && *state_ == state && !icon_.isNull()) {
        return false;
    }

    QIcon icon;
    for (const int size : {16, 22, 32}) {
        icon.addPixmap(QPixmap::fromImage(renderTrayIconImage(state, QSize(size, size))));
    }
    icon_ = icon;
    state_ = state;
    ++regenerationCount_;
    return true;
}

QIcon TrayIconRenderer::icon() const {
    return icon_;
}

int TrayIconRenderer::regenerationCount() const {
    return regenerationCount_;
}

std::optional<TrayVisualState> TrayIconRenderer::state() const {
    return state_;
}

} // namespace nsl
