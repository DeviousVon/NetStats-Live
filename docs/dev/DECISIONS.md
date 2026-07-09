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


## 2026-07-07 — Lifecycle and packaging

Source: Bob's fourth prompt.
Reason: finish user-facing app lifecycle behavior and produce an installable Ubuntu package.

Decisions:
- Use `NSL_FAKE_DATE=YYYY-MM-DD` or `YYYY-MM` as a test-only date override for monthly total rollover.
- Archive monthly totals under `history/<YYYY-MM>/rxMonth` and `history/<YYYY-MM>/txMonth` while resetting the active month bucket on rollover.
- Generate user autostart entries from the current executable path, quote paths with spaces, and include `X-KDE-autostart-after=panel` so the SNI host is available first.
- Use DBus for single-instance activation (`io.github.DeviousVon.NetStatsLive`, `/io/github/DeviousVon/NetStatsLive/MainWindow`) rather than a local socket.
- Package with CPack DEB, install a clean-room hicolor icon, and keep generated `.deb` files out of git while leaving them in `outputs/final/`.


## 2026-07-07 — Final QA lifecycle hardening

Source: final QA pass.
Reason: SIGTERM did not save totals/config before process exit.

Decision: bridge SIGTERM/SIGINT into the Qt event loop via a pipe and `QSocketNotifier`, then call `MainWindow::shutdownForSignal()` to flush totals/config before quitting. Hard crashes/SIGKILL remain bounded by the 60-second totals flush interval.

Wayland decision: Always on Top reapplication uses a live hide/show cycle because Qt Wayland window flags and LayerShellQt surface state require window recreation. This can flicker briefly but avoids process restart.
