#!/usr/bin/env bash
set -euo pipefail
umask 022

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly SCRIPT_DIR
ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd -P)"
readonly ROOT

flavor="all"
output_rel="outputs/release"
verify=1
reproducible=0
validate_output_only=0
jobs="${NSL_BUILD_JOBS:-2}"

usage() {
    cat <<'EOF'
Usage: scripts/build-debs.sh [options]

Build deterministic NetStats-Live Debian packages outside the source tree.

Options:
  --flavor generic|kde|all  Package flavor(s) to build (default: all)
  --output outputs/NAME     Bounded repository-relative output (default: outputs/release)
  --jobs N                  Parallel build jobs (default: NSL_BUILD_JOBS or 2)
  --no-verify               Build without package verification
  --reproducible            Build twice and require byte-identical packages
  --validate-output-only    Validate/create the bounded output root, then exit
  -h, --help                Show this help

SOURCE_DATE_EPOCH defaults to the candidate commit timestamp and is required
when the source is not a Git checkout. KDE builds fail if neither Qt 6
LayerShellQt nor KF6 WindowSystem is actually enabled.
EOF
}

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

while (($#)); do
    case "$1" in
    --flavor)
        (($# >= 2)) || die "--flavor requires a value"
        flavor="$2"
        shift 2
        ;;
    --output)
        (($# >= 2)) || die "--output requires a value"
        output_rel="$2"
        shift 2
        ;;
    --jobs)
        (($# >= 2)) || die "--jobs requires a value"
        jobs="$2"
        shift 2
        ;;
    --no-verify)
        verify=0
        shift
        ;;
    --reproducible)
        reproducible=1
        shift
        ;;
    --validate-output-only)
        validate_output_only=1
        shift
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    *)
        die "unknown option: $1"
        ;;
    esac
done

[[ "$flavor" == generic || "$flavor" == kde || "$flavor" == all ]] || die "invalid flavor: $flavor"
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || die "--jobs must be a positive integer"
[[ "$output_rel" =~ ^outputs/[A-Za-z0-9][A-Za-z0-9._-]*$ ]] ||
    die "--output must be exactly outputs/NAME using letters, digits, dot, underscore, or hyphen"

readonly OUTPUT_ROOT="$ROOT/$output_rel"
readonly OUTPUT_PARENT="$ROOT/outputs"

validate_output_boundary() {
    [[ ! -L "$OUTPUT_PARENT" ]] || die "output parent must not be a symlink: $OUTPUT_PARENT"
    if [[ -e "$OUTPUT_PARENT" ]]; then
        [[ -d "$OUTPUT_PARENT" ]] || die "output parent is not a directory: $OUTPUT_PARENT"
    else
        mkdir -- "$OUTPUT_PARENT"
    fi
    [[ ! -L "$OUTPUT_PARENT" ]] || die "output parent became a symlink: $OUTPUT_PARENT"
    [[ "$(cd -- "$OUTPUT_PARENT" && pwd -P)" == "$OUTPUT_PARENT" ]] ||
        die "output parent resolves outside the repository: $OUTPUT_PARENT"
    [[ ! -e "$OUTPUT_ROOT" && ! -L "$OUTPUT_ROOT" ]] ||
        die "output root already exists; choose a fresh outputs/NAME: $OUTPUT_ROOT"
    mkdir -- "$OUTPUT_ROOT"
    [[ ! -L "$OUTPUT_ROOT" && -d "$OUTPUT_ROOT" ]] || die "failed to create a safe output root"
}

validate_output_boundary
if ((validate_output_only)); then
    printf 'output boundary validation passed: %s\n' "$OUTPUT_ROOT"
    exit 0
fi

required=(cmake cpack ctest dpkg dpkg-deb dpkg-shlibdeps readelf ldd strings python3 cmp gzip)
if ((verify)); then
    required+=(appstreamcli desktop-file-validate lintian)
fi
for command_name in "${required[@]}"; do
    command -v "$command_name" >/dev/null 2>&1 || die "required command not found: $command_name"
done

if [[ -z "${SOURCE_DATE_EPOCH:-}" ]]; then
    command -v git >/dev/null 2>&1 || die "SOURCE_DATE_EPOCH is required outside a Git checkout"
    git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1 ||
        die "SOURCE_DATE_EPOCH is required outside a Git checkout"
    SOURCE_DATE_EPOCH="$(git -C "$ROOT" show -s --format=%ct HEAD)"
fi
[[ "$SOURCE_DATE_EPOCH" =~ ^[0-9]+$ ]] || die "SOURCE_DATE_EPOCH must be an integer"
export SOURCE_DATE_EPOCH TZ=UTC LC_ALL=C LANG=C

mkdir -- "$OUTPUT_ROOT/work" "$OUTPUT_ROOT/packages"

validate_source_metadata() {
    desktop-file-validate "$ROOT/config/netstats-live.desktop"
    appstreamcli validate --pedantic "$ROOT/config/netstats-live.metainfo.xml"
}

feature_contract() {
    local build_dir="$1"
    local expected_flavor="$2"
    python3 - "$build_dir/nsl-build-features.json" "$expected_flavor" <<'PY'
import json
from pathlib import Path
import sys

features = json.loads(Path(sys.argv[1]).read_text())
expected = sys.argv[2]
assert features["package_flavor"] == expected, features
if expected == "generic":
    assert features["layer_shell_qt6"] is False, features
    assert features["kf6_window_system"] is False, features
else:
    assert features["layer_shell_qt6"] is True or features["kf6_window_system"] is True, features
print(json.dumps(features, sort_keys=True))
PY
}

root_command() {
    if ((EUID == 0)); then
        "$@"
    elif command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
        sudo -n "$@"
    else
        die "package dry-run verification requires root or passwordless sudo"
    fi
}

verify_package() {
    local package="$1"
    local expected_flavor="$2"
    local verify_root="$3"
    local extract_root="$verify_root/extracted"
    local binary="$extract_root/usr/bin/netstats-live"
    local depends

    [[ ! -e "$verify_root" && ! -L "$verify_root" ]] || die "verification root already exists: $verify_root"
    mkdir -p -- "$extract_root"

    [[ "$(dpkg-deb -f "$package" Package)" == netstats-live ]] || die "wrong package identity: $package"
    [[ "$(dpkg-deb -f "$package" Version)" == 0.2.0 ]] || die "wrong package version: $package"
    [[ "$(dpkg-deb -f "$package" Section)" == net ]] || die "wrong package section: $package"
    [[ "$(dpkg-deb -f "$package" Priority)" == optional ]] || die "wrong package priority: $package"
    [[ "$(dpkg-deb -f "$package" Homepage)" == https://github.com/DeviousVon/NetStats-Live ]] ||
        die "wrong package homepage: $package"
    [[ "$(dpkg-deb -f "$package" Breaks)" == *"nsl-linux (<< 0.2.0)"* ]] || die "missing Breaks"
    [[ "$(dpkg-deb -f "$package" Conflicts)" == *"nsl-linux (<< 0.2.0)"* ]] || die "missing Conflicts"
    [[ "$(dpkg-deb -f "$package" Replaces)" == *"nsl-linux (<< 0.2.0)"* ]] || die "missing Replaces"

    depends="$(dpkg-deb -f "$package" Depends)"
    [[ -n "$depends" ]] || die "generated Depends is empty"
    [[ ! "$depends" =~ [Qq][Tt]5 && ! "$depends" =~ layershellqtinterface5 ]] || die "Qt5 dependency in $package"
    if [[ "$expected_flavor" == generic ]]; then
        [[ ! "$depends" =~ layershellqt && ! "$depends" =~ kf6windowsystem ]] ||
            die "generic package contains KDE-only dependency: $depends"
    fi

    dpkg-deb -x "$package" "$extract_root"
    python3 - "$extract_root" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
expected = {
    "usr/bin/netstats-live",
    "usr/share/applications/netstats-live.desktop",
    "usr/share/icons/hicolor/64x64/apps/netstats-live.png",
    "usr/share/icons/hicolor/128x128/apps/netstats-live.png",
    "usr/share/icons/hicolor/256x256/apps/netstats-live.png",
    "usr/share/metainfo/io.github.deviousvon.netstats-live.metainfo.xml",
    "usr/share/doc/netstats-live/copyright",
    "usr/share/doc/netstats-live/NEWS.gz",
    "usr/share/doc/netstats-live/changelog.gz",
    "usr/share/man/man1/netstats-live.1.gz",
}
observed = set()
for path in root.rglob("*"):
    if path.is_symlink():
        raise SystemExit(f"symlink rejected: {path.relative_to(root)}")
    if path.is_file():
        observed.add(path.relative_to(root).as_posix())
assert observed == expected, {"missing": sorted(expected - observed), "extra": sorted(observed - expected)}
PY

    cmp --silent "$ROOT/config/netstats-live.desktop" "$extract_root/usr/share/applications/netstats-live.desktop"
    cmp --silent "$ROOT/config/netstats-live.metainfo.xml" \
        "$extract_root/usr/share/metainfo/io.github.deviousvon.netstats-live.metainfo.xml"
    cmp --silent "$ROOT/CHANGELOG.md" <(gzip -dc "$extract_root/usr/share/doc/netstats-live/NEWS.gz")
    for size in 64 128 256; do
        cmp --silent "$ROOT/assets/icons/hicolor/${size}x${size}/apps/netstats-live.png" \
            "$extract_root/usr/share/icons/hicolor/${size}x${size}/apps/netstats-live.png"
    done

    python3 - "$extract_root/usr/share/doc/netstats-live/copyright" \
        "$extract_root/usr/share/doc/netstats-live/changelog.gz" \
        "$extract_root/usr/share/man/man1/netstats-live.1.gz" <<'PY'
from pathlib import Path
import gzip
import sys

copyright_text = Path(sys.argv[1]).read_text()
assert "License: GPL-3+" in copyright_text
assert "/usr/share/common-licenses/GPL-3" in copyright_text
with gzip.open(sys.argv[2], "rt") as handle:
    debian_changelog = handle.read()
assert debian_changelog.startswith("netstats-live (0.2.0) UNRELEASED; urgency=medium\n")
assert " -- DeviousVon <DeviousVon@users.noreply.github.com>  Mon, 24 Aug 2026 00:00:00 +0000" in debian_changelog
with gzip.open(sys.argv[3], "rt") as handle:
    manual = handle.read()
assert ".TH NETSTATS-LIVE 1" in manual
assert "netstats-live --help" in manual
PY

    desktop-file-validate "$extract_root/usr/share/applications/netstats-live.desktop"
    appstreamcli validate --pedantic \
        "$extract_root/usr/share/metainfo/io.github.deviousvon.netstats-live.metainfo.xml"
    appstreamcli validate-tree --pedantic "$extract_root"

    python3 - "$binary" "$ROOT" <<'PY'
from pathlib import Path
import subprocess
import sys

binary = Path(sys.argv[1])
source_root = sys.argv[2]
dynamic = subprocess.check_output(["readelf", "-d", str(binary)], text=True)
assert "RPATH" not in dynamic and "RUNPATH" not in dynamic, dynamic
assert "Qt5" not in dynamic and "LayerShellQtInterface.so.5" not in dynamic, dynamic
linked = subprocess.check_output(["ldd", str(binary)], text=True, stderr=subprocess.STDOUT)
assert "not found" not in linked, linked
assert "Qt5" not in linked and "LayerShellQtInterface.so.5" not in linked, linked
strings = subprocess.check_output(["strings", "-a", str(binary)], text=True, errors="replace")
home_prefix = str(Path("/").joinpath("home")) + "/"
for marker in (source_root, home_prefix, ".deps/"):
    assert marker not in strings, marker
PY

    root_command dpkg --dry-run --auto-deconfigure -i "$package" >/dev/null
    lintian --pedantic --fail-on error "$package"
}

build_once() {
    local expected_flavor="$1"
    local run_name="$2"
    local run_root="$OUTPUT_ROOT/work/$run_name/$expected_flavor"
    local build_dir="$run_root/build"
    local package_dir="$run_root/packages"
    local -a flags

    [[ ! -e "$run_root" && ! -L "$run_root" ]] || die "build run root already exists: $run_root"
    mkdir -p -- "$package_dir"
    if [[ "$expected_flavor" == generic ]]; then
        flags=(-DUSE_LAYER_SHELL=OFF -DUSE_KWINDOWSYSTEM=OFF)
    else
        flags=(-DUSE_LAYER_SHELL=ON -DUSE_KWINDOWSYSTEM=ON)
    fi

    cmake -S "$ROOT" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release "${flags[@]}"
    feature_contract "$build_dir" "$expected_flavor"
    cmake --build "$build_dir" --parallel "$jobs"
    QT_QPA_PLATFORM=offscreen ctest --test-dir "$build_dir" --output-on-failure
    cpack --config "$build_dir/CPackConfig.cmake" -G DEB -B "$package_dir"

    mapfile -t built_packages < <(printf '%s\n' "$package_dir"/*.deb)
    ((${#built_packages[@]} == 1)) || die "expected one $expected_flavor package, found ${#built_packages[@]}"
    [[ -f "${built_packages[0]}" ]] || die "package was not created: ${built_packages[0]}"
    [[ "$(basename -- "${built_packages[0]}")" == *"_${expected_flavor}_"* ]] ||
        die "package filename does not match feature flavor: ${built_packages[0]}"

    if ((verify)); then
        verify_package "${built_packages[0]}" "$expected_flavor" "$run_root/verify"
    fi
    LAST_PACKAGE="${built_packages[0]}"
}

validate_source_metadata

flavors=()
if [[ "$flavor" == all ]]; then
    flavors=(generic kde)
else
    flavors=("$flavor")
fi

for current_flavor in "${flavors[@]}"; do
    build_once "$current_flavor" run-1
    first_package="$LAST_PACKAGE"
    final_package="$OUTPUT_ROOT/packages/$(basename -- "$first_package")"
    cp -- "$first_package" "$final_package"

    if ((reproducible)); then
        build_once "$current_flavor" run-2
        second_package="$LAST_PACKAGE"
        cmp --silent "$first_package" "$second_package" ||
            die "$current_flavor package is not byte-for-byte reproducible"
    fi

    printf '%s  %s\n' "$(sha256sum "$final_package" | cut -d' ' -f1)" "$(basename -- "$final_package")"
done

printf 'release package gate passed: flavor=%s output=%s reproducible=%s\n' \
    "$flavor" "$OUTPUT_ROOT/packages" "$reproducible"
