#!/bin/sh
set -eu

executable=$1
info_plist=$2
bundle=$3
qml_dir=$4
macdeployqt=$5

contents="$bundle/Contents"
macos_dir="$contents/MacOS"

mkdir -p "$macos_dir"
cp "$executable" "$macos_dir/TPC_Qt"
cp "$info_plist" "$contents/Info.plist"

"$macdeployqt" "$bundle" "-qmldir=$qml_dir"

# macdeployqt copies every installed SQL driver even though this application
# does not use QtSql. Some optional drivers refer to SDKs that may not be
# installed on the deployment machine, so keep them out of the bundle.
rm -rf "$contents/PlugIns/sqldrivers"
