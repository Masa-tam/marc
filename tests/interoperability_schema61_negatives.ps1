[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$MarcCli,
    [Parameter(Mandatory = $true)][string]$BundleDirectory,
    [Parameter(Mandatory = $true)][string]$EvidenceDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (Test-Path -LiteralPath $EvidenceDirectory) { throw 'Evidence directory already exists' }
$null = New-Item -ItemType Directory -Path $EvidenceDirectory
$source = (Resolve-Path -LiteralPath $BundleDirectory).Path
$root = (Resolve-Path -LiteralPath $EvidenceDirectory).Path
$baseline = Get-Content -LiteralPath (Join-Path $source 'manifest.json') -Raw | ConvertFrom-Json
if ($baseline.schema_version -ne 61 -or @($baseline.archives).Count -ne 71) {
    throw 'Negative tests require a complete schema-61 source bundle'
}
$cases = @('downgrade', 'codec-set', 'missing', 'duplicate', 'order', 'archive-hash',
    'archive-size', 'input-hash', 'identity-dictionary', 'identity-context', 'header-truncated')
foreach ($offset in @(4, 6, 12, 14, 16, 18, 96, 98)) {
    $cases += "identity-byte-$offset"
    $cases += "identity-byte-$($offset + 1)"
}
$receipts = @()
foreach ($case in $cases) {
    $bundle = Join-Path $root $case
    Copy-Item -LiteralPath $source -Destination $bundle -Recurse
    $path = Join-Path $bundle 'manifest.json'
    $manifest = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    $expected = ''
    switch ($case) {
        'downgrade' {
            $manifest.schema_version = 60; $manifest.codec_set = 'marc-cli-v60'
            $expected = 'Interoperability manifest must contain exactly 70 archives'
        }
        'codec-set' { $manifest.codec_set = 'marc-cli-v60'; $expected = 'Unsupported interoperability codec set*' }
        'missing' { $manifest.archives = @($manifest.archives[0..69]); $expected = 'Interoperability manifest must contain exactly 71 archives' }
        'duplicate' { $manifest.archives[70] = $manifest.archives[69]; $expected = 'Unknown or duplicate codec*' }
        'order' {
            $last = $manifest.archives[70]; $manifest.archives[70] = $manifest.archives[69]; $manifest.archives[69] = $last
            $expected = 'Codec is out of schema order*'
        }
        'archive-hash' { $manifest.archives[70].sha256 = '0' * 64; $expected = 'Archive size or SHA-256 does not match*' }
        'archive-size' { $manifest.archives[70].bytes += 1; $expected = 'Archive size or SHA-256 does not match*' }
        'input-hash' { $manifest.input.sha256 = '0' * 64; $expected = 'Input size or SHA-256 does not match*' }
        default {
            $archive = Join-Path $bundle $manifest.archives[70].file
            $bytes = [System.IO.File]::ReadAllBytes($archive)
            if ($case -eq 'identity-dictionary') { $bytes[14] = 10 }
            elseif ($case -eq 'identity-context') { $bytes[98] = 11 }
            elseif ($case -eq 'header-truncated') { $bytes = [byte[]]$bytes[0..110] }
            else {
                $offset = [int]$case.Substring('identity-byte-'.Length)
                $bytes[$offset] = $bytes[$offset] -bxor 1
            }
            [System.IO.File]::WriteAllBytes($archive, $bytes)
            $manifest.archives[70].bytes = $bytes.Length
            $manifest.archives[70].sha256 = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
            $expected = if ($case -eq 'header-truncated') { '8 MiB position-distance archive header is truncated' }
                else { '8 MiB position-distance archive does not carry exact identity*' }
        }
    }
    [System.IO.File]::WriteAllText($path, ($manifest | ConvertTo-Json -Depth 5), [System.Text.UTF8Encoding]::new($false))
    $output = Join-Path $root "verified-$case"
    $rejected = $false
    try {
        & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
            -MarcCli $MarcCli -BundleDirectory $bundle -OutputDirectory $output
    } catch {
        if ($_.Exception.Message -notlike $expected) { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw "Verifier accepted schema-61 negative: $case" }
    if (@(Get-ChildItem -LiteralPath $output -File -Recurse).Count -ne 0) {
        throw "Verifier published output before complete manifest admission: $case"
    }
    $receipts += [ordered]@{ case = $case; rejected = $true; output_files = 0; expected = $expected }
}
[System.IO.File]::WriteAllText((Join-Path $root 'qualification.json'),
    ([ordered]@{ passed = $true; cases = $receipts } | ConvertTo-Json -Depth 5),
    [System.Text.UTF8Encoding]::new($false))
Write-Host "Rejected $($cases.Count) schema-61 negatives before codec launch"
