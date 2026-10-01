#!/bin/sh
set -eu

architecture="${1:-arm64}"
build_type="${2:-debug}"

case "$architecture" in
  arm64) triplet="arm64-osx" ;;
  x86_64) triplet="x64-osx" ;;
  *) echo "Unsupported macOS architecture: $architecture" >&2; exit 2 ;;
esac

case "$build_type" in
  debug|release) ;;
  *) echo "Unsupported build type: $build_type" >&2; exit 2 ;;
esac

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
qt_root="${QT_ROOT:-$HOME/Qt/6.12.0/macos}"
vcpkg_root="${VCPKG_ROOT:-$HOME/dev/vcpkg}"
build_dir="$project_dir/.build/meson-macos-$architecture-$build_type"
install_root="$project_dir/.build/meson-vcpkg/$triplet"
dependency_prefix="$install_root/$triplet"
machine_dir="$project_dir/.build/meson-machines"
machine_file="$machine_dir/macos-$architecture-$build_type.ini"

test "$(uname -s)" = Darwin || { echo "macOS setup must run on macOS." >&2; exit 2; }
test -x "$qt_root/bin/qmake6" || { echo "Qt qmake6 not found in $qt_root; set QT_ROOT." >&2; exit 2; }
test -x "$vcpkg_root/vcpkg" || { echo "vcpkg not found in $vcpkg_root; set VCPKG_ROOT." >&2; exit 2; }
xcrun --find clang++ >/dev/null

mkdir -p "$machine_dir"
sed "s|@QT_ROOT@|$qt_root|g" \
  "$project_dir/meson/native/macos.ini.in" > "$machine_file"

"$vcpkg_root/vcpkg" install \
  --x-manifest-root="$project_dir" \
  --x-install-root="$install_root" \
  --triplet="$triplet"

cpp_args="-arch $architecture"
cpp_link_args="-arch $architecture"

set --
if test -f "$build_dir/meson-private/coredata.dat"; then set -- --reconfigure; fi
PATH="$qt_root/bin:$PATH" meson setup \
  "$build_dir" \
  "$project_dir" \
  --native-file="$machine_file" \
  --buildtype="$build_type" \
  --cmake-prefix-path="$dependency_prefix" \
  -Dcpp_args="$cpp_args" \
  -Dcpp_link_args="$cpp_link_args" \
  -Db_lto="$(test "$build_type" = release && echo true || echo false)" \
  "$@"

echo "Configured: $build_dir"
echo "Build with: meson compile -C $build_dir"
