# Artifacts

## Reference

- `assets/reference/analogx-nsl.gif` — original AnalogX NetStat Live screenshot from the AnalogX page.
- `docs/source/analogx-nsl-Freeware.html` — saved source page HTML.

## Build outputs

Ignored by git:

- `build/nsl-linux` — compiled application from verified local build.
- `build/nsl_core_tests` — pure core test binary.
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
- `src/ClipCap.*`
- `src/Settings.*`
- `src/Core.*`
- `tests/test_core.cpp`
- `config/nsl-linux.desktop`

## Visual fidelity outputs

- `outputs/reports/visual/nsl-visual-pass-final.png` — generated `--screenshot` output after visual pass.
- `outputs/reports/visual/nsl-visual-comparison-final.png` — side-by-side original vs generated comparison image.
