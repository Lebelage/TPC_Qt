#!/bin/sh
set -eu

executable=$1
info_plist=$2
bundle=$3
qml_dir=$4
macdeployqt=$5
build_type="${6:-release}"
case "$build_type" in debug|release) ;; *) echo "Invalid build type" >&2; exit 2 ;; esac
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
bundle_parent=$(CDPATH= cd -- "$(dirname -- "$bundle")" && pwd -P)
build_root=$(CDPATH= cd -- "$project_dir/.build" && pwd -P)
case "$bundle_parent" in "$build_root"/*) ;; *) echo "Bundle must be a generated .build output." >&2; exit 2 ;; esac
test "$(basename -- "$bundle")" = TPC_Qt.app || { echo "Unexpected bundle name." >&2; exit 2; }
bundle="$bundle_parent/TPC_Qt.app"
test ! -L "$bundle" || { echo "Bundle must not be a symlink." >&2; exit 2; }
rm -rf -- "$bundle"

contents="$bundle/Contents"
macos_dir="$contents/MacOS"

mkdir -p "$macos_dir"
cp "$executable" "$macos_dir/TPC_Qt"
cp "$info_plist" "$contents/Info.plist"

set --
if test "$build_type" = debug; then set -- -no-strip; fi
"$macdeployqt" "$bundle" "-qmldir=$qml_dir" "$@"

# macdeployqt copies every installed SQL driver even though this application
# does not use QtSql. Some optional drivers refer to SDKs that may not be
# installed on the deployment machine, so keep them out of the bundle.
rm -rf "$contents/PlugIns/sqldrivers"
if test "$build_type" = release; then
  rm -rf -- "$contents/PlugIns/qmltooling"
  find "$contents" -type f -name '*.qmltypes' -delete
fi
# Deployment changes Mach-O load paths; refresh the local ad-hoc signature.
codesign --force --deep --sign - "$bundle"
