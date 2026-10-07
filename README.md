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

Clone the project with its pinned dependency:

```sh
git clone --recurse-submodules https://github.com/Lebelage/TPC_Qt.git
cd TPC_Qt
git checkout feature/slint-migration
git submodule update --init --recursive
```

After pulling application changes, run `git submodule update --init --recursive`
again. A detached HEAD inside the submodule is normal: the application pins
an exact API commit, not the API's `master` branch. Changes inside a submodule
must be committed/published in that repository first; then update the gitlink
in the application repository. Do not use `git submodule update --remote` to
resolve build mismatches.

### Repair a copied dependency folder on Windows

If `git submodule update` fails because `subprojects/tpc_api` is non-empty,
and `git -C subprojects/tpc_api rev-parse HEAD` returns the application's commit,
the folder is not an independent checkout. Run:

```powershell
cd E:\TPC\TPC_Qt
& .\scripts\repair-submodule.ps1
cmake --preset windows-release
cmake --build .build/windows-release --target install
```

The script preserves an unregistered folder as a timestamped
`subprojects/tpc_api_backup-*`, initializes the pinned checkout and verifies
its HEAD. Existing dirty registered submodules are left untouched. It does not
reset, delete, commit, push or pull the application repository. Backups are
ignored by Git and retained for comparison. No global Git settings or system
PowerShell execution policy are changed. If your policy blocks unsigned scripts,
review it and use your organization's approved execution method.

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
