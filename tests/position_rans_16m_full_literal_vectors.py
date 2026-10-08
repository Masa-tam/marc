"""Independent DD-1536/1542 finite vectors; no production codec dependency.

With --write, regenerate the checked-in mathematical vectors. Otherwise
compare their exact bytes and run bounded seeded model/decision checks.
Isolated distance vectors intentionally do not claim valid frame history.
"""
import argparse
import importlib.util
import json
import random
import struct
from pathlib import Path

from position_rans_16m_full_literal_oracle import install

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    'mathematics', ROOT / 'tools/position_distance_rans_diagnostic.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
install(p)
F = 1 << 24
VECTOR_FILE = Path(__file__).with_name('position_rans_16m_full_literal_vectors.json')


def parse_compact(data):
    """Separate forward parser: all seven mask bytes are usable at 56 contexts."""
    if len(data) < 7:
        raise ValueError('mask extent')
    mask, cursor, result = int.from_bytes(data[:7], 'little'), 7, []
    for context, alphabet in enumerate(p.ALPHABETS):
        model = {}
        if mask & (1 << context):
            mode = data[cursor]
            cursor += 1
            if mode == 0:
                model[data[cursor]] = 4096
                cursor += 1
            elif mode == 1:
                values = struct.unpack_from('<' + 'H' * (alphabet - 1), data, cursor)
                cursor += 2 * (alphabet - 1)
                values = (*values, 4096 - sum(values))
                model = {s: f for s, f in enumerate(values) if f}
            elif mode == 2:
                count, = struct.unpack_from('<H', data, cursor)
                cursor += 2
                if not 2 <= count <= alphabet:
                    raise ValueError('sparse count')
                previous = -1
                for _ in range(count - 1):
                    s, f = struct.unpack_from('<BH', data, cursor)
                    cursor += 3
                    if not previous < s < alphabet or not 0 < f <= 4096:
                        raise ValueError('sparse record')
                    model[s] = f
                    previous = s
                s = data[cursor]
                cursor += 1
                if not previous < s < alphabet:
                    raise ValueError('sparse final symbol')
                model[s] = 4096 - sum(model.values())
            else:
                raise ValueError('record mode')
            if (sum(model.values()) != 4096 or
                    any(not 0 <= s < alphabet or not 0 < f <= 4096
                        for s, f in model.items())):
                raise ValueError('model bounds')
        result.append(model)
    if cursor != len(data):
        raise ValueError('trailing model')
    return result


def descriptor(models, count, payload):
    return struct.pack('<IIBBHI', count, len(payload), 12, 0, 56, 4658) + p.pack_models_compact(models)


def qualify_events(label, events):
    models = p.models_for(events)
    payload = p.encode_events(events, models)
    compact = p.pack_models_compact(models)
    assert parse_compact(compact) == models
    reader = p.ForwardDecoder(payload, parse_compact(compact), len(events))
    assert [(c, reader.read(c)) for c, _ in events] == events
    reader.finish()
    return dict(label=label, events=events,
                descriptor_hex=descriptor(models, len(events), payload).hex(),
                payload_hex=payload.hex())


def stream_header(frame, original):
    data = bytearray(112)
    data[:4] = b'MARC'
    for offset, value in ((4, 2), (8, 64), (10, 1), (12, 2), (14, 12),
                          (16, 4), (18, 9), (82, 56), (96, 1), (98, 18)):
        struct.pack_into('<H', data, offset, value)
    for offset, value in ((20, frame), (28, 16), (32, 16), (48, 16),
                          (64, F), (68, 3), (72, 258), (84, 4658)):
        struct.pack_into('<I', data, offset, value)
    struct.pack_into('<Q', data, 40, original)
    data[80:82] = bytes((12, 1))
    return data.hex()


def vectors():
    result = [qualify_events('empty', [])]
    assert result[0]['descriptor_hex'] == '00000000080000000c00380032120000' + '00' * 7
    assert result[0]['payload_hex'] == '0000008000000000'
    result.append(qualify_events('last-mask-bit-valid', [(55, 1)]))
    assert bytes.fromhex(result[-1]['descriptor_hex'])[16:23] == bytes.fromhex('00000000000080')
    result.append(qualify_events('literal-history-across-match', list(p.decisions(
        [(0, 0x10, 0, 0), (1, 0, 1, 5), (0, 0xfe, 0, 0)]))))
    result.append(qualify_events('all-literal-buckets', list(p.decisions(
        [(0, s, 0, 0) for s in range(256)]))))
    for length in (3, 4, 5, 6, 8, 12, 20, 36, 68, 132, 258):
        result.append(qualify_events('isolated-length-' + str(length),
                                    list(p.decisions([(1, 0, F, length)]))))
    for distance in (1, 2, 3, (1 << 23) - 1, 1 << 23, F - 258, F - 3, F - 1, F):
        events = list(p.decisions([(1, 0, distance, 3)]))
        result.append(qualify_events('isolated-distance-' + str(distance), events))
        if distance == F:
            assert (31, 24) in events and events[-24:] == [(32 + b, 0) for b in range(24)]
    models = [{s: 4096 // a + (s < 4096 % a) for s in range(a)} for a in p.ALPHABETS]
    dense = descriptor(models, 4658, struct.pack('<Q', p.LOWER))
    assert len(dense) == 9283 and dense[16:23] == b'\xff' * 7
    assert parse_compact(dense[16:]) == models
    return dict(window_bytes=F, contexts=56, frequencies=4658, mask_bytes=7,
                maximum_descriptor_hex=dense.hex(), empty_stream_hex=stream_header(F, 0),
                isolated_decision_vectors=result,
                reachable_frame_recipes=[dict(prefix_bytes=F-length, final_distance=F-length,
                                              final_length=length, frame_bytes=F)
                                         for length in (3, 258)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    data = vectors()
    text = json.dumps(data, indent=2) + '\n'
    if args.write:
        VECTOR_FILE.write_text(text, encoding='utf8')
    else:
        assert VECTOR_FILE.read_text(encoding='utf8') == text
    rng = random.Random(1542)
    for _ in range(200):
        models = []
        for alphabet in p.ALPHABETS:
            present = rng.sample(range(alphabet), rng.randrange(alphabet + 1))
            counts = {s: rng.randrange(1, 1 << 20) for s in present}
            models.append(p.normalize(counts) if counts else {})
        compact = p.pack_models_compact(models)
        assert parse_compact(compact) == models and len(compact) + 16 <= 9283
    for index in range(200):
        events = []
        for _ in range(rng.randrange(1, 513)):
            context = rng.randrange(-1, 56)
            events.append((context, rng.randrange(2 if context == -1 else p.ALPHABETS[context])))
        qualify_events('seed-' + str(index), events)
    print(json.dumps(dict(passed=True, frozen_vectors=len(data['isolated_decision_vectors']),
                          seeded_models=200, seeded_payloads=200,
                          production_implementation_tested=False)))


if __name__ == '__main__':
    main()
