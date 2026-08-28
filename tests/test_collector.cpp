// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "Collector.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTimer>

#include <iostream>

namespace {

int failures = 0;

void expectTrue(bool value, const char* expression) {
    if (!value) {
        std::cerr << "FAIL: " << expression << "\n";
        ++failures;
    }
}

bool writeExecutable(const QString& path, const QString& content) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    if (file.write(content.toUtf8()) != content.toUtf8().size() || !file.flush()) {
        return false;
    }
    file.close();
    return QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}

QString readAll(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    expectTrue(temp.isValid(), "collector probe temporary directory is valid");

    const QString binDir = QDir(temp.path()).filePath(QStringLiteral("bin"));
    expectTrue(QDir().mkpath(binDir), "collector probe bin directory created");
    const QString statePath = QDir(temp.path()).filePath(QStringLiteral("ping-state"));
    const QString pingLog = QDir(temp.path()).filePath(QStringLiteral("ping-args"));
    const QString tracerouteLog = QDir(temp.path()).filePath(QStringLiteral("traceroute-args"));
    const QString localeLog = QDir(temp.path()).filePath(QStringLiteral("probe-locale"));

    const QString pingScript = QStringLiteral(
        "#!/bin/sh\n"
        "printf '%s\\n' \"$LC_ALL\" > \"$NSL_PROBE_LOCALE\"\n"
        "printf 'ping' >> \"$NSL_PING_LOG\"\n"
        "for arg in \"$@\"; do printf '<%s>' \"$arg\" >> \"$NSL_PING_LOG\"; done\n"
        "printf '\\n' >> \"$NSL_PING_LOG\"\n"
        "if test ! -f \"$NSL_PING_STATE\"; then\n"
        "  : > \"$NSL_PING_STATE\"\n"
        "  printf '64 bytes from 192.0.2.1: time=10.0 ms\\n'\n"
        "  exit 0\n"
        "fi\n"
        "exit 1\n");
    const QString tracerouteScript = QStringLiteral(
        "#!/bin/sh\n"
        "printf 'traceroute' >> \"$NSL_TRACEROUTE_LOG\"\n"
        "for arg in \"$@\"; do printf '<%s>' \"$arg\" >> \"$NSL_TRACEROUTE_LOG\"; done\n"
        "printf '\\n 1  192.0.2.1  1.0 ms\\n'\n");

    expectTrue(writeExecutable(QDir(binDir).filePath(QStringLiteral("ping")), pingScript), "fake ping executable created");
    expectTrue(writeExecutable(QDir(binDir).filePath(QStringLiteral("traceroute")), tracerouteScript), "fake traceroute executable created");

    const QByteArray originalPath = qgetenv("PATH");
    qputenv("PATH", (binDir + QDir::listSeparator() + QString::fromLocal8Bit(originalPath)).toLocal8Bit());
    qputenv("NSL_PING_STATE", statePath.toLocal8Bit());
    qputenv("NSL_PING_LOG", pingLog.toLocal8Bit());
    qputenv("NSL_TRACEROUTE_LOG", tracerouteLog.toLocal8Bit());
    qputenv("NSL_PROBE_LOCALE", localeLog.toLocal8Bit());
    qunsetenv("LC_ALL");

    {
        nsl::Collector collector;
        collector.start(QStringLiteral("ALL"), QStringLiteral("probe.invalid"), QStringLiteral("2026-08"), 0, 0);

        QTimer::singleShot(6500, &app, &QCoreApplication::quit);
        app.exec();

        const nsl::CollectorSnapshot snapshot = collector.snapshot();
        expectTrue(!snapshot.pingValid, "a failed follow-up ping invalidates the stale successful result");
    }

    {
        nsl::Collector invalidTargetCollector;
        invalidTargetCollector.start(QStringLiteral("ALL"), QStringLiteral("https://-f"), QStringLiteral("2026-08"), 0, 0);
        QTimer::singleShot(250, &app, &QCoreApplication::quit);
        app.exec();
    }

    const QString pingArguments = readAll(pingLog);
    const QString tracerouteArguments = readAll(tracerouteLog);
    expectTrue(pingArguments.contains(QStringLiteral("<--><probe.invalid>")), "ping destination follows an option terminator");
    expectTrue(tracerouteArguments.contains(QStringLiteral("<-w><1><--><probe.invalid>")), "traceroute has a bounded wait and option terminator");
    expectTrue(!pingArguments.contains(QStringLiteral("<-f>")) && !tracerouteArguments.contains(QStringLiteral("<-f>")) &&
                   !pingArguments.contains(QStringLiteral("<https://-f>")) &&
                   !tracerouteArguments.contains(QStringLiteral("<https://-f>")),
               "URL host extraction cannot bypass probe-target validation");
    expectTrue(readAll(localeLog).trimmed() == QStringLiteral("C"), "external probe parsing uses the C locale");

    qputenv("PATH", originalPath);
    qunsetenv("NSL_PING_STATE");
    qunsetenv("NSL_PING_LOG");
    qunsetenv("NSL_TRACEROUTE_LOG");
    qunsetenv("NSL_PROBE_LOCALE");

    if (failures != 0) {
        std::cerr << failures << " collector test failure(s)\n";
        return 1;
    }
    return 0;
}
