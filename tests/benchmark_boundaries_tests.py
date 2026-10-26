"""Keep public frame/withholding recipes when retiring per-window probes."""
import argparse
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from run_public_benchmark import digest, parse_report

WINDOWS = ('64k', '1m', '4m', '8m', '16m', '32m', '64m')
EXTENTS = (65536, 1048576, 4194304, 8388608, 16777216, 33554432, 67108864)
CONTEXTUAL = ('64k', '1m', '4m', '16m', '64m')


def codec_name(backend, window, native):
    family = 'position-distance' if native else 'contextual'
    return f'lzss-{family}-{backend}' + ('' if window == '64k' else '-' + window)


def identity(backend, window, native):
    if not native:
        index = CONTEXTUAL.index(window)
        entropy = {'rans': 4, 'tans': 5, 'blocked-huffman': 2, 'adaptive-huffman': 1}[backend]
        return (2, 2 + index, entropy, 3 if backend == 'rans' else 2, 1, 1 + index)
    index = WINDOWS.index(window)
    if backend == 'rans':
        return (2, 8 + index, 4, (5, 4, 7, 8, 9, 10, 11)[index],
                1, (9, 10, 16, 17, 18, 19, 20)[index])
    entropy = {'tans': 5, 'blocked-huffman': 2, 'adaptive-huffman': 1}[backend]
    context = {'tans': (21, 22, 23, 24, 25, 26, 26)[index],
               'blocked-huffman': 28 + index, 'adaptive-huffman': 35 + index}[backend]
    return (2, 8 + index, entropy, 3 + index, 1, context)


