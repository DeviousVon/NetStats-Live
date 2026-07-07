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
    expectEqual(formatRate(128.0, UnitMode::Bits), std::string("1.0Kb"), "bits mode multiplies by 8 and uses lowercase b");

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

    const CpuTimes before{100, 0, 50, 850, 0, 0, 0, 0, 0, 0};
    const CpuTimes after{150, 0, 50, 900, 0, 0, 0, 0, 0, 0};
    expectNear(cpuLoadPercent(before, after), 50.0, 0.001, "CPU percent from proc/stat deltas");

    expectEqual(parseLoadAvgThreadTotal("0.12 0.15 0.20 2/1370 12345\n"), 1370, "parse thread total from /proc/loadavg");
    expectEqual(parseTracerouteHopCount(" 1  192.168.1.1  1.0 ms\n 2  10.0.0.1  2.0 ms\n"), 2, "parse final traceroute hop");

    if (failures != 0) {
        std::cerr << failures << " test failure(s)\n";
        return 1;
    }
    return 0;
}
