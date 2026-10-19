# Exact native Adaptive Huffman exchange identities; admit profiles in window order.
function Assert-PositionAdaptiveHuffmanHeader([byte[]]$Bytes, [uint64]$OriginalBytes, [string]$Codec) {
    if ($Codec -ceq 'lzss-position-distance-adaptive-huffman') {
        $spec = @(65536,8,35,3,48,4570,9188); $label = '64 KiB'
    } elseif ($Codec -ceq 'lzss-position-distance-adaptive-huffman-1m') {
        $spec = @(1048576,9,36,4,52,4614,9280); $label = '1 MiB'
    } elseif ($Codec -ceq 'lzss-position-distance-adaptive-huffman-4m') {
        $spec = @(4194304,10,37,5,54,4636,9326); $label = '4 MiB'
    } elseif ($Codec -ceq 'lzss-position-distance-adaptive-huffman-8m') {
        $spec = @(8388608,11,38,6,55,4647,9349); $label = '8 MiB'
    } elseif ($Codec -ceq 'lzss-position-distance-adaptive-huffman-16m') {
        $spec = @(16777216,12,39,7,56,4658,9372); $label = '16 MiB'
    } else { throw 'Unsupported native Adaptive Huffman exchange profile' }
    if ($Bytes.Length -lt 112) { throw "$label position Adaptive Huffman archive header is truncated" }
    $expected = [byte[]]::new(112)
    [System.Text.Encoding]::ASCII.GetBytes('MARC').CopyTo($expected, 0)
    foreach ($field in @(@(4,2,2), @(8,2,64), @(10,2,1), @(12,2,2), @(14,2,$spec[1]),
            @(16,2,1), @(18,2,$spec[3]), @(20,4,$spec[0]), @(28,4,16), @(32,4,16),
            @(40,8,$OriginalBytes), @(48,4,16), @(64,4,$spec[0]), @(68,4,3), @(72,4,258),
            @(80,1,1), @(81,1,1), @(82,2,$spec[4]), @(84,4,$spec[5]), @(88,4,$spec[6]), @(96,2,1), @(98,2,$spec[2]))) {
        for ($index = 0; $index -lt $field[1]; ++$index) {
            $expected[$field[0] + $index] = [byte](([uint64]$field[2] -shr (8 * $index)) -band 255)
        }
    }
    for ($index = 0; $index -lt 112; ++$index) {
        if ($Bytes[$index] -ne $expected[$index]) {
            throw "$label position Adaptive Huffman archive does not carry exact identity and parameters"
        }
    }
}
