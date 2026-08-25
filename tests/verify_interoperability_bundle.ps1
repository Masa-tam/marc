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
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try {
            return ([System.BitConverter]::ToString($sha256.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
        }
        finally {
            $sha256.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
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
$schema10Profiles = $schema9Profiles + @('lz78-adaptive-huffman')
$schema11Profiles = $schema10Profiles + @('lzw-adaptive-huffman')
$schema12Profiles = $schema11Profiles + @('lzd-adaptive-huffman')
$schema13Profiles = $schema12Profiles + @('lzmw-adaptive-huffman')
$schema14Profiles = $schema13Profiles + @('lz77-dynamic-range')
$schema15Profiles = $schema14Profiles + @('lzss-dynamic-range')
$schema16Profiles = $schema15Profiles + @('lz78-dynamic-range')
$schema17Profiles = $schema16Profiles + @('lzw-dynamic-range')
$schema18Profiles = $schema17Profiles + @('lzd-dynamic-range')
$schema19Profiles = $schema18Profiles + @('lzmw-dynamic-range')
$schema20Profiles = $schema19Profiles + @('lz77-rans')
$schema21Profiles = $schema20Profiles + @('lzss-rans')
$schema22Profiles = $schema21Profiles + @('lz78-rans')
$schema23Profiles = $schema22Profiles + @('lzw-rans')
$schema24Profiles = $schema23Profiles + @('lzd-rans')
$schema25Profiles = $schema24Profiles + @('lzmw-rans')
$schema26Profiles = $schema25Profiles + @('lz77-tans')
$schema27Profiles = $schema26Profiles + @('lzss-tans')
$schema28Profiles = $schema27Profiles + @('lz78-tans')
$schema29Profiles = $schema28Profiles + @('lzw-tans')
$schema30Profiles = $schema29Profiles + @('lzd-tans')
$schema31Profiles = $schema30Profiles + @('lzmw-tans')
$schema32Profiles = $schema31Profiles + @('lzss-contextual-dynamic-range')
$schema33Profiles = $schema32Profiles + @('lzss-contextual-rans-compact')
$schema34Profiles = $schema33Profiles + @('lzss-contextual-tans')
$schema35Profiles = $schema34Profiles + @('lzss-contextual-blocked-huffman')
$schema36Profiles = $schema35Profiles + @('lzss-contextual-adaptive-huffman')
$schema37Profiles = @(
    $schema36Profiles | ForEach-Object {
        if ($_ -eq 'lzss-contextual-rans-compact') {
            'lzss-contextual-rans'
        } else {
            $_
        }
    })
$schema38Profiles = $schema37Profiles + @(
    'lzss-contextual-dynamic-range-1m')
$schema39Profiles = $schema38Profiles + @(
    'lzss-contextual-rans-1m')
$schema40Profiles = $schema39Profiles + @(
    'lzss-contextual-tans-1m')
$schema41Profiles = $schema40Profiles + @(
    'lzss-contextual-blocked-huffman-1m')
$schema42Profiles = $schema41Profiles + @(
    'lzss-contextual-adaptive-huffman-1m')
$schema43Profiles = $schema42Profiles + @(
    'lzss-contextual-dynamic-range-4m')
$schema44Profiles = $schema43Profiles + @(
    'lzss-contextual-rans-4m')
$schema45Profiles = $schema44Profiles + @(
    'lzss-contextual-tans-4m')
$schema46Profiles = $schema45Profiles + @(
    'lzss-contextual-blocked-huffman-4m')
$schema47Profiles = $schema46Profiles + @(
    'lzss-contextual-adaptive-huffman-4m')
$schema48Profiles = $schema47Profiles + @(
    'lzss-contextual-dynamic-range-16m')
$schema49Profiles = $schema48Profiles + @(
    'lzss-contextual-rans-16m')
$schema50Profiles = $schema49Profiles + @(
    'lzss-contextual-tans-16m')
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
} elseif ($manifest.schema_version -eq 10) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v10') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema10Profiles
} elseif ($manifest.schema_version -eq 11) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v11') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema11Profiles
} elseif ($manifest.schema_version -eq 12) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v12') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema12Profiles
} elseif ($manifest.schema_version -eq 13) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v13') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema13Profiles
} elseif ($manifest.schema_version -eq 14) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v14') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema14Profiles
} elseif ($manifest.schema_version -eq 15) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v15') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema15Profiles
} elseif ($manifest.schema_version -eq 16) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v16') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema16Profiles
} elseif ($manifest.schema_version -eq 17) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v17') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema17Profiles
} elseif ($manifest.schema_version -eq 18) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v18') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema18Profiles
} elseif ($manifest.schema_version -eq 19) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v19') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema19Profiles
} elseif ($manifest.schema_version -eq 20) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v20') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema20Profiles
} elseif ($manifest.schema_version -eq 21) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v21') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema21Profiles
} elseif ($manifest.schema_version -eq 22) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v22') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema22Profiles
} elseif ($manifest.schema_version -eq 23) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v23') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema23Profiles
} elseif ($manifest.schema_version -eq 24) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v24') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema24Profiles
} elseif ($manifest.schema_version -eq 25) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v25') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema25Profiles
} elseif ($manifest.schema_version -eq 26) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v26') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema26Profiles
} elseif ($manifest.schema_version -eq 27) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v27') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema27Profiles
} elseif ($manifest.schema_version -eq 28) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v28') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema28Profiles
} elseif ($manifest.schema_version -eq 29) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v29') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema29Profiles
} elseif ($manifest.schema_version -eq 30) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v30') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema30Profiles
} elseif ($manifest.schema_version -eq 31) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v31') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema31Profiles
} elseif ($manifest.schema_version -eq 32) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v32') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema32Profiles
} elseif ($manifest.schema_version -eq 33) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v33') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema33Profiles
} elseif ($manifest.schema_version -eq 34) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v34') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema34Profiles
} elseif ($manifest.schema_version -eq 35) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v35') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema35Profiles
} elseif ($manifest.schema_version -eq 36) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v36') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema36Profiles
} elseif ($manifest.schema_version -eq 37) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v37') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema37Profiles
} elseif ($manifest.schema_version -eq 38) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v38') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema38Profiles
} elseif ($manifest.schema_version -eq 39) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v39') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema39Profiles
} elseif ($manifest.schema_version -eq 40) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v40') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema40Profiles
} elseif ($manifest.schema_version -eq 41) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v41') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema41Profiles
} elseif ($manifest.schema_version -eq 42) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v42') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema42Profiles
} elseif ($manifest.schema_version -eq 43) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v43') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema43Profiles
} elseif ($manifest.schema_version -eq 44) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v44') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema44Profiles
} elseif ($manifest.schema_version -eq 45) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v45') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema45Profiles
} elseif ($manifest.schema_version -eq 46) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v46') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema46Profiles
} elseif ($manifest.schema_version -eq 47) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v47') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema47Profiles
} elseif ($manifest.schema_version -eq 48) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v48') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema48Profiles
} elseif ($manifest.schema_version -eq 49) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v49') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema49Profiles
} elseif ($manifest.schema_version -eq 50) {
    if ([string]$manifest.codec_set -ne 'marc-cli-v50') {
        throw "Unsupported interoperability codec set: $($manifest.codec_set)"
    }
    $expectedProfiles = $schema50Profiles
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
    $cliCodec = if ($codec -eq 'lzss-contextual-rans-compact') {
        'lzss-contextual-rans'
    } else {
        $codec
    }
    Invoke-Marc @(
        'decode', '--codec', $cliCodec, $archivePath, $decodedPath)
    if (-not (Test-FileBytesEqual $inputPath $decodedPath)) {
        throw "Decoded bytes differ from the fixture: $codec"
    }

    Invoke-Marc @(
        'encode', '--codec', $cliCodec, $inputPath, $reencodedPath)
    if (-not (Test-FileBytesEqual $archivePath $reencodedPath)) {
        throw "Locally re-encoded archive differs: $codec"
    }
    ++$verified
}

Write-Host (
    "Verified {0} archives from {1} ({2}, {3}), revision {4}" -f
    $verified, $manifest.platform, $manifest.compiler, $manifest.architecture,
    $manifest.source_revision)
