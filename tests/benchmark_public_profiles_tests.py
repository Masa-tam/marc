"""Qualify benchmark profile dispatch against frozen v0.8.0 producer vectors."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from run_public_benchmark import parse_report


def fixture():
    pattern = b'ABABX\x00\xff'
    return bytes(i if i < 256 else 0 if i < 1280
                 else pattern[(i - 1280) % len(pattern)] if i < 3328
                 else (i * 73 + (i >> 3) * 19 + 41) & 255
                 for i in range(8193))


def main():
    executable = str(Path(sys.argv[1]).resolve())
    vectors = json.loads((Path(__file__).parent / 'data/benchmark-v080.json').read_text())
    listed = subprocess.run([executable, '--list'], check=True, capture_output=True,
                            text=True, timeout=60).stdout.splitlines()
    expected = {x['codec'] for x in vectors['archives']}
    assert len(listed) == 102 and len(set(listed)) == 102 and set(listed) == expected
    data = fixture()
    assert hashlib.sha256(data).hexdigest() == vectors['input_sha256']
    with tempfile.TemporaryDirectory(prefix='marc-benchmark-profiles-') as directory:
        root = Path(directory)
        source = root / 'fixture'
        source.write_bytes(data)

        def worker(direction, codec, input_file, output, expected_status=0, size=len(data)):
            result = subprocess.run(
                [executable, '--worker', direction, codec, str(input_file), str(output), str(size)],
                capture_output=True, text=True, timeout=60)
            assert result.returncode == expected_status, (codec, direction, result.stderr)
            if expected_status:
                assert not result.stdout
                return None
            return parse_report(result.stdout)

        for index, vector in enumerate(vectors['archives']):
            codec = vector['codec']
            encoded = root / f'{index}-encode'
            restored = root / f'{index}-decode'
            encoder = worker('encode', codec, source, encoded)
            archive = (encoded / 'output').read_bytes()
            assert len(archive) == vector['bytes'], codec
            assert hashlib.sha256(archive).hexdigest() == vector['sha256'], codec
            decoder = worker('decode', codec, encoded / 'output', restored)
            assert (restored / 'output').read_bytes() == data, codec
            assert encoder['input_bytes'] == decoder['output_bytes'] == len(data)
            for report in (encoder, decoder):
                if report['resource_limit_bytes']:
                    assert report['resource_bytes'] <= report['resource_limit_bytes'], codec
            if codec.startswith('lzss-position-distance'):
                direction_specific = codec in {
                    'lzss-position-distance-dynamic-range-' + suffix
                    for suffix in ('8m', '16m', '32m', '64m')}
                three_spans = codec in {
                    'lzss-position-distance-dynamic-range',
                    'lzss-position-distance-dynamic-range-1m',
                    'lzss-position-distance-dynamic-range-4m'}
                scopes = ('initial-only', 'capacity-only') if direction_specific else (
                    ('caller-workspace', 'caller-workspace') if three_spans else
                    ('aggregate-admission', 'aggregate-admission'))
                assert (encoder['resource_scope'], decoder['resource_scope']) == scopes, codec
                normal = subprocess.run([executable, codec, str(source), '1'], check=True,
                                        capture_output=True, text=True, timeout=60)
                report = dict(line.split('=', 1) for line in normal.stdout.splitlines())
                assert int(report['encoded_bytes']) == len(archive), codec
                assert int(report['codec_peak_workspace_available']) == int(three_spans), codec
                if not three_spans:
                    assert report['codec_peak_workspace_bytes'] == '0', codec
            # Successful directories are exclusive; a second invocation cannot
            # replace either the archive or its metadata/report destination.
            if index == 0:
                worker('encode', codec, source, encoded, expected_status=1)
                assert (encoded / 'output').read_bytes() == archive
        empty = root / 'empty'
        empty.write_bytes(b'')
        for index, codec in enumerate(x for x in listed if x.startswith('lzss-position-distance')):
            encoded = root / f'empty-{index}-encode'
            restored = root / f'empty-{index}-decode'
            worker('encode', codec, empty, encoded, size=0)
            worker('decode', codec, encoded / 'output', restored, size=0)
            assert (restored / 'output').read_bytes() == b'', codec
        assert source.read_bytes() == data
        # Wrong original size and invalid directions cannot produce a report.
        worker('encode', 'checksum-raw', source, root / 'wrong-size',
               expected_status=1, size=len(data) + 1)
        worker('invalid', 'checksum-raw', source, root / 'wrong-mode', expected_status=2)
        assert not (root / 'wrong-mode').exists()

        runner = Path(__file__).resolve().parents[1] / 'tools/run_public_benchmark.py'
        campaign = root / 'campaign'
        subprocess.run([sys.executable, '-B', str(runner), '--benchmark', executable,
                        '--input', str(source), '--codec', 'checksum-raw',
                        '--codec', 'lzss-position-distance-dynamic-range-8m',
                        '--iterations', '2', '--warmups', '1', '--output', str(campaign)],
                       check=True, capture_output=True, text=True, timeout=120)
        report_bytes = (campaign / 'report.json').read_bytes()
        report = json.loads(report_bytes)
        samples = [json.loads(line) for line in (campaign / 'samples.jsonl').read_text().splitlines()]
        assert len(report['measurements']) == 2 and len(samples) == 12
        assert sum(x['warmup'] for x in samples) == 4
        assert not list(campaign.glob('*/output'))
        assert not list(campaign.glob('*/*/output'))
        for summary in report['measurements']:
            for direction in ('encode', 'decode'):
                timed = [x for x in samples if x['codec'] == summary['codec']
                         and x['direction'] == direction and not x['warmup']]
                assert summary[direction]['peak_resident_bytes_max'] == max(
                    x['peak_resident_bytes'] for x in timed)
                assert summary[direction]['process_seconds_median'] == sum(
                    x['process_seconds'] for x in timed) / 2
        repeated = subprocess.run([sys.executable, '-B', str(runner), '--benchmark', executable,
                                   '--input', str(source), '--codec', 'checksum-raw',
                                   '--output', str(campaign)], capture_output=True, timeout=60)
        assert repeated.returncode != 0 and (campaign / 'report.json').read_bytes() == report_bytes
        refused = root / 'unverified-corpus'
        invalid_corpus = subprocess.run(
            [sys.executable, '-B', str(runner), '--benchmark', executable,
             '--silesia', str(root), '--output', str(refused)], capture_output=True, timeout=60)
        assert invalid_corpus.returncode != 0 and not refused.exists()
    print('102 public benchmark profiles match frozen v0.8.0 bytes and restore exactly')


if __name__ == '__main__':
    main()
