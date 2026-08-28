# Changelog

All notable changes to NetStats-Live for Linux are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project uses alpha release tags such as `v0.1.0-alpha`.

## [Unreleased]

### Added

- Added profile-specific single-instance locking and DBus activation authority,
  including stale-lock recovery and isolated profile identities.
- Added keyboard context-menu and minimize operation, accessible title/minimize
  roles, names, actions, focus indication, and focus restoration after tray
  restore.
- Added a deterministic repository-owned DEB build/verification gate, generated
  shared-library dependencies, AppStream metadata, and a top-level security
  policy.
- Added clean-container CI contracts for Ubuntu 22.04, Ubuntu 24.04, Debian 12,
  a genuine Qt 6 KDE baseline, ASan/UBSan, Qt 5 rejection, package inspection,
  and fixed-epoch reproducibility.
- Added a portable QtDBus StatusNotifierItem and DBusMenu backend with dynamic
  icon/tooltip updates, watcher recovery, activation-token forwarding, and safe
  tray-loss degradation on generic Qt 6 desktops.
- Added reproducible generic DEB, RPM, and AppImage release artifacts.

### Fixed

- Stabilized compact UI typography at regular weight so values no longer appear to change family or become randomly bold between renders.
- Promoted large accumulated traffic totals from MB to GB before they overflow the three-column totals layout.
- Added a visible, aligned title-strip minimize control and made it use normal window minimization semantics.
- Made tray and single-instance activation restore minimized windows consistently; second launches forward their xdg activation token, KDE builds with KF6 WindowSystem preserve the same Wayland surface and compositor position, and tokenless or generic paths retain a reachability-first remap fallback.
- Corrected per-interface traffic deltas, CPU guest accounting, strict proc/net
  parsing, route selection, bounded probes, stale state clearing, monitor
  clamping, activation-token scope, desktop escaping, and graph contrast.
- Made settings, monthly totals, migrations, and autostart transactions report
  synchronization and authority failures instead of silently claiming success.
- Hardened autostart publication, migration, rollback, and parent-directory
  authority against symlink, substitution, inode-reuse, and concurrent-path
  races while preserving recovery copies on successful retirement.
- Reconciled the collector's latest totals at shutdown and prevented failed
  month rollover from dropping the prior month or adopting an unarchived month.
- Replaced synchronous Klipper polling with a timeout-bounded asynchronous
  one-request-in-flight implementation that rejects stale replies and handles
  service loss/return.
- Turning Always on Top off now destroys layer-shell authority and creates a
  normal xdg-toplevel; turning it back on creates a fresh layer surface.
- Restored the window when a tray host disappears so a minimized application
  cannot remain unreachable, and corrected secure autostart publication on
  filesystems that reject the empty-path link operation.

### Changed

- **Breaking:** the product identity is now consistently `NetStats-Live` / `netstats-live`. The installed binary, desktop entry, icon name, window class, DBus service, and config path moved from the old `nsl-linux` names.
- Settings now migrate once by copying `~/.config/nsl-linux/nsl-linux.conf` to `~/.config/netstats-live/netstats-live.conf` when the new config file does not already exist, and legacy autostart entries are rewritten to `netstats-live`.
- Debian packages now declare `Breaks`, `Conflicts`, and `Replaces` for the old `nsl-linux` package name to avoid side-by-side stale launchers on upgrade.
- The unreleased package version is `0.2.0`, uses dependencies generated from
  the packaged binary, and installs AppStream, license, and changelog metadata.
- The generic source/package baseline now includes Ubuntu 22.04 (CMake 3.22,
  GCC 11, Qt 6.2), Ubuntu 24.04, and Debian 12. KDE package labeling fails closed
  unless a genuine Qt 6 LayerShellQt or KF6 integration is enabled.
- Live release acceptance now covers Ubuntu and Fedora GNOME Wayland with an
  AppIndicator host, KDE Plasma Wayland, Cinnamon X11, and XFCE X11. Desktops
  without a usable tray host retain a visible recovery path.
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
