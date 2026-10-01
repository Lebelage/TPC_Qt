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

Install Qt and Visual Studio for the requested target architecture. Open the
matching Visual Studio Native Tools prompt. An `arm64_x64` prompt targets x64;
use an `arm64` prompt when producing ARM64 binaries.

From Developer Command Prompt (`cmd.exe`), run:

```batch
set "QT_ROOT=C:\Qt\6.11.2\msvc2022_64"
set "VCPKG_ROOT=C:\dev\vcpkg"
scripts\meson\setup-windows.cmd -Architecture x86_64
meson compile -C .build\meson-windows-x86_64-release
meson compile -C .build\meson-windows-x86_64-release deploy
```

For Windows ARM64 in Developer Command Prompt:

```batch
set "QT_ROOT=C:\Qt\6.11.2\msvc2022_arm64"
set "VCPKG_ROOT=C:\dev\vcpkg"
scripts\meson\setup-windows.cmd -Architecture arm64
meson compile -C .build\meson-windows-arm64-release
meson compile -C .build\meson-windows-arm64-release deploy
```

PowerShell uses different environment-variable syntax:

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
$env:VCPKG_ROOT = "C:\dev\vcpkg"
scripts\meson\setup-windows.cmd -Architecture arm64
meson compile -C .build\meson-windows-arm64-release
meson compile -C .build\meson-windows-arm64-release deploy
```

The Windows machine files default to Release, size optimization, LTO, and the
dynamic MSVC runtime. Pass `-BuildType debug` to setup for debug Qt, debug
vcpkg libraries, the debug MSVC runtime and no LTO. Setup reconfigures an
existing build without deleting it, and writes a separate machine file for
each architecture and build type. The `deploy` target copies Qt and vcpkg
runtime dependencies next to the EXE. The Release-only `package` target
creates a clean distribution folder and ZIP under `dist`. Windows setup installs the package names from
`vcpkg.json` in classic mode, so no registry baseline is required; vcpkg may
select newer port versions as its checkout is updated.

The `.cmd` launcher applies `ExecutionPolicy Bypass` only to the child
PowerShell process that runs the setup script. It does not modify the user or
machine execution policy. The `.ps1` file can still be invoked directly on
systems where local PowerShell scripts are already allowed.

## VS Code on Windows

Install the recommended Microsoft C/C++ and Meson extensions. Set `tpc.qtRoot`
(x64), `tpc.qtRootArm64` and `tpc.vcpkgRoot` in `.vscode/settings.json`.
Select Debug/Release and x64/ARM64 in Run and Debug, then press F5 to debug
or Ctrl+F5 to run. Ctrl+Shift+B builds Debug x64 by default. Use the attach
configuration to debug an already running process. Release is optimized
and has no application debug symbols; use Debug for source breakpoints.

Tasks are provided for each of the four combinations:

| Task | Result |
| --- | --- |
| `Meson: configure ...` | Install dependencies and configure/reconfigure Meson |
| `Meson: build ...` | Compile and prepare DLLs; Release also refreshes `dist` |
| `Meson: run ...` | Build, deploy and run |
| `Meson: clean ...` | Clean compiler outputs for this configuration |
| `Meson: package Release ...` | Build a fresh Release folder and ZIP |

The tasks initialize Visual Studio with the matching target architecture
and preserve the configured vcpkg path. ARM64 requires its own Qt kit and
Visual Studio ARM64 C++ tools. Running/debugging ARM64 requires compatible
Windows hardware; the provided launch configurations do not configure remote debugging.

Build artifacts remain in `.build/meson-windows-<architecture>-<buildtype>`.
Release folders are `dist/TPC_Qt-windows-x86_64` and
`dist/TPC_Qt-windows-arm64`; ZIPs have the same names with `.zip` appended.
Send the entire folder or ZIP, rather than only the EXE. Each release build
replaces its generated distribution folder, so stop a running copy before
rebuilding. ZIPs are refreshed by the package task only.

Distribution folders contain the EXE, imported vcpkg DLLs, Qt runtime/QML
files, plugins, `qt.conf` and the MSVC redistributable installer supplied by
`windeployqt`. On a new machine install `vc_redist` if necessary. PDB, OBJ,
LIB, EXP, Meson files, logs, QML editor metadata and QML debugging plugins
are excluded. Keep the Qt runtime QML files and plugin subdirectories.

The same workflow is available from PowerShell:

```powershell
$env:QT_ROOT = "C:\Qt\6.12.0\msvc2022_64"
$env:VCPKG_ROOT = "C:\vcpkg"
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\meson\vscode-windows.ps1 -Action build -BuildType debug -Architecture x86_64
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\meson\vscode-windows.ps1 -Action package -BuildType release -Architecture x86_64
```

For ARM64, set `QT_ROOT` to the ARM64 kit and pass `-Architecture arm64`.

Windows embeds QML using `qml/windows-qml.qrc` to avoid backslashes in
resource URLs generated by Meson 1.12. New QML files must be added to this
QRC and their module's `qmldir` file.

## VS Code on macOS Apple Silicon

Run VS Code natively on an Apple Silicon Mac. Install Xcode Command Line
Tools, Meson, Ninja, CMake, an ARM64-compatible Qt macOS kit and vcpkg.
Install the recommended CodeLLDB extension for debugging.

Set `tpc.qtRootMacos` and `tpc.vcpkgRootMacos` in `.vscode/settings.json` to
absolute paths on the Mac. Empty values default to `$HOME/Qt/6.12.0/macos`
and `$HOME/dev/vcpkg`. These settings are independent of the Windows paths.
The task adds `/opt/homebrew/bin` to PATH for Homebrew build tools.

Select `TPC_Qt: Debug (macOS Apple Silicon)` or
`TPC_Qt: Release (macOS Apple Silicon)` and press F5 (debug) or Ctrl+F5
(run without debugging). Ctrl+Shift+B selects native Debug on Windows/macOS.
The macOS LLDB attach configuration can attach to an existing process.

Use `Meson: configure/build/run/clean Debug macOS ARM64` or the corresponding
Release tasks. `Meson: package Release macOS ARM64` creates:

- `dist/TPC_Qt-macos-arm64.app`
- `dist/TPC_Qt-macos-arm64.zip`

Build files remain in `.build/meson-macos-arm64-debug` or
`.build/meson-macos-arm64-release`. Debug preserves symbols. Release bundles
contain the deployed application, frameworks, dylibs, QML runtime files and
plugins; build files and editor QML metadata are excluded. Rebuilding starts
with a fresh application bundle. ZIP packaging preserves bundle permissions
and symlinks using `ditto`.

Bundles receive a local ad-hoc signature after deployment. Developer ID
signing and notarization for distribution are separate steps.

From a macOS terminal the same actions are available as:

```sh
export QT_ROOT="$HOME/Qt/6.12.0/macos"
export VCPKG_ROOT="$HOME/dev/vcpkg"
sh scripts/meson/vscode-macos.sh build debug
sh scripts/meson/vscode-macos.sh run release
sh scripts/meson/vscode-macos.sh package release
```

## TPC_API

Meson first looks for an installed `TPC` CMake package. If one is not present,
it downloads the pinned TPC_API revision declared in
`subprojects/tpc_api.wrap` and applies the local Meson overlay.
