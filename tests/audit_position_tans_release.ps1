[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BundleDirectory,
    [Parameter(Mandatory = $true)][string]$BaselineDirectory,
    [Parameter(Mandatory = $true)][string]$MarcCli,
    [Parameter(Mandatory = $true)][string]$BackupCli
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Read-Manifest([string]$Directory) {
    return Get-Content -LiteralPath (Join-Path $Directory 'manifest.json') -Raw |
        ConvertFrom-Json
}

$bundle = (Resolve-Path -LiteralPath $BundleDirectory).Path
$baseline = (Resolve-Path -LiteralPath $BaselineDirectory).Path
$cli = (Resolve-Path -LiteralPath $MarcCli).Path
$backup = (Resolve-Path -LiteralPath $BackupCli).Path
$manifest = Read-Manifest $bundle
$old = Read-Manifest $baseline
if ($manifest.schema_version -ne 78 -or $manifest.codec_set -ne 'marc-cli-v78') {
    throw 'The final bundle is not schema-78 / marc-cli-v78'
}
if (@($manifest.archives).Count -ne 88 -or @($old.archives).Count -ne 87) {
    throw 'The final and baseline archive counts are not 88 and 87'
}
for ($index = 0; $index -lt 81; ++$index) {
    foreach ($field in @('codec', 'file', 'bytes', 'sha256')) {
        if ($manifest.archives[$index].$field -ne $old.archives[$index].$field) {
            throw "Archive prefix changed at index $index ($field)"
        }
    }
}
$expected = @(
    'lzss-position-distance-tans',
    'lzss-position-distance-tans-1m',
    'lzss-position-distance-tans-4m',
    'lzss-position-distance-tans-8m',
    'lzss-position-distance-tans-16m',
    'lzss-position-distance-tans-32m',
    'lzss-position-distance-tans-64m')
for ($index = 0; $index -lt $expected.Count; ++$index) {
    if ($manifest.archives[81 + $index].codec -ne $expected[$index]) {
        throw "Archive tail order changed at index $($81 + $index)"
    }
}
foreach ($entry in $manifest.archives) {
    $archivePath = Join-Path $bundle $entry.file
    if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
        throw "Missing archive: $($entry.file)"
    }
    if ((Get-Item -LiteralPath $archivePath).Length -ne $entry.bytes -or
        (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
        throw "Archive body mismatch: $($entry.file)"
    }
}
for ($index = 0; $index -lt 81; ++$index) {
    $entry = $old.archives[$index]
    $archivePath = Join-Path $baseline $entry.file
    if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf) -or
        (Get-Item -LiteralPath $archivePath).Length -ne $entry.bytes -or
        (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
        throw "Baseline archive body mismatch: $($entry.file)"
    }
}
[pscustomobject]@{
    passed = $true
    schema_version = $manifest.schema_version
    archive_count = @($manifest.archives).Count
    prefix_count = 81
    appended_codecs = ($expected -join ',')
    release_present = (Test-Path -LiteralPath $cli)
    backup_present = (Test-Path -LiteralPath $backup)
    release_sha256 = (Get-FileHash -LiteralPath $cli -Algorithm SHA256).Hash.ToLowerInvariant()
    backup_sha256 = (Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash.ToLowerInvariant()
} | ConvertTo-Json -Compress
