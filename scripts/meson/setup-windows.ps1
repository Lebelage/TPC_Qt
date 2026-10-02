param(
    [ValidateSet("x86_64", "arm64")]
    [string]$Architecture = "x86_64"
)

$ErrorActionPreference = "Stop"

$ProjectDir = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$QtRoot = if ($env:QT_ROOT) { $env:QT_ROOT } else { throw "QT_ROOT must point to the matching Windows Qt installation" }
$VcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { throw "VCPKG_ROOT must point to vcpkg" }

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

New-Item -ItemType Directory -Force -Path $MachineDir | Out-Null
$NormalizedQtRoot = $QtRoot.Replace("\", "/")
(Get-Content -Raw $CrossTemplate).Replace("@QT_ROOT@", $NormalizedQtRoot) | Set-Content -NoNewline $CrossFile

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw "cl.exe was not found. Run this script from the matching Visual Studio Developer PowerShell (x64 or ARM64)."
}

& (Join-Path $VcpkgRoot "vcpkg.exe") install `
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
