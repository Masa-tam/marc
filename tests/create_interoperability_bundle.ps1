[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$MarcCli,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [Parameter(Mandatory = $true)]
    [string]$Platform,

    [Parameter(Mandatory = $true)]
    [string]$Compiler,

    [Parameter(Mandatory = $true)]
    [string]$SourceRevision
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest


function Assert-PositionRans64kHeader([byte[]]$Bytes, [uint64]$OriginalBytes) {
    if ($Bytes.Length -lt 112) { throw '64 KiB position rANS archive header is truncated' }
    $expected = [byte[]]::new(112)
    [System.Text.Encoding]::ASCII.GetBytes('MARC').CopyTo($expected, 0)
    foreach ($field in @(@(4,2,2), @(6,2,0), @(8,2,64), @(10,2,1),
            @(12,2,2), @(14,2,8), @(16,2,4), @(18,2,5), @(20,4,65536),
            @(28,4,16), @(32,4,16), @(40,8,$OriginalBytes), @(48,4,16),
            @(64,4,65536), @(68,4,3), @(72,4,258), @(80,1,12), @(81,1,1),
            @(82,2,40), @(84,4,2522), @(96,2,1), @(98,2,9))) {
        for ($index = 0; $index -lt $field[1]; ++$index) {
            $expected[$field[0] + $index] = [byte](([uint64]$field[2] -shr (8 * $index)) -band 255)
        }
    }
    for ($index = 0; $index -lt 112; ++$index) {
        if ($Bytes[$index] -ne $expected[$index]) {
            throw '64 KiB position rANS archive does not carry exact identity and parameters'
        }
    }
}

function Assert-PositionRans4mHeader([byte[]]$Bytes, [uint64]$OriginalBytes) {
    if ($Bytes.Length -lt 112) { throw '4 MiB position rANS archive header is truncated' }
    $expected = [byte[]]::new(112)
    [System.Text.Encoding]::ASCII.GetBytes('MARC').CopyTo($expected, 0)
    foreach ($field in @(@(4,2,2), @(6,2,0), @(8,2,64), @(10,2,1),
            @(12,2,2), @(14,2,10), @(16,2,4), @(18,2,7), @(20,4,4194304),
            @(28,4,16), @(32,4,16), @(40,8,$OriginalBytes), @(48,4,16),
            @(64,4,4194304), @(68,4,3), @(72,4,258), @(80,1,12), @(81,1,1),
            @(82,2,54), @(84,4,4636), @(96,2,1), @(98,2,16))) {
        for ($index = 0; $index -lt $field[1]; ++$index) {
            $expected[$field[0] + $index] = [byte](([uint64]$field[2] -shr (8 * $index)) -band 255)
        }
    }
    for ($index = 0; $index -lt 112; ++$index) {
        if ($Bytes[$index] -ne $expected[$index]) {
            throw '4 MiB position rANS archive does not carry exact identity and parameters'
        }
    }
}


function Assert-PositionRans8mHeader([byte[]]$Bytes, [uint64]$OriginalBytes) {
    if ($Bytes.Length -lt 112) { throw '8 MiB position rANS archive header is truncated' }
    $expected = [byte[]]::new(112)
    [System.Text.Encoding]::ASCII.GetBytes('MARC').CopyTo($expected, 0)
    foreach ($field in @(@(4,2,2), @(6,2,0), @(8,2,64), @(10,2,1),
            @(12,2,2), @(14,2,11), @(16,2,4), @(18,2,8), @(20,4,8388608),
            @(28,4,16), @(32,4,16), @(40,8,$OriginalBytes), @(48,4,16),
            @(64,4,8388608), @(68,4,3), @(72,4,258), @(80,1,12), @(81,1,1),
            @(82,2,55), @(84,4,4647), @(96,2,1), @(98,2,17))) {
        for ($index = 0; $index -lt $field[1]; ++$index) {
            $expected[$field[0] + $index] = [byte](([uint64]$field[2] -shr (8 * $index)) -band 255)
        }
    }
    for ($index = 0; $index -lt 112; ++$index) {
        if ($Bytes[$index] -ne $expected[$index]) {
            throw '8 MiB position rANS archive does not carry exact identity and parameters'
        }
    }
}

function Assert-PositionRans16mHeader([byte[]]$Bytes, [uint64]$OriginalBytes) {
    if ($Bytes.Length -lt 112) { throw '16 MiB position rANS archive header is truncated' }
    $expected = [byte[]]::new(112)
    [System.Text.Encoding]::ASCII.GetBytes('MARC').CopyTo($expected, 0)
    foreach ($field in @(@(4,2,2), @(6,2,0), @(8,2,64), @(10,2,1),
            @(12,2,2), @(14,2,12), @(16,2,4), @(18,2,9), @(20,4,16777216),
            @(28,4,16), @(32,4,16), @(40,8,$OriginalBytes), @(48,4,16),
            @(64,4,16777216), @(68,4,3), @(72,4,258), @(80,1,12), @(81,1,1),
            @(82,2,56), @(84,4,4658), @(96,2,1), @(98,2,18))) {
        for ($index = 0; $index -lt $field[1]; ++$index) {
            $expected[$field[0] + $index] = [byte](([uint64]$field[2] -shr (8 * $index)) -band 255)
        }
    }
    for ($index = 0; $index -lt 112; ++$index) {
        if ($Bytes[$index] -ne $expected[$index]) {
            throw '16 MiB position rANS archive does not carry exact identity and parameters'
        }
    }
}



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

function Invoke-Marc([string[]]$Arguments) {
    & $script:resolvedCli @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "marc failed with exit code ${LASTEXITCODE}: $($Arguments -join ' ')"
    }
}

