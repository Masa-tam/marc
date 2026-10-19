# Exact native Adaptive Huffman exchange identities; admit profiles in window order.
function Assert-PositionAdaptiveHuffmanHeader([byte[]]$Bytes, [uint64]$OriginalBytes, [string]$Codec) {
    if ($Codec -ne 'lzss-position-distance-adaptive-huffman') { throw 'Unsupported native Adaptive Huffman exchange profile' }
    if ($Bytes.Length -lt 112) { throw '64 KiB position Adaptive Huffman archive header is truncated' }
    $expected = [byte[]]::new(112)
    [System.Text.Encoding]::ASCII.GetBytes('MARC').CopyTo($expected, 0)
    foreach ($field in @(@(4,2,2), @(8,2,64), @(10,2,1), @(12,2,2), @(14,2,8),
            @(16,2,1), @(18,2,3), @(20,4,65536), @(28,4,16), @(32,4,16),
            @(40,8,$OriginalBytes), @(48,4,16), @(64,4,65536), @(68,4,3), @(72,4,258),
            @(80,1,1), @(81,1,1), @(82,2,48), @(84,4,4570), @(88,4,9188), @(96,2,1), @(98,2,35))) {
        for ($index = 0; $index -lt $field[1]; ++$index) {
            $expected[$field[0] + $index] = [byte](([uint64]$field[2] -shr (8 * $index)) -band 255)
        }
    }
    for ($index = 0; $index -lt 112; ++$index) {
        if ($Bytes[$index] -ne $expected[$index]) {
            throw '64 KiB position Adaptive Huffman archive does not carry exact identity and parameters'
        }
    }
}
