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