$resolvedCli = (Resolve-Path -LiteralPath $MarcCli).Path
if (Test-Path -LiteralPath $OutputDirectory) {
    throw "Output directory already exists: $OutputDirectory"
}
$null = New-Item -ItemType Directory -Path $OutputDirectory
$resolvedOutput = (Resolve-Path -LiteralPath $OutputDirectory).Path

$fixture = [byte[]]::new(8193)
$pattern = [byte[]](0x41, 0x42, 0x41, 0x42, 0x58, 0x00, 0xff)
for ($index = 0; $index -lt $fixture.Length; ++$index) {
    if ($index -lt 256) {
        $fixture[$index] = [byte]$index
    } elseif ($index -lt 1280) {
        $fixture[$index] = 0
    } elseif ($index -lt 3328) {
        $fixture[$index] = $pattern[($index - 1280) % $pattern.Length]
    } else {
        $fixture[$index] = [byte](
            (($index * 73) + (($index -shr 3) * 19) + 41) -band 0xff)
    }
}

$inputPath = Join-Path $resolvedOutput 'input.bin'
[System.IO.File]::WriteAllBytes($inputPath, $fixture)

$profiles = @(
    'checksum-raw',
    'lz77',
    'lz77-blocked-huffman',
    'lzss',
    'lz78',
    'lzw',
    'lzd',
    'lzmw',
    'blocked-huffman',
    'adaptive-huffman',
    'dynamic-range',
    'rans',
    'tans',
    'lzss-blocked-huffman',
    'lz78-blocked-huffman',
    'lzw-blocked-huffman',
    'lzd-blocked-huffman',
    'lzmw-blocked-huffman',
    'lz77-adaptive-huffman',
    'lzss-adaptive-huffman',
    'lz78-adaptive-huffman',
    'lzw-adaptive-huffman',
    'lzd-adaptive-huffman',
    'lzmw-adaptive-huffman',
    'lz77-dynamic-range',
    'lzss-dynamic-range',
    'lz78-dynamic-range',
    'lzw-dynamic-range',
    'lzd-dynamic-range',
    'lzmw-dynamic-range',
    'lz77-rans',
    'lzss-rans',
    'lz78-rans',
    'lzw-rans',
    'lzd-rans',
    'lzmw-rans',
    'lz77-tans',
    'lzss-tans',
    'lz78-tans',
    'lzw-tans',
    'lzd-tans',
    'lzmw-tans',
    'lzss-contextual-dynamic-range',
    'lzss-contextual-rans',
    'lzss-contextual-tans',
    'lzss-contextual-blocked-huffman',
    'lzss-contextual-adaptive-huffman',
    'lzss-contextual-dynamic-range-1m',
    'lzss-contextual-rans-1m',
    'lzss-contextual-tans-1m',
    'lzss-contextual-blocked-huffman-1m',
    'lzss-contextual-adaptive-huffman-1m',
    'lzss-contextual-dynamic-range-4m',
    'lzss-contextual-rans-4m',
    'lzss-contextual-tans-4m',
    'lzss-contextual-blocked-huffman-4m',
    'lzss-contextual-adaptive-huffman-4m',
    'lzss-contextual-dynamic-range-16m',
    'lzss-contextual-rans-16m',
    'lzss-contextual-tans-16m',
    'lzss-contextual-blocked-huffman-16m',
    'lzss-contextual-adaptive-huffman-16m',
    'lzss-contextual-dynamic-range-64m',
    'lzss-contextual-rans-64m',
    'lzss-contextual-tans-64m',
    'lzss-contextual-blocked-huffman-64m',
    'lzss-contextual-adaptive-huffman-64m',
    'lzss-position-distance-dynamic-range',
    'lzss-position-distance-dynamic-range-1m',
    'lzss-position-distance-dynamic-range-4m',
    'lzss-position-distance-dynamic-range-8m',
    'lzss-position-distance-dynamic-range-16m',
    'lzss-position-distance-dynamic-range-32m',
    'lzss-position-distance-dynamic-range-64m',
    'lzss-position-distance-rans-1m',
    'lzss-position-distance-rans',
    'lzss-position-distance-rans-4m',
    'lzss-position-distance-rans-8m',
    'lzss-position-distance-rans-16m'
)
$entries = @()
foreach ($profile in $profiles) {
    $archiveName = "$profile.marc"
    $archivePath = Join-Path $resolvedOutput $archiveName
    $decodedPath = Join-Path $resolvedOutput "$profile.decoded"
    Invoke-Marc @('encode', '--codec', $profile, $inputPath, $archivePath)
    if ($profile -eq 'lzss-position-distance-dynamic-range') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -lt 112) {
            throw 'Position-distance archive header is truncated'
        }
        foreach ($field in @(@(4, 2), @(6, 0), @(12, 2), @(14, 8),
                @(16, 3), @(18, 2), @(96, 1), @(98, 9))) {
            if ($archiveBytes[$field[0]] -ne $field[1] -or
                    $archiveBytes[$field[0] + 1] -ne 0) {
                throw 'Position-distance archive does not carry exact identity 2.0: 2/8 + 1/9 + 3/2'
            }
        }
    }
    if ($profile -eq 'lzss-position-distance-dynamic-range-1m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -lt 112) {
            throw '1 MiB position-distance archive header is truncated'
        }
        foreach ($field in @(@(4, 2), @(6, 0), @(12, 2), @(14, 9),
                @(16, 3), @(18, 2), @(96, 1), @(98, 10))) {
            if ($archiveBytes[$field[0]] -ne $field[1] -or
                    $archiveBytes[$field[0] + 1] -ne 0) {
                throw '1 MiB position-distance archive does not carry exact identity 2.0: 2/9 + 1/10 + 3/2'
            }
        }
    }
    if ($profile -eq 'lzss-position-distance-dynamic-range-4m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -lt 112) {
            throw '4 MiB position-distance archive header is truncated'
        }
        foreach ($field in @(@(4, 2), @(6, 0), @(12, 2), @(14, 10),
                @(16, 3), @(18, 2), @(96, 1), @(98, 11))) {
            if ($archiveBytes[$field[0]] -ne $field[1] -or
                    $archiveBytes[$field[0] + 1] -ne 0) {
                throw '4 MiB position-distance archive does not carry exact identity 2.0: 2/10 + 1/11 + 3/2'
            }
        }
    }
    if ($profile -eq 'lzss-position-distance-dynamic-range-8m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -lt 112) {
            throw '8 MiB position-distance archive header is truncated'
        }
        foreach ($field in @(@(4, 2), @(6, 0), @(12, 2), @(14, 11),
                @(16, 3), @(18, 2), @(96, 1), @(98, 12))) {
            if ($archiveBytes[$field[0]] -ne $field[1] -or
                    $archiveBytes[$field[0] + 1] -ne 0) {
                throw '8 MiB position-distance archive does not carry exact identity 2.0: 2/11 + 1/12 + 3/2'
            }
        }
    }
    if ($profile -eq 'lzss-position-distance-dynamic-range-16m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -lt 112) {
            throw '16 MiB position-distance archive header is truncated'
        }
        foreach ($field in @(@(4, 2), @(6, 0), @(12, 2), @(14, 12),
                @(16, 3), @(18, 2), @(96, 1), @(98, 13))) {
            if ($archiveBytes[$field[0]] -ne $field[1] -or
                    $archiveBytes[$field[0] + 1] -ne 0) {
                throw '16 MiB position-distance archive does not carry exact identity 2.0: 2/12 + 1/13 + 3/2'
            }
        }
    }
    if ($profile -eq 'lzss-position-distance-dynamic-range-32m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -lt 112) {
            throw '32 MiB position-distance archive header is truncated'
        }
        foreach ($field in @(@(4, 2), @(6, 0), @(12, 2), @(14, 13),
                @(16, 3), @(18, 2), @(96, 1), @(98, 14))) {
            if ($archiveBytes[$field[0]] -ne $field[1] -or
                    $archiveBytes[$field[0] + 1] -ne 0) {
                throw '32 MiB position-distance archive does not carry exact identity 2.0: 2/13 + 1/14 + 3/2'
            }
        }
    }
    if ($profile -eq 'lzss-position-distance-dynamic-range-64m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -lt 112) {
            throw '64 MiB position-distance archive header is truncated'
        }
        foreach ($field in @(@(4, 2), @(6, 0), @(12, 2), @(14, 14),
                @(16, 3), @(18, 2), @(84, 50), @(96, 1), @(98, 15))) {
            if ($archiveBytes[$field[0]] -ne $field[1] -or
                    $archiveBytes[$field[0] + 1] -ne 0) {
                throw '64 MiB position-distance archive does not carry exact identity 2.0: 2/14 + 1/15 + 3/2, fifty contexts'
            }
        }
    }
    if ($profile -eq 'lzss-position-distance-rans-8m') {
        Assert-PositionRans8mHeader ([System.IO.File]::ReadAllBytes($archivePath)) ([uint64]$fixture.Length)
    }
    if ($profile -eq 'lzss-position-distance-rans-16m') {
        Assert-PositionRans16mHeader ([System.IO.File]::ReadAllBytes($archivePath)) ([uint64]$fixture.Length)
    }
    if ($profile -eq 'lzss-position-distance-rans-4m') {
        Assert-PositionRans4mHeader ([System.IO.File]::ReadAllBytes($archivePath)) ([uint64]$fixture.Length)
    }
    if ($profile -eq 'lzss-position-distance-rans') {
        Assert-PositionRans64kHeader ([System.IO.File]::ReadAllBytes($archivePath)) ([uint64]$fixture.Length)
    }
    if ($profile -eq 'lzss-position-distance-rans-1m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -lt 112) { throw 'Position rANS archive header is truncated' }
        if ($archiveBytes[80] -ne 12 -or $archiveBytes[81] -ne 1 -or
                $archiveBytes[84] -ne 6 -or $archiveBytes[85] -ne 10 -or
                $archiveBytes[86] -ne 0 -or $archiveBytes[87] -ne 0) {
            throw 'Position rANS archive does not carry exact identity parameters'
        }
        foreach ($field in @(@(4, 2), @(6, 0), @(12, 2), @(14, 9),
                @(16, 4), @(18, 4), @(82, 44), @(96, 1), @(98, 10))) {
            if ($archiveBytes[$field[0]] -ne $field[1] -or $archiveBytes[$field[0] + 1] -ne 0) {
                throw 'Position rANS archive does not carry exact identity 2.0: 2/9 + 1/10 + 4/4, forty-four contexts'
            }
        }
    }
    if ($profile -eq 'lzss-contextual-dynamic-range-1m' -or
            $profile -eq 'lzss-contextual-rans-1m' -or
            $profile -eq 'lzss-contextual-tans-1m' -or
            $profile -eq 'lzss-contextual-blocked-huffman-1m' -or
            $profile -eq 'lzss-contextual-adaptive-huffman-1m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 3 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[98] -ne 2 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry dictionary/context variants 3/2"
        }
        if ($profile -eq 'lzss-contextual-tans-1m' -and
                ($archiveBytes[16] -ne 5 -or
                 $archiveBytes[17] -ne 0 -or
                 $archiveBytes[18] -ne 2 -or
                 $archiveBytes[19] -ne 0)) {
            throw "$profile archive does not carry entropy identity 5/2"
        }
        if ($profile -eq 'lzss-contextual-blocked-huffman-1m' -and
                ($archiveBytes[16] -ne 2 -or
                 $archiveBytes[17] -ne 0 -or
                 $archiveBytes[18] -ne 2 -or
                 $archiveBytes[19] -ne 0)) {
            throw "$profile archive does not carry entropy identity 2/2"
        }
        if ($profile -eq 'lzss-contextual-adaptive-huffman-1m' -and
                ($archiveBytes[16] -ne 1 -or
                 $archiveBytes[17] -ne 0 -or
                 $archiveBytes[18] -ne 2 -or
                 $archiveBytes[19] -ne 0)) {
            throw "$profile archive does not carry entropy identity 1/2"
        }
    }
    if ($profile -eq 'lzss-contextual-dynamic-range-4m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 4 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 3 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 2 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 3 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/4 + 1/3 + 3/2"
        }
    }
    if ($profile -eq 'lzss-contextual-rans-4m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 4 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 4 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 3 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 3 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/4 + 1/3 + 4/3"
        }
    }
    if ($profile -eq 'lzss-contextual-tans-4m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 4 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 5 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 2 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 3 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/4 + 1/3 + 5/2"
        }
    }
    if ($profile -eq 'lzss-contextual-blocked-huffman-4m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 4 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 2 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 2 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 3 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/4 + 1/3 + 2/2"
        }
    }
    if ($profile -eq 'lzss-contextual-adaptive-huffman-4m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 4 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 1 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 2 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 3 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/4 + 1/3 + 1/2"
        }
    }
    if ($profile -eq 'lzss-contextual-dynamic-range-16m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 5 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 3 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 2 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 4 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/5 + 1/4 + 3/2"
        }
    }
    if ($profile -eq 'lzss-contextual-rans-16m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 5 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 4 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 3 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 4 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/5 + 1/4 + 4/3"
        }
    }
    if ($profile -eq 'lzss-contextual-tans-16m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 5 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 5 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 2 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 4 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/5 + 1/4 + 5/2"
        }
    }
    if ($profile -eq 'lzss-contextual-blocked-huffman-16m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 5 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 2 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 2 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 4 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/5 + 1/4 + 2/2"
        }
    }
    if ($profile -eq 'lzss-contextual-adaptive-huffman-16m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 5 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 1 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 2 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 4 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/5 + 1/4 + 1/2"
        }
    }
    if ($profile -eq 'lzss-contextual-dynamic-range-64m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 6 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 3 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 2 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 5 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/6 + 1/5 + 3/2"
        }
    }
    if ($profile -eq 'lzss-contextual-rans-64m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 6 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 4 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 3 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 5 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/6 + 1/5 + 4/3"
        }
    }
    if ($profile -eq 'lzss-contextual-tans-64m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 6 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 5 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 2 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 5 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/6 + 1/5 + 5/2"
        }
    }
    if ($profile -eq 'lzss-contextual-blocked-huffman-64m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 6 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 2 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 2 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 5 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/6 + 1/5 + 2/2"
        }
    }
    if ($profile -eq 'lzss-contextual-adaptive-huffman-64m') {
        $archiveBytes = [System.IO.File]::ReadAllBytes($archivePath)
        if ($archiveBytes.Length -le 98 -or
                $archiveBytes[14] -ne 6 -or
                $archiveBytes[15] -ne 0 -or
                $archiveBytes[16] -ne 1 -or
                $archiveBytes[17] -ne 0 -or
                $archiveBytes[18] -ne 2 -or
                $archiveBytes[19] -ne 0 -or
                $archiveBytes[98] -ne 5 -or
                $archiveBytes[99] -ne 0) {
            throw "$profile archive does not carry exact identity 2/6 + 1/5 + 1/2"
        }
    }
    Invoke-Marc @('decode', '--codec', $profile, $archivePath, $decodedPath)
    if (-not (Test-FileBytesEqual $inputPath $decodedPath)) {
        throw "Generated archive did not round trip: $profile"
    }
    Remove-Item -LiteralPath $decodedPath
    $archive = Get-Item -LiteralPath $archivePath
    $entries += [ordered]@{
        codec = $profile
        file = $archiveName
        bytes = [int64]$archive.Length
        sha256 = Get-Sha256 $archivePath
    }
}

$manifest = [ordered]@{
    schema_version = 69
    codec_set = 'marc-cli-v69'
    source_revision = $SourceRevision
    platform = $Platform
    compiler = $Compiler
    os = [System.Runtime.InteropServices.RuntimeInformation]::OSDescription
    architecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
    cli_sha256 = Get-Sha256 $resolvedCli
    input = [ordered]@{
        file = 'input.bin'
        bytes = [int64]$fixture.Length
        sha256 = Get-Sha256 $inputPath
    }
    archives = $entries
}

$json = $manifest | ConvertTo-Json -Depth 5
$utf8 = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText(
    (Join-Path $resolvedOutput 'manifest.json'),
    $json + [Environment]::NewLine,
    $utf8)

Write-Host "Created interoperability bundle: $resolvedOutput"
