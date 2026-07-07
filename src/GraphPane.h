#pragma once

#include "Core.h"
#include "PaneWidget.h"

#include <QString>
#include <vector>

namespace nsl {

enum class GraphValueMode { NetworkRate, Percent, Count };

class GraphPane : public PaneWidget {
    Q_OBJECT
public:
    explicit GraphPane(QString title, GraphValueMode mode, QWidget* parent = nullptr);

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
    static constexpr std::size_t MaxSamples = 120;
};

} // namespace nsl
