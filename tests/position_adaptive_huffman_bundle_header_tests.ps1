param([ValidateSet('64k','1m','4m','8m','16m','32m','64m')][string]$Window = '64k')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'position_adaptive_huffman_bundle_header.ps1')
$count = 0
$headers = 0
$Window = $Window.ToLowerInvariant()
$suffix = if ($Window -eq '64k') { '' } else { '-'+$Window }
$codec = 'lzss-position-distance-adaptive-huffman' + $suffix
$label = if ($Window -eq '64k') { '64 KiB' } else { $Window.Substring(0,$Window.Length-1)+' MiB' }
foreach ($fixture in @("position_adaptive_huffman_${Window}_frame_vectors.json","position_adaptive_huffman_${Window}_boundary_vectors.json")) {
    $vectors = (Get-Content -LiteralPath (Join-Path $PSScriptRoot "data/$fixture") -Raw | ConvertFrom-Json).vectors
    foreach ($row in $vectors) {
        $header = [byte[]]::new(112)
        for ($i = 0; $i -lt 112; ++$i) { $header[$i] = [Convert]::ToByte($row.stream_hex.Substring(2*$i,2),16) }
        $original = if ($row.PSObject.Properties.Name -contains 'raw_repeat') { [uint64]$row.raw_repeat.count } else { [uint64]($row.raw_hex.Length/2) }
        Assert-PositionAdaptiveHuffmanHeader $header $original $codec
        ++$headers
        foreach ($offset in 0..111) {
            $damaged = [byte[]]$header.Clone(); $damaged[$offset] = $damaged[$offset] -bxor 1
            $rejected = $false
            try { Assert-PositionAdaptiveHuffmanHeader $damaged $original $codec }
            catch { if ($_.Exception.Message -notlike "$label position Adaptive Huffman archive does not carry exact identity*") { throw }; $rejected = $true }
            if (-not $rejected) { throw "Header accepted changed byte$offset of$($row.name)" }
            ++$count
        }
        $rejected = $false
        try { Assert-PositionAdaptiveHuffmanHeader ([byte[]]$header[0..110]) $original $codec }
        catch { if ($_.Exception.Message -ne "$label position Adaptive Huffman archive header is truncated") { throw }; $rejected = $true }
        if (-not $rejected) { throw 'Header accepted truncation' }
        ++$count
    }
}
if ($headers -ne 19 -or $count -ne 2147) { throw 'Unexpected independent header test coverage' }
Write-Host "Verified$headers independent headers and rejected$count directed header alterations"
