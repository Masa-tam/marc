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
    'lzmw'
)
$entries = @()
foreach ($profile in $profiles) {
    $archiveName = "$profile.marc"
    $archivePath = Join-Path $resolvedOutput $archiveName
    $decodedPath = Join-Path $resolvedOutput "$profile.decoded"
    Invoke-Marc @('encode', '--codec', $profile, $inputPath, $archivePath)
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
    schema_version = 2
    codec_set = 'marc-cli-v2'
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
