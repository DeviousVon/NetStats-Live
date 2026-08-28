# Contributing

Thanks for considering a contribution to NetStats-Live for Linux.

By contributing code, documentation, tests, or other project material, you agree that your contribution is accepted under the same license as the project: GPL-3.0-or-later.

## Development environment

Generic builds are validated on Ubuntu 22.04, Ubuntu 24.04, and Debian 12.
The genuine KDE feature/package gate uses a distribution that provides Qt 6
LayerShellQt and/or KF6 WindowSystem.

Install common dependencies:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake \
  qt6-base-dev qmake6 qmake6-bin \
  libgl-dev libopengl-dev \
  iputils-ping traceroute
```

For the KDE/Wayland build, install the **Qt 6** variants available on the target
distribution:

```bash
sudo apt install -y liblayershellqtinterface-dev libkf6windowsystem-dev
```

Ubuntu 22.04 and Ubuntu 24.04 provide a Qt 5 LayerShellQt development package;
that package is deliberately rejected and does not satisfy the KDE gate.

## Build and test

Generic source build and complete test suite:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DUSE_LAYER_SHELL=OFF -DUSE_KWINDOWSYSTEM=OFF
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Deterministic generic and KDE packages:

```bash
scripts/build-debs.sh --flavor generic --reproducible
scripts/build-debs.sh --flavor kde --reproducible
```

The KDE command must fail when its requested features degrade to generic. The
script validates exact installed contents, generated dependencies, AppStream
and desktop metadata, Qt major, RPATH/RUNPATH, private paths, dry-run install,
and byte-for-byte reproducibility.

Sanitizer build:

```bash
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DUSE_LAYER_SHELL=OFF -DUSE_KWINDOWSYSTEM=OFF \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitize --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
QT_QPA_PLATFORM=offscreen \
  ctest --test-dir build-sanitize --output-on-failure
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
tests/test_main_window.cpp real-window minimize/restore, position, DBus-slot, and activation-token tests
config/netstats-live.metainfo.xml AppStream identity and release metadata
scripts/build-debs.sh deterministic package build and verification authority
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
- [ ] Generic and KDE feature detection was checked; KDE did not silently degrade to generic.
- [ ] `scripts/build-debs.sh --flavor ... --reproducible` passed for every advertised package flavor.
- [ ] `shellcheck`, AppStream, desktop-file, package-content, dependency, Qt-major, RPATH/RUNPATH, and dry-run gates passed.
- [ ] ASan/UBSan and the complete CTest suite passed.
- [ ] Public docs were updated for changed behavior or limitations.
- [ ] No credentials, private hostnames/IPs, or local absolute paths were committed.
