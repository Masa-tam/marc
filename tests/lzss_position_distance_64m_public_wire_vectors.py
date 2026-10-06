"""Independent legal short-match wire recipes for the public compact decoder."""
from pathlib import Path
import sys
from lzss_position_distance_64m_frame_reference import frame

def write(path):
    output = '#pragma once\n#include <array>\n#include <cstdint>\nnamespace public_wire_vectors {\n'
    for length in (3, 4):
        header, body = frame([(0, 65, 0, 0), (1, 0, 1, length)])
        wire = header + body
        output += f'inline constexpr std::array<std::uint8_t,{len(wire)}> length{length} = {{'
        output += ','.join(map(str, wire)) + '};\n'
    output += '}\n'
    with Path(path).open('xb') as sink: sink.write(output.encode())

if __name__ == '__main__':
    assert len(sys.argv) == 2
    write(sys.argv[1])
