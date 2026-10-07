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

Clone the project; the TPC API sources are included in the same repository:

```sh
git clone --branch feature/slint-migration https://github.com/Lebelage/TPC_Qt.git
cd TPC_Qt
```

`subprojects/tpc_api` is an ordinary tracked directory, not a Git submodule.
Commit and push its changes together with the application. No separate clone,
submodule initialization, or API repository push is required. Keep nested `.git`
metadata out of this directory until the dependency is deliberately split into
its own repository again.

### Updating a previous submodule-based Windows checkout

Before pulling the conversion commit, preserve the old dependency directory
(including any local changes and nested Git metadata). Then pull normally:

```powershell
cd E:\TPC\TPC_Qt
$backupName = 'tpc_api_backup-' + [Guid]::NewGuid().ToString('N')
Rename-Item -LiteralPath .\subprojects\tpc_api -NewName $backupName
git pull --ff-only
cmake --preset windows-release
cmake --build .build/windows-release --target install
```

The backup is retained and ignored by Git; compare local changes before removing
it. Pull only after the conversion commit has been published to your branch,
and preserve any application changes before pulling. Do not run
`git submodule update` anymore. These commands do not change PowerShell policy.

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

The bundled `subprojects/tpc_api` sources are used by default. Set
`TPC_SLINT_USE_INSTALLED_TPC=ON` for an installed TPC package, or set
`TPC_API_DIR` to another source checkout.
