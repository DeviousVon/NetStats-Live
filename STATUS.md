# Status

## Current state

First working implementation milestone is complete in the local workspace.

Built artifact:

```text
/home/bob/projects/nsl-linux/build/nsl-linux
```

## Verification run

- `cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/.deps/root/usr" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` — configured successfully with layer-shell-qt found from local extracted dev package.
- `cmake --build build -j$(nproc)` — succeeded.
- `ctest --test-dir build --output-on-failure` — 1/1 test passed.
- `QT_QPA_PLATFORM=offscreen build/nsl-linux --help` — exit 0, showed CLI help.
- 20-second offscreen smoke/perf run: timeout exit expected; CPU rounded to 0%, max RSS ~35 MB.

## Dependency note

System Qt runtime libraries were present, but system Qt development packages were not installed and `sudo -n apt-get install ...` required interactive authentication. For this milestone, dev `.deb` packages were downloaded/extracted under ignored `.deps/` so the project could be configured and linked without changing system packages.

Normal local build should use the README apt package list.

## Next actions

Use Bob's second prompt in the next session to continue fidelity/polish/packaging work. Suggested follow-ups:

1. Run visually on the real KDE Wayland desktop and compare against `assets/reference/analogx-nsl.gif`.
2. Tighten exact pane heights/typography/colors if needed.
3. Add more tests around settings/autostart and Collector parser edge cases.
4. Decide packaging target if requested: local install, `.deb`, AppImage, or simple tarball.

## 2026-07-06 visual fidelity pass

Implemented second-prompt visual tightening:

- Pane headers now use dark raised strips, left-aligned small-caps-style text, and a condensed font fallback chain.
- Graphs now draw 60 one-pixel vertical bars, subtle 25/50/75% grid lines, and a 1px lighter average line.
- Graph value blocks now use dense `Cur` / `Avg` / `Max` right-aligned columns.
- The window is frameless, dark, marginless, and uses a 1px dark-gray outer border.
- Added `--screenshot <path>` deterministic PNG mode using `QWidget::grab()` for visual regression checks.

Rendered artifacts:

- `outputs/reports/visual/nsl-visual-pass-final.png`
- `outputs/reports/visual/nsl-visual-comparison-final.png`


## 2026-07-06 tray simulation / demo pass

Implemented third-prompt tray verification work:

- Added hidden `--simulate` mode that drives the Collector with deterministic synthetic traffic: rx-only burst, tx-only burst, bidirectional burst, then silence frames around 60s and 120s.
- Split tray visual-state logic into testable code: activity bucket, TX/RX state mapping, 16/22/32px icon rendering, and renderer cache.
- Added `nsl_tray_tests` to CTest. It verifies left/right flash mapping, green/yellow/red silence thresholds, no regeneration for unchanged visual state, 16px/22px legibility, and StatusNotifierItem activation mapping.
- Verified on the live KDE Wayland session that `nsl-linux --simulate --minimized` registers a StatusNotifierItem with `Id`/`Title` `nsl-linux`; DBus `Activate` and `ContextMenu` calls both returned exit 0.


## 2026-07-07 lifecycle/package pass

Implemented fourth-prompt lifecycle and packaging work:

- `NSL_FAKE_DATE` controls `AppSettings::currentMonthKey()` for tests.
- Monthly totals rollover archives old values under `history/<YYYY-MM>/rxMonth` and `history/<YYYY-MM>/txMonth` and resets the active month bucket.
- Auto Start creates/removes `~/.config/autostart/nsl-linux.desktop`, quoting build-dir paths and adding `X-KDE-autostart-after=panel`.
- Auto Minimize startup decision is factored/tested and still starts the app hidden with tray visible.
- Single-instance DBus guard added; second launch calls `activateFromInstanceRequest` on the existing process and exits.
- Added clean-room hicolor icons and CPack DEB packaging.
- Package artifact: `outputs/final/nsl-linux_0.1.0_amd64.deb` (ignored generated artifact).

Verification added/updated:

- `nsl_lifecycle_tests` covers fake date, rollover history, autostart create/remove, auto-minimize decision, and single-instance constants.
- Runtime single-instance smoke: first process stayed running; second launch exited 0.
- Package checks: CPack generated a `.deb`, `dpkg-deb --info/--contents` verified metadata/installed files, `dpkg --dry-run -i` exited 0.


## 2026-07-07 final QA pass

Final QA found and fixed one lifecycle defect: SIGTERM previously used the default process action and did not save totals/config. Added a SIGTERM/SIGINT bridge into the Qt event loop plus a lifecycle regression test.

QA evidence:

- Real-traffic Wayland soak: ~175 seconds sampled, CPU avg 0.349%, max 0.400%, RSS delta 0 KiB.
- SIGTERM direct smoke after fix: exit 0, config saved, simulated monthly totals non-zero.
- SIGKILL/crash direct smoke after 65 seconds: config retained flushed totals, bounding hard-crash loss to the existing 60-second flush interval.
- `ctest --test-dir build --output-on-failure`: 3/3 passed after the fix.

Report: `docs/qa/final-qa-2026-07-07.md`.
