# NSL-Linux

NSL-Linux (`nsl-linux`) is a Linux desktop clone of AnalogX NetStat Live v2.15, built for Kubuntu/KDE Plasma on Wayland while keeping the implementation portable across normal Linux desktops.

## Current status

First working milestone exists:

- C++20, Qt6 Widgets, CMake, no QML.
- Custom-painted compact 170 px vertical window using QPainter.
- Panes for Local Machine, Remote Machine, Incoming/Outgoing Totals, Incoming/Outgoing graphs, Threads, and CPU.
- `/proc/net/dev`, `/proc/stat`, and `/proc/loadavg` collection on a 500 ms timer.
- Async `ping -c 1` rolling average and async `traceroute -n -m 30 -q 1` hop count.
- Runtime-painted system tray icon with TX/RX flash halves and activity-age triangle.
- Right-click context menu with pane toggles, config toggles, interface radio menu, reset, minimize, exit.
- URL ClipCap via `QClipboard::dataChanged` plus KDE Klipper DBus polling fallback.
- QSettings INI persistence at `~/.config/nsl-linux/nsl-linux.conf`.
- Optional `layer-shell-qt` integration when the dev package is present.

Reference material is preserved under:

- `assets/reference/analogx-nsl.gif`
- `docs/source/analogx-nsl-Freeware.html`

## Build on Kubuntu

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

`liblayershellqtinterface-dev` is optional. If it is missing, CMake disables the Wayland layer-shell path and the app still builds; Always on Top falls back to `Qt::WindowStaysOnTopHint` plus the README/KWin-rule hint.

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

Render a deterministic visual-fidelity screenshot for regression review:

```bash
QT_QPA_PLATFORM=offscreen ./build/nsl-linux --screenshot outputs/reports/visual/nsl-linux.png
```

The screenshot mode seeds stable demo values, hides persistence writes, grabs the widget with `QWidget::grab()`, saves a PNG, and exits.

Install into a prefix:

```bash
cmake --install build --prefix ~/.local
```

## Wayland / KDE notes

- The main window is frameless and draggable from the background using `QWindow::startSystemMove()`.
- `QSystemTrayIcon` maps to KDE Plasma's StatusNotifierItem support.
- Background clipboard access is Wayland-restricted. URL ClipCap uses normal Qt clipboard notifications when available and polls Klipper over DBus (`org.kde.klipper`, `/klipper`, `getClipboardContents`) every 2 seconds as a KDE fallback.
- If Always on Top is not honored on KDE Wayland and the binary was built without layer-shell support, add a KWin window rule: match window class `nsl-linux`, set **Keep above other windows**.

## Tests

The current automated test covers pure core behavior:

- NetStat-style units and bits/bytes conversion.
- `/proc/net/dev` parsing and ALL-interface summing excluding `lo`.
- Counter-reset delta handling.
- `/proc/stat` CPU delta percent.
- `/proc/loadavg` thread total parsing.
- traceroute hop parsing.

Run it through CTest:

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
src/ClipCap.*
src/Settings.*
src/Core.*
tests/test_core.cpp
config/nsl-linux.desktop
```
