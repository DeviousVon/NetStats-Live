// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "PaneWidget.h"

#include "Theme.h"

#include <QFontMetrics>
#include <QPainter>
#include <algorithm>
#include <utility>

namespace nsl {

PaneWidget::PaneWidget(QString title, int preferredHeight, QWidget* parent)
    : QWidget(parent), title_(std::move(title)), preferredHeight_(preferredHeight) {
    setFixedHeight(preferredHeight_);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

int PaneWidget::preferredHeight() const {
    return preferredHeight_;
}

void PaneWidget::setPaneTitle(const QString& title) {
    title_ = title;
    update();
}

QColor PaneWidget::backgroundColor() {
    return Theme::Background;
}

QColor PaneWidget::panelColor() {
    return Theme::PanelBackground;
}

QColor PaneWidget::valueColor() {
    return Theme::ValueText;
}

QColor PaneWidget::averageColor() {
    return Theme::GraphFill;
}

QColor PaneWidget::gridColor() {
    return Theme::DimRuleLine;
}

QColor PaneWidget::dimColor() {
    return Theme::LabelText;
}

QColor PaneWidget::borderColor() {
    return Theme::Border;
}

QFont PaneWidget::headerFont() {
    return Theme::headerFont();
}

QFont PaneWidget::labelFont() {
    return Theme::labelFont();
}

QFont PaneWidget::valueFont() {
    return Theme::valueFont();
}

QFont PaneWidget::graphValueFont() {
    return Theme::graphValueFont();
}

int PaneWidget::headerHeight() {
    return 16;
}

void PaneWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), backgroundColor());

    const QRect outer = rect().adjusted(0, 0, -1, -1);
    painter.setPen(Theme::Border);
    painter.drawLine(outer.topLeft(), outer.topRight());
    painter.setPen(Theme::DimRuleLine);
    painter.drawLine(outer.bottomLeft(), outer.bottomRight());

    const int titleHeight = title_.isEmpty() ? 0 : headerHeight();
    if (!title_.isEmpty()) {
        const QRect headerRect = QRect(1, 0, width() - 2, titleHeight);
        painter.setFont(headerFont());
        const QFontMetrics metrics(painter.font());
        const int textWidth = metrics.horizontalAdvance(title_);
        const int textLeft = headerRect.left() + (headerRect.width() - textWidth) / 2;
        const int ruleY = headerRect.center().y() + 1;
        const int gap = 6;
        painter.setPen(Theme::RuleLine);
        painter.drawLine(headerRect.left() + 4, ruleY, std::max(headerRect.left() + 4, textLeft - gap), ruleY);
        painter.drawLine(std::min(headerRect.right() - 4, textLeft + textWidth + gap), ruleY, headerRect.right() - 4, ruleY);
        painter.setPen(Theme::HeaderText);
        painter.drawText(headerRect.adjusted(0, -1, 0, 0), Qt::AlignCenter, title_);
    }

    paintContent(painter, rect().adjusted(5, titleHeight + 1, -5, -2));
}

} // namespace nsl
