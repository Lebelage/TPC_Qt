# Compatible with Windows PowerShell 5.1 and PowerShell 7.
[CmdletBinding()]
param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-Git {
    param([string[]]$Arguments)
    # Native stderr is an ErrorRecord on Windows PowerShell 5.1; check exit code explicitly.
    $ErrorActionPreference = 'Continue'
    $output = & git @Arguments 2>&1
    $code = $LASTEXITCODE
    $message = ($output | ForEach-Object { $_.ToString() }) -join "`n"
    if ($code -ne 0) { throw "git failed ($code): $message" }
    return $message.Trim()
}

function Same-Path {
    param([string]$Left, [string]$Right)
    if ([string]::IsNullOrWhiteSpace($Left) -or [string]::IsNullOrWhiteSpace($Right)) { return $false }
    $leftPath = [IO.Path]::GetFullPath($Left).TrimEnd([char[]]'\/')
    $rightPath = [IO.Path]::GetFullPath($Right).TrimEnd([char[]]'\/')
    return $leftPath.Equals($rightPath, [StringComparison]::OrdinalIgnoreCase)
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw 'Git is not installed or is not on PATH.' }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$root = Invoke-Git -Arguments @('-C', $ProjectRoot, 'rev-parse', '--show-toplevel')
if (-not (Same-Path $root $ProjectRoot)) { throw 'ProjectRoot must be the repository root.' }
$entry = Invoke-Git -Arguments @('-C', $ProjectRoot, 'ls-tree', 'HEAD', '--', 'subprojects/tpc_api')
if ($entry -notmatch '^160000 commit ([0-9a-f]{40})\s+subprojects/tpc_api$') {
    throw 'HEAD does not pin subprojects/tpc_api as a Git submodule. Update the main repository first.'
}
$expected = $Matches[1]
$api = Join-Path $ProjectRoot 'subprojects/tpc_api'
$backup = $null

if (Test-Path -LiteralPath $api) {
    $item = Get-Item -LiteralPath $api -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw 'Submodule path is a link/junction. Resolve it manually; nothing has been moved.'
    }
    $registered = $false
    if ($item.PSIsContainer) {
        try {
            $top = Invoke-Git -Arguments @('-C', $api, 'rev-parse', '--show-toplevel')
            if (Same-Path $top $api) {
                $parent = Invoke-Git -Arguments @('-C', $api, 'rev-parse', '--show-superproject-working-tree')
                $registered = Same-Path $parent $ProjectRoot
            }
        } catch { $registered = $false }
    }
    if ($registered) {
        $changes = Invoke-Git -Arguments @('-C', $api, 'status', '--porcelain', '--untracked-files=all')
        if ($changes) { throw "Submodule has local changes. Preserve/commit them before updating:`n$changes" }
    } else {
        $suffix = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
        $backup = Join-Path $ProjectRoot ('subprojects/tpc_api_backup-' + $suffix)
        if (Test-Path -LiteralPath $backup) { throw 'Backup path already exists; nothing moved.' }
        Move-Item -LiteralPath $api -Destination $backup
        Write-Host "Old files preserved: $backup"
    }
}

try {
    Write-Host (Invoke-Git -Arguments @('-C', $ProjectRoot, 'submodule', 'sync', '--recursive'))
    Write-Host (Invoke-Git -Arguments @('-C', $ProjectRoot, 'submodule', 'update', '--init', '--recursive', '--', 'subprojects/tpc_api'))
    $actual = Invoke-Git -Arguments @('-C', $api, 'rev-parse', 'HEAD')
    $top = Invoke-Git -Arguments @('-C', $api, 'rev-parse', '--show-toplevel')
    if (-not (Same-Path $top $api) -or $actual -ne $expected) { throw 'Submodule verification failed.' }
    Write-Host "OK: tpc_api at pinned commit $actual"
    if ($backup) { Write-Host "Backup retained: $backup. Compare local changes before removing it." }
} catch {
    if ($backup) { Write-Warning "Repair failed; original files remain safe in $backup. No files were deleted." }
    throw
}
