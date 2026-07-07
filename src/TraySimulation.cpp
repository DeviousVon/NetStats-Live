#include "TraySimulation.h"

namespace nsl {

std::vector<TraySimulationFrame> buildTraySimulationFrames() {
    constexpr std::uint64_t smallBurst = 4 * 1024;
    constexpr std::uint64_t largeBurst = 24 * 1024;
    return {
        {QStringLiteral("rx-only-1"), largeBurst, 0, 0},
        {QStringLiteral("rx-only-2"), smallBurst, 0, 0},
        {QStringLiteral("rx-only-3"), largeBurst / 2, 0, 0},
        {QStringLiteral("tx-only-1"), 0, largeBurst, 0},
        {QStringLiteral("tx-only-2"), 0, smallBurst, 0},
        {QStringLiteral("tx-only-3"), 0, largeBurst / 2, 0},
        {QStringLiteral("both-1"), largeBurst, largeBurst, 0},
        {QStringLiteral("both-2"), smallBurst, smallBurst, 0},
        {QStringLiteral("both-3"), largeBurst / 2, largeBurst / 2, 0},
        {QStringLiteral("silent-green-early"), 0, 0, 1000},
        {QStringLiteral("silent-green-boundary-minus-1"), 0, 0, 59999},
        {QStringLiteral("silent-yellow-at-60s"), 0, 0, 60000},
        {QStringLiteral("silent-red-at-120s"), 0, 0, 120000},
    };
}

void applyTraySimulationFrame(CollectorSnapshot& snapshot, const TraySimulationFrame& frame) {
    if (snapshot.hostname.isEmpty()) {
        snapshot.hostname = QStringLiteral("nsl-linux-sim");
    }
    if (snapshot.ipAddress.isEmpty()) {
        snapshot.ipAddress = QStringLiteral("10.0.0.42");
    }
    snapshot.selectedInterface = QStringLiteral("SIM");
    snapshot.interfaces = {QStringLiteral("SIM")};
    snapshot.remoteTarget = QStringLiteral("simulated-gateway");
    snapshot.pingValid = true;
    snapshot.averagePingMs = 1.2;
    snapshot.hopValid = true;
    snapshot.hopCount = 1;
    if (snapshot.monthKey.isEmpty()) {
        snapshot.monthKey = QStringLiteral("SIM-MONTH");
    }

    snapshot.rxDelta = frame.rxDelta;
    snapshot.txDelta = frame.txDelta;
    snapshot.rxRate = static_cast<double>(frame.rxDelta) * 2.0;
    snapshot.txRate = static_cast<double>(frame.txDelta) * 2.0;
    snapshot.rxSession += frame.rxDelta;
    snapshot.txSession += frame.txDelta;
    snapshot.rxMonth += frame.rxDelta;
    snapshot.txMonth += frame.txDelta;
    snapshot.lastActivityAgeMs = frame.lastActivityAgeMs;
    snapshot.threadTotal = 1400 + static_cast<int>((snapshot.rxSession + snapshot.txSession) % 37);
    snapshot.cpuPercent = (frame.rxDelta > 0 || frame.txDelta > 0) ? 18.0 : 3.0;
}

} // namespace nsl
