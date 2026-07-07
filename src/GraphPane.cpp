#include "GraphPane.h"

#include <QPainter>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace nsl {
namespace {

constexpr int LabelRowHeight = 9;
constexpr int ValueRowHeight = 13;
constexpr int ValueBlockHeight = LabelRowHeight + ValueRowHeight + 1;

} // namespace

GraphPane::GraphPane(QString title, GraphValueMode mode, QWidget* parent)
    : PaneWidget(std::move(title), 84, parent), mode_(mode) {}

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

    const int colWidth = contentRect.width() / 3;
    const int labelsY = contentRect.top();
    const int valuesY = labelsY + LabelRowHeight;
    const QString labels[3] = {QStringLiteral("Cur"), QStringLiteral("Avg"), QStringLiteral("Max")};
    const QString values[3] = {formatValue(current), formatValue(avg), formatValue(max)};

    for (int i = 0; i < 3; ++i) {
        const int left = contentRect.left() + i * colWidth;
        const int width = (i == 2) ? contentRect.right() - left + 1 : colWidth;
        const QRect labelRect(left, labelsY, width, LabelRowHeight);
        const QRect valueRect(left, valuesY, width, ValueRowHeight);
        painter.setFont(labelFont());
        painter.setPen(valueColor());
        painter.drawText(labelRect.adjusted(0, 0, -1, 0), Qt::AlignRight | Qt::AlignVCenter, labels[i]);
        painter.setFont(valueFont());
        painter.drawText(valueRect.adjusted(0, -1, -1, 0), Qt::AlignRight | Qt::AlignVCenter, values[i]);
    }

    QRect graphRect(contentRect.left(), contentRect.top() + ValueBlockHeight, contentRect.width(), contentRect.height() - ValueBlockHeight);
    graphRect = graphRect.adjusted(0, 0, 0, -1);
    painter.fillRect(graphRect, Qt::black);

    painter.setPen(gridColor());
    for (int i = 1; i < 4; ++i) {
        const int y = graphRect.top() + (graphRect.height() * i) / 4;
        painter.drawLine(graphRect.left(), y, graphRect.right(), y);
    }

    const double scale = max > 0.0 ? max : 1.0;
    painter.setPen(valueColor());
    const int slots = static_cast<int>(MaxSamples);
    const int startSlot = std::max(0, slots - static_cast<int>(samples_.size()));
    for (std::size_t i = 0; i < samples_.size(); ++i) {
        const int slot = startSlot + static_cast<int>(i);
        const int x = graphRect.left() + (slot * std::max(1, graphRect.width() - 1)) / std::max(1, slots - 1);
        const double normalized = std::clamp(samples_[i] / scale, 0.0, 1.0);
        const int y = graphRect.bottom() - static_cast<int>(std::lround(normalized * static_cast<double>(std::max(1, graphRect.height() - 1))));
        painter.drawLine(x, y, x, graphRect.bottom());
    }

    if (avg > 0.0) {
        const double normalized = std::clamp(avg / scale, 0.0, 1.0);
        const int y = graphRect.bottom() - static_cast<int>(std::lround(normalized * static_cast<double>(std::max(1, graphRect.height() - 1))));
        painter.setPen(averageColor());
        painter.drawLine(graphRect.left(), y, graphRect.right(), y);
    }

    painter.setPen(dimColor());
    painter.drawLine(graphRect.left(), graphRect.bottom(), graphRect.right(), graphRect.bottom());
}

} // namespace nsl
