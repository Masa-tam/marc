"""Regenerate finite 32 MiB expected bytes with independent model equations.

The older first-party header supplies semantic tokens only; no payload is reused.
Run with a fresh destination path to retain prior generated artifacts.
"""
from pathlib import Path
import hashlib
import re
import struct
import sys
import textwrap
import lzss_position_distance_32m_reference_vectors as oracle

INPUT_SHA256 = '6c41aa4143e2374c51e5300a91fcbbab78e882d22cf8fba5e07a911252b20661'

def generate():
    source = Path(__file__).with_name('lzss_position_distance_16m_token_range_vectors.hpp')
    data = source.read_bytes().replace(b'\r\n', b'\n')
    assert hashlib.sha256(data).hexdigest() == INPUT_SHA256
    pattern = r'\{\s*"([^"]+)"\s*,\s*((?:"[0-9a-f]*"\s*)+),\s*((?:"[0-9a-f]*"\s*)+),\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}'
    rows = re.findall(pattern, data.decode('ascii'))
    assert len(rows) == 29
    header = '#ifndef MARC_TEST_POSITION_DISTANCE32M_TOKEN_RANGE_VECTORS_HPP\n#define MARC_TEST_POSITION_DISTANCE32M_TOKEN_RANGE_VECTORS_HPP\n#include <string_view>\n#include <array>\nnamespace token_range_vectors {\nstruct Vector { std::string_view name, tokens, payload; unsigned f,t,e,d; };\ninline constexpr std::array<Vector,29> vectors{{\n'
    def quoted(value):
        return '\n'.join('"' + part + '"' for part in textwrap.wrap(value, 960)) if value else '""'
    for name, token_parts, unused_old_payload, *unused_old_counts in rows:
        token_hex = ''.join(re.findall(r'"([0-9a-f]*)"', token_parts))
        if token_hex:
            tokens = list(struct.iter_unpack('<BBII', bytes.fromhex(token_hex)))
        elif name == 'literal_rescale':
            tokens = [(0, 65, 0, 0)] * 70000
        elif name == 'match_rescale':
            tokens = [(0, 65, 0, 0)] + [(1, 0, 1, 5)] * 70000
        elif name in ('far3', 'far258'):
            tokens = oracle.recipe(name)
        else:
            assert name == 'distance_classes'
            tokens = [(0, 65, 0, 0)] * 4096 + [(1, 0, 1 << i, 5) for i in range(13)]
        payload, events, decisions, raw = oracle.encode(tokens)
        header += '{"' + name + '",' + quoted(token_hex) + ',' + quoted(payload.hex()) + f',{raw},{len(tokens)},{events},{decisions}' + '},\n'
    return header + '}};\n} // namespace token_range_vectors\n#endif\n'

if __name__ == '__main__':
    with Path(sys.argv[1]).open('x', encoding='ascii') as destination:
        destination.write(generate())
