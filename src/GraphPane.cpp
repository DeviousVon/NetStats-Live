#include "GraphPane.h"

#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace nsl {
namespace {

constexpr int LabelRowHeight = 14;
constexpr int ValueRowHeight = 22;
constexpr int ValueBlockGap = 1;
constexpr int ValueBlockHeight = LabelRowHeight + ValueRowHeight + ValueBlockGap;

Qt::Alignment columnAlignment(int index) {
    switch (index) {
    case 0: return Qt::AlignLeft | Qt::AlignVCenter;
    case 1: return Qt::AlignHCenter | Qt::AlignVCenter;
    default: return Qt::AlignRight | Qt::AlignVCenter;
    }
}

QPainterPath filledAreaPath(const std::vector<double>& samples, const QRect& graphRect, double scale) {
    const int slots = static_cast<int>(GraphPane::MaxSamplesForRendering);
    const int startSlot = std::max(0, slots - static_cast<int>(samples.size()));
    const int usableWidth = std::max(1, graphRect.width() - 1);
    const int usableHeight = std::max(1, graphRect.height() - 1);
    const auto smoothed = smoothGraphSamples(samples);

    QPainterPath path;
    if (smoothed.empty()) {
        return path;
    }

    auto pointFor = [&](std::size_t i) {
        const int slot = startSlot + static_cast<int>(i);
        const qreal x = static_cast<qreal>(graphRect.left()) + (static_cast<qreal>(slot) * usableWidth) / std::max(1, slots - 1);
        const double normalized = std::clamp(smoothed[i] / scale, 0.0, 1.0);
        const qreal y = static_cast<qreal>(graphRect.bottom()) - static_cast<qreal>(normalized * usableHeight);
        return QPointF(x, y);
    };

    const QPointF first = pointFor(0);
    path.moveTo(first.x(), graphRect.bottom());
    path.lineTo(first);
    for (std::size_t i = 1; i < smoothed.size(); ++i) {
        const QPointF previous = pointFor(i - 1);
        const QPointF current = pointFor(i);
        const QPointF control((previous.x() + current.x()) / 2.0, previous.y());
        const QPointF end((previous.x() + current.x()) / 2.0, current.y());
        path.quadTo(control, end);
        path.quadTo(QPointF(current.x(), current.y()), current);
    }
    const QPointF last = pointFor(smoothed.size() - 1);
    path.lineTo(last.x(), graphRect.bottom());
    path.closeSubpath();
    return path;
}

} // namespace

std::array<QString, 3> graphMetricLabels() {
    return {QStringLiteral("Current"), QStringLiteral("Average"), QStringLiteral("Max")};
}

std::vector<double> smoothGraphSamples(const std::vector<double>& samples) {
    if (samples.size() < 3) {
        return samples;
    }
    std::vector<double> smoothed(samples.size(), 0.0);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const std::size_t begin = i == 0 ? 0 : i - 1;
        const std::size_t end = std::min(samples.size() - 1, i + 1);
        double total = 0.0;
        for (std::size_t j = begin; j <= end; ++j) {
            total += samples[j];
        }
        smoothed[i] = total / static_cast<double>(end - begin + 1);
    }
    return smoothed;
}

GraphPane::GraphPane(QString title, GraphValueMode mode, QWidget* parent)
    : PaneWidget(std::move(title), 92, parent), mode_(mode) {}

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
    const auto labels = graphMetricLabels();
    const QString values[3] = {formatValue(current), formatValue(avg), formatValue(max)};

    painter.setRenderHint(QPainter::TextAntialiasing, true);
    for (int i = 0; i < 3; ++i) {
        const int left = contentRect.left() + i * colWidth;
        const int width = (i == 2) ? contentRect.right() - left + 1 : colWidth;
        const QRect labelRect(left, labelsY, width, LabelRowHeight);
        const QRect valueRect(left, valuesY, width, ValueRowHeight);
        painter.setFont(labelFont());
        painter.setPen(dimColor());
        painter.drawText(labelRect.adjusted(0, -1, -1, 0), columnAlignment(i), labels[i]);
        painter.setFont(graphValueFont());
        painter.setPen(valueColor());
        painter.drawText(valueRect.adjusted(0, -2, -1, 0), columnAlignment(i), values[i]);
    }

    QRect graphRect(contentRect.left(), contentRect.top() + ValueBlockHeight, contentRect.width(), contentRect.height() - ValueBlockHeight);
    graphRect = graphRect.adjusted(0, 1, 0, -1);
    painter.fillRect(graphRect, Theme::Background);

    const double scale = max > 0.0 ? max : 1.0;
    if (!samples_.empty()) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPainterPath area = filledAreaPath(samples_, graphRect, scale);
        painter.fillPath(area, Theme::GraphFill);
        painter.setPen(QPen(Theme::GraphFill, 1.0));
        painter.drawPath(area);
        painter.setRenderHint(QPainter::Antialiasing, false);
    }

    painter.setPen(Theme::RuleLine);
    painter.drawLine(graphRect.left(), graphRect.bottom(), graphRect.right(), graphRect.bottom());
}

} // namespace nsl
