#pragma once

#include "Core.h"

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cstdint>
#include <deque>
#include <optional>

namespace nsl {

struct CollectorSnapshot {
    QString hostname;
    QString ipAddress;
    QString selectedInterface;
    QStringList interfaces;

    QString remoteTarget;
    bool pingValid = false;
    double averagePingMs = 0.0;
    bool hopValid = false;
    int hopCount = 0;

    std::uint64_t rxSession = 0;
    std::uint64_t txSession = 0;
    std::uint64_t rxMonth = 0;
    std::uint64_t txMonth = 0;
    double rxRate = 0.0;
    double txRate = 0.0;
    std::uint64_t rxDelta = 0;
    std::uint64_t txDelta = 0;
    qint64 lastActivityAgeMs = -1;

    int threadTotal = 0;
    double cpuPercent = 0.0;
    QString monthKey;
};

class Collector : public QObject {
    Q_OBJECT
public:
    explicit Collector(QObject* parent = nullptr);
    ~Collector() override;

    void start(const QString& selectedInterface,
               const QString& remoteTarget,
               const QString& monthKey,
               std::uint64_t rxMonth,
               std::uint64_t txMonth);
    void setSelectedInterface(const QString& selectedInterface);
    void setRemoteTarget(const QString& target);
    void resetSessionTotals();
    void startSimulation(const QString& monthKey = {}, std::uint64_t rxMonth = 0, std::uint64_t txMonth = 0);

    CollectorSnapshot snapshot() const;
    QString defaultGateway() const;

Q_SIGNALS:
    void updated(const nsl::CollectorSnapshot& snapshot);
    void interfaceFallbackToAll();

private Q_SLOTS:
    void tick();
    void simulationTick();
    void pingFinished(int exitCode, QProcess::ExitStatus status);
    void tracerouteFinished(int exitCode, QProcess::ExitStatus status);

private:
    void refreshNetwork(double elapsedSeconds);
    void refreshCpu();
    void refreshThreads();
    void refreshLocalInfo();
    void maybeStartPing();
    void startTraceroute();
    QString readTextFile(const QString& path) const;
    QString firstAddressForInterface(const QString& interfaceName) const;
    void rollMonthIfNeeded();

    QTimer tickTimer_;
    QTimer simulationTimer_;
    QElapsedTimer elapsed_;
    CollectorSnapshot snapshot_;
    std::optional<NetworkCounters> previousCounters_;
    std::optional<CpuTimes> previousCpu_;
    QElapsedTimer pingElapsed_;
    QProcess pingProcess_;
    QProcess tracerouteProcess_;
    std::deque<double> pingSamples_;
    QElapsedTimer activityElapsed_;
    bool hasActivity_ = false;
    int simulationFrameIndex_ = 0;
};

} // namespace nsl