def write_input(path, size, repeated=False):
    chunk = b'A' * 65536 if repeated else bytes(range(256)) * 256
    with path.open('xb') as output:
        while size:
            count = min(size, len(chunk))
            output.write(chunk[:count])
            size -= count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--benchmark', type=Path, required=True)
    parser.add_argument('--cli', type=Path, required=True)
    parser.add_argument('--section', choices=('blocked', 'tans', 'large', 'literal'), required=True)
    args = parser.parse_args()
    benchmark, cli = str(args.benchmark.resolve()), str(args.cli.resolve())
    cases = 0

    def worker(mode, codec, source, output, original, success=True):
        result = subprocess.run([benchmark, '--worker', mode, codec, str(source),
                                 str(output), str(original)], capture_output=True,
                                text=True, timeout=180)
        assert (result.returncode == 0) == success, (mode, codec, result.stderr)
        if success:
            return parse_report(result.stdout)
        assert not result.stdout
        return None

    def run_case(backend, window, native, size):
        nonlocal cases
        cases += 1
        extent = EXTENTS[WINDOWS.index(window)]
        codec = codec_name(backend, window, native)
        with tempfile.TemporaryDirectory(prefix='marc-benchmark-boundary-') as directory:
            root = Path(directory)
            source = root / 'source'
            write_input(source, size)
            original_hash = digest(source)
            golden = root / 'cli.marc'
            subprocess.run([cli, 'encode', '--codec', codec, str(source), str(golden)],
                           capture_output=True, check=True, timeout=180)
            encoded, restored = root / 'encode', root / 'decode'
            encode = worker('encode', codec, source, encoded, size)
            assert digest(encoded / 'output') == digest(golden), codec
            decode = worker('decode', codec, encoded / 'output', restored, size)
            assert encode['input_bytes'] == decode['output_bytes'] == size
            assert digest(restored / 'output') == digest(source) == original_hash
            with (encoded / 'output').open('rb') as stream:
                header = stream.read(112)
            assert header[:4] == b'MARC'
            ids = (*struct.unpack_from('<4H', header, 12), *struct.unpack_from('<2H', header, 96))
            assert ids == identity(backend, window, native), (codec, ids)
            assert struct.unpack_from('<I', header, 20)[0] == extent
            assert struct.unpack_from('<I', header, 64)[0] == extent
            assert struct.unpack_from('<Q', header, 40)[0] == size
            if native and size == 1 and backend in ('tans', 'blocked-huffman'):
                # Preserve the old measurement tests' late terminal/padding
                # errors: a failed first frame must release no raw bytes.
                bad = bytearray((encoded / 'output').read_bytes())
                if backend == 'tans':
                    descriptor = struct.unpack_from('<I', bad, 112 + 36)[0]
                    struct.pack_into('<H', bad, 112 + 64 + descriptor, 1)
                else:
                    bad[-3] = 1
                broken = root / 'broken'
                broken.write_bytes(bad)
                worker('decode', codec, broken, root / 'refused', size, success=False)
                assert (root / 'refused/output').stat().st_size == 0
        print(codec, size, 'verified', flush=True)

    if args.section == 'blocked':
        for native, windows in ((True, ('64k', '1m', '4m', '8m')),
                                 (False, ('64k', '1m', '4m', '16m'))):
            for window in windows:
                extent = EXTENTS[WINDOWS.index(window)]
                for size in (0, 1, extent - 1, extent, extent + 1):
                    run_case('blocked-huffman', window, native, size)
    elif args.section == 'tans':
        for window in ('64k', '1m'):
            extent = EXTENTS[WINDOWS.index(window)]
            for native in (True, False):
                for size in (0, 1, extent - 1, extent, extent + 1):
                    run_case('tans', window, native, size)
        for window in ('4m', '16m', '64m'):
            run_case('tans', window, False,
                     max(8388609, EXTENTS[WINDOWS.index(window)] + 1))
    elif args.section == 'large':
        for window in ('32m', '64m'):
            run_case('rans', window, True, EXTENTS[WINDOWS.index(window)] + 1)
        run_case('rans', '64m', False, 67108865)
        for window in WINDOWS:
            run_case('adaptive-huffman', window, True, EXTENTS[WINDOWS.index(window)] + 1)
        for window in CONTEXTUAL:
            run_case('adaptive-huffman', window, False, EXTENTS[WINDOWS.index(window)] + 1)
    else:
        # Retained independently assembled finite vectors from the 4/8-MiB
        # admission probes. One-symbol literal payloads fill token capacities;
        # they do not depend on an encoder selecting overlapping LZ matches.
        for window, contexts, table_entries in (('4m', 54, 4636), ('8m', 55, 4647)):
            extent = EXTENTS[WINDOWS.index(window)]
            codec = codec_name('blocked-huffman', window, True)
            with tempfile.TemporaryDirectory(prefix='marc-benchmark-literal-') as directory:
                root = Path(directory)
                source = root / 'source'
                write_input(source, extent, repeated=True)
                worker('encode', codec, source, root / 'encoded', extent)
                with (root / 'encoded/output').open('rb') as stream:
                    header = stream.read(112)
                descriptor = bytearray(35)
                struct.pack_into('<I', descriptor, 0, 2 * extent)
                descriptor[9:12] = bytes((15, 5, 3))
                struct.pack_into('<H', descriptor, 12, contexts)
                struct.pack_into('<I', descriptor, 16, table_entries)
                descriptor[34] = 65
                frame = bytearray(64)
                frame[:4] = b'MRF2'
                struct.pack_into('<H', frame, 4, 64)
                struct.pack_into('<8I', frame, 16,
                                 extent, extent, 2 * extent, 2 * extent, 0, 35, 0, 0)
                archive = root / 'literal.marc'
                archive.write_bytes(header + frame + descriptor)
                worker('decode', codec, archive, root / 'literal', extent)
                assert digest(root / 'literal/output') == digest(source)
                assert archive.stat().st_size == 211
                cases += 1
            print(codec, 'independent full-literal vector verified', flush=True)
    print(f'{args.section}: {cases} public boundary/vector cases passed')


if __name__ == '__main__':
    main()
