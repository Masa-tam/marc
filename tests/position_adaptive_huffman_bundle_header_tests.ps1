$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'position_adaptive_huffman_bundle_header.ps1')
$count = 0
foreach ($fixture in @('position_adaptive_huffman_64k_frame_vectors.json','position_adaptive_huffman_64k_boundary_vectors.json')) {
    $vectors = (Get-Content -LiteralPath (Join-Path $PSScriptRoot "data/$fixture") -Raw | ConvertFrom-Json).vectors
    foreach ($row in $vectors) {
        $header = [byte[]]::new(112)
        for ($i = 0; $i -lt 112; ++$i) { $header[$i] = [Convert]::ToByte($row.stream_hex.Substring(2*$i,2),16) }
        $original = if ($row.PSObject.Properties.Name -contains 'raw_repeat') { [uint64]$row.raw_repeat.count } else { [uint64]($row.raw_hex.Length/2) }
        Assert-PositionAdaptiveHuffmanHeader $header $original 'lzss-position-distance-adaptive-huffman'
        foreach ($offset in 0..111) {
            $damaged = [byte[]]$header.Clone(); $damaged[$offset] = $damaged[$offset] -bxor 1
            $rejected = $false
            try { Assert-PositionAdaptiveHuffmanHeader $damaged $original 'lzss-position-distance-adaptive-huffman' }
            catch { if ($_.Exception.Message -notlike '64 KiB position Adaptive Huffman archive does not carry exact identity*') { throw }; $rejected = $true }
            if (-not $rejected) { throw "Header accepted changed byte$offset of$($row.name)" }
            ++$count
        }
        $rejected = $false
        try { Assert-PositionAdaptiveHuffmanHeader ([byte[]]$header[0..110]) $original 'lzss-position-distance-adaptive-huffman' }
        catch { if ($_.Exception.Message -ne '64 KiB position Adaptive Huffman archive header is truncated') { throw }; $rejected = $true }
        if (-not $rejected) { throw 'Header accepted truncation' }
        ++$count
    }
}
Write-Host "Verified19 independent headers and rejected$count directed header alterations"
