# NSL-Linux

NSL-Linux (`nsl-linux`) is a compact Linux desktop network monitor inspired by AnalogX NetStat Live v2.15. It targets Kubuntu/KDE Plasma on Wayland first, while staying portable across normal Linux desktops with Qt6 Widgets.

This is a clean-room reimplementation. AnalogX NetStat Live is credited as the inspiration; NSL-Linux does not reuse AnalogX code or original assets. Reference screenshots saved in this repository are for development comparison only and are not installed into the application package.

## Features

- C++20, Qt6 Widgets, CMake, no QML.
- Custom-painted frameless dark vertical panel using QPainter.
- Panes for Local Machine, Remote Machine, Incoming/Outgoing Totals, Incoming/Outgoing graphs, Threads, and CPU.
- `/proc/net/dev`, `/proc/stat`, and `/proc/loadavg` collection on a 500 ms timer.
- Async `ping -c 1` rolling average and async `traceroute -n -m 30 -q 1` hop count.
- Runtime-painted StatusNotifierItem/QSystemTrayIcon with cached TX/RX flash states and green/yellow/red activity-age triangle.
- Right-click context menu with pane toggles, config toggles, interface radio menu, reset, minimize, exit.
- URL ClipCap via `QClipboard::dataChanged` plus KDE Klipper DBus polling fallback.
- QSettings INI persistence at `~/.config/nsl-linux/nsl-linux.conf`.
- Monthly total history with calendar-month rollover; set `NSL_FAKE_DATE=YYYY-MM-DD` or `YYYY-MM` for tests.
- Auto Start creates `~/.config/autostart/nsl-linux.desktop` with `X-KDE-autostart-after=panel` and the current executable path.
- Auto Minimize starts with the main window hidden and tray icon visible.
- Single-instance guard over DBus: a second launch activates the existing window and exits.
- Optional `layer-shell-qt` integration when the dev package is present.
- CPack `.deb` packaging for Ubuntu 24.04+.

## Screenshots / reference material

Reference material is preserved under:

- `assets/reference/analogx-nsl.gif` — original screenshot used only as visual reference.
- `docs/source/analogx-nsl-Freeware.html` — saved source page HTML.

Generated NSL-Linux visual artifacts:

- `outputs/reports/visual/nsl-visual-pass-final.png`
- `outputs/reports/visual/nsl-visual-comparison-final.png`

Render a deterministic visual-fidelity screenshot for regression review:

```bash
QT_QPA_PLATFORM=offscreen ./build/nsl-linux --screenshot outputs/reports/visual/nsl-linux.png
```

The screenshot mode seeds stable demo values, disables persistence writes, grabs the widget with `QWidget::grab()`, saves a PNG, and exits.

## Build on Kubuntu / Ubuntu 24.04+

Install build/runtime dependencies:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake \
  qt6-base-dev qmake6 qmake6-bin \
  libqt6dbus6 libqt6network6 libqt6widgets6 \
  liblayershellqtinterface-dev \
  traceroute
```

`liblayershellqtinterface-dev` is optional. If it is missing, CMake disables the Wayland layer-shell path and the app still builds; Always on Top falls back to `Qt::WindowStaysOnTopHint` plus the KWin rule below.

Build and test:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Run:

```bash
./build/nsl-linux
```

Start hidden in tray:

```bash
./build/nsl-linux --minimized
```

Hidden tray simulation/demo mode for deterministic local tray testing:

```bash
./build/nsl-linux --simulate --minimized
```

`--simulate` is intentionally hidden from `--help`; it feeds synthetic rx-only, tx-only, bidirectional, and silence frames into the Collector so tray icon states and StatusNotifierItem behavior can be exercised without live network traffic.

## Install and package

Install into a prefix:

```bash
cmake --install build --prefix ~/.local
```

Build the Debian package:

```bash
cpack --config build/CPackConfig.cmake -G DEB
```

Generated package path:

```text
outputs/final/nsl-linux_0.1.0_amd64.deb
```

The package installs:

- `/usr/bin/nsl-linux`
- `/usr/share/applications/nsl-linux.desktop`
- `/usr/share/icons/hicolor/{64x64,128x128,256x256}/apps/nsl-linux.png`

Package metadata declares Qt6 runtime dependencies and `traceroute` as a Recommends.

Dry-run install check:

```bash
dpkg --dry-run -i outputs/final/nsl-linux_0.1.0_amd64.deb
```

## KDE / Wayland notes

- The main window is frameless and draggable from the background using `QWindow::startSystemMove()`.
- `QSystemTrayIcon` maps to KDE Plasma's StatusNotifierItem support.
- Single-instance activation uses DBus. Launching `nsl-linux` again activates the existing window and exits.
- Background clipboard access is Wayland-restricted. URL ClipCap uses normal Qt clipboard notifications when available and polls Klipper over DBus (`org.kde.klipper`, `/klipper`, `getClipboardContents`) every 2 seconds as a KDE fallback.

### KWin “Keep above” window rule

If Always on Top is not honored on KDE Wayland and the binary was built without layer-shell support:

1. Open **System Settings**.
2. Go to **Window Management → Window Rules**.
3. Click **Add New**.
4. Match window class / resource class exactly:

   ```text
   nsl-linux
   ```

5. Add property **Keep above other windows**.
6. Set it to **Force → Yes** or **Apply Initially → Yes**.
7. Save/apply the rule and restart NSL-Linux.

You can also open the rules module directly with:

```bash
kcmshell6 kwinrules
```

## Tests

CTest currently covers:

- NetStat-style units and bits/bytes conversion.
- `/proc/net/dev` parsing and ALL-interface summing excluding `lo`.
- Counter-reset delta handling.
- `/proc/stat` CPU delta percent.
- `/proc/loadavg` thread total parsing.
- traceroute hop parsing.
- Tray activity bucket transitions at 60s/120s of silence.
- Tray left/right TX/RX flash state mapping.
- Tray renderer cache: unchanged visual state does not regenerate pixmaps.
- Tray icon legibility at 22x22 and 16x16 through pixel-marker checks.
- StatusNotifierItem activation mapping: `Trigger` toggles; `Context` is left for the menu.
- `NSL_FAKE_DATE` month rollover and monthly history archiving.
- Auto Start desktop-file creation/removal, including quoted build-dir Exec path and KDE panel ordering.
- Auto Minimize startup decision logic.
- Single-instance DBus service/path constants.

Run:

```bash
ctest --test-dir build --output-on-failure
```

## Files

```text
src/main.cpp
src/MainWindow.*
src/PaneWidget.*
src/GraphPane.*
src/TextPane.*
src/Collector.*
src/TrayIcon.*
src/TrayIconVisual.*
src/TraySimulation.*
src/Lifecycle.*
src/ClipCap.*
src/Settings.*
src/Core.*
tests/test_core.cpp
tests/test_tray_icon.cpp
tests/test_lifecycle.cpp
config/nsl-linux.desktop
assets/icons/hicolor/*/apps/nsl-linux.png
```
