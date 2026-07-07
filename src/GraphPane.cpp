#include "GraphPane.h"

#include <QPainter>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace nsl {

GraphPane::GraphPane(QString title, GraphValueMode mode, QWidget* parent)
    : PaneWidget(std::move(title), 70, parent), mode_(mode) {}

void GraphPane::setUnitMode(UnitMode mode) {
    unitMode_ = mode;
    update();
}

void GraphPane::pushSample(double value) {
    const double clipped = std::max(0.0, value);
    samples_.push_back(clipped);
    if (samples_.size() > MaxSamples) {
        samples_.erase(samples_.begin(), samples_.begin() + static_cast<long>(samples_.size() - MaxSamples));
    }
    maximumSeen_ = std::max(maximumSeen_, clipped);
    update();
}

void GraphPane::resetGraph() {
    samples_.clear();
    maximumSeen_ = 0.0;
    update();
}

QString GraphPane::formatValue(double value) const {
    switch (mode_) {
    case GraphValueMode::NetworkRate:
        return QString::fromStdString(formatRate(value, unitMode_));
    case GraphValueMode::Percent:
        return QStringLiteral("%1%").arg(static_cast<int>(std::lround(value)));
    case GraphValueMode::Count:
        return QString::number(static_cast<int>(std::lround(value)));
    }
    return {};
}

double GraphPane::average() const {
    if (samples_.empty()) {
        return 0.0;
    }
    const double total = std::accumulate(samples_.begin(), samples_.end(), 0.0);
    return total / static_cast<double>(samples_.size());
}

void GraphPane::paintContent(QPainter& painter, const QRect& contentRect) {
    const double current = samples_.empty() ? 0.0 : samples_.back();
    const double avg = average();
    const double max = maximumSeen_;

    painter.setFont(labelFont());
    painter.setPen(valueColor());
    const int labelY = contentRect.top();
    painter.drawText(QRect(contentRect.left(), labelY, 48, 10), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Current"));
    painter.drawText(QRect(contentRect.center().x() - 28, labelY, 56, 10), Qt::AlignHCenter | Qt::AlignVCenter, QStringLiteral("Average"));
    painter.drawText(QRect(contentRect.right() - 42, labelY, 42, 10), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("Max"));

    painter.setFont(valueFont());
    const int valueY = labelY + 10;
    painter.drawText(QRect(contentRect.left(), valueY, 52, 13), Qt::AlignLeft | Qt::AlignVCenter, formatValue(current));
    painter.drawText(QRect(contentRect.center().x() - 34, valueY, 68, 13), Qt::AlignHCenter | Qt::AlignVCenter, formatValue(avg));
    painter.drawText(QRect(contentRect.right() - 52, valueY, 52, 13), Qt::AlignRight | Qt::AlignVCenter, formatValue(max));

    QRect graphRect(contentRect.left(), valueY + 15, contentRect.width(), contentRect.bottom() - valueY - 15);
    graphRect = graphRect.adjusted(0, 0, 0, -1);
    painter.fillRect(graphRect, Qt::black);
    painter.setPen(QColor(0x00, 0x28, 0x22));
    for (int i = 1; i < 4; ++i) {
        const int y = graphRect.top() + (graphRect.height() * i) / 4;
        painter.drawLine(graphRect.left(), y, graphRect.right(), y);
    }

    const double scale = max > 0.0 ? max : 1.0;
    QPolygonF poly;
    poly.reserve(static_cast<int>(samples_.size()) + 2);
    poly.append(QPointF(graphRect.left(), graphRect.bottom()));
    if (!samples_.empty()) {
        for (int x = 0; x < graphRect.width(); ++x) {
            const double pos = static_cast<double>(x) / std::max(1, graphRect.width() - 1);
            const auto index = static_cast<std::size_t>(std::min<double>(samples_.size() - 1, std::floor(pos * static_cast<double>(samples_.size()))));
            const double sample = samples_[index];
            const double normalized = std::clamp(sample / scale, 0.0, 1.0);
            const double y = graphRect.bottom() - normalized * static_cast<double>(graphRect.height() - 1);
            poly.append(QPointF(graphRect.left() + x, y));
        }
    }
    poly.append(QPointF(graphRect.right(), graphRect.bottom()));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x00, 0xe0, 0x00, 180));
    painter.drawPolygon(poly);

    if (avg > 0.0) {
        const double y = graphRect.bottom() - std::clamp(avg / scale, 0.0, 1.0) * static_cast<double>(graphRect.height() - 1);
        painter.setPen(QColor(0x90, 0xff, 0x90));
        painter.drawLine(graphRect.left(), static_cast<int>(std::lround(y)), graphRect.right(), static_cast<int>(std::lround(y)));
    }

    painter.setPen(dimColor());
    painter.drawRect(graphRect.adjusted(0, 0, -1, -1));
}

} // namespace nsl
