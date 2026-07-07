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

## 2026-07-06 — Visual screenshot mode uses original-reference profile

Source: second creation prompt.
Reason: Bob requested iteration against the original AnalogX screenshot. `--screenshot` seeds deterministic visual data and hides Threads to match the reference image, while normal app use keeps Threads implemented/toggleable.

## 2026-07-06 — Match reference screenshot width for fidelity pass

Source: visual comparison against `assets/reference/analogx-nsl.gif`.
Reason: the reference image is 224 px wide; the first 170 px estimate made the columns/spacing too cramped for faithful proportions.
