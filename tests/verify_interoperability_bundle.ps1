[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$MarcCli,

    [Parameter(Mandatory = $true)]
    [string]$BundleDirectory,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Test-FileBytesEqual([string]$Left, [string]$Right) {
    $leftBytes = [System.IO.File]::ReadAllBytes($Left)
    $rightBytes = [System.IO.File]::ReadAllBytes($Right)
    if ($leftBytes.Length -ne $rightBytes.Length) {
        return $false
    }
    for ($index = 0; $index -lt $leftBytes.Length; ++$index) {
        if ($leftBytes[$index] -ne $rightBytes[$index]) {
            return $false
        }
    }
    return $true
}

function Assert-LeafName([string]$Name) {
    if ([System.IO.Path]::GetFileName($Name) -ne $Name) {
        throw "Manifest file name is not a leaf name: $Name"
    }
}

function Invoke-Marc([string[]]$Arguments) {
    & $script:resolvedCli @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "marc failed with exit code ${LASTEXITCODE}: $($Arguments -join ' ')"
    }
}

$resolvedCli = (Resolve-Path -LiteralPath $MarcCli).Path
$resolvedBundle = (Resolve-Path -LiteralPath $BundleDirectory).Path
if (Test-Path -LiteralPath $OutputDirectory) {
    throw "Output directory already exists: $OutputDirectory"
}
$null = New-Item -ItemType Directory -Path $OutputDirectory
$resolvedOutput = (Resolve-Path -LiteralPath $OutputDirectory).Path

$manifestPath = Join-Path $resolvedBundle 'manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$legacyProfiles = @(
    'lz77',
    'lz77-blocked-huffman',
    'lzss',
    'lz78',
    'lzw',
    'lzd',
    'lzmw'
)
$schema2Profiles = @('checksum-raw') + $legacyProfiles
$entropyProfiles = @(
    'blocked-huffman',
    'adaptive-huffman',
    'dynamic-range',
    'rans',
    'tans'
)
$schema3Profiles = $schema2Profiles + $entropyProfiles
$compositionProfiles = @(
    'lzss-blocked-huffman',
    'lz78-blocked-huffman'
)
$schema4Profiles = $schema3Profiles + $compositionProfiles
$schema5Profiles = $schema4Profiles + @('lzw-blocked-huffman')
$schema6Profiles = $schema5Profiles + @('lzd-blocked-huffman')
$schema7Profiles = $schema6Profiles + @('lzmw-blocked-huffman')
$schema8Profiles = $schema7Profiles + @('lz77-adaptive-huffman')
$schema9Profiles = $schema8Profiles + @('lzss-adaptive-huffman')
if ($manifest.schema_version -eq 1) {
    if ($null -ne $manifest.PSObject.Properties['codec_set']) {
        throw 'Schema 1 interoperability manifests must not declare a codec set'
    }
    $expectedProfiles = $legacyProfiles
} elseif ($manifest.schema_version -eq 2) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v2') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema2Profiles
} elseif ($manifest.schema_version -eq 3) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v3') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema3Profiles
} elseif ($manifest.schema_version -eq 4) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v4') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema4Profiles
} elseif ($manifest.schema_version -eq 5) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v5') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema5Profiles
} elseif ($manifest.schema_version -eq 6) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v6') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema6Profiles
} elseif ($manifest.schema_version -eq 7) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v7') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema7Profiles
} elseif ($manifest.schema_version -eq 8) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v8') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema8Profiles
} elseif ($manifest.schema_version -eq 9) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v9') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema9Profiles
} else {
    throw "Unsupported interoperability manifest version: $($manifest.schema_version)"
}
if (-not [System.Text.RegularExpressions.Regex]::IsMatch(
        [string]$manifest.source_revision,
        '^(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64})$')) {
    throw 'Manifest source revision is not a full Git object ID'
}

if (@($manifest.archives).Count -ne $expectedProfiles.Count) {
    throw "Interoperability manifest must contain exactly $($expectedProfiles.Count) archives"
}

Assert-LeafName $manifest.input.file
$inputPath = Join-Path $resolvedBundle $manifest.input.file
$input = Get-Item -LiteralPath $inputPath
if ($input.Length -ne $manifest.input.bytes -or
        (Get-Sha256 $inputPath) -ne $manifest.input.sha256) {
    throw 'Input size or SHA-256 does not match the manifest'
}

$verified = 0
$seenProfiles = @{}
foreach ($entry in $manifest.archives) {
    $codec = [string]$entry.codec
    if ($expectedProfiles -notcontains $codec -or
            $seenProfiles.ContainsKey($codec)) {
        throw "Unknown or duplicate codec in manifest: $codec"
    }
    if ($codec -ne $expectedProfiles[$verified]) {
        throw "Codec is out of schema order at archive ${verified}: $codec"
    }
    $seenProfiles[$codec] = $true
    Assert-LeafName $entry.file
    $archivePath = Join-Path $resolvedBundle $entry.file
    $archive = Get-Item -LiteralPath $archivePath
    if ($archive.Length -ne $entry.bytes -or
            (Get-Sha256 $archivePath) -ne $entry.sha256) {
        throw "Archive size or SHA-256 does not match: $codec"
    }

    $decodedPath = Join-Path $resolvedOutput "$codec.decoded"
    $reencodedPath = Join-Path $resolvedOutput "$codec.marc"
    Invoke-Marc @(
        'decode', '--codec', $codec, $archivePath, $decodedPath)
    if (-not (Test-FileBytesEqual $inputPath $decodedPath)) {
        throw "Decoded bytes differ from the fixture: $codec"
    }

    Invoke-Marc @(
        'encode', '--codec', $codec, $inputPath, $reencodedPath)
    if (-not (Test-FileBytesEqual $archivePath $reencodedPath)) {
        throw "Locally re-encoded archive differs: $codec"
    }
    ++$verified
}

Write-Host (
    "Verified {0} archives from {1} ({2}, {3}), revision {4}" -f
    $verified, $manifest.platform, $manifest.compiler, $manifest.architecture,
    $manifest.source_revision)
