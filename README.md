# TPC Qt

The repository has one CMake/vcpkg build for macOS and Windows. Qt, TPC API,
and all third-party libraries are resolved without machine-specific paths.

## Prerequisites

- CMake 3.25 or newer;
- Ninja;
- a C++23 compiler (Apple Clang on macOS, Visual Studio 2022 on Windows);
- a vcpkg checkout at the baseline recorded in `vcpkg.json` or newer.

The macOS presets are ready for a standard Apple Silicon/Homebrew setup:

- Ninja at `/opt/homebrew/bin/ninja`;
- vcpkg at `~/Tools/vcpkg`.

No environment variables or extra CMake options are required in CLion. Enable
CMake presets and select `debug` or `release`.

The same presets work in a terminal:

```sh
cmake --preset debug
cmake --build --preset debug
```

On Windows, set `VCPKG_ROOT` before starting CLion and select
`windows-debug` or `windows-release`:

```powershell
$env:VCPKG_ROOT = "C:\src\vcpkg"
clion64.exe .
```

The first configure installs the manifest dependencies for the selected
platform. Windows terminal builds use the matching preset names:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
```

To create a deployable Release application with Qt plugins and runtime
libraries, run:

```sh
cmake --preset release
cmake --build --preset package
```

The macOS result is installed to `out/release/TPC_Qt.app`. On Windows, use
`cmake --build --preset windows-package`; the executable and required runtime
files are installed under `out/windows-release/bin`.

Clone the repository with its bundled TPC API:

```sh
git clone --recurse-submodules <repository-url>
```

For an existing checkout, run `git submodule update --init --recursive` once.
`subprojects/tpc_api` is the bundled application library. To consume a
separately installed package instead, configure with
`-DTPC_QT_USE_INSTALLED_TPC=ON`. To use another source checkout, set
`-DTPC_API_DIR=/path/to/TPC_API`.
