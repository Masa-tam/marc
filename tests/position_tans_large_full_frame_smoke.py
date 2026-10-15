"""Maximum-frame, late-failure and public encoder boundary qualification."""
import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path
from position_tans_full_frame_vectors import fixtures, qualify_acceleration


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--window', type=int, choices=(8, 16, 32, 64), required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix=f'{args.window}m-', dir=args.output))
    size = args.window << 20
    n = size.bit_length() - 1
    checks = qualify_acceleration()
    receipts = []

    def invoke(mode, path, raw, chunk, capacity):
        command = [str(args.probe.resolve()), mode, str(path.resolve()), str(raw), str(chunk), str(capacity)]
        result = subprocess.run(command, capture_output=True, timeout=120)
        label = f'{len(receipts):02}-{mode}-{chunk}-{capacity}'
        (run / f'{label}.stdout').write_bytes(result.stdout)
        (run / f'{label}.stderr').write_bytes(result.stderr)
        assert result.returncode == 0, (command, result.returncode, result.stderr.decode(errors='replace'))
        record = json.loads(result.stdout)
        assert record['verified'] and record['peak_resident_bytes'] > 0
        assert record['queried_bytes'] > 0 and record['seconds'] > 0
        record.update(input_chunk=chunk, output_chunk=capacity, wire_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        receipts.append(record)
        print(json.dumps(record), flush=True)

    for kind, wire in fixtures(n).items():
        path = run / f'{kind}.marc'
        path.write_bytes(wire)
        raw = size - 1 if kind == 'late-expansion' else size + 1 if 'tail' in kind else size
        for chunk, capacity in ((65536, 65536), (17, 65521)):
            invoke(kind, path, raw, chunk, capacity)
        for field in ('consumed', 'produced', 'status', 'error_byte', 'error_bit'):
            assert receipts[-1][field] == receipts[-2][field], (kind, field)
    for raw in (size - 1, size, size + 1):
        path = run / f'encoded-{raw}.marc'
        invoke('encode', path, raw, 65536, 65536)
        invoke('tail' if raw > size else 'valid', path, raw, 17, 65521)
    (run / 'receipt.json').write_text(json.dumps({'window_bytes': size, 'scalar_crosschecks': checks, 'cases': receipts}, indent=2) + '\n')
    print(json.dumps({'verified': True, 'cases': len(receipts), 'window_bytes': size}), flush=True)


if __name__ == '__main__':
    main()
