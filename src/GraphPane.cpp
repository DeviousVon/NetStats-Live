// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

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

QPainterPath filledAreaPath(const std::vector<double>& samples, const QRect& graphRect, const GraphScaleRange& scaleRange) {
    const int slots = static_cast<int>(GraphPane::MaxSamplesForRendering);
    const int startSlot = std::max(0, slots - static_cast<int>(samples.size()));
    const int usableWidth = std::max(1, graphRect.width() - 1);
    const int usableHeight = std::max(1, graphRect.height() - 1);
    const auto smoothed = smoothGraphSamples(samples);

    QPainterPath path;
    if (smoothed.empty() || scaleRange.drawEmpty) {
        return path;
    }

    auto pointFor = [&](std::size_t i) {
        const int slot = startSlot + static_cast<int>(i);
        const qreal x = static_cast<qreal>(graphRect.left()) + (static_cast<qreal>(slot) * usableWidth) / std::max(1, slots - 1);
        const double normalized = normalizeGraphSample(smoothed[i], scaleRange);
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
    smoothed.front() = samples.front();
    smoothed.back() = samples.back();
    for (std::size_t i = 1; i + 1 < samples.size(); ++i) {
        const std::size_t begin = i - 1;
        const std::size_t end = i + 1;
        double total = 0.0;
        for (std::size_t j = begin; j <= end; ++j) {
            total += samples[j];
        }
        smoothed[i] = total / static_cast<double>(end - begin + 1);
    }
    return smoothed;
}

GraphScaleRange targetGraphScaleRange(GraphValueMode mode, const std::vector<double>& samples) {
    if (samples.empty()) {
        return {};
    }

    const auto [minIt, maxIt] = std::minmax_element(samples.begin(), samples.end());
    const double visibleMin = *minIt;
    const double visibleMax = *maxIt;

    if (mode == GraphValueMode::NetworkRate) {
        if (visibleMax <= 0.0) {
            return {0.0, 1.0, true};
        }
        return {0.0, std::max(1.0, visibleMax * 1.10), false};
    }

    const double center = (visibleMin + visibleMax) / 2.0;
    const double visibleRange = visibleMax - visibleMin;
    const double range = visibleRange > 0.0 ? visibleRange : std::max(1.0, std::abs(center) * 0.10);
    const double padding = range * 0.10;
    if (visibleRange <= 0.0) {
        return {center - range / 2.0, center + range / 2.0, false};
    }
    return {visibleMin - padding, visibleMax + padding, false};
}

GraphScaleRange easeGraphScaleRange(const GraphScaleRange& current, const GraphScaleRange& target, bool initialized) {
    if (!initialized || target.drawEmpty) {
        return target;
    }

    constexpr double Ease = 0.20;
    return {current.minimum * (1.0 - Ease) + target.minimum * Ease,
            current.maximum * (1.0 - Ease) + target.maximum * Ease,
            target.drawEmpty};
}

double normalizeGraphSample(double sample, const GraphScaleRange& range) {
    const double span = range.maximum - range.minimum;
    if (span <= 0.0) {
        return 0.5;
    }
    return std::clamp((sample - range.minimum) / span, 0.0, 1.0);
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
    scaleRange_ = {};
    scaleInitialized_ = false;
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

    if (samples_.empty()) {
        scaleRange_ = {};
        scaleInitialized_ = false;
    } else {
        scaleRange_ = easeGraphScaleRange(scaleRange_, targetGraphScaleRange(mode_, samples_), scaleInitialized_);
        scaleInitialized_ = true;
    }
    if (!samples_.empty() && !scaleRange_.drawEmpty) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPainterPath area = filledAreaPath(samples_, graphRect, scaleRange_);
        painter.fillPath(area, Theme::GraphFill);
        painter.setPen(QPen(Theme::GraphFill, 1.0));
        painter.drawPath(area);

        const int usableHeight = std::max(1, graphRect.height() - 1);
        const double averagePosition = normalizeGraphSample(avg, scaleRange_);
        const int averageY = graphRect.bottom() - static_cast<int>(std::lround(averagePosition * usableHeight));
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(QPen(Theme::AverageLine, 1));
        painter.drawLine(graphRect.left(), averageY, graphRect.right(), averageY);
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Theme::RuleLine);
    painter.drawLine(graphRect.left(), graphRect.bottom(), graphRect.right(), graphRect.bottom());
}

} // namespace nsl
