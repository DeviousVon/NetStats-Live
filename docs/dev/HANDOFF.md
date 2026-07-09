# Handoff — NetStats-Live

## What this is

`netstats-live` is a C++20 / Qt6 Widgets clone of AnalogX NetStat Live for Linux, focused on Kubuntu KDE Plasma on Wayland and portable Linux desktops.

## Current workspace

```text
<repo>
```

## Current build artifact

```text
<repo>/build/netstats-live
```

## Build normally

```bash
sudo apt update
sudo apt install -y build-essential cmake qt6-base-dev qmake6 qmake6-bin libqt6dbus6 libqt6network6 libqt6widgets6 liblayershellqtinterface-dev traceroute
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

## Build in current session state

Because sudo package install required interactive authentication, this session used ignored local extracted dev packages:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/.deps/root/usr" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Verification already run

- CMake configure succeeded.
- Build succeeded with `-Wall -Wextra -Wpedantic` and no compiler warnings in the final run.
- CTest: `1/1 Test #1: nsl_core_tests ... Passed`.
- `QT_QPA_PLATFORM=offscreen build/netstats-live --help` exited 0.
- 20s offscreen smoke/perf run: CPU rounded to 0%, RSS about 35 MB. The timeout exit was expected because the GUI event loop keeps running.

## Known caveats

- No real KDE Wayland visual screenshot pass was done yet; offscreen startup and link were verified.
- `traceroute` was not installed system-wide; the app degrades to `n/a` when missing, as required. README lists it.
- The current build binary has a RUNPATH into `.deps/root` because that is how this no-sudo build linked. A normal apt-installed build will link against system Qt paths.
- Exact pixel fidelity will likely need one visual pass against `assets/reference/analogx-nsl.gif`.

## Next likely action

Continue with Bob's next creation prompt from this workspace. First recommended command:

```bash
cd <repo> && git status --short
```

## Visual fidelity pass notes

The app now supports deterministic screenshot rendering:

```bash
QT_QPA_PLATFORM=offscreen ./build/netstats-live --screenshot /tmp/netstats-live-screenshot.png
```

The screenshot mode seeds demo values, hides the Threads pane to match the AnalogX reference screenshot, prevents persistence writes, grabs the widget to PNG, and exits. The Threads pane remains implemented/toggleable in normal app use.

Latest visual artifacts:

- `outputs/reports/visual/nsl-visual-pass-final.png`
- `outputs/reports/visual/nsl-visual-comparison-final.png`

Implementation changed the main window width to match the 224px reference screenshot proportions rather than the earlier rough 170px estimate.


## Tray simulation/demo pass notes

The app now has a hidden deterministic tray demo mode:

```bash
./build/netstats-live --simulate --minimized
```

It feeds synthetic Collector snapshots in this sequence: rx-only burst, tx-only burst, bidirectional burst, cyan active silence just under 60s, yellow at 60s, and red at 120s. `--simulate` is hidden from `--help`.

Tray visual logic is factored into `TrayIconVisual.*`; simulation frames live in `TraySimulation.*`. CTest includes `nsl_tray_tests` for TX/RX half mapping, activity triangle thresholds, cache churn avoidance, 16px/22px legibility, and SNI activation reason mapping.

Live KDE Wayland verification registered a `netstats-live` StatusNotifierItem and DBus `Activate` / `ContextMenu` method calls returned success.


## Lifecycle/package pass notes

Lifecycle features implemented:

- `NSL_FAKE_DATE` supports `YYYY-MM-DD` and `YYYY-MM` for month-rollover tests.
- Monthly totals are archived to `history/<YYYY-MM>/rxMonth` and `history/<YYYY-MM>/txMonth`; active month totals reset on rollover.
- Auto Start writes `~/.config/autostart/netstats-live.desktop` with quoted current executable path, `--minimized`, `Icon=netstats-live`, and `X-KDE-autostart-after=panel`.
- Auto Minimize continues through startup decision helper `shouldShowMainWindow()`.
- Single-instance guard uses DBus service `io.github.DeviousVon.NetStatsLive` and object `/io/github/DeviousVon/NetStatsLive/MainWindow`; second launch calls `activateFromInstanceRequest` and exits.
- CPack generates `outputs/final/NetStats-Live_0.1.0_<flavor>_amd64.deb`. The generated package is intentionally ignored by git.

Recommended resume verification:

```bash
cd <repo>
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/.deps/root/usr" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake -G DEB
dpkg --dry-run -i outputs/final/NetStats-Live_0.1.0_<flavor>_amd64.deb
```


## Final QA pass notes

Final QA found one defect: SIGTERM did not save totals/config. Fixed with a Unix signal pipe + `QSocketNotifier` in `main.cpp` and `MainWindow::shutdownForSignal()`. Added a lifecycle regression test that launches `build/netstats-live --simulate --minimized`, sends SIGTERM, and verifies non-zero totals are written.

Other QA evidence lives in `docs/dev/final-qa-2026-07-07.md`:

- Wayland real-traffic soak stayed below 1% CPU and had 0 KiB RSS delta.
- SIGKILL after 65 seconds retained flushed totals; hard crashes can lose at most the last 60 seconds.
- Always on Top now re-applies via a live hide/show cycle; brief flicker is expected on Wayland.

Resume verification:

```bash
cd <repo>
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake -G DEB
dpkg --dry-run -i outputs/final/NetStats-Live_0.1.0_<flavor>_amd64.deb
```

## AnalogX cyan visual rework handoff — 2026-07-07

Current visual direction is the user's supplied AnalogX screenshot, not earlier bright-green/Cur-Avg-Max guidance. The app now uses `src/Theme.h` as the palette source of truth: cyan graph fill/header, brighter cyan values, olive labels, dark background/rules, and cyan tray active state. Graph panes use spelled Current/Average/Max labels, larger bold value text, and smoothed filled area graphs. Fresh artifacts are under `generated visual pass artifact (not committed)` and `generated visual comparison artifact (not committed)`.

## Dynamic graph scaling handoff — 2026-07-07

GraphPane now uses visible-window scaling: Count/Percent panes use padded 60-sample min/max; NetworkRate panes keep zero baseline but scale to the visible max. `maximumSeen_` remains only for the `Max` text. Scale easing is `0.20`, all-zero traffic returns `drawEmpty`, flat non-traffic values get a centered artificial range, newest edge bursts are preserved through smoothing, and `Theme::AverageLine` marks the visible-sample average. Runtime verification also fixed shutdown of live ping/traceroute QProcess children.
