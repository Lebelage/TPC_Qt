#!/bin/sh
set -eu

bundle=$1
architecture="${2:-arm64}"
action="${3:-package}"
case "$architecture" in arm64|x86_64) ;; *) echo "Unsupported architecture" >&2; exit 2 ;; esac
if test "$#" -gt 3; then
  shift 3
  "$@"
fi
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
test -f "$bundle/Contents/MacOS/TPC_Qt" || { echo "Build the application bundle first." >&2; exit 2; }
dist_root="$project_dir/dist"
stage_root="$project_dir/.build/package-staging"
test ! -L "$dist_root" && test ! -L "$stage_root" || { echo "Output roots must not be symlinks." >&2; exit 2; }
mkdir -p "$dist_root" "$stage_root"
stage=$(mktemp -d "$stage_root/macos.XXXXXX")
output="$dist_root/TPC_Qt-macos-$architecture.app"
test ! -L "$output" || { echo "Output bundle must not be a symlink." >&2; exit 2; }
ditto "$bundle" "$stage/TPC_Qt-macos-$architecture.app"
case "$output" in "$project_dir"/dist/TPC_Qt-macos-arm64.app|"$project_dir"/dist/TPC_Qt-macos-x86_64.app) ;; *) exit 2 ;; esac
rm -rf -- "$output"
mv "$stage/TPC_Qt-macos-$architecture.app" "$output"
rmdir "$stage"
echo "Release bundle: $output"
if test "$action" = package; then
  ditto -c -k --sequesterRsrc --keepParent "$output" "$dist_root/TPC_Qt-macos-$architecture.zip"
  echo "Release archive: $dist_root/TPC_Qt-macos-$architecture.zip"
fi
