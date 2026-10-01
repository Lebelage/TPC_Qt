param(
    [ValidateSet("configure", "build", "run", "package", "clean")]
    [string]$Action = "build",
    [ValidateSet("debug", "release")]
    [string]$BuildType = "debug",
    [ValidateSet("x86_64", "arm64")]
    [string]$Architecture = "x86_64"
)

$ErrorActionPreference = "Stop"
$ProjectDir = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ProjectDir
$BuildDir = Join-Path $ProjectDir ".build\meson-windows-$Architecture-$BuildType"
if ($Action -eq "package" -and $BuildType -ne "release") {
    throw "Packaging is available for Release only. Use -BuildType release."
}
if ($Action -eq "clean" -and -not (Test-Path (Join-Path $BuildDir "meson-private\coredata.dat"))) {
    Write-Host "Nothing to clean: $BuildDir"
    exit 0
}
if (-not $env:QT_ROOT -or -not (Test-Path (Join-Path $env:QT_ROOT "bin\qmake.exe"))) {
    throw "Set the matching Qt kit in .vscode/settings.json (tpc.qtRoot or tpc.qtRootArm64)."
}

# Import the target compiler environment even when VS Code was opened normally.
$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $VsWhere)) {
    throw "Visual Studio Installer/vswhere.exe was not found. Install Visual Studio with C++ tools."
}
$TargetArch = if ($Architecture -eq "arm64") { "arm64" } else { "x64" }
$RequiredTools = if ($Architecture -eq "arm64") { "Microsoft.VisualStudio.Component.VC.Tools.ARM64" } else { "Microsoft.VisualStudio.Component.VC.Tools.x86.x64" }
$VsPath = & $VsWhere -latest -products * -requires $RequiredTools -property installationPath
if (-not $VsPath) { throw "Visual Studio with $TargetArch C++ tools was not found." }
$DevCmd = Join-Path $VsPath "Common7\Tools\VsDevCmd.bat"
$RequestedQtRoot = $env:QT_ROOT
$RequestedVcpkgRoot = $env:VCPKG_ROOT
$CompilerEnvironment = & $env:ComSpec /d /c "call `"$DevCmd`" -no_logo -arch=$TargetArch -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) { throw "Failed to initialize the Visual Studio $TargetArch environment." }
foreach ($Line in $CompilerEnvironment) {
    if ($Line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
}

# VsDevCmd can replace VCPKG_ROOT with Visual Studio's bundled vcpkg.
$env:QT_ROOT = $RequestedQtRoot
$env:VCPKG_ROOT = $RequestedVcpkgRoot

if ($Action -eq "configure" -or -not (Test-Path (Join-Path $BuildDir "meson-private\coredata.dat"))) {
    # A child process contains setup's exit statements on failure.
    & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "setup-windows.ps1") -Architecture $Architecture -BuildType $BuildType
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
if ($Action -eq "configure") { exit 0 }
if ($Action -eq "clean") {
    & meson compile -C $BuildDir --clean
    exit $LASTEXITCODE
}

& meson compile -C $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$DeployOptions = @()
if ($BuildType -eq "release") { $DeployOptions += "-Package" }
if ($Action -eq "package") { $DeployOptions += "-Archive" }
& powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "deploy-windows.ps1") -BuildDir $BuildDir @DeployOptions
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Action -eq "run") {
    $RunDir = if ($BuildType -eq "release") { Join-Path $ProjectDir "dist\TPC_Qt-windows-$Architecture" } else { $BuildDir }
    & (Join-Path $RunDir "TPC_Qt.exe")
    exit $LASTEXITCODE
}
