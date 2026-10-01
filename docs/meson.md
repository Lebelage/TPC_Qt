# Meson build

Meson is the project's build system. Meson 1.7 or newer is required.

## CLion

Open the repository directory (or its top-level `meson.build`) as a Meson
project. Before the first CLion reload, run the setup script for the desired
platform once; it creates the build directory and records the Qt/vcpkg paths
in a generated machine file. In **Settings | Build, Execution, Deployment |
Meson**, point the profile at that generated build directory, for example
`.build/meson-macos-arm64-debug`.

Use the `TPC_Qt` target for normal Run/Debug configurations and the `deploy`
target only when a self-contained application bundle is needed.

## macOS

The setup script installs manifest dependencies with vcpkg and configures the
requested architecture and build type:

```sh
scripts/meson/setup-macos.sh arm64 debug
meson compile -C .build/meson-macos-arm64-debug
```

Other supported combinations are `arm64 release`, `x86_64 debug`, and
`x86_64 release`. `QT_ROOT` and `VCPKG_ROOT` may override their defaults.
The setup script writes a local machine file under `.build/meson-machines`, so
Qt remains discoverable when Ninja or CLion regenerates the project later.

To create a deployed application bundle:

```sh
meson compile -C .build/meson-macos-arm64-release deploy
```

The resulting bundle is written to `TPC_Qt.app` inside the Meson build
directory.

## Windows Release

Install Qt and Visual Studio for the requested target architecture. From the
matching Visual Studio Developer PowerShell, set `QT_ROOT` and `VCPKG_ROOT`,
then run:

```powershell
$env:QT_ROOT = "C:\Qt\6.11.2\msvc2022_64"
$env:VCPKG_ROOT = "C:\dev\vcpkg"
scripts\meson\setup-windows.cmd -Architecture x86_64
meson compile -C .build\meson-windows-x86_64-release
meson compile -C .build\meson-windows-x86_64-release deploy
```

For Windows ARM64, use an ARM64 Qt installation and Developer PowerShell:

```powershell
$env:QT_ROOT = "C:\Qt\6.11.2\msvc2022_arm64"
scripts\meson\setup-windows.cmd -Architecture arm64
meson compile -C .build\meson-windows-arm64-release
meson compile -C .build\meson-windows-arm64-release deploy
```

The Windows machine files select Release, size optimization, LTO, and the
dynamic MSVC runtime. The `deploy` target runs `windeployqt` with the project's
QML source directory. Windows setup installs the package names from
`vcpkg.json` in classic mode, so no registry baseline is required; vcpkg may
select newer port versions as its checkout is updated.

The `.cmd` launcher applies `ExecutionPolicy Bypass` only to the child
PowerShell process that runs the setup script. It does not modify the user or
machine execution policy. The `.ps1` file can still be invoked directly on
systems where local PowerShell scripts are already allowed.

## TPC_API

Meson first looks for an installed `TPC` CMake package. If one is not present,
it downloads the pinned TPC_API revision declared in
`subprojects/tpc_api.wrap` and applies the local Meson overlay.
