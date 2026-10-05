"""Closed full-window parse recipes, independently serialized by the math oracle."""
from pathlib import Path
import struct
from lzss_position_distance_32m_reference_vectors import F
from lzss_position_distance_32m_frame_reference import frame

def tokens(length):
    if not length:
        result = [(0, 65, 0, 0)]
        remaining = F - 1
    else:
        result = [(0, 1 + i % 255, 0, 0) for i in range(length)]
        result.append((0, 0, 0, 0))
        remaining = F - 2 * length - 1
    count, tail = divmod(remaining, 258)
    result += [(1, 0, 1, 258)] * count
    if tail >= 5:
        result.append((1, 0, 1, tail))
    else:
        result += [(0, 0 if length else 65, 0, 0)] * tail
    if length:
        result.append((1, 0, F - length, length))
    assert sum(1 if t[0] == 0 else t[3] for t in result) == F
    return result

def write(path):
    with Path(path).open('xb') as sink:
        sink.write(b'M32E0001' + struct.pack('<I', 3))
        for length in (0, 5, 258):
            header, body = frame(tokens(length))
            sink.write(struct.pack('<II', length, len(body)))
            sink.write(header + body)
