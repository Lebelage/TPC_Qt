param(
    [ValidateSet("x86_64", "arm64")]
    [string]$Architecture = "x86_64"
)

$ErrorActionPreference = "Stop"

$ProjectDir = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$QtKitSuffix = if ($Architecture -eq "arm64") { "arm64" } else { "64" }
$QtRoot = if ($env:QT_ROOT) {
    $env:QT_ROOT
} else {
    throw "QT_ROOT is not set. In cmd.exe use: set QT_ROOT=C:\Qt\6.11.2\msvc2022_$QtKitSuffix"
}
$VcpkgRoot = if ($env:VCPKG_ROOT) {
    $env:VCPKG_ROOT
} else {
    throw "VCPKG_ROOT is not set. In cmd.exe use: set VCPKG_ROOT=C:\dev\vcpkg"
}

$Triplet = if ($Architecture -eq "arm64") { "arm64-windows" } else { "x64-windows" }
$CrossTemplate = Join-Path $ProjectDir "meson\cross\windows-$Architecture.ini"
$BuildDir = Join-Path $ProjectDir ".build\meson-windows-$Architecture-release"
$InstallRoot = Join-Path $ProjectDir ".build\meson-vcpkg\$Triplet"
$DependencyPrefix = Join-Path $InstallRoot $Triplet
$MachineDir = Join-Path $ProjectDir ".build\meson-machines"
$CrossFile = Join-Path $MachineDir "windows-$Architecture.ini"
$Manifest = Get-Content -Raw (Join-Path $ProjectDir "vcpkg.json") | ConvertFrom-Json
$VcpkgPackages = @($Manifest.dependencies | ForEach-Object {
    if ($_ -is [string]) { $_ } else { $_.name }
})

$Qmake = Join-Path $QtRoot "bin\qmake.exe"
$Vcpkg = Join-Path $VcpkgRoot "vcpkg.exe"
$ExpectedVsArchitecture = if ($Architecture -eq "arm64") { "arm64" } else { "x64" }

if (-not (Test-Path -PathType Leaf $Qmake)) {
    throw "qmake.exe was not found at '$Qmake'. QT_ROOT must point to the matching Qt $Architecture kit."
}

if (-not (Test-Path -PathType Leaf $Vcpkg)) {
    throw "vcpkg.exe was not found at '$Vcpkg'. Check VCPKG_ROOT."
}

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw "cl.exe was not found. Run this script from the matching Visual Studio Developer Command Prompt or Developer PowerShell."
}

if ($env:VSCMD_ARG_TGT_ARCH -and $env:VSCMD_ARG_TGT_ARCH -ne $ExpectedVsArchitecture) {
    throw "Visual Studio targets '$env:VSCMD_ARG_TGT_ARCH', but '$Architecture' was requested. Open the $ExpectedVsArchitecture Native Tools prompt and try again."
}

New-Item -ItemType Directory -Force -Path $MachineDir | Out-Null
$NormalizedQtRoot = $QtRoot.Replace("\", "/")
(Get-Content -Raw $CrossTemplate).Replace("@QT_ROOT@", $NormalizedQtRoot) | Set-Content -NoNewline $CrossFile

& $Vcpkg install `
    --classic `
    "--x-install-root=$InstallRoot" `
    "--triplet=$Triplet" `
    @VcpkgPackages

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$env:Path = "$(Join-Path $QtRoot 'bin');$env:Path"

meson setup `
    $BuildDir `
    $ProjectDir `
    "--cross-file=$CrossFile" `
    "--cmake-prefix-path=$DependencyPrefix" `
    --wipe

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Configured: $BuildDir"
Write-Host "Build with: meson compile -C `"$BuildDir`""
Write-Host "Deploy with: meson compile -C `"$BuildDir`" deploy"
