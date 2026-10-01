#!/bin/sh
set -eu

action="${1:-build}"
build_type="${2:-debug}"
case "$action" in configure|build|run|clean|package) ;; *) echo "Unknown action: $action" >&2; exit 2 ;; esac
case "$build_type" in debug|release) ;; *) echo "Unknown build type: $build_type" >&2; exit 2 ;; esac
test "$(uname -s)" = Darwin || { echo "This task requires macOS." >&2; exit 2; }
test "$(uname -m)" = arm64 || { echo "Run VS Code natively on Apple Silicon, without Rosetta." >&2; exit 2; }
if test "$action" = package && test "$build_type" != release; then
  echo "Packaging requires Release." >&2; exit 2
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
QT_ROOT="${QT_ROOT:-$HOME/Qt/6.12.0/macos}"
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/dev/vcpkg}"
PATH="/opt/homebrew/bin:$QT_ROOT/bin:$PATH"
export QT_ROOT VCPKG_ROOT PATH
cd "$project_dir"
build_dir="$project_dir/.build/meson-macos-arm64-$build_type"

if test "$action" = clean; then
  if test -f "$build_dir/meson-private/coredata.dat"; then meson compile -C "$build_dir" --clean; fi
  exit 0
fi
if test "$action" = configure || ! test -f "$build_dir/meson-private/coredata.dat"; then
  sh "$script_dir/setup-macos.sh" arm64 "$build_type"
fi
test "$action" != configure || exit 0
meson compile -C "$build_dir" deploy

run_bundle="$build_dir/TPC_Qt.app"
if test "$build_type" = release; then
  sh "$script_dir/package-macos.sh" "$build_dir/TPC_Qt.app" arm64 "$action"
  run_bundle="$project_dir/dist/TPC_Qt-macos-arm64.app"
fi
if test "$action" = run; then
  exec "$run_bundle/Contents/MacOS/TPC_Qt"
fi
