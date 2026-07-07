# Decisions

## 2026-07-06 — Use Qt6 Widgets/QPainter, no QML

Source: Bob's prompt.
Reason: faithful late-90s desktop-widget clone and low dependency surface.

## 2026-07-06 — Local-first Linux implementation

Source: Bob's prompt and standing preference.
Reason: all monitoring data comes from local `/proc`; no cloud services or external telemetry.

## 2026-07-06 — Optional layer-shell-qt

Source: Bob's prompt.
Reason: KDE Wayland often ignores `Qt::WindowStaysOnTopHint`; layer-shell gives a better overlay path when available while preserving portability when missing.

## 2026-07-06 — Use AnalogX page screenshot as reference

Source: AnalogX page inspection.
Reason: no separate screenshot was attached in-chat; the page exposes `nsl.gif`, which is the canonical available visual reference.
