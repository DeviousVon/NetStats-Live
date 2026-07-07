#include "Core.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>

namespace nsl {
namespace {

std::string trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1));
}

std::uint64_t parseU64(std::string_view value) {
    std::uint64_t result = 0;
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (ec != std::errc{} || ptr == value.data()) {
        return 0;
    }
    return result;
}

std::string formatScaled(double value, const char* unit, bool integerOnly) {
    std::ostringstream out;
    if (integerOnly) {
        out << static_cast<unsigned long long>(std::llround(std::max(0.0, value)));
    } else {
        out << std::fixed << std::setprecision(1) << value;
    }
    out << unit;
    return out.str();
}

std::uint64_t totalCpuTicks(const CpuTimes& value) {
    return value.user + value.nice + value.system + value.idle + value.iowait + value.irq +
           value.softirq + value.steal + value.guest + value.guestNice;
}

std::uint64_t idleCpuTicks(const CpuTimes& value) {
    return value.idle + value.iowait;
}

} // namespace

std::string formatRate(double bytesPerSecond, UnitMode mode) {
    double value = std::max(0.0, bytesPerSecond);
    if (mode == UnitMode::Bits) {
        value *= 8.0;
        if (value < 1024.0) {
            return formatScaled(value, "b", true);
        }
        value /= 1024.0;
        if (value < 1024.0) {
            return formatScaled(value, "Kb", false);
        }
        return formatScaled(value / 1024.0, "Mb", false);
    }

    if (value < 1024.0) {
        return formatScaled(value, "B", true);
    }
    value /= 1024.0;
    if (value < 1024.0) {
        return formatScaled(value, "KB", false);
    }
    return formatScaled(value / 1024.0, "MB", false);
}

std::vector<NetworkCounters> parseProcNetDev(const std::string& text) {
    std::vector<NetworkCounters> counters;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const auto name = trim(std::string_view(line).substr(0, colon));
        if (name.empty()) {
            continue;
        }
        std::istringstream values(line.substr(colon + 1));
        std::array<std::string, 16> fields{};
        bool complete = true;
        for (auto& field : fields) {
            if (!(values >> field)) {
                complete = false;
                break;
            }
        }
        if (!complete) {
            continue;
        }
        counters.push_back(NetworkCounters{name, parseU64(fields[0]), parseU64(fields[8])});
    }
    return counters;
}

NetworkCounters selectNetworkCounters(const std::vector<NetworkCounters>& counters, const std::string& selectedInterface) {
    NetworkCounters selected;
    if (selectedInterface.empty() || selectedInterface == "ALL") {
        selected.name = "ALL";
        for (const auto& counter : counters) {
            if (counter.name == "lo") {
                continue;
            }
            selected.rxBytes += counter.rxBytes;
            selected.txBytes += counter.txBytes;
        }
        return selected;
    }

    const auto it = std::find_if(counters.begin(), counters.end(), [&](const NetworkCounters& value) {
        return value.name == selectedInterface;
    });
    if (it != counters.end()) {
        return *it;
    }
    selected.name = "ALL";
    for (const auto& counter : counters) {
        if (counter.name == "lo") {
            continue;
        }
        selected.rxBytes += counter.rxBytes;
        selected.txBytes += counter.txBytes;
    }
    return selected;
}

std::uint64_t nonNegativeDelta(std::uint64_t previous, std::uint64_t current) {
    if (current < previous) {
        return 0;
    }
    return current - previous;
}

std::optional<CpuTimes> parseProcStatCpuLine(const std::string& text) {
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.rfind("cpu ", 0) != 0) {
            continue;
        }
        std::istringstream parts(line);
        std::string label;
        CpuTimes times;
        parts >> label >> times.user >> times.nice >> times.system >> times.idle >> times.iowait >>
            times.irq >> times.softirq >> times.steal >> times.guest >> times.guestNice;
        if (!parts.fail()) {
            return times;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

double cpuLoadPercent(const CpuTimes& previous, const CpuTimes& current) {
    const auto previousTotal = totalCpuTicks(previous);
    const auto currentTotal = totalCpuTicks(current);
    if (currentTotal <= previousTotal) {
        return 0.0;
    }
    const auto totalDelta = currentTotal - previousTotal;
    const auto previousIdle = idleCpuTicks(previous);
    const auto currentIdle = idleCpuTicks(current);
    const auto idleDelta = currentIdle >= previousIdle ? currentIdle - previousIdle : 0;
    const auto busyDelta = totalDelta > idleDelta ? totalDelta - idleDelta : 0;
    return (static_cast<double>(busyDelta) * 100.0) / static_cast<double>(totalDelta);
}

int parseLoadAvgThreadTotal(const std::string& text) {
    std::istringstream input(text);
    std::string one;
    std::string five;
    std::string fifteen;
    std::string runnableTotal;
    input >> one >> five >> fifteen >> runnableTotal;
    const auto slash = runnableTotal.find('/');
    if (slash == std::string::npos) {
        return 0;
    }
    return static_cast<int>(parseU64(runnableTotal.substr(slash + 1)));
}

int parseTracerouteHopCount(const std::string& text) {
    std::istringstream input(text);
    std::string line;
    int hop = 0;
    while (std::getline(input, line)) {
        auto cleaned = trim(line);
        if (cleaned.empty()) {
            continue;
        }
        std::istringstream row(cleaned);
        int candidate = 0;
        if (row >> candidate) {
            hop = candidate;
        }
    }
    return hop;
}

std::string parseDefaultGatewayHex(const std::string& routeText) {
    std::istringstream input(routeText);
    std::string line;
    bool header = true;
    while (std::getline(input, line)) {
        if (header) {
            header = false;
            continue;
        }
        std::istringstream row(line);
        std::string iface;
        std::string destination;
        std::string gateway;
        row >> iface >> destination >> gateway;
        if (destination != "00000000" || gateway.size() != 8) {
            continue;
        }
        unsigned long raw = 0;
        try {
            raw = std::stoul(gateway, nullptr, 16);
        } catch (...) {
            continue;
        }
        std::ostringstream out;
        out << (raw & 0xffUL) << '.' << ((raw >> 8) & 0xffUL) << '.' << ((raw >> 16) & 0xffUL) << '.' << ((raw >> 24) & 0xffUL);
        return out.str();
    }
    return {};
}

} // namespace nsl
