// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 NAME

#pragma once

#include "Collector.h"

#include <QString>
#include <QtGlobal>

#include <cstdint>
#include <vector>

namespace nsl {

struct TraySimulationFrame {
    QString label;
    std::uint64_t rxDelta = 0;
    std::uint64_t txDelta = 0;
    qint64 lastActivityAgeMs = -1;
};

std::vector<TraySimulationFrame> buildTraySimulationFrames();
void applyTraySimulationFrame(CollectorSnapshot& snapshot, const TraySimulationFrame& frame);

} // namespace nsl
