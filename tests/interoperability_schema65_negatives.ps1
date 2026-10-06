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
$null = Resolve-Path -LiteralPath $MarcCli
$launchMarker = Join-Path $root 'unexpected-codec-launch.txt'
$launchGuard = Join-Path $root 'codec-must-not-run.ps1'
$escapedMarker = $launchMarker.Replace("'", "''")
[System.IO.File]::WriteAllText($launchGuard,
    "[System.IO.File]::WriteAllText('$escapedMarker', 'unexpected')`nthrow 'Codec launched before manifest admission'",
    [System.Text.UTF8Encoding]::new($false))
$baseline = Get-Content -LiteralPath (Join-Path $source 'manifest.json') -Raw | ConvertFrom-Json
if ($baseline.schema_version -ne 65 -or @($baseline.archives).Count -ne 75) {
    throw 'Negative tests require a complete schema-65 source bundle'
}
$cases = @('old-name', 'downgrade', 'codec-set', 'missing', 'duplicate', 'order', 'archive-hash',
    'archive-size', 'input-hash', 'identity-dictionary', 'identity-context', 'identity-context-count', 'header-truncated')
foreach ($offset in @(4, 6, 12, 14, 16, 18, 80, 82, 84, 86, 96, 98)) {
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
        'old-name' {
            $manifest.archives[74].codec = 'lzss-position-rans-1m'
            $expected = 'Unknown or duplicate codec*'
        }
        'downgrade' {
            $manifest.schema_version = 64; $manifest.codec_set = 'marc-cli-v64'
            $expected = 'Interoperability manifest must contain exactly 74 archives'
        }
        'codec-set' { $manifest.codec_set = 'marc-cli-v64'; $expected = 'Unsupported interoperability codec set*' }
        'missing' { $manifest.archives = @($manifest.archives[0..73]); $expected = 'Interoperability manifest must contain exactly 75 archives' }
        'duplicate' { $manifest.archives[74] = $manifest.archives[73]; $expected = 'Unknown or duplicate codec*' }
        'order' {
            $last = $manifest.archives[74]; $manifest.archives[74] = $manifest.archives[73]; $manifest.archives[73] = $last
            $expected = 'Codec is out of schema order*'
        }
        'archive-hash' { $manifest.archives[74].sha256 = '0' * 64; $expected = 'Archive size or SHA-256 does not match*' }
        'archive-size' { $manifest.archives[74].bytes += 1; $expected = 'Archive size or SHA-256 does not match*' }
        'input-hash' { $manifest.input.sha256 = '0' * 64; $expected = 'Input size or SHA-256 does not match*' }
        default {
            $archive = Join-Path $bundle $manifest.archives[74].file
            $bytes = [System.IO.File]::ReadAllBytes($archive)
            if ($case -eq 'identity-dictionary') { $bytes[14] = 8 }
            elseif ($case -eq 'identity-context') { $bytes[98] = 9 }
            elseif ($case -eq 'identity-context-count') { $bytes[82] = 43 }
            elseif ($case -eq 'header-truncated') { $bytes = [byte[]]$bytes[0..110] }
            else {
                $offset = [int]$case.Substring('identity-byte-'.Length)
                $bytes[$offset] = $bytes[$offset] -bxor 1
            }
            [System.IO.File]::WriteAllBytes($archive, $bytes)
            $manifest.archives[74].bytes = $bytes.Length
            $manifest.archives[74].sha256 = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
            $expected = if ($case -eq 'header-truncated') { 'Position rANS archive header is truncated' }
                else { 'Position rANS archive does not carry exact identity*' }
        }
    }
    [System.IO.File]::WriteAllText($path, ($manifest | ConvertTo-Json -Depth 5), [System.Text.UTF8Encoding]::new($false))
    $output = Join-Path $root "verified-$case"
    $rejected = $false
    try {
        & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') `
            -MarcCli $launchGuard -BundleDirectory $bundle -OutputDirectory $output
    } catch {
        if ($_.Exception.Message -notlike $expected) { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw "Verifier accepted schema-65 negative: $case" }
    if (Test-Path -LiteralPath $launchMarker) { throw "Codec launched before admission: $case" }
    if (@(Get-ChildItem -LiteralPath $output -File -Recurse).Count -ne 0) {
        throw "Verifier published output before complete manifest admission: $case"
    }
    $receipts += [ordered]@{ case = $case; rejected = $true; output_files = 0; codec_launches = 0; expected = $expected }
}
[System.IO.File]::WriteAllText((Join-Path $root 'qualification.json'),
    ([ordered]@{ passed = $true; cases = $receipts } | ConvertTo-Json -Depth 5),
    [System.Text.UTF8Encoding]::new($false))
Write-Host "Rejected $($cases.Count) schema-65 negatives before codec launch"
