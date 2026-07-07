# Artifacts

## Reference

- `assets/reference/analogx-nsl.gif` — original AnalogX NetStat Live screenshot from the AnalogX page.
- `docs/source/analogx-nsl-Freeware.html` — saved source page HTML.

## Build outputs

Ignored by git:

- `build/nsl-linux` — compiled application from verified local build.
- `build/nsl_core_tests` — pure core test binary.
- `build/nsl_tray_tests` — tray visual-state/simulation test binary.
- `build/nsl_lifecycle_tests` — lifecycle/settings/package-support test binary.
- `.deps/` — local extracted Debian dev packages used only because sudo package install was not available non-interactively.

## Important source files

- `CMakeLists.txt`
- `src/main.cpp`
- `src/MainWindow.*`
- `src/PaneWidget.*`
- `src/GraphPane.*`
- `src/TextPane.*`
- `src/Collector.*`
- `src/TrayIcon.*`
- `src/TrayIconVisual.*`
- `src/TraySimulation.*`
- `src/ClipCap.*`
- `src/Settings.*`
- `src/Lifecycle.*`
- `src/Core.*`
- `tests/test_core.cpp`
- `tests/test_tray_icon.cpp`
- `tests/test_lifecycle.cpp`
- `config/nsl-linux.desktop`

## Visual fidelity outputs

- `outputs/reports/visual/nsl-visual-pass-final.png` — generated `--screenshot` output after visual pass.
- `outputs/reports/visual/nsl-visual-comparison-final.png` — side-by-side original vs generated comparison image.


## Tray simulation / StatusNotifier evidence

- Hidden runtime mode: `./build/nsl-linux --simulate --minimized`.
- Test target: `build/nsl_tray_tests` / CTest test `nsl_tray_tests`.
- Live KDE Wayland check: StatusNotifierWatcher listed an item with `Id`/`Title` `nsl-linux`; DBus `org.kde.StatusNotifierItem.Activate 0 0` and `ContextMenu 0 0` both returned exit 0 while simulate mode was running.


## Package outputs

- `outputs/final/nsl-linux_0.1.0_amd64.deb` — generated CPack Debian package for Ubuntu 24.04+; ignored by git as a build artifact.

Package contents verified with `dpkg-deb --contents`:

- `/usr/bin/nsl-linux`
- `/usr/share/applications/nsl-linux.desktop`
- `/usr/share/icons/hicolor/64x64/apps/nsl-linux.png`
- `/usr/share/icons/hicolor/128x128/apps/nsl-linux.png`
- `/usr/share/icons/hicolor/256x256/apps/nsl-linux.png`


## QA reports

- `docs/qa/final-qa-2026-07-07.md` — final real-traffic/performance, SIGTERM/SIGKILL, and Wayland limitation QA report.

## 2026-07-07 AnalogX cyan visual pass

- `reports/visual/nsl-analogx-cyan-pass.png` — fresh `--screenshot` render after reworking palette, headers, labels, typography, width, and graph style.
- `reports/visual/nsl-analogx-cyan-comparison.png` — crop of the supplied AnalogX reference next to the reworked render for visual comparison.
- Mechanical screenshot marker counts from the render: cyan 3328, brighter value cyan 848, olive 81, legacy bright green 0.
