// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 NAME

#include "TrayIconVisual.h"
#include "TraySimulation.h"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QSize>

#include <cstdlib>
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

int countNear(const QImage& image, const QColor& target, int tolerance, QRect bounds = {}) {
    if (bounds.isNull()) {
        bounds = image.rect();
    }
    int count = 0;
    for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
        for (int x = bounds.left(); x <= bounds.right(); ++x) {
            const QColor color = QColor::fromRgb(image.pixel(x, y));
            if (std::abs(color.red() - target.red()) <= tolerance &&
                std::abs(color.green() - target.green()) <= tolerance &&
                std::abs(color.blue() - target.blue()) <= tolerance) {
                ++count;
            }
        }
    }
    return count;
}

void expectWhiteHalves(const nsl::TrayVisualState& state, int size, bool leftWhite, bool rightWhite, const char* expression) {
    const QImage image = nsl::renderTrayIconImage(state, QSize(size, size));
    const QRect left(0, 0, size / 2 - 1, (size * 3) / 4);
    const QRect right(size / 2, 0, size - size / 2, (size * 3) / 4);
    const int leftCount = countNear(image, Qt::white, 8, left);
    const int rightCount = countNear(image, Qt::white, 8, right);
    const int threshold = size == 16 ? 5 : 12;
    expectTrue(leftWhite ? leftCount >= threshold : leftCount < threshold, expression);
    expectTrue(rightWhite ? rightCount >= threshold : rightCount < threshold, expression);
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    using namespace nsl;

    expectEqual(trayActivityBucket(-1), TrayActivityBucket::Red, "no prior activity is red");
    expectEqual(trayActivityBucket(0), TrayActivityBucket::Green, "active tick is green");
    expectEqual(trayActivityBucket(59999), TrayActivityBucket::Green, "silence under 60s is green");
    expectEqual(trayActivityBucket(60000), TrayActivityBucket::Yellow, "silence at 60s is yellow");
    expectEqual(trayActivityBucket(119999), TrayActivityBucket::Yellow, "silence under 120s is yellow");
    expectEqual(trayActivityBucket(120000), TrayActivityBucket::Red, "silence at 120s is red");

    const TrayVisualState rxOnly = makeTrayVisualState(0, 512, 0);
    const TrayVisualState txOnly = makeTrayVisualState(512, 0, 0);
    const TrayVisualState both = makeTrayVisualState(512, 512, 0);
    expectTrue(!rxOnly.txActive && rxOnly.rxActive, "rx-only maps to right-half state only");
    expectTrue(txOnly.txActive && !txOnly.rxActive, "tx-only maps to left-half state only");
    expectTrue(both.txActive && both.rxActive, "bidirectional maps to both halves");

    expectWhiteHalves(rxOnly, 22, false, true, "22px rx-only white half placement");
    expectWhiteHalves(txOnly, 22, true, false, "22px tx-only white half placement");
    expectWhiteHalves(both, 22, true, true, "22px bidirectional white half placement");
    expectWhiteHalves(rxOnly, 16, false, true, "16px rx-only white half placement");
    expectWhiteHalves(txOnly, 16, true, false, "16px tx-only white half placement");
    expectWhiteHalves(both, 16, true, true, "16px bidirectional white half placement");

    const QImage greenIcon = renderTrayIconImage(makeTrayVisualState(0, 0, 0), QSize(22, 22));
    const QImage yellowIcon = renderTrayIconImage(makeTrayVisualState(0, 0, 60000), QSize(22, 22));
    const QImage redIcon = renderTrayIconImage(makeTrayVisualState(0, 0, 120000), QSize(22, 22));
    const QRect bottom(0, 14, 22, 8);
    expectTrue(countNear(greenIcon, trayActivityColor(TrayActivityBucket::Green), 80, bottom) >= 4, "green triangle visible at 22px");
    expectTrue(countNear(yellowIcon, trayActivityColor(TrayActivityBucket::Yellow), 80, bottom) >= 4, "yellow triangle visible at 22px");
    expectTrue(countNear(redIcon, trayActivityColor(TrayActivityBucket::Red), 80, bottom) >= 4, "red triangle visible at 22px");

    TrayIconRenderer renderer;
    expectTrue(renderer.update(rxOnly), "first icon update regenerates pixmap");
    expectEqual(renderer.regenerationCount(), 1, "first regeneration count");
    expectTrue(!renderer.update(rxOnly), "same visual state does not regenerate pixmap");
    expectEqual(renderer.regenerationCount(), 1, "unchanged regeneration count");
    expectTrue(renderer.update(txOnly), "changed visual state regenerates pixmap");
    expectEqual(renderer.regenerationCount(), 2, "changed regeneration count");

    expectTrue(trayActivationTogglesWindow(QSystemTrayIcon::Trigger), "StatusNotifierItem Activate/Trigger toggles the window");
    expectTrue(!trayActivationTogglesWindow(QSystemTrayIcon::Context), "right-click context menu does not toggle the window");
    expectTrue(!trayActivationTogglesWindow(QSystemTrayIcon::DoubleClick), "double-click does not double-toggle the window");

    const auto frames = buildTraySimulationFrames();
    expectTrue(frames.size() >= 13, "simulation has enough frames to cover bursts and silence thresholds");
    expectEqual(makeTrayVisualState(frames.at(0).txDelta, frames.at(0).rxDelta, frames.at(0).lastActivityAgeMs), rxOnly, "simulation starts with rx-only burst");
    expectEqual(makeTrayVisualState(frames.at(3).txDelta, frames.at(3).rxDelta, frames.at(3).lastActivityAgeMs), txOnly, "simulation includes tx-only burst");
    expectEqual(makeTrayVisualState(frames.at(6).txDelta, frames.at(6).rxDelta, frames.at(6).lastActivityAgeMs), both, "simulation includes bidirectional burst");
    expectEqual(trayActivityBucket(frames.at(10).lastActivityAgeMs), TrayActivityBucket::Green, "pre-60s silence remains green");
    expectEqual(trayActivityBucket(frames.at(11).lastActivityAgeMs), TrayActivityBucket::Yellow, "60s silence turns yellow");
    expectEqual(trayActivityBucket(frames.at(12).lastActivityAgeMs), TrayActivityBucket::Red, "120s silence turns red");

    if (failures != 0) {
        std::cerr << failures << " tray icon test failure(s)\n";
        return 1;
    }
    return 0;
}
