#include "PaneWidget.h"

#include <QPainter>
#include <utility>

namespace nsl {
namespace {

QFont makeSmallFont(int pointSize, QFont::Weight weight = QFont::Normal, bool smallCaps = false) {
    QFont font;
    font.setFamilies({QStringLiteral("DejaVu Sans Condensed"),
                      QStringLiteral("Noto Sans Condensed"),
                      QStringLiteral("Liberation Sans Narrow"),
                      QStringLiteral("Arial Narrow"),
                      QStringLiteral("Sans Serif")});
    font.setPointSize(pointSize);
    font.setWeight(weight);
    font.setStyleHint(QFont::SansSerif);
    font.setKerning(false);
    if (smallCaps) {
        font.setCapitalization(QFont::SmallCaps);
    }
    return font;
}

} // namespace

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
    return QColor(0x05, 0x05, 0x05);
}

QColor PaneWidget::panelColor() {
    return QColor(0x0a, 0x0a, 0x0a);
}

QColor PaneWidget::valueColor() {
    return QColor(0x00, 0xe0, 0x00);
}

QColor PaneWidget::averageColor() {
    return QColor(0x9b, 0xff, 0x9b);
}

QColor PaneWidget::gridColor() {
    return QColor(0x00, 0x26, 0x20);
}

QColor PaneWidget::dimColor() {
    return QColor(0x00, 0x65, 0x55);
}

QColor PaneWidget::borderColor() {
    return QColor(0x32, 0x32, 0x32);
}

QFont PaneWidget::headerFont() {
    return makeSmallFont(8, QFont::DemiBold, true);
}

QFont PaneWidget::labelFont() {
    return makeSmallFont(7);
}

QFont PaneWidget::valueFont() {
    return makeSmallFont(8, QFont::DemiBold);
}

int PaneWidget::headerHeight() {
    return 12;
}

void PaneWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), backgroundColor());

    const QRect outer = rect().adjusted(0, 0, -1, -1);
    painter.setPen(QColor(0x18, 0x18, 0x18));
    painter.drawLine(outer.topLeft(), outer.topRight());
    painter.setPen(QColor(0x00, 0x13, 0x10));
    painter.drawLine(outer.bottomLeft(), outer.bottomRight());

    const int titleHeight = title_.isEmpty() ? 0 : headerHeight();
    if (!title_.isEmpty()) {
        const QRect headerRect = QRect(1, 1, width() - 2, titleHeight - 1);
        painter.fillRect(headerRect, QColor(0x10, 0x10, 0x10));
        painter.setPen(QColor(0x24, 0x24, 0x24));
        painter.drawLine(headerRect.topLeft(), headerRect.topRight());
        painter.drawLine(headerRect.topLeft(), headerRect.bottomLeft());
        painter.setPen(QColor(0x00, 0x1b, 0x16));
        painter.drawLine(headerRect.bottomLeft(), headerRect.bottomRight());
        painter.setFont(headerFont());
        painter.setPen(valueColor());
        painter.drawText(headerRect.adjusted(5, -1, -3, 0), Qt::AlignLeft | Qt::AlignVCenter, title_);
    }

    paintContent(painter, rect().adjusted(4, titleHeight + 2, -4, -2));
}

} // namespace nsl
