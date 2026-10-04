# TPC Slint

Cross-platform Slint frontend for the TPC controller. The application UI is
compiled from `ui/app.slint`; the TPC, settings, field-calculation, and export
services remain ordinary C++23 code.

## Prerequisites

- CMake 3.25 or newer;
- Ninja;
- a C++23 compiler (Apple Clang on macOS or Visual Studio 2022 on Windows);
- Rust 1.92 or newer when an installed Slint C++ SDK is not available;
- vcpkg at the baseline recorded in `vcpkg.json` or newer.

The build first looks for an installed Slint 1.18 C++ package. If it cannot
find one, CMake fetches the pinned Slint 1.18.1 source release and builds it.
That fallback requires `rustc` and `cargo` on `PATH`.

## Build

The macOS presets expect Ninja at `/opt/homebrew/bin/ninja` and vcpkg at
`~/Tools/vcpkg`:

```sh
cmake --preset debug
cmake --build --preset debug
```

On Windows, set `VCPKG_ROOT` before starting CLion and select
`windows-debug` or `windows-release`:

```powershell
$env:VCPKG_ROOT = "C:\src\vcpkg"
cmake --preset windows-debug
cmake --build --preset windows-debug
```

Release packaging installs `TPC_Slint.app` on macOS or `TPC_Slint.exe` on
Windows:

```sh
cmake --preset release
cmake --build --preset package
```

The bundled `subprojects/tpc_api` checkout is used by default. Set
`TPC_SLINT_USE_INSTALLED_TPC=ON` for an installed TPC package, or set
`TPC_API_DIR` to another source checkout.
