// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "GraphPane.h"
#include "PaneWidget.h"
#include "Theme.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRect>
#include <QString>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void expectTrue(bool value, const char* expression) {
    if (!value) {
        std::cerr << "FAIL: " << expression << "\n";
        ++failures;
    }
}

template <typename L, typename R>
void expectEqual(const L& left, const R& right, const char* expression) {
    if (!(left == right)) {
        std::cerr << "FAIL: " << expression << "\n";
        ++failures;
    }
}

bool nearColor(const QColor& color, const QColor& target, int tolerance) {
    return std::abs(color.red() - target.red()) <= tolerance &&
           std::abs(color.green() - target.green()) <= tolerance &&
           std::abs(color.blue() - target.blue()) <= tolerance;
}

double relativeLuminance(const QColor& color) {
    const auto linear = [](double component) {
        component /= 255.0;
        return component <= 0.04045 ? component / 12.92 : std::pow((component + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.red()) + 0.7152 * linear(color.green()) + 0.0722 * linear(color.blue());
}

double contrastRatio(const QColor& first, const QColor& second) {
    const double firstLuminance = relativeLuminance(first);
    const double secondLuminance = relativeLuminance(second);
    return (std::max(firstLuminance, secondLuminance) + 0.05) /
           (std::min(firstLuminance, secondLuminance) + 0.05);
}

int countNear(const QImage& image, const QColor& target, int tolerance, QRect bounds = {}) {
    if (bounds.isNull()) {
        bounds = image.rect();
    }
    int count = 0;
    for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
        for (int x = bounds.left(); x <= bounds.right(); ++x) {
            if (nearColor(QColor::fromRgb(image.pixel(x, y)), target, tolerance)) {
                ++count;
            }
        }
    }
    return count;
}

QImage renderWidget(QWidget& widget) {
    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    widget.render(&image);
    return image;
}

bool writeText(const QString& path, const QString& content) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(content.toUtf8()) == content.toUtf8().size() && file.flush();
}

