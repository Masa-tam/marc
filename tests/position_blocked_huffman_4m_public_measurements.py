"""Qualify paired normal measurement paths against the public CLI bytes."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import uuid

benchmark, resources, cli = [Path(p).resolve() for p in sys.argv[1:4]]
stage = Path(sys.argv[4]).resolve() / ('run-' + uuid.uuid4().hex)
stage.mkdir(parents=True)


def invoke(label, args, success=True):
    result = subprocess.run([str(p) for p in args], capture_output=True, timeout=30)
    (stage / (label + '.stdout')).write_bytes(result.stdout)
    (stage / (label + '.stderr')).write_bytes(result.stderr)
    assert (result.returncode == 0) == success, (label, result.returncode, result.stderr)
    return result


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


for size in (0, 1, 4194303, 4194304, 4194305):
    raw = stage / f'raw-{size}.bin'
    raw.write_bytes((bytes(range(256)) * ((size + 255) // 256))[:size])
    for profile, codec, ids in (
            ('position', 'lzss-position-distance-blocked-huffman-4m', (2, 10, 2, 5, 1, 30)),
            ('contextual', 'lzss-contextual-blocked-huffman-4m', (2, 4, 2, 2, 1, 3))):
        label = f'{size}-{profile}'
        wire = stage / (label + '.marc')
        golden = stage / (label + '.golden.marc')
        decoded = stage / (label + '.decoded')
        invoke(label + '-cli', [cli, 'encode', '--codec', codec, raw, golden])
        for mode, source, target, expected in (
                ('encode', raw, wire, golden), ('decode', wire, decoded, raw)):
            receipt = json.loads(invoke(label + '-' + mode + '-resources',
                                       [resources, profile, mode, source, target]).stdout)
            assert receipt['transform_completed'] and receipt['peak_resident_bytes'] > 0
            assert receipt['frame_size'] == receipt['window_size'] == 4194304
            if profile == 'position':
                assert receipt['queried_bytes'] > 0
            assert digest(target) == digest(expected)
            timing = json.loads(invoke(label + '-' + mode + '-benchmark',
                                      [benchmark, profile, mode, source, expected]).stdout)
            assert timing['verified'] and timing['warmups'] == 1
            assert len(timing['seconds']) == 3 and all(t > 0 for t in timing['seconds'])
            if profile == 'position':
                assert timing['queried_bytes'] == receipt['queried_bytes'] + timing['comparison_vector_capacity_bytes']
        header = wire.read_bytes()[:112]
        assert (*struct.unpack_from('<4H', header, 12), *struct.unpack_from('<2H', header, 96)) == ids
        assert struct.unpack_from('<I', header, 20)[0] == struct.unpack_from('<I', header, 64)[0] == 4194304
        # A wrong comparison buffer must fail even if transform completion succeeds.
        wrong = stage / (label + '.wrong')
        wrong.write_bytes(b'wrong')
        invoke(label + '-wrong-comparison', [benchmark, profile, 'decode', wire, wrong], False)
        before = digest(wire)
        invoke(label + '-existing', [resources, profile, 'encode', raw, wire], False)
        assert digest(wire) == before

# The resource driver commits only complete files, including a late entropy error.
wire = stage / '1-position.marc'
bad = bytearray(wire.read_bytes())
bad[len(bad)-3]=1
broken = stage / 'broken.marc'
broken.write_bytes(bad)
refused = stage / 'refused.raw'
invoke('late-failed-frame', [resources, 'position', 'decode', broken, refused], False)
assert not refused.exists() and Path(str(refused) + '.tmp').stat().st_size == 0
print(json.dumps({'passed': True, 'stage': str(stage), 'boundary_cases': 5,
                  'profiles': 2, 'directions': 2, 'sample_count': 3, 'warmups': 1}))
