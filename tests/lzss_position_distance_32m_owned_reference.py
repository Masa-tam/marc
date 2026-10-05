"""Independent closed dictionary recipes and explicit multi-frame stream layout."""
from pathlib import Path
import struct
from lzss_position_distance_32m_reference_vectors import F
from lzss_position_distance_32m_frame_reference import frame, stream_header
from lzss_position_distance_32m_frame_encode_reference import tokens

def repetition(size):
    count, tail = divmod(size - 1, 258)
    result = [(0, 65, 0, 0)] + [(1, 0, 1, 258)] * count
    if tail >= 5: result.append((1, 0, 1, tail))
    else: result += [(0, 65, 0, 0)] * tail
    return result

def write(path):
    cases = [(F-1, 0), (F, 0), (F+1, 0), (2*F, 0), (2*F+1, 0), (F, 5), (F, 258)]
    with Path(path).open('xb') as sink:
        sink.write(b'M32O0001' + struct.pack('<I', len(cases)))
        for size, length in cases:
            header = bytearray(stream_header(F))
            struct.pack_into('<Q', header, 40, size)
            stream = bytes(header)
            for pos in range(0, size, F):
                recipe = tokens(length) if length else repetition(min(F, size-pos))
                _, body = frame(recipe)
                body = bytearray(body)
                struct.pack_into('<Q', body, 8, pos//F)
                stream += body
            sink.write(struct.pack('<III', size, length, len(stream)) + stream)
