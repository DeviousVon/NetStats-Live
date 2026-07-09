# Contributing

Thanks for considering a contribution to NetStats-Live for Linux.

By contributing code, documentation, tests, or other project material, you agree that your contribution is accepted under the same license as the project: GPL-3.0-or-later.

## Development environment

Ubuntu/Kubuntu 24.04+ is the reference development target.

Install common dependencies:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake \
  qt6-base-dev qmake6 qmake6-bin \
  libqt6dbus6 libqt6network6 libqt6widgets6 \
  iputils-ping traceroute
```

For the KDE/Wayland layer-shell build, also install:

```bash
sudo apt install -y liblayershellqtinterface-dev
```

## Build and test

Generic source build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

KDE/layer-shell package build:

```bash
cmake -S . -B build-kde -DCMAKE_BUILD_TYPE=Release -DUSE_LAYER_SHELL=ON
cmake --build build-kde -j"$(nproc)"
ctest --test-dir build-kde --output-on-failure
cpack --config build-kde/CPackConfig.cmake -G DEB
```

Generic package build:

```bash
cmake -S . -B build-generic -DCMAKE_BUILD_TYPE=Release -DUSE_LAYER_SHELL=OFF
cmake --build build-generic -j"$(nproc)"
ctest --test-dir build-generic --output-on-failure
cpack --config build-generic/CPackConfig.cmake -G DEB
```

Package smoke check:

```bash
dpkg --dry-run -i outputs/final/NetStats-Live_0.1.0_kde_amd64.deb
dpkg --dry-run -i outputs/final/NetStats-Live_0.1.0_generic_amd64.deb
```

## Code layout

```text
src/main.cpp              QApplication setup, CLI parsing, signal bridge, DBus single-instance activation
src/MainWindow.*          main widget, pane/menu wiring, tray behavior, layer-shell/always-on-top handling
src/Collector.*           Linux metric collection from /proc plus ping/traceroute probes
src/Settings.*            QSettings persistence, autostart desktop-file generation, monthly total archiving
src/ClipCap.*             clipboard URL capture and KDE Klipper DBus fallback
src/Core.*                pure parsing/formatting helpers
tests/test_core.cpp       parser/formatter unit tests
tests/test_tray_icon.cpp  tray visual-state and rendering tests
tests/test_lifecycle.cpp  settings, rollover, startup, and lifecycle tests
tests/test_visual_theme.cpp visual-regression guardrails
```

## Style notes

- Keep UI code in Qt Widgets/QPainter; do not add QML.
- Prefer small pure helpers with unit tests for parsing, formatting, and lifecycle decisions.
- Keep desktop-specific behavior behind explicit checks and document degraded behavior.
- Do not make the app able to start invisible without a tray/status-notifier host.
- Keep comments focused on non-obvious lifecycle, desktop-integration, persistence, and platform behavior.
- Do not commit generated build directories, `.deb` packages, local dependency trees, or environment files.

## Pull request checklist

- [ ] `cmake --build ...` succeeds.
- [ ] `ctest --test-dir ... --output-on-failure` passes.
- [ ] If packaging changed, both KDE and generic `.deb` builds were smoke-tested with `dpkg --dry-run`.
- [ ] Public docs were updated for changed behavior or limitations.
- [ ] No credentials, private hostnames/IPs, or local absolute paths were committed.
