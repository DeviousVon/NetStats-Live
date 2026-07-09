// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 NAME

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nsl {

enum class UnitMode { Bytes, Bits };

struct NetworkCounters {
    std::string name;
    std::uint64_t rxBytes = 0;
    std::uint64_t txBytes = 0;
};

struct CpuTimes {
    std::uint64_t user = 0;
    std::uint64_t nice = 0;
    std::uint64_t system = 0;
    std::uint64_t idle = 0;
    std::uint64_t iowait = 0;
    std::uint64_t irq = 0;
    std::uint64_t softirq = 0;
    std::uint64_t steal = 0;
    std::uint64_t guest = 0;
    std::uint64_t guestNice = 0;
};

std::string formatRate(double bytesPerSecond, UnitMode mode);
std::vector<NetworkCounters> parseProcNetDev(const std::string& text);
NetworkCounters selectNetworkCounters(const std::vector<NetworkCounters>& counters, const std::string& selectedInterface);
std::uint64_t nonNegativeDelta(std::uint64_t previous, std::uint64_t current);
std::optional<CpuTimes> parseProcStatCpuLine(const std::string& text);
double cpuLoadPercent(const CpuTimes& previous, const CpuTimes& current);
int parseLoadAvgThreadTotal(const std::string& text);
int parseTracerouteHopCount(const std::string& text);
std::string parseDefaultGatewayHex(const std::string& routeText);

} // namespace nsl
