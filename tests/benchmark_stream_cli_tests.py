"""Integration checks for bounded benchmark storage and verification."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    executable = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix='marc-benchmark-test-') as directory:
        root = Path(directory)
        temporary = root / 'temporary'
        temporary.mkdir()
        source = root / 'input.bin'
        environment = dict(os.environ, TMP=str(temporary), TEMP=str(temporary),
                           TMPDIR=str(temporary))

        def invoke(*args, expected=0):
            result = subprocess.run([executable, *map(str, args)], text=True,
                                    capture_output=True, env=environment, timeout=60)
            if result.returncode != expected:
                raise RuntimeError(f'{args}: {result.returncode}: {result.stderr}')
            if list(temporary.iterdir()):
                raise RuntimeError('benchmark did not clean its temporary files')
            return dict(line.split('=', 1) for line in result.stdout.splitlines()
                        if '=' in line)

        # More than two complete I/O buffers, with a final partial buffer.
        source.write_bytes(bytes(range(256)) * 512 + b'final-partial')
        original_hash = hashlib.sha256(source.read_bytes()).digest()
        for capacity in (65536, 17):
            report = invoke('checksum-raw', source, 2, '--io-size', capacity)
            assert report['timing_contract'] == 'chunked-process-v1'
            assert int(report['input_bytes']) == source.stat().st_size
            assert int(report['io_buffer_bytes']) == capacity
        assert hashlib.sha256(source.read_bytes()).digest() == original_hash

        # Exercise finalization/draining through actual public entropy/dictionary
        # transforms, including empty streams and one-byte process capacity.
        for data in (b'', bytes(range(256)) + b'a'):
            source.write_bytes(data)
            for codec in ('checksum-raw', 'blocked-huffman', 'rans', 'lzw'):
                small = invoke(codec, source, 2, '--io-size', 1)
                regular = invoke(codec, source, 1)
                assert small['encoded_bytes'] == regular['encoded_bytes']
                assert int(small['input_bytes']) == len(data)
        for args in (
                ('checksum-raw', source, 0),
                ('checksum-raw', source, 4294967296),
                ('checksum-raw', source, '--io-size', 0),
                ('checksum-raw', source, '--io-size', 65537),
                ('checksum-raw', source, '--io-size', 1, 'extra'),
                ('checksum-raw-typo', source, 1)):
            assert not invoke(*args, expected=2)
        assert not invoke('checksum-raw', root / 'missing', 1, expected=1)
    print('bounded benchmark CLI checks passed')


if __name__ == '__main__':
    main()
