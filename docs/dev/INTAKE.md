# Intake

Bob supplied the first creation prompt on 2026-07-06.

## Supplied requirements

Build a Linux desktop application called nsl-linux: a faithful C++20 / Qt6 Widgets clone of AnalogX NetStat Live v2.15 for Kubuntu KDE Plasma on Wayland, portable to other Linux desktops. Use custom-painted QPainter widgets, /proc collection, async ping/traceroute, runtime tray icon, context menus, QSettings persistence, URL ClipCap with Klipper DBus fallback, optional layer-shell-qt always-on-top, and build/link verification with -Wall -Wextra.

## Source/reference inspected

- AnalogX NetStat Live page: `https://www.analogx.com/contents/download/Network/nsl/Freeware.htm`
- Saved HTML: `docs/source/analogx-nsl-Freeware.html`
- Screenshot image discovered from page: `/contents/graphics/screenshot/nsl.gif`
- Saved screenshot: `assets/reference/analogx-nsl.gif`

## Decisions captured from prompt

- Stack: C++20 + Qt6 Widgets + CMake.
- KDE/Wayland support matters; portable Linux support should remain practical.
- No QML.
- Custom paint with QPainter.
- Use `/proc` and standard tools; avoid extra libraries.
- Persist settings through QSettings INI.
- Optional layer-shell-qt guarded by CMake.

## Open questions

- No additional screenshot was attached in-chat beyond the AnalogX page reference; the screenshot from the AnalogX page was used.
- Later prompts may refine fidelity, packaging, behavior, or polish.
