# Tasks

## Completed

- [x] Create project workspace and local git repository.
- [x] Preserve AnalogX source page and reference screenshot.
- [x] Write core parser/format tests first and verify RED compile failure before implementation.
- [x] Implement CMake project and requested source layout.
- [x] Implement custom-painted text and graph panes.
- [x] Implement `/proc` network/CPU/thread collector.
- [x] Implement async ping and traceroute collection.
- [x] Implement runtime-painted tray icon.
- [x] Implement context menus and pane/config toggles.
- [x] Implement URL ClipCap with Klipper DBus fallback.
- [x] Implement QSettings persistence and autostart desktop-file writer.
- [x] Build, link, run tests, and smoke-run the app.

## Pending / likely next prompts

- [ ] Visual pass on live KDE Wayland desktop.
- [ ] Exact AnalogX fidelity adjustments.
- [ ] Broader automated tests.
- [ ] Packaging/install refinements.

## Completed in visual fidelity pass

- [x] Add `--screenshot <path>` deterministic visual regression mode.
- [x] Render PNG via `QWidget::grab()` under offscreen Qt.
- [x] Compare generated screenshot side-by-side with original reference.
- [x] Tighten pane headers, graph bars/grid/average line, value rows, margins, border, and font fallback.


## Completed in tray simulation/demo pass

- [x] Add hidden `--simulate` deterministic Collector traffic mode.
- [x] Add pure tray visual-state renderer/cache.
- [x] Add CTest tray coverage for TX/RX halves, 60s/120s triangle thresholds, cache churn, 16px/22px legibility, and activation mapping.
- [x] Verify KDE Wayland StatusNotifierItem registration and DBus Activate/ContextMenu calls.


## Completed in lifecycle/package pass

- [x] Add `NSL_FAKE_DATE` test override.
- [x] Archive old monthly totals to config history on rollover.
- [x] Add lifecycle tests for fake date, rollover, autostart, auto-minimize, and single-instance constants.
- [x] Add DBus single-instance guard and existing-window activation slot.
- [x] Add clean-room hicolor icon assets.
- [x] Add CPack DEB config with Qt runtime Depends and `traceroute` Recommends.
- [x] Build package and verify with `dpkg --dry-run -i`.
- [x] Update README with features, screenshots, KWin rule instructions, packaging, and AnalogX inspiration/clean-room note.


## Completed in final QA pass

- [x] Run real-traffic Wayland soak and record CPU/RSS stability.
- [x] Reproduce and fix SIGTERM totals/config persistence.
- [x] Verify SIGKILL/crash loses no more than the latest unflushed 60-second interval.
- [x] Tighten Always on Top reapplication with a live hide/show cycle for Wayland/layer-shell state.
- [x] Document Wayland limitations vs the original Windows app.
- [x] Add final QA report under `docs/dev/final-qa-2026-07-07.md`.

## 2026-07-07 AnalogX cyan visual rework

- [x] Replace legacy green palette with named centralized `Theme` cyan/teal + olive palette.
- [x] Rework pane headers to centered cyan text with side rules.
- [x] Spell graph labels as Current/Average/Max and enlarge graph values.
- [x] Replace vertical bar graphs/grid lines with smoothed filled cyan area graphs.
- [x] Apply the same palette/typography to Threads/CPU/network graph panes.
- [x] Render and compare screenshot artifacts against the supplied AnalogX reference.

## 2026-07-07 dynamic graph scaling

- [x] Add tests for windowed dynamic range, traffic zero baseline, all-zero traffic empty rendering, flat-data centering, scale easing, and edge burst preservation.
- [x] Implement visible-window graph scaling with per-mode baseline behavior and a dim average reference line.
- [x] Generate dynamic-scale visual report showing Threads, CPU, and Incoming burst behavior.
- [x] Run simulate and real-traffic smoke checks; fix live shutdown crash discovered during real-traffic verification.
