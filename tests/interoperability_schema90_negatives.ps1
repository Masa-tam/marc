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
$root = (Resolve-Path -LiteralPath $EvidenceDirectory).Path
$source = (Resolve-Path -LiteralPath $BundleDirectory).Path
$null = Resolve-Path -LiteralPath $MarcCli
$baselineJson = Get-Content -LiteralPath (Join-Path $source 'manifest.json') -Raw
$baseline = $baselineJson | ConvertFrom-Json
if ($baseline.schema_version -ne 90 -or @($baseline.archives).Count -ne 100 -or
        $baseline.archives[99].codec -ne 'lzss-position-distance-adaptive-huffman-16m') {
    throw 'Negative tests require a complete schema-90 source bundle'
}
$marker = Join-Path $root 'unexpected-codec-launch.txt'
$guard = Join-Path $root 'codec-must-not-run.ps1'
$escapedMarker = $marker.Replace("'", "''")
[System.IO.File]::WriteAllText($guard,
    "[System.IO.File]::WriteAllText('$escapedMarker', 'unexpected')`nthrow 'Codec launched before manifest admission'",
    [System.Text.UTF8Encoding]::new($false))
$bundle = Join-Path $root 'working-bundle'
Copy-Item -LiteralPath $source -Destination $bundle -Recurse
$lastFile = [string]$baseline.archives[99].file
$original = [System.IO.File]::ReadAllBytes((Join-Path $source $lastFile))
$cases = @('old-name','native-case','native64-case','native1m-case','native4m-case','native8m-case','downgrade','codec-set','missing','duplicate','order','revision',
    'archive-hash','archive-size','input-hash','archive-path','input-path','header-truncated')
foreach ($offset in 0..111) { $cases += "identity-byte-$offset" }
$receipts = @()
foreach ($case in $cases) {
    $caseRoot = Join-Path $root $case
    $null = New-Item -ItemType Directory -Path $caseRoot
    [System.IO.File]::WriteAllBytes((Join-Path $bundle $lastFile), $original)
    $manifest = $baselineJson | ConvertFrom-Json
    switch ($case) {
        'old-name' { $manifest.archives[99].codec = 'lzss-position-adaptive-huffman-16m'; $expected = 'Unknown or duplicate codec*' }
        'native-case' { $manifest.archives[99].codec = $manifest.archives[99].codec.ToUpperInvariant(); $expected = 'Unsupported native Adaptive Huffman exchange profile' }
        'native64-case' { $manifest.archives[95].codec = $manifest.archives[95].codec.ToUpperInvariant(); $expected = 'Unsupported native Adaptive Huffman exchange profile' }
        'native1m-case' { $manifest.archives[96].codec = $manifest.archives[96].codec.ToUpperInvariant(); $expected = 'Unsupported native Adaptive Huffman exchange profile' }
        'native4m-case' { $manifest.archives[97].codec = $manifest.archives[97].codec.ToUpperInvariant(); $expected = 'Unsupported native Adaptive Huffman exchange profile' }
        'native8m-case' { $manifest.archives[98].codec = $manifest.archives[98].codec.ToUpperInvariant(); $expected = 'Unsupported native Adaptive Huffman exchange profile' }
        'downgrade' { $manifest.schema_version = 89; $manifest.codec_set = 'marc-cli-v89'; $expected = 'Interoperability manifest must contain exactly 99 archives' }
        'codec-set' { $manifest.codec_set = 'marc-cli-v89'; $expected = 'Unsupported interoperability codec set*' }
        'missing' { $manifest.archives = @($manifest.archives[0..98]); $expected = 'Interoperability manifest must contain exactly 100 archives' }
        'duplicate' { $manifest.archives[99] = $manifest.archives[98]; $expected = 'Unknown or duplicate codec*' }
        'order' { $tail = $manifest.archives[99]; $manifest.archives[99] = $manifest.archives[98]; $manifest.archives[98] = $tail; $expected = 'Codec is out of schema order*' }
        'revision' { $manifest.source_revision = 'not-a-revision'; $expected = 'Manifest source revision is not a full Git object ID' }
        'archive-hash' { $manifest.archives[99].sha256 = '0' * 64; $expected = 'Archive size or SHA-256 does not match*' }
        'archive-size' { $manifest.archives[99].bytes += 1; $expected = 'Archive size or SHA-256 does not match*' }
        'input-hash' { $manifest.input.sha256 = '0' * 64; $expected = 'Input size or SHA-256 does not match*' }
        'archive-path' { $manifest.archives[99].file = '../outside.marc'; $expected = 'Manifest file name is not a leaf name*' }
        'input-path' { $manifest.input.file = '../outside.bin'; $expected = 'Manifest file name is not a leaf name*' }
        default {
            $bytes = [byte[]]$original.Clone()
            if ($case -eq 'header-truncated') { $bytes = [byte[]]$bytes[0..110]; $expected = '16 MiB position Adaptive Huffman archive header is truncated' }
            else {
                $offset = [int]$case.Substring('identity-byte-'.Length)
                $bytes[$offset] = $bytes[$offset] -bxor 1
                $expected = '16 MiB position Adaptive Huffman archive does not carry exact identity*'
            }
            [System.IO.File]::WriteAllBytes((Join-Path $bundle $lastFile), $bytes)
            $manifest.archives[99].bytes = $bytes.Length
            $manifest.archives[99].sha256 = (Get-FileHash -LiteralPath (Join-Path $bundle $lastFile) -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    $manifestPath = Join-Path $bundle 'manifest.json'
    [System.IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 5), [System.Text.UTF8Encoding]::new($false))
    Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $caseRoot 'manifest.json')
    Copy-Item -LiteralPath (Join-Path $bundle $lastFile) -Destination (Join-Path $caseRoot $lastFile)
    $output = Join-Path $root "verified-$case"
    $rejected = $false
    try { & (Join-Path $PSScriptRoot 'verify_interoperability_bundle.ps1') -MarcCli $guard -BundleDirectory $bundle -OutputDirectory $output }
    catch { if ($_.Exception.Message -notlike $expected) { throw }; $rejected = $true }
    if (-not $rejected) { throw "Verifier accepted schema-90 negative: $case" }
    if (Test-Path -LiteralPath $marker) { throw "Codec launched before admission: $case" }
    if (@(Get-ChildItem -LiteralPath $output -File -Recurse).Count -ne 0) { throw "Verifier published output before complete admission: $case" }
    $receipts += [ordered]@{ case = $case; rejected = $true; output_files = 0; codec_launches = 0 }
}
[System.IO.File]::WriteAllText((Join-Path $root 'qualification.json'),
    ([ordered]@{ passed = $true; cases = $receipts } | ConvertTo-Json -Depth 5), [System.Text.UTF8Encoding]::new($false))
Write-Host "Rejected $($cases.Count) schema-90 negatives before codec launch"
