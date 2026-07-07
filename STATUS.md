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
