// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "Core.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

template <typename L, typename R>
void expectEqual(const L& left, const R& right, const char* expression) {
    if (!(left == right)) {
        std::cerr << "FAIL: " << expression << "\n";
        ++failures;
    }
}

void expectNear(double left, double right, double epsilon, const char* expression) {
    if (std::fabs(left - right) > epsilon) {
        std::cerr << "FAIL: " << expression << " left=" << left << " right=" << right << "\n";
        ++failures;
    }
}

} // namespace

int main() {
    using namespace nsl;

    expectEqual(formatRate(0.0, UnitMode::Bytes), std::string("0B"), "zero bytes formats as 0B");
    expectEqual(formatRate(63.0, UnitMode::Bytes), std::string("63B"), "bytes stay bytes under 1024");
    expectEqual(formatRate(28.0 * 1024.0, UnitMode::Bytes), std::string("28.0KB"), "bytes scale to KB");
    expectEqual(formatRate(2.5 * 1024.0 * 1024.0, UnitMode::Bytes), std::string("2.5MB"), "bytes scale to MB");
    expectEqual(formatRate(1023.0 * 1024.0 * 1024.0, UnitMode::Bytes), std::string("1023.0MB"), "byte totals below the GB boundary stay in MB");
    expectEqual(formatRate(1024.0 * 1024.0 * 1024.0, UnitMode::Bytes), std::string("1.0GB"), "byte totals at the GB boundary promote to GB");
    expectEqual(formatRate(1536.0 * 1024.0 * 1024.0, UnitMode::Bytes), std::string("1.5GB"), "byte totals above the GB boundary stay compact");
    expectEqual(formatRate(344873.0 * 1024.0 * 1024.0, UnitMode::Bytes), std::string("336.8GB"), "large traffic totals scale to compact GB");
    expectEqual(formatRate(128.0, UnitMode::Bits), std::string("1.0Kb"), "bits mode multiplies by 8 and uses lowercase b");
    expectEqual(formatRate(2.5 * 1024.0 * 1024.0, UnitMode::Bits), std::string("20.0Mb"), "moderate bit rates remain in Mb");
    expectEqual(formatRate(1023.0 * 1024.0 * 1024.0 / 8.0, UnitMode::Bits), std::string("1023.0Mb"), "bit totals below the Gb boundary stay in Mb");
    expectEqual(formatRate(128.0 * 1024.0 * 1024.0, UnitMode::Bits), std::string("1.0Gb"), "large bit totals scale to compact Gb");

    const std::string dev =
        "Inter-|   Receive                                                |  Transmit\n"
        " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
        "    lo: 1000 0 0 0 0 0 0 0 2000 0 0 0 0 0 0 0\n"
        "enp1s0: 12345 0 0 0 0 0 0 0 98765 0 0 0 0 0 0 0\n"
        " wlan0: 10 0 0 0 0 0 0 0 20 0 0 0 0 0 0 0\n";
    auto counters = parseProcNetDev(dev);
    expectEqual(counters.size(), std::size_t{3}, "parse /proc/net/dev row count");
    expectEqual(counters[1].name, std::string("enp1s0"), "parse interface name");
    expectEqual(counters[1].rxBytes, std::uint64_t{12345}, "parse rx bytes");
    expectEqual(counters[1].txBytes, std::uint64_t{98765}, "parse tx bytes");
    auto all = selectNetworkCounters(counters, "ALL");
    expectEqual(all.rxBytes, std::uint64_t{12355}, "ALL excludes lo and sums rx");
    expectEqual(all.txBytes, std::uint64_t{98785}, "ALL excludes lo and sums tx");
    auto single = selectNetworkCounters(counters, "wlan0");
    expectEqual(single.rxBytes, std::uint64_t{10}, "select interface rx");
    expectEqual(single.txBytes, std::uint64_t{20}, "select interface tx");
    expectEqual(nonNegativeDelta(100, 90), std::uint64_t{0}, "counter reset does not create negative spike");
    expectEqual(nonNegativeDelta(90, 100), std::uint64_t{10}, "counter delta normal path");

    const std::vector<NetworkCounters> previousInterfaces = {
        {"eth0", 100, 200},
    };
    const std::vector<NetworkCounters> interfaceAppeared = {
        {"eth0", 110, 230},
        {"vpn0", 1000000, 2000000},
    };
    const NetworkCounterDelta appearedDelta = networkCounterDelta(previousInterfaces, interfaceAppeared, "ALL");
    expectEqual(appearedDelta.rxBytes, std::uint64_t{10}, "new interface lifetime rx is not imported into ALL totals");
    expectEqual(appearedDelta.txBytes, std::uint64_t{30}, "new interface lifetime tx is not imported into ALL totals");

    const std::vector<NetworkCounters> interfaceDisappeared = {
        {"eth0", 125, 250},
    };
    const NetworkCounterDelta disappearedDelta = networkCounterDelta(interfaceAppeared, interfaceDisappeared, "ALL");
    expectEqual(disappearedDelta.rxBytes, std::uint64_t{15}, "disappearing interface does not suppress remaining rx delta");
    expectEqual(disappearedDelta.txBytes, std::uint64_t{20}, "disappearing interface does not suppress remaining tx delta");

    const NetworkCounterDelta selectedDelta = networkCounterDelta(previousInterfaces, interfaceAppeared, "eth0");
    expectEqual(selectedDelta.rxBytes, std::uint64_t{10}, "selected interface rx uses only its own baseline");
    expectEqual(selectedDelta.txBytes, std::uint64_t{30}, "selected interface tx uses only its own baseline");

    const CpuTimes before{100, 0, 50, 850, 0, 0, 0, 0, 0, 0};
    const CpuTimes after{150, 0, 50, 900, 0, 0, 0, 0, 0, 0};
    expectNear(cpuLoadPercent(before, after), 50.0, 0.001, "CPU percent from proc/stat deltas");
    const CpuTimes guestBefore{100, 0, 0, 900, 0, 0, 0, 0, 50, 0};
    const CpuTimes guestAfter{200, 0, 0, 1000, 0, 0, 0, 0, 100, 0};
    expectNear(cpuLoadPercent(guestBefore, guestAfter), 50.0, 0.001,
               "guest ticks already included in user time are not double-counted");

    expectEqual(parseLoadAvgThreadTotal("0.12 0.15 0.20 2/1370 12345\n"), 1370, "parse thread total from /proc/loadavg");
    expectEqual(parseTracerouteHopCount(" 1  198.51.100.1  1.0 ms\n 2  203.0.113.1  2.0 ms\n"), 2, "parse final traceroute hop");

    const std::string routes =
        "Iface Destination Gateway Flags RefCnt Use Metric Mask MTU Window IRTT\n"
        "down0 00000000 0100000A 0000 0 0 0 00000000 0 0 0\n"
        "slow0 00000000 010200C0 0003 0 0 900 00000000 0 0 0\n"
        "fast0 00000000 017100CB 0003 0 0 100 00000000 0 0 0\n";
    expectEqual(parseDefaultGatewayHex(routes), std::string("203.0.113.1"),
                "default gateway ignores inactive routes and selects the lowest metric");
    expectEqual(parseDefaultGatewayHex(
                    "Iface Destination Gateway Flags RefCnt Use Metric Mask MTU Window IRTT\n"
                    "down0 00000000 0100000A 0000 0 0 0 00000000 0 0 0\n"),
                std::string(), "default gateway rejects routes that are not up gateways");

    expectEqual(isSafeProbeTarget("example.com"), true, "normal DNS probe target is accepted");
    expectEqual(isSafeProbeTarget("2001:db8::1"), true, "IPv6 probe target is accepted");
    expectEqual(isSafeProbeTarget("-f"), false, "leading command option is rejected as a probe target");
    expectEqual(isSafeProbeTarget("example.com --help"), false, "whitespace is rejected in a probe target");
    expectEqual(isSafeProbeTarget(""), false, "empty probe target is rejected");

    if (failures != 0) {
        std::cerr << failures << " test failure(s)\n";
        return 1;
    }
    return 0;
}