QString readAll(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QImage renderGraphPane(const QString& title, nsl::GraphValueMode mode, const std::vector<double>& samples) {
    nsl::GraphPane graph(title, mode);
    graph.resize(238, graph.preferredHeight());
    for (double sample : samples) {
        graph.pushSample(sample);
    }
    return renderWidget(graph);
}

void writeDynamicScaleReport(const QString& path) {
    constexpr double kb = 1024.0;
    std::vector<double> threads;
    std::vector<double> cpu;
    std::vector<double> traffic;
    threads.reserve(nsl::GraphPane::MaxSamplesForRendering);
    cpu.reserve(nsl::GraphPane::MaxSamplesForRendering);
    traffic.reserve(nsl::GraphPane::MaxSamplesForRendering);
    for (int i = 0; i < 60; ++i) {
        threads.push_back(3077.0 + static_cast<double>((i * 17) % 97));
        cpu.push_back(31.0 + 6.0 * std::sin(static_cast<double>(i) / 5.0));
    }
    traffic.push_back(659.0 * kb);
    for (int i = 0; i < 59; ++i) {
        traffic.push_back(0.0);
    }
    traffic.push_back(64.0 * kb);

    const std::vector<QImage> panels = {
        renderGraphPane(QStringLiteral("Threads"), nsl::GraphValueMode::Count, threads),
        renderGraphPane(QStringLiteral("CPU"), nsl::GraphValueMode::Percent, cpu),
        renderGraphPane(QStringLiteral("Incoming"), nsl::GraphValueMode::NetworkRate, traffic),
    };
    const int gap = 8;
    QImage sheet(238, static_cast<int>(panels.size()) * panels.front().height() + gap * (static_cast<int>(panels.size()) - 1),
                 QImage::Format_ARGB32_Premultiplied);
    sheet.fill(nsl::Theme::Background);
    QPainter painter(&sheet);
    int y = 0;
    for (const QImage& panel : panels) {
        painter.drawImage(0, y, panel);
        y += panel.height() + gap;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    sheet.save(path, "PNG");
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    using namespace nsl;

    expectEqual(Theme::GraphFill, QColor(0x00, 0xe0, 0xe0), "graph fill is AnalogX cyan");
    expectEqual(Theme::ValueText, QColor(0x40, 0xe8, 0xe8), "value text is brighter cyan");
    expectEqual(Theme::LabelText, QColor(0xa8, 0xa0, 0x78), "labels are dim warm gray-olive");
    expectEqual(Theme::HeaderText, Theme::GraphFill, "section headers use graph cyan");
    expectEqual(PaneWidget::valueColor(), Theme::ValueText, "PaneWidget value color comes from Theme");
    expectEqual(PaneWidget::dimColor(), Theme::LabelText, "PaneWidget dim label color comes from Theme");
    expectTrue(contrastRatio(Theme::AverageLine, Theme::Background) >= 3.0,
               "average reference line has 3:1 contrast against the graph background");
    expectTrue(contrastRatio(Theme::AverageLine, Theme::GraphFill) >= 3.0,
               "average reference line has 3:1 contrast where it crosses graph fill");

    const QFont headerFont = Theme::headerFont();
    const QFont labelFont = Theme::labelFont();
    const QFont valueFont = Theme::valueFont();
    const QFont graphValueFont = Theme::graphValueFont();
    expectEqual(headerFont.pixelSize(), 12, "header font uses fixed pixels instead of screen-dependent points");
    expectEqual(labelFont.pixelSize(), 12, "label font uses fixed pixels instead of screen-dependent points");
    expectEqual(valueFont.pixelSize(), 12, "value font uses fixed pixels instead of screen-dependent points");
    expectEqual(graphValueFont.pixelSize(), 17, "graph value font uses fixed pixels instead of screen-dependent points");
    expectEqual(headerFont.weight(), QFont::Normal, "header font keeps the regular face");
    expectEqual(labelFont.weight(), QFont::Normal, "label font keeps the regular face");
    expectEqual(valueFont.weight(), QFont::Normal, "value font does not substitute a bold face");
    expectEqual(graphValueFont.weight(), QFont::Normal, "graph value font does not substitute a bold face");

    const auto labels = graphMetricLabels();
    expectEqual(labels[0], QStringLiteral("Current"), "first graph label is spelled out");
    expectEqual(labels[1], QStringLiteral("Average"), "second graph label is spelled out");
    expectEqual(labels[2], QStringLiteral("Max"), "third graph label is spelled out");

    GraphPane graph(QStringLiteral("Incoming"), GraphValueMode::NetworkRate);
    graph.resize(238, graph.preferredHeight());
    graph.pushSample(0.0);
    graph.pushSample(1024.0 * 12.0);
    graph.pushSample(1024.0 * 48.0);
    graph.pushSample(1024.0 * 18.0);
    graph.pushSample(1024.0 * 4.0);
    graph.pushSample(0.0);
    expectEqual(graph.accessibleName(), QStringLiteral("Incoming"), "graph pane exposes its title to assistive technology");
    expectTrue(graph.accessibleDescription().contains(QStringLiteral("Current")) &&
                   graph.accessibleDescription().contains(QStringLiteral("Average")) &&
                   graph.accessibleDescription().contains(QStringLiteral("Maximum")),
               "graph pane exposes current, average, and maximum values to assistive technology");
    const QImage graphImage = renderWidget(graph);
    expectTrue(countNear(graphImage, Theme::GraphFill, 16) >= 30, "rendered graph contains cyan filled area/header rule pixels");
    expectEqual(countNear(graphImage, QColor(0x00, 0xe0, 0x00), 8), 0, "rendered graph contains no legacy bright green pixels");

    const std::vector<double> raw = {0.0, 30.0, 90.0, 30.0, 0.0};
    const auto smooth = smoothGraphSamples(raw);
    expectEqual(smooth.size(), raw.size(), "smoothing preserves sample count");
    expectTrue(smooth[2] < raw[2], "smoothing softens isolated peaks");
    expectTrue(smooth[1] > raw[1], "smoothing spreads adjacent slope upward");
    const std::vector<double> edgeBurst = {0.0, 0.0, 0.0, 90.0};
    const auto edgeSmooth = smoothGraphSamples(edgeBurst);
    expectEqual(edgeSmooth.back(), edgeBurst.back(), "smoothing preserves newest edge burst height");

    const std::vector<double> narrowThreads = {3077.0, 3091.0, 3110.0, 3173.0};
    const GraphScaleRange threadTarget = targetGraphScaleRange(GraphValueMode::Count, narrowThreads);
    expectTrue(threadTarget.minimum > 3000.0, "thread graph baseline is windowed around visible values, not zero");
    expectTrue(threadTarget.minimum < 3077.0, "thread graph range has lower padding");
    expectTrue(threadTarget.maximum > 3173.0, "thread graph range has upper padding");
    expectTrue(normalizeGraphSample(3173.0, threadTarget) - normalizeGraphSample(3077.0, threadTarget) > 0.7,
               "narrow thread variation uses most of graph height");

    const std::vector<double> traffic = {0.0, 1200.0, 2000.0};
    const GraphScaleRange trafficTarget = targetGraphScaleRange(GraphValueMode::NetworkRate, traffic);
    expectEqual(trafficTarget.minimum, 0.0, "traffic graph keeps zero baseline");
    expectTrue(trafficTarget.maximum > 2000.0 && trafficTarget.maximum < 2300.0,
               "traffic graph top scales to visible max with modest padding");
    expectTrue(!trafficTarget.drawEmpty, "nonzero traffic graph renders area");

    const std::vector<double> allZeroTraffic = {0.0, 0.0, 0.0};
    const GraphScaleRange zeroTrafficTarget = targetGraphScaleRange(GraphValueMode::NetworkRate, allZeroTraffic);
    expectEqual(zeroTrafficTarget.minimum, 0.0, "all-zero traffic minimum remains zero");
    expectEqual(zeroTrafficTarget.drawEmpty, true, "all-zero traffic graph renders empty");

    const std::vector<double> flatThreads = {3173.0, 3173.0, 3173.0};
    const GraphScaleRange flatTarget = targetGraphScaleRange(GraphValueMode::Count, flatThreads);
    expectTrue(flatTarget.minimum < 3173.0 && flatTarget.maximum > 3173.0, "flat non-traffic graph gets artificial range");
    expectTrue(std::abs(normalizeGraphSample(3173.0, flatTarget) - 0.5) < 0.02,
               "flat non-traffic graph sits at mid-height");

    const GraphScaleRange eased = easeGraphScaleRange({0.0, 1000.0, false}, {0.0, 2000.0, false}, true);
    expectTrue(eased.maximum > 1150.0 && eased.maximum < 1250.0, "scale easing moves 20 percent toward target");

    QTemporaryDir screenshotDir;
    const QString binaryPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("netstats-live"));
    expectTrue(QFile::exists(binaryPath), "netstats-live binary exists beside visual theme test");
    QProcess screenshotProcess;
    QProcessEnvironment screenshotEnv = QProcessEnvironment::systemEnvironment();
    screenshotEnv.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    screenshotEnv.insert(QStringLiteral("XDG_CONFIG_HOME"), screenshotDir.path());
    screenshotEnv.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("unix:path=%1/no-session-bus").arg(screenshotDir.path()));
    screenshotProcess.setProcessEnvironment(screenshotEnv);
    const QString screenshotPath = QDir(screenshotDir.path()).filePath(QStringLiteral("visual.png"));
    screenshotProcess.setProgram(binaryPath);
    screenshotProcess.setArguments({QStringLiteral("--screenshot"), screenshotPath});
    screenshotProcess.start();
    expectTrue(screenshotProcess.waitForFinished(10000), "screenshot mode exits promptly");
    expectEqual(screenshotProcess.exitCode(), 0, "screenshot mode exits cleanly");
    expectTrue(QFile::exists(screenshotPath), "screenshot mode writes PNG");
    const QString screenshotStderr = QString::fromUtf8(screenshotProcess.readAllStandardError());
    expectTrue(!screenshotStderr.contains(QStringLiteral("traceroute"), Qt::CaseInsensitive), "screenshot mode does not start live traceroute collector");
    const QImage screenshot(screenshotPath);
    expectEqual(screenshot.size(), QSize(238, 523), "clean screenshot has deterministic compact dimensions");
    const QString cleanConfigPath = QDir(screenshotDir.path()).filePath(QStringLiteral("netstats-live/netstats-live.conf"));
    expectTrue(!QFile::exists(cleanConfigPath), "screenshot mode does not create user config");
    const QRect minimizeButtonBounds(screenshot.width() - 18, 1, 16, 16);
    expectTrue(countNear(screenshot, Theme::HeaderText, 8, minimizeButtonBounds) >= 6,
               "title strip visibly renders the cyan minimize control");

    QTemporaryDir customScreenshotDir;
    const QString customConfigPath = QDir(customScreenshotDir.path()).filePath(QStringLiteral("netstats-live/netstats-live.conf"));
    const QString customConfig = QStringLiteral(
        "[config]\n"
        "alwaysOnTop=true\n"
        "unitMode=bits\n"
        "[panes]\n"
        "localMachine=false\n"
        "remoteMachine=false\n"
        "incomingTotals=false\n"
        "incoming=false\n"
        "outgoingTotals=false\n"
        "outgoing=false\n"
        "threads=true\n"
        "cpu=false\n");
    const QString legacyAutostartPath = QDir(customScreenshotDir.path()).filePath(QStringLiteral("autostart/nsl-linux.desktop"));
    const QString legacyAutostart = QStringLiteral("[Desktop Entry]\nType=Application\nName=NSL-Linux\nExec=nsl-linux --minimized\n");
    expectTrue(writeText(customConfigPath, customConfig), "custom screenshot config fixture written");
    expectTrue(writeText(legacyAutostartPath, legacyAutostart), "legacy autostart fixture written");

    QProcess customScreenshotProcess;
    QProcessEnvironment customScreenshotEnv = QProcessEnvironment::systemEnvironment();
    customScreenshotEnv.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    customScreenshotEnv.insert(QStringLiteral("XDG_CONFIG_HOME"), customScreenshotDir.path());
    customScreenshotEnv.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("unix:path=%1/no-session-bus").arg(customScreenshotDir.path()));
    customScreenshotProcess.setProcessEnvironment(customScreenshotEnv);
    const QString customScreenshotPath = QDir(customScreenshotDir.path()).filePath(QStringLiteral("visual.png"));
    customScreenshotProcess.setProgram(binaryPath);
    customScreenshotProcess.setArguments({QStringLiteral("--screenshot"), customScreenshotPath});
    customScreenshotProcess.start();
    expectTrue(customScreenshotProcess.waitForFinished(10000), "custom-config screenshot mode exits promptly");
    expectEqual(customScreenshotProcess.exitCode(), 0, "custom-config screenshot mode exits cleanly");
    const QImage customScreenshot(customScreenshotPath);
    expectEqual(customScreenshot.size(), QSize(238, 523), "disabled panes cannot collapse screenshot width");
    expectEqual(customScreenshot, screenshot, "screenshot output is independent of user settings");
    expectEqual(readAll(customConfigPath), customConfig, "screenshot mode leaves existing config byte-identical");
    expectEqual(readAll(legacyAutostartPath), legacyAutostart, "screenshot mode does not migrate legacy autostart");
    expectTrue(!QFile::exists(QDir(customScreenshotDir.path()).filePath(QStringLiteral("autostart/netstats-live.desktop"))),
               "screenshot mode creates no replacement autostart entry");

    if (qEnvironmentVariableIsSet("NSL_DYNAMIC_SCALE_REPORT")) {
        writeDynamicScaleReport(QString::fromLocal8Bit(qgetenv("NSL_DYNAMIC_SCALE_REPORT")));
    }

    if (failures != 0) {
        std::cerr << failures << " visual theme test failure(s)\n";
        return 1;
    }
    return 0;
}
