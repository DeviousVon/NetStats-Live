# Changelog

All notable changes to NetStats-Live for Linux are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project uses alpha release tags such as `v0.1.0-alpha`.

## [Unreleased]

### Changed

- **Breaking:** the product identity is now consistently `NetStats-Live` / `netstats-live`. The installed binary, desktop entry, icon name, window class, DBus service, and config path moved from the old `nsl-linux` names.
- Settings now migrate once by copying `~/.config/nsl-linux/nsl-linux.conf` to `~/.config/netstats-live/netstats-live.conf` when the new config file does not already exist.
- Removed generated/development output directories from the tracked repository; release packages are generated artifacts and remain outside git.

## [0.1.0-alpha] - 2026-07-09

### Added

- First public alpha of the Qt6/C++ Linux desktop network monitor.
- Custom-painted AnalogX-inspired vertical UI with local/remote status, incoming/outgoing totals, live graphs, thread count, and CPU use.
- Linux `/proc` collection for network counters, CPU load, and thread totals.
- Async `ping` rolling average and `traceroute` hop count support.
- Runtime-painted StatusNotifierItem/QSystemTrayIcon traffic indicator.
- Context menu for pane visibility, unit mode, interface selection, Auto Start, Auto Minimize, URL ClipCap, Always on Top, reset, minimize, and exit.
- URL ClipCap through Qt clipboard events plus KDE Klipper DBus fallback.
- QSettings persistence, autostart desktop file creation, monthly transfer total archiving, and single-instance DBus activation.
- Deterministic screenshot mode and tray simulation path for QA.
- Debian package generation for KDE/layer-shell and generic Qt6 builds.
- Ubuntu 24.04 GitHub Actions CI for build/test/package smoke checks.

### Fixed

- Last Month totals now display the archived previous calendar month instead of hardcoded zeroes.
- Startup/minimize behavior now stays reachable on desktops with no tray/status-notifier host.
- Package metadata now points to the GitHub project and recommends both `iputils-ping` and `traceroute`.

### Known limitations

- KDE Plasma Wayland is the only fully supported desktop for this alpha.
- KDE/Cinnamon/XFCE X11 are expected to work but still need broader validation.
- GNOME usually requires an AppIndicator/KStatusNotifier extension for tray access; always-on-top and ClipCap are degraded there.
- Other Wayland compositors are unverified because tray, keep-above, and clipboard behavior vary by compositor.
- Debian packages currently target Ubuntu/Kubuntu 24.04+ style Qt6 runtime dependencies on `amd64`.
