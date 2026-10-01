param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,
    [switch]$Package,
    [switch]$Archive,
    [string]$QtRoot = $env:QT_ROOT
)

$ErrorActionPreference = "Stop"
$ProjectDir = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$Executable = Join-Path $BuildDir "TPC_Qt.exe"
if (-not (Test-Path -LiteralPath $Executable)) { throw "Executable was not found: $Executable" }
$WinDeployQt = Join-Path $QtRoot "bin\windeployqt.exe"
if (-not (Test-Path -LiteralPath $WinDeployQt)) { throw "Check QT_ROOT: windeployqt.exe was not found at '$WinDeployQt'." }
$BuildOptions = Get-Content -Raw (Join-Path $BuildDir "meson-info\intro-buildoptions.json") | ConvertFrom-Json
$BuildType = ($BuildOptions | Where-Object name -EQ "buildtype").value
$Machines = Get-Content -Raw (Join-Path $BuildDir "meson-info\intro-machines.json") | ConvertFrom-Json
$Architecture = if ($Machines.host.cpu_family -eq "aarch64") { "arm64" } else { "x86_64" }
$Triplet = if ($Architecture -eq "arm64") { "arm64-windows" } else { "x64-windows" }
if ($Package -and $BuildType -ne "release") { throw "Only Release builds can be packaged." }
if ($Archive -and -not $Package) { throw "Use -Package with -Archive." }
$QtConfiguration = if ($BuildType -eq "debug") { "--debug" } else { "--release" }
$DependencyPrefix = Join-Path $ProjectDir ".build\meson-vcpkg\$Triplet\$Triplet"
$DependencyBin = if ($BuildType -eq "debug") { Join-Path $DependencyPrefix "debug\bin" } else { Join-Path $DependencyPrefix "bin" }
if (-not (Test-Path -LiteralPath $DependencyBin)) { throw "vcpkg runtime directory was not found: $DependencyBin" }

$Destination = $BuildDir
if ($Package) {
    $StageRoot = Join-Path $ProjectDir ".build\package-staging"
    $Destination = Join-Path $StageRoot ([Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Copy-Item -LiteralPath $Executable -Destination $Destination
}
$DeployArgs = @($QtConfiguration, "--no-translations", "--verbose", "0", "--qmldir", (Join-Path $ProjectDir "qml"), "--dir", $Destination)
if ($Package) { $DeployArgs += @("--skip-plugin-types", "qmltooling") }
& $WinDeployQt @DeployArgs $Executable
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Copy only vcpkg DLLs imported by the executable and their dependencies.
$Dumpbin = Get-Command dumpbin.exe -ErrorAction Stop
$Pending = New-Object 'System.Collections.Generic.Queue[string]'
$Visited = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$Pending.Enqueue($Executable)
while ($Pending.Count -gt 0) {
    $Binary = $Pending.Dequeue()
    if (-not $Visited.Add($Binary)) { continue }
    $Imports = & $Dumpbin.Source /nologo /dependents $Binary
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect dependencies: $Binary" }
    foreach ($Line in $Imports) {
        if ($Line -match '^\s+([\w.+-]+\.dll)\s*$') {
            $Dependency = Join-Path $DependencyBin $Matches[1]
            if (Test-Path -LiteralPath $Dependency) {
                Copy-Item -LiteralPath $Dependency -Destination $Destination -Force
                $Pending.Enqueue($Dependency)
            }
        }
    }
}
if ($Package) {
    # qmltypes are editor metadata; QML sources and qmldir files are runtime data.
    Get-ChildItem -LiteralPath $Destination -Recurse -File -Filter *.qmltypes | ForEach-Object {
        Remove-Item -LiteralPath $_.FullName
    }
    Set-Content -LiteralPath (Join-Path $Destination "qt.conf") -Encoding ASCII -Value "[Paths]","Prefix=.","Plugins=.","QmlImports=qml"
    $DistRoot = [IO.Path]::GetFullPath((Join-Path $ProjectDir "dist"))
    $OutputDir = [IO.Path]::GetFullPath((Join-Path $DistRoot "TPC_Qt-windows-$Architecture"))
    # Restrict replacement to this exact generated distribution directory.
    if ([IO.Path]::GetDirectoryName($OutputDir) -ne $DistRoot -or
        -not $Destination.StartsWith(([IO.Path]::GetFullPath($StageRoot) + "\"), [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe distribution path."
    }
    New-Item -ItemType Directory -Path $DistRoot -Force | Out-Null
    if ((Get-Item -LiteralPath $DistRoot).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Distribution root must not be a link." }
    if (Test-Path -LiteralPath $OutputDir) {
        if ((Get-Item -LiteralPath $OutputDir).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Distribution directory must not be a link." }
        Remove-Item -LiteralPath $OutputDir -Recurse -Force
    }
    Move-Item -LiteralPath $Destination -Destination $OutputDir
    Write-Host "Release folder: $OutputDir"
    if ($Archive) {
        $ArchivePath = "$OutputDir.zip"
        Compress-Archive -LiteralPath $OutputDir -DestinationPath $ArchivePath -Force
        Write-Host "Release archive: $ArchivePath"
    }
} else {
    Write-Host "Runtime dependencies deployed to: $Destination"
}
