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

try {
    $null = New-Item -ItemType Directory -Path $root
    & (Join-Path $PSScriptRoot 'create_interoperability_bundle.ps1') `
        -MarcCli $resolvedCli `
        -OutputDirectory $schema5 `
        -Platform 'local-schema-test' `
        -Compiler 'local-schema-test' `
        -SourceRevision ('0' * 40)
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

    Write-Host 'Verified interoperability schemas 1, 2, 3, 4, and 5'
} finally {
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}
