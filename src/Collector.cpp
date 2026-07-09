// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 NAME

#include "Collector.h"
#include "Settings.h"
#include "TraySimulation.h"

#include <QCoreApplication>
#include <QFile>
#include <QHostInfo>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

#include <algorithm>
#include <numeric>

namespace nsl {
namespace {

QStringList interfaceNames(const std::vector<NetworkCounters>& counters) {
    QStringList names;
    for (const auto& counter : counters) {
        if (counter.name == "lo") {
            continue;
        }
        names.append(QString::fromStdString(counter.name));
    }
    names.removeDuplicates();
    names.sort(Qt::CaseInsensitive);
    return names;
}

QString cleanTarget(QString target) {
    target = target.trimmed();
    if (target.startsWith(QStringLiteral("http://")) || target.startsWith(QStringLiteral("https://"))) {
        const QUrl url(target);
        return url.host().isEmpty() ? target : url.host();
    }
    return target;
}

} // namespace

Collector::Collector(QObject* parent)
    : QObject(parent) {
    tickTimer_.setInterval(500);
    simulationTimer_.setInterval(500);
    connect(&tickTimer_, &QTimer::timeout, this, &Collector::tick);
    connect(&simulationTimer_, &QTimer::timeout, this, &Collector::simulationTick);
    connect(&pingProcess_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &Collector::pingFinished);
    connect(&tracerouteProcess_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &Collector::tracerouteFinished);
}

Collector::~Collector() {
    tickTimer_.stop();
    simulationTimer_.stop();
    disconnect(&pingProcess_, nullptr, this, nullptr);
    disconnect(&tracerouteProcess_, nullptr, this, nullptr);
    stopProcess(pingProcess_);
    stopProcess(tracerouteProcess_);
}

void Collector::start(const QString& selectedInterface,
                      const QString& remoteTarget,
                      const QString& monthKey,
                      std::uint64_t rxMonth,
                      std::uint64_t txMonth) {
    simulationTimer_.stop();
    snapshot_.selectedInterface = selectedInterface.isEmpty() ? QStringLiteral("ALL") : selectedInterface;
    snapshot_.monthKey = monthKey.isEmpty() ? AppSettings::currentMonthKey() : monthKey;
    snapshot_.rxMonth = rxMonth;
    snapshot_.txMonth = txMonth;
    snapshot_.remoteTarget = cleanTarget(remoteTarget);
    if (snapshot_.remoteTarget.isEmpty()) {
        snapshot_.remoteTarget = defaultGateway();
    }
    if (snapshot_.remoteTarget.isEmpty()) {
        snapshot_.remoteTarget = QStringLiteral("n/a");
    }
    elapsed_.start();
    pingElapsed_.invalidate();
    tick();
    tickTimer_.start();
    startTraceroute();
}

void Collector::setSelectedInterface(const QString& selectedInterface) {
    snapshot_.selectedInterface = selectedInterface.isEmpty() ? QStringLiteral("ALL") : selectedInterface;
    previousCounters_.reset();
    refreshLocalInfo();
    Q_EMIT updated(snapshot_);
}

void Collector::setRemoteTarget(const QString& target) {
    const QString cleaned = cleanTarget(target);
    if (cleaned.isEmpty() || cleaned == snapshot_.remoteTarget) {
        return;
    }
    snapshot_.remoteTarget = cleaned;
    snapshot_.pingValid = false;
    snapshot_.averagePingMs = 0.0;
    snapshot_.hopValid = false;
    snapshot_.hopCount = 0;
    pingSamples_.clear();
    pingElapsed_.invalidate();
    stopProcess(pingProcess_);
    stopProcess(tracerouteProcess_);
    startTraceroute();
    maybeStartPing();
    Q_EMIT updated(snapshot_);
}

void Collector::resetSessionTotals() {
    snapshot_.rxSession = 0;
    snapshot_.txSession = 0;
    previousCounters_.reset();
    Q_EMIT updated(snapshot_);
}

void Collector::startSimulation(const QString& monthKey, std::uint64_t rxMonth, std::uint64_t txMonth) {
    tickTimer_.stop();
    stopProcess(pingProcess_);
    stopProcess(tracerouteProcess_);
    previousCounters_.reset();
    previousCpu_.reset();
    pingSamples_.clear();
    hasActivity_ = true;
    snapshot_ = CollectorSnapshot{};
    snapshot_.monthKey = monthKey.isEmpty() ? AppSettings::currentMonthKey() : monthKey;
    snapshot_.rxMonth = rxMonth;
    snapshot_.txMonth = txMonth;
    simulationFrameIndex_ = 0;
    simulationTick();
    simulationTimer_.start();
}

CollectorSnapshot Collector::snapshot() const {
    return snapshot_;
}

QString Collector::defaultGateway() const {
    return QString::fromStdString(parseDefaultGatewayHex(readTextFile(QStringLiteral("/proc/net/route")).toStdString()));
}

void Collector::tick() {
    const qint64 elapsedMs = elapsed_.isValid() ? elapsed_.restart() : 500;
    const double elapsedSeconds = std::max(0.001, static_cast<double>(elapsedMs) / 1000.0);
    rollMonthIfNeeded();
    refreshNetwork(elapsedSeconds);
    refreshCpu();
    refreshThreads();
    refreshLocalInfo();
    maybeStartPing();
    Q_EMIT updated(snapshot_);
}

void Collector::simulationTick() {
    const auto frames = buildTraySimulationFrames();
    if (frames.empty()) {
        return;
    }
    applyTraySimulationFrame(snapshot_, frames.at(static_cast<std::size_t>(simulationFrameIndex_) % frames.size()));
    simulationFrameIndex_ = (simulationFrameIndex_ + 1) % static_cast<int>(frames.size());
    Q_EMIT updated(snapshot_);
}

void Collector::refreshNetwork(double elapsedSeconds) {
    const QString text = readTextFile(QStringLiteral("/proc/net/dev"));
    const auto counters = parseProcNetDev(text.toStdString());
    snapshot_.interfaces = interfaceNames(counters);
    if (snapshot_.selectedInterface != QStringLiteral("ALL") && !snapshot_.interfaces.contains(snapshot_.selectedInterface)) {
        snapshot_.selectedInterface = QStringLiteral("ALL");
        previousCounters_.reset();
        Q_EMIT interfaceFallbackToAll();
    }

    const auto selected = selectNetworkCounters(counters, snapshot_.selectedInterface.toStdString());
    std::uint64_t rxDelta = 0;
    std::uint64_t txDelta = 0;
    if (previousCounters_.has_value()) {
        rxDelta = nonNegativeDelta(previousCounters_->rxBytes, selected.rxBytes);
        txDelta = nonNegativeDelta(previousCounters_->txBytes, selected.txBytes);
    }
    previousCounters_ = selected;

    snapshot_.rxDelta = rxDelta;
    snapshot_.txDelta = txDelta;
    snapshot_.rxRate = static_cast<double>(rxDelta) / elapsedSeconds;
    snapshot_.txRate = static_cast<double>(txDelta) / elapsedSeconds;
    snapshot_.rxSession += rxDelta;
    snapshot_.txSession += txDelta;
    snapshot_.rxMonth += rxDelta;
    snapshot_.txMonth += txDelta;
    if (rxDelta > 0 || txDelta > 0) {
        hasActivity_ = true;
        activityElapsed_.restart();
    }
    snapshot_.lastActivityAgeMs = hasActivity_ ? activityElapsed_.elapsed() : -1;
}

void Collector::refreshCpu() {
    const auto parsed = parseProcStatCpuLine(readTextFile(QStringLiteral("/proc/stat")).toStdString());
    if (!parsed.has_value()) {
        snapshot_.cpuPercent = 0.0;
        return;
    }
    if (previousCpu_.has_value()) {
        snapshot_.cpuPercent = std::clamp(cpuLoadPercent(*previousCpu_, *parsed), 0.0, 100.0);
    }
    previousCpu_ = *parsed;
}

void Collector::refreshThreads() {
    snapshot_.threadTotal = parseLoadAvgThreadTotal(readTextFile(QStringLiteral("/proc/loadavg")).toStdString());
}

void Collector::refreshLocalInfo() {
    snapshot_.hostname = QHostInfo::localHostName();
    snapshot_.ipAddress = firstAddressForInterface(snapshot_.selectedInterface);
    if (snapshot_.ipAddress.isEmpty()) {
        snapshot_.ipAddress = QStringLiteral("n/a");
    }
}

void Collector::maybeStartPing() {
    if (snapshot_.remoteTarget.isEmpty() || snapshot_.remoteTarget == QStringLiteral("n/a")) {
        return;
    }
    if (pingProcess_.state() != QProcess::NotRunning) {
        return;
    }
    if (pingElapsed_.isValid() && pingElapsed_.elapsed() < 5000) {
        return;
    }
    const QString ping = QStandardPaths::findExecutable(QStringLiteral("ping"));
    if (ping.isEmpty()) {
        snapshot_.pingValid = false;
        return;
    }
    pingElapsed_.restart();
    pingProcess_.start(ping, {QStringLiteral("-c"), QStringLiteral("1"), QStringLiteral("-W"), QStringLiteral("2"), snapshot_.remoteTarget});
}

void Collector::startTraceroute() {
    if (snapshot_.remoteTarget.isEmpty() || snapshot_.remoteTarget == QStringLiteral("n/a")) {
        snapshot_.hopValid = false;
        return;
    }
    const QString traceroute = QStandardPaths::findExecutable(QStringLiteral("traceroute"));
    if (traceroute.isEmpty()) {
        snapshot_.hopValid = false;
        snapshot_.hopCount = 0;
        return;
    }
    tracerouteProcess_.start(traceroute, {QStringLiteral("-n"), QStringLiteral("-m"), QStringLiteral("30"), QStringLiteral("-q"), QStringLiteral("1"), snapshot_.remoteTarget});
}

void Collector::stopProcess(QProcess& process) {
    if (process.state() == QProcess::NotRunning) {
        return;
    }
    process.terminate();
    if (!process.waitForFinished(1000)) {
        process.kill();
        process.waitForFinished(1000);
    }
}

void Collector::pingFinished(int exitCode, QProcess::ExitStatus status) {
    const QString output = QString::fromLocal8Bit(pingProcess_.readAllStandardOutput()) + QString::fromLocal8Bit(pingProcess_.readAllStandardError());
    if (status == QProcess::NormalExit && exitCode == 0) {
        const QRegularExpression re(QStringLiteral("time=([0-9]+(?:\\.[0-9]+)?)\\s*ms"));
        const auto match = re.match(output);
        if (match.hasMatch()) {
            pingSamples_.push_back(match.captured(1).toDouble());
            while (pingSamples_.size() > 10) {
                pingSamples_.pop_front();
            }
            const double total = std::accumulate(pingSamples_.begin(), pingSamples_.end(), 0.0);
            snapshot_.averagePingMs = total / static_cast<double>(pingSamples_.size());
            snapshot_.pingValid = true;
        }
    }
    Q_EMIT updated(snapshot_);
}

void Collector::tracerouteFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(exitCode)
    const QString output = QString::fromLocal8Bit(tracerouteProcess_.readAllStandardOutput()) + QString::fromLocal8Bit(tracerouteProcess_.readAllStandardError());
    const int hops = status == QProcess::NormalExit ? parseTracerouteHopCount(output.toStdString()) : 0;
    snapshot_.hopCount = hops;
    snapshot_.hopValid = hops > 0;
    Q_EMIT updated(snapshot_);
}

QString Collector::readTextFile(const QString& path) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromLocal8Bit(file.readAll());
}

QString Collector::firstAddressForInterface(const QString& interfaceName) const {
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : interfaces) {
        if (!(iface.flags().testFlag(QNetworkInterface::IsUp)) || iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        if (interfaceName != QStringLiteral("ALL") && iface.name() != interfaceName) {
            continue;
        }
        for (const auto& entry : iface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
                return address.toString();
            }
        }
    }
    return {};
}

void Collector::rollMonthIfNeeded() {
    const QString current = AppSettings::currentMonthKey();
    if (snapshot_.monthKey == current) {
        return;
    }
    snapshot_.monthKey = current;
    snapshot_.rxMonth = 0;
    snapshot_.txMonth = 0;
}

} // namespace nsl
