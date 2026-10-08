"""Exercise full-frame workspace queries through both measurement drivers."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import uuid

benchmark, resources = map(lambda p: Path(p).resolve(), sys.argv[1:3])
stage = Path(sys.argv[3]).resolve() / ('run-' + uuid.uuid4().hex)
stage.mkdir(parents=True)
raw = stage / 'raw.bin'
raw.write_bytes(bytes(67108865))
for profile, identity in [('position', (2, 14, 4, 11, 1, 20)),
                          ('contextual', (2, 6, 4, 3, 1, 5))]:
    wire = stage / (profile + '.marc')
    decoded = stage / (profile + '.decoded')
    for mode, source, target in [('encode', raw, wire), ('decode', wire, decoded)]:
        result = subprocess.run([str(resources), profile, mode, str(source), str(target)],
                                capture_output=True, timeout=120)
        (stage / f'{profile}-{mode}-resources.log').write_bytes(result.stdout + result.stderr)
        assert result.returncode == 0, (profile, mode, result.returncode, result.stderr)
        assert json.loads(result.stdout)['verified']
        expected = wire if mode == 'encode' else raw
        result = subprocess.run([str(benchmark), profile, mode, str(source), str(expected)],
                                capture_output=True, timeout=120)
        (stage / f'{profile}-{mode}-benchmark.log').write_bytes(result.stdout + result.stderr)
        assert result.returncode == 0, (profile, mode, result.returncode, result.stderr)
        measured = json.loads(result.stdout)
        assert measured['verified'] and len(measured['seconds']) == 3
    assert decoded.read_bytes() == raw.read_bytes()
    header = wire.read_bytes()[:112]
    assert (*struct.unpack_from('<4H', header, 12), *struct.unpack_from('<2H', header, 96)) == identity
    assert struct.unpack_from('<I', header, 20)[0] == 67108864
    assert struct.unpack_from('<I', header, 64)[0] == 67108864
print('Both measurement drivers: full64MiB frame plus final byte verified in four modes.')
