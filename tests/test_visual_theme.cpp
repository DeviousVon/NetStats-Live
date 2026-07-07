#include "GraphPane.h"
#include "PaneWidget.h"
#include "Theme.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRect>
#include <QString>
#include <QTemporaryDir>

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
    const QImage graphImage = renderWidget(graph);
    expectTrue(countNear(graphImage, Theme::GraphFill, 16) >= 30, "rendered graph contains cyan filled area/header rule pixels");
    expectEqual(countNear(graphImage, QColor(0x00, 0xe0, 0x00), 8), 0, "rendered graph contains no legacy bright green pixels");

    const std::vector<double> raw = {0.0, 30.0, 90.0, 30.0, 0.0};
    const auto smooth = smoothGraphSamples(raw);
    expectEqual(smooth.size(), raw.size(), "smoothing preserves sample count");
    expectTrue(smooth[2] < raw[2], "smoothing softens isolated peaks");
    expectTrue(smooth[1] > raw[1], "smoothing spreads adjacent slope upward");

    QTemporaryDir screenshotDir;
    const QString binaryPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("nsl-linux"));
    expectTrue(QFile::exists(binaryPath), "nsl-linux binary exists beside visual theme test");
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

    if (failures != 0) {
        std::cerr << failures << " visual theme test failure(s)\n";
        return 1;
    }
    return 0;
}
