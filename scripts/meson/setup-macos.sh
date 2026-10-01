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
qt_root="${QT_ROOT:-$HOME/Qt/6.11.2/macos}"
vcpkg_root="${VCPKG_ROOT:-$HOME/dev/vcpkg}"
build_dir="$project_dir/.build/meson-macos-$architecture-$build_type"
install_root="$project_dir/.build/meson-vcpkg/$triplet"
dependency_prefix="$install_root/$triplet"
machine_dir="$project_dir/.build/meson-machines"
machine_file="$machine_dir/macos-$architecture.ini"

mkdir -p "$machine_dir"
sed "s|@QT_ROOT@|$qt_root|g" \
  "$project_dir/meson/native/macos.ini.in" > "$machine_file"

"$vcpkg_root/vcpkg" install \
  --x-manifest-root="$project_dir" \
  --x-install-root="$install_root" \
  --triplet="$triplet"

cpp_args="-arch $architecture"
cpp_link_args="-arch $architecture"

PATH="$qt_root/bin:$PATH" meson setup \
  "$build_dir" \
  "$project_dir" \
  --native-file="$machine_file" \
  --buildtype="$build_type" \
  --cmake-prefix-path="$dependency_prefix" \
  -Dcpp_args="$cpp_args" \
  -Dcpp_link_args="$cpp_link_args" \
  -Db_lto="$(test "$build_type" = release && echo true || echo false)" \
  --wipe

echo "Configured: $build_dir"
echo "Build with: meson compile -C $build_dir"
