#include "PaneWidget.h"

#include <QPainter>
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
    return QColor(0x0a, 0x0a, 0x0a);
}

QColor PaneWidget::valueColor() {
    return QColor(0x00, 0xe0, 0x00);
}

QColor PaneWidget::dimColor() {
    return QColor(0x00, 0x78, 0x60);
}

QFont PaneWidget::labelFont() {
    QFont font(QStringLiteral("Sans Serif"), 7);
    font.setStyleHint(QFont::SansSerif);
    font.setPixelSize(9);
    return font;
}

QFont PaneWidget::valueFont() {
    QFont font(QStringLiteral("Sans Serif"), 8, QFont::DemiBold);
    font.setStyleHint(QFont::SansSerif);
    font.setPixelSize(11);
    return font;
}

void PaneWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.fillRect(rect(), backgroundColor());

    const QRect outer = rect().adjusted(0, 0, -1, -1);
    painter.setPen(QColor(0x00, 0x55, 0x4a));
    painter.drawLine(outer.topLeft(), outer.topRight());
    painter.setPen(QColor(0x00, 0x20, 0x1c));
    painter.drawLine(outer.bottomLeft(), outer.bottomRight());

    const int titleHeight = title_.isEmpty() ? 0 : 13;
    if (!title_.isEmpty()) {
        painter.setFont(labelFont());
        painter.setPen(valueColor());
        painter.drawText(QRect(0, 0, width(), titleHeight), Qt::AlignCenter, title_);
    }
    paintContent(painter, rect().adjusted(4, titleHeight, -4, -3));
}

} // namespace nsl
