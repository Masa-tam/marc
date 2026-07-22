[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$MarcCli
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Write-Manifest([string]$Path, [object]$Manifest) {
    $json = $Manifest | ConvertTo-Json -Depth 5
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText(
        $Path, $json + [Environment]::NewLine, $utf8)
}

function Convert-Bundle(
    [string]$Source,
    [string]$Destination,
    [int]$SchemaVersion,
    [AllowNull()][string]$CodecSet,
    [string[]]$Profiles) {
    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse
    $manifestPath = Join-Path $Destination 'manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $manifest.schema_version = $SchemaVersion
    if ([string]::IsNullOrEmpty($CodecSet)) {
        $manifest.PSObject.Properties.Remove('codec_set')
    } else {
        $manifest.codec_set = $CodecSet
    }
    $manifest.archives = @(
        $manifest.archives | Where-Object { $Profiles -contains $_.codec })
    Get-ChildItem -LiteralPath $Destination -Filter '*.marc' -File |
        Where-Object { $Profiles -notcontains $_.BaseName } |
        Remove-Item -Force
    Write-Manifest $manifestPath $manifest
}

$resolvedCli = (Resolve-Path -LiteralPath $MarcCli).Path
$root = Join-Path ([System.IO.Path]::GetTempPath()) (
    'marc-interoperability-' + [System.Guid]::NewGuid().ToString('N'))
$schema13 = Join-Path $root 'schema13'
$schema13Reordered = Join-Path $root 'schema13-reordered'
$schema12 = Join-Path $root 'schema12'
$schema11 = Join-Path $root 'schema11'
$schema10 = Join-Path $root 'schema10'
$schema9 = Join-Path $root 'schema9'
$schema8 = Join-Path $root 'schema8'
$schema7 = Join-Path $root 'schema7'
$schema6 = Join-Path $root 'schema6'
$schema5 = Join-Path $root 'schema5'
$schema4 = Join-Path $root 'schema4'
$schema3 = Join-Path $root 'schema3'
$schema2 = Join-Path $root 'schema2'
$schema1 = Join-Path $root 'schema1'
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
$schema4Profiles = $schema3Profiles + @(
    'lzss-blocked-huffman',
    'lz78-blocked-huffman'
)
$schema5Profiles = $schema4Profiles + @('lzw-blocked-huffman')
$schema6Profiles = $schema5Profiles + @('lzd-blocked-huffman')
$schema7Profiles = $schema6Profiles + @('lzmw-blocked-huffman')
$schema8Profiles = $schema7Profiles + @('lz77-adaptive-huffman')
$schema9Profiles = $schema8Profiles + @('lzss-adaptive-huffman')
$schema10Profiles = $schema9Profiles + @('lz78-adaptive-huffman')
$schema11Profiles = $schema10Profiles + @('lzw-adaptive-huffman')
$schema12Profiles = $schema11Profiles + @('lzd-adaptive-huffman')

try {
    $null = New-Item -ItemType Directory -Path $root
    & (Join-Path $PSScriptRoot 'create_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -OutputDirectory $schema13 `
        -Platform 'local-schema-test' `
        -Compiler 'local-schema-test' `
        -SourceRevision ('0' * 40)
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema13 `
        -OutputDirectory (Join-Path $root 'verified13')

    Copy-Item -LiteralPath $schema13 -Destination $schema13Reordered -Recurse
    $reorderedManifestPath = Join-Path $schema13Reordered 'manifest.json'
    $reorderedManifest = Get-Content -LiteralPath $reorderedManifestPath -Raw |
        ConvertFrom-Json
    $firstArchive = $reorderedManifest.archives[0]
    $reorderedManifest.archives[0] = $reorderedManifest.archives[1]
    $reorderedManifest.archives[1] = $firstArchive
    Write-Manifest $reorderedManifestPath $reorderedManifest
    $reorderedRejected = $false
    try {
        & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
            -MarcCli $resolvedCli `
            -BundleDirectory $schema13Reordered `
            -OutputDirectory (Join-Path $root 'verified13-reordered')
    } catch {
        if ($_.Exception.Message -notlike 'Codec is out of schema order*') {
            throw
        }
        $reorderedRejected = $true
    }
    if (-not $reorderedRejected) {
        throw 'Verifier accepted a reordered schema-13 manifest'
    }

    Convert-Bundle $schema13 $schema12 12 'marc-cli-v12' $schema12Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema12 `
        -OutputDirectory (Join-Path $root 'verified12')

    Convert-Bundle $schema12 $schema11 11 'marc-cli-v11' $schema11Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema11 `
        -OutputDirectory (Join-Path $root 'verified11')

    Convert-Bundle $schema11 $schema10 10 'marc-cli-v10' $schema10Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema10 `
        -OutputDirectory (Join-Path $root 'verified10')

    Convert-Bundle $schema10 $schema9 9 'marc-cli-v9' $schema9Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema9 `
        -OutputDirectory (Join-Path $root 'verified9')

    Convert-Bundle $schema9 $schema8 8 'marc-cli-v8' $schema8Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema8 `
        -OutputDirectory (Join-Path $root 'verified8')

    Convert-Bundle $schema8 $schema7 7 'marc-cli-v7' $schema7Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema7 `
        -OutputDirectory (Join-Path $root 'verified7')

    Convert-Bundle $schema7 $schema6 6 'marc-cli-v6' $schema6Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema6 `
        -OutputDirectory (Join-Path $root 'verified6')

    Convert-Bundle $schema6 $schema5 5 'marc-cli-v5' $schema5Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema5 `
        -OutputDirectory (Join-Path $root 'verified5')

    Convert-Bundle $schema5 $schema4 4 'marc-cli-v4' $schema4Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema4 `
        -OutputDirectory (Join-Path $root 'verified4')

    Convert-Bundle $schema4 $schema3 3 'marc-cli-v3' $schema3Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema3 `
        -OutputDirectory (Join-Path $root 'verified3')

    Convert-Bundle $schema3 $schema2 2 'marc-cli-v2' $schema2Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema2 `
        -OutputDirectory (Join-Path $root 'verified2')

    Convert-Bundle $schema3 $schema1 1 $null $legacyProfiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema1 `
        -OutputDirectory (Join-Path $root 'verified1')

    Write-Host 'Verified interoperability schemas 1 through 13'
} finally {
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}
