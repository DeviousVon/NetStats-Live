#pragma once

#include "Core.h"
#include "PaneWidget.h"

#include <QPainterPath>
#include <QString>
#include <array>
#include <vector>

namespace nsl {

enum class GraphValueMode { NetworkRate, Percent, Count };

std::array<QString, 3> graphMetricLabels();
std::vector<double> smoothGraphSamples(const std::vector<double>& samples);

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
    static constexpr std::size_t MaxSamples = MaxSamplesForRendering;
};

} // namespace nsl
