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
$schema33 = Join-Path $root 'schema33'
$schema33Reordered = Join-Path $root 'schema33-reordered'
$schema32 = Join-Path $root 'schema32'
$schema31 = Join-Path $root 'schema31'
$schema30 = Join-Path $root 'schema30'
$schema29 = Join-Path $root 'schema29'
$schema28 = Join-Path $root 'schema28'
$schema27 = Join-Path $root 'schema27'
$schema26 = Join-Path $root 'schema26'
$schema25 = Join-Path $root 'schema25'
$schema24 = Join-Path $root 'schema24'
$schema23 = Join-Path $root 'schema23'
$schema22 = Join-Path $root 'schema22'
$schema21 = Join-Path $root 'schema21'
$schema20 = Join-Path $root 'schema20'
$schema19 = Join-Path $root 'schema19'
$schema18 = Join-Path $root 'schema18'
$schema17 = Join-Path $root 'schema17'
$schema16 = Join-Path $root 'schema16'
$schema15 = Join-Path $root 'schema15'
$schema14 = Join-Path $root 'schema14'
$schema13 = Join-Path $root 'schema13'
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

try {
    $null = New-Item -ItemType Directory -Path $root
    & (Join-Path $PSScriptRoot 'create_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -OutputDirectory $schema33 `
        -Platform 'local-schema-test' `
        -Compiler 'local-schema-test' `
        -SourceRevision ('0' * 40)
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema33 `
        -OutputDirectory (Join-Path $root 'verified33')

    Copy-Item -LiteralPath $schema33 -Destination $schema33Reordered -Recurse
    $reorderedManifestPath = Join-Path $schema33Reordered 'manifest.json'
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
            -BundleDirectory $schema33Reordered `
            -OutputDirectory (Join-Path $root 'verified33-reordered')
    } catch {
        if ($_.Exception.Message -notlike 'Codec is out of schema order*') {
            throw
        }
        $reorderedRejected = $true
    }
    if (-not $reorderedRejected) {
        throw 'Verifier accepted a reordered schema-33 manifest'
    }

    Convert-Bundle $schema33 $schema32 32 'marc-cli-v32' $schema32Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema32 `
        -OutputDirectory (Join-Path $root 'verified32')

    Convert-Bundle $schema32 $schema31 31 'marc-cli-v31' $schema31Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema31 `
        -OutputDirectory (Join-Path $root 'verified31')

    Convert-Bundle $schema31 $schema30 30 'marc-cli-v30' $schema30Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema30 `
        -OutputDirectory (Join-Path $root 'verified30')

    Convert-Bundle $schema30 $schema29 29 'marc-cli-v29' $schema29Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema29 `
        -OutputDirectory (Join-Path $root 'verified29')

    Convert-Bundle $schema29 $schema28 28 'marc-cli-v28' $schema28Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema28 `
        -OutputDirectory (Join-Path $root 'verified28')

    Convert-Bundle $schema28 $schema27 27 'marc-cli-v27' $schema27Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema27 `
        -OutputDirectory (Join-Path $root 'verified27')

    Convert-Bundle $schema27 $schema26 26 'marc-cli-v26' $schema26Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema26 `
        -OutputDirectory (Join-Path $root 'verified26')

    Convert-Bundle $schema26 $schema25 25 'marc-cli-v25' $schema25Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema25 `
        -OutputDirectory (Join-Path $root 'verified25')

    Convert-Bundle $schema25 $schema24 24 'marc-cli-v24' $schema24Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema24 `
        -OutputDirectory (Join-Path $root 'verified24')

    Convert-Bundle $schema24 $schema23 23 'marc-cli-v23' $schema23Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema23 `
        -OutputDirectory (Join-Path $root 'verified23')

    Convert-Bundle $schema23 $schema22 22 'marc-cli-v22' $schema22Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema22 `
        -OutputDirectory (Join-Path $root 'verified22')

    Convert-Bundle $schema22 $schema21 21 'marc-cli-v21' $schema21Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema21 `
        -OutputDirectory (Join-Path $root 'verified21')

    Convert-Bundle $schema21 $schema20 20 'marc-cli-v20' $schema20Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema20 `
        -OutputDirectory (Join-Path $root 'verified20')

    Convert-Bundle $schema20 $schema19 19 'marc-cli-v19' $schema19Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema19 `
        -OutputDirectory (Join-Path $root 'verified19')

    Convert-Bundle $schema19 $schema18 18 'marc-cli-v18' $schema18Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema18 `
        -OutputDirectory (Join-Path $root 'verified18')

    Convert-Bundle $schema18 $schema17 17 'marc-cli-v17' $schema17Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema17 `
        -OutputDirectory (Join-Path $root 'verified17')

    Convert-Bundle $schema17 $schema16 16 'marc-cli-v16' $schema16Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema16 `
        -OutputDirectory (Join-Path $root 'verified16')

    Convert-Bundle $schema16 $schema15 15 'marc-cli-v15' $schema15Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema15 `
        -OutputDirectory (Join-Path $root 'verified15')

    Convert-Bundle $schema15 $schema14 14 'marc-cli-v14' $schema14Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema14 `
        -OutputDirectory (Join-Path $root 'verified14')

    Convert-Bundle $schema14 $schema13 13 'marc-cli-v13' $schema13Profiles
    & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -BundleDirectory $schema13 `
        -OutputDirectory (Join-Path $root 'verified13')

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

    Write-Host 'Verified interoperability schemas 1 through 33'
} finally {
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}
