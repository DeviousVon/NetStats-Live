// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#pragma once

#include "Core.h"
#include "PaneWidget.h"

#include <QPainterPath>
#include <QString>
#include <array>
#include <vector>

namespace nsl {

enum class GraphValueMode { NetworkRate, Percent, Count };

// Visible-window graph scale after padding and empty-traffic handling.
struct GraphScaleRange {
    double minimum = 0.0;
    double maximum = 1.0;
    bool drawEmpty = false;
};

std::array<QString, 3> graphMetricLabels();
std::vector<double> smoothGraphSamples(const std::vector<double>& samples);
GraphScaleRange targetGraphScaleRange(GraphValueMode mode, const std::vector<double>& samples);
GraphScaleRange easeGraphScaleRange(const GraphScaleRange& current, const GraphScaleRange& target, bool initialized);
double normalizeGraphSample(double sample, const GraphScaleRange& range);

// Custom-painted scrolling graph pane with current/average/max text.
class GraphPane : public PaneWidget {
    Q_OBJECT
public:
    explicit GraphPane(QString title, GraphValueMode mode, QWidget* parent = nullptr);
    static constexpr std::size_t MaxSamplesForRendering = 60;

    void setUnitMode(UnitMode mode);
    void pushSample(double value);
    void resetGraph();

protected:
    void paintContent(QPainter& painter, const QRect& contentRect) override;

private:
    QString formatValue(double value) const;
    double average() const;

    GraphValueMode mode_;
    UnitMode unitMode_ = UnitMode::Bytes;
    std::vector<double> samples_;
    double maximumSeen_ = 0.0;
    GraphScaleRange scaleRange_;
    bool scaleInitialized_ = false;
    static constexpr std::size_t MaxSamples = MaxSamplesForRendering;
};

} // namespace nsl
