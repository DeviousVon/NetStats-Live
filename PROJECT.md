# NSL-Linux Project Brief

## Identity

- Name: NSL-Linux
- Binary/project ID: `nsl-linux`
- Meaning: NetStat Live for Linux
- Category: software / Linux desktop utility
- Workspace: `/home/bob/projects/nsl-linux`

## Purpose

Build a Linux desktop application called nsl-linux: a faithful C++20 / Qt6 Widgets clone of AnalogX NetStat Live v2.15 for Kubuntu KDE Plasma on Wayland, portable to other Linux desktops. Use custom-painted QPainter widgets, /proc collection, async ping/traceroute, runtime tray icon, context menus, QSettings persistence, URL ClipCap with Klipper DBus fallback, optional layer-shell-qt always-on-top, and build/link verification with -Wall -Wextra.

## Target platform

Primary target: KubuntuClaw / Kubuntu KDE Plasma on Wayland. Secondary target: any normal Linux desktop with Qt6 Widgets and `/proc`.

## Visual direction

Match AnalogX NetStat Live v2.15 as closely as practical:

- Compact fixed-width vertical panel, about 170 px wide.
- Near-black background.
- Thin late-90s beveled pane separators.
- Small bitmap-style sans labels.
- AnalogX-style cyan/teal values and graph fills, with dim warm gray-olive labels.
- Last-60-second graphs with faint grid, filled area, 60-second average line, and maximum since reset.
- Runtime-painted tray icon reproducing the TX/RX flash and activity-age indicator concept.

Reference screenshot saved at `assets/reference/analogx-nsl.gif`.

## Implementation constraints

- C++20, Qt6 Widgets, CMake.
- No QML.
- Custom QPainter widgets.
- Allowed dependencies: Qt6 Widgets/DBus/Network, optional layer-shell-qt, standard Linux tools `ping` and `traceroute`.
- Data from `/proc` where possible.
- UI collection on main-thread QTimer; external commands through async QProcess only.
- 500 ms polling tick.
- Target idle resource use: <1% CPU, <50 MB RSS.

## Definition of done for first prompt

- CMake project exists and builds.
- Source shape matches requested files under `src/`.
- Core collectors and panes implemented.
- Runtime tray icon implemented.
- Context menu implemented.
- URL ClipCap and Klipper fallback implemented.
- QSettings persistence implemented.
- README and `.desktop` file included.
- `-Wall -Wextra` build is clean.
- App links and starts.
