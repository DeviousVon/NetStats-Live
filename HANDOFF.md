# Handoff — NSL-Linux

## What this is

`nsl-linux` is a C++20 / Qt6 Widgets clone of AnalogX NetStat Live for Linux, focused on Kubuntu KDE Plasma on Wayland and portable Linux desktops.

## Current workspace

```text
/home/bob/projects/nsl-linux
```

## Current build artifact

```text
/home/bob/projects/nsl-linux/build/nsl-linux
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
- `QT_QPA_PLATFORM=offscreen build/nsl-linux --help` exited 0.
- 20s offscreen smoke/perf run: CPU rounded to 0%, RSS about 35 MB. The timeout exit was expected because the GUI event loop keeps running.

## Known caveats

- No real KDE Wayland visual screenshot pass was done yet; offscreen startup and link were verified.
- `traceroute` was not installed system-wide; the app degrades to `n/a` when missing, as required. README lists it.
- The current build binary has a RUNPATH into `.deps/root` because that is how this no-sudo build linked. A normal apt-installed build will link against system Qt paths.
- Exact pixel fidelity will likely need one visual pass against `assets/reference/analogx-nsl.gif`.

## Next likely action

Continue with Bob's next creation prompt from this workspace. First recommended command:

```bash
cd /home/bob/projects/nsl-linux && git status --short
```

## Visual fidelity pass notes

The app now supports deterministic screenshot rendering:

```bash
QT_QPA_PLATFORM=offscreen ./build/nsl-linux --screenshot outputs/reports/visual/nsl-linux.png
```

The screenshot mode seeds demo values, hides the Threads pane to match the AnalogX reference screenshot, prevents persistence writes, grabs the widget to PNG, and exits. The Threads pane remains implemented/toggleable in normal app use.

Latest visual artifacts:

- `outputs/reports/visual/nsl-visual-pass-final.png`
- `outputs/reports/visual/nsl-visual-comparison-final.png`

Implementation changed the main window width to match the 224px reference screenshot proportions rather than the earlier rough 170px estimate.


## Tray simulation/demo pass notes

The app now has a hidden deterministic tray demo mode:

```bash
./build/nsl-linux --simulate --minimized
```

It feeds synthetic Collector snapshots in this sequence: rx-only burst, tx-only burst, bidirectional burst, green silence just under 60s, yellow at 60s, and red at 120s. `--simulate` is hidden from `--help`.

Tray visual logic is factored into `TrayIconVisual.*`; simulation frames live in `TraySimulation.*`. CTest includes `nsl_tray_tests` for TX/RX half mapping, activity triangle thresholds, cache churn avoidance, 16px/22px legibility, and SNI activation reason mapping.

Live KDE Wayland verification registered an `nsl-linux` StatusNotifierItem and DBus `Activate` / `ContextMenu` method calls returned success.
