"""Measure public profiles in fresh directional workers; verify every sample."""
import argparse
import hashlib
import json
import math
import platform
from pathlib import Path
import re
import statistics
import subprocess
import sys

from verify_silesia_corpus import VerificationError, verify_directory


def digest(path):
    hasher = hashlib.sha256()
    with path.open('rb') as source:
        for data in iter(lambda: source.read(1024 * 1024), b''):
            hasher.update(data)
    return hasher.hexdigest()


def positive(value):
    number = int(value)
    if not 1 <= number <= 100:
        raise argparse.ArgumentTypeError('must be in 1..100')
    return number


def parse_report(text):
    result = {}
    for line in text.splitlines():
        key, separator, value = line.partition('=')
        if not separator or key in result:
            raise RuntimeError('invalid or duplicate worker report field')
        result[key] = value
    for key in ('process_seconds', 'lifecycle_seconds'):
        result[key] = float(result[key])
        if not math.isfinite(result[key]) or result[key] < 0:
            raise RuntimeError(f'invalid {key}')
    for key in ('input_bytes', 'output_bytes', 'io_buffer_bytes',
                'peak_resident_bytes', 'resource_bytes', 'resource_limit_bytes',
                'caller_workspace_bytes'):
        result[key] = int(result[key])
        if result[key] < 0:
            raise RuntimeError(f'invalid {key}')
    if result['timing_contract'] != 'chunked-process-v1':
        raise RuntimeError('unsupported timing contract')
    if result['resource_scope'] not in (
            'caller-workspace', 'aggregate-admission', 'initial-only', 'capacity-only'):
        raise RuntimeError('unknown resource scope')
    if result['peak_resident_bytes'] == 0:
        raise RuntimeError('missing physical memory observation')
    if result['lifecycle_seconds'] < result['process_seconds']:
        raise RuntimeError('lifecycle time does not include process time')
    return result


def measure(args):
    executable = args.benchmark.resolve(strict=True)
    inventory = subprocess.run([str(executable), '--list'], check=True,
                               capture_output=True, text=True, timeout=60).stdout.splitlines()
    if (not inventory or len(inventory) != len(set(inventory))
            or any(not re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*', x) for x in inventory)):
        raise RuntimeError('invalid benchmark profile inventory')
    codecs = args.codec if args.codec else inventory
    if len(codecs) != len(set(codecs)) or set(codecs) - set(inventory):
        raise ValueError('unknown or duplicate codec selection')
    if args.silesia:
        verified = verify_directory(args.silesia)
        inputs = [(args.silesia / record.name).resolve(strict=True) for record in verified]
    else:
        inputs = [path.resolve(strict=True) for path in args.input]
    if len(inputs) != len(set(inputs)):
        raise ValueError('duplicate input selection')
    source_records = []
    for source in inputs:
        if not source.is_file() or source.stat().st_size > 1024 ** 3:
            raise ValueError('input must be a file no larger than 1 GiB')
        source_records.append(dict(path=str(source), bytes=source.stat().st_size,
                                   sha256=digest(source)))
    output = args.output.resolve()
    output.mkdir()  # Exclusive: never reuse or overwrite a previous campaign.
    metadata = dict(schema_version=1, timing_contract='chunked-process-v1',
                    benchmark=str(executable), benchmark_sha256=digest(executable),
                    platform=platform.platform(), python=sys.version,
                    iterations=args.iterations, warmups=args.warmups,
                    keep_artifacts=args.keep_artifacts,
                    io_buffer_bytes=65536, codecs=codecs, inputs=source_records)
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    summaries = []
    with (output / 'samples.jsonl').open('x', encoding='utf-8') as journal:
        for index, source in enumerate(inputs):
            original = source_records[index]
            for codec in codecs:
                samples = []
                archive_hash = None
                archive_bytes = None
                resource_reports = {}
                campaign = output / f'{index:03}-{codec}'
                campaign.mkdir()
                for iteration in range(args.warmups + args.iterations):
                    if digest(source) != original['sha256']:
                        raise RuntimeError('source changed during measurement')
                    archive = None
                    for direction in ('encode', 'decode'):
                        target = campaign / f'{iteration:03}-{direction}'
                        current_input = source if direction == 'encode' else archive
                        completed = subprocess.run(
                            [str(executable), '--worker', direction, codec,
                             str(current_input), str(target), str(original['bytes'])],
                            capture_output=True, text=True, check=True, timeout=args.timeout)
                        report = parse_report(completed.stdout)
                        if (report['codec'] != codec or report['direction'] != direction
                                or report['io_buffer_bytes'] != 65536
                                or report['input_bytes'] != current_input.stat().st_size):
                            raise RuntimeError('worker report identity/size mismatch')
                        result_file = target / 'output'
                        if report['output_bytes'] != result_file.stat().st_size:
                            raise RuntimeError('worker output size mismatch')
                        result_hash = digest(result_file)
                        if direction == 'encode':
                            archive = result_file
                            archive_bytes = report['output_bytes']
                            if archive_hash is None:
                                archive_hash = result_hash
                            if result_hash != archive_hash:
                                raise RuntimeError('non-deterministic archive')
                        elif (result_hash != original['sha256']
                              or report['output_bytes'] != original['bytes']):
                            raise RuntimeError('round trip verification failed')
                        resources = {key: report[key] for key in (
                            'resource_scope', 'resource_bytes', 'resource_limit_bytes',
                            'caller_workspace_bytes')}
                        if direction in resource_reports and resource_reports[direction] != resources:
                            raise RuntimeError('resource report changed between iterations')
                        resource_reports[direction] = resources
                        sample = dict(input_index=index, iteration=iteration,
                                      warmup=iteration < args.warmups,
                                      output_sha256=result_hash, **report)
                        journal.write(json.dumps(sample) + '\n')
                        journal.flush()
                        if not sample['warmup']:
                            samples.append(sample)
                    if not args.keep_artifacts:
                        # Both outputs have been verified. Remove only these
                        # known files and empty directories created by us.
                        for direction in ('encode', 'decode'):
                            target = campaign / f'{iteration:03}-{direction}'
                            (target / 'output').unlink()
                            target.rmdir()
                if digest(source) != original['sha256']:
                    raise RuntimeError('source changed during measurement')
                summary = dict(input_index=index, codec=codec, input_bytes=original['bytes'],
                               archive_bytes=archive_bytes, archive_sha256=archive_hash,
                               encoded_to_input_ratio=(archive_bytes / original['bytes']
                                                       if original['bytes'] else 0))
                for direction in ('encode', 'decode'):
                    selected = [x for x in samples if x['direction'] == direction]
                    seconds = statistics.median(x['process_seconds'] for x in selected)
                    summary[direction] = dict(
                        process_seconds_median=seconds,
                        mib_per_second=(original['bytes'] / 1024 ** 2 / seconds if seconds else 0),
                        lifecycle_seconds_median=statistics.median(
                            x['lifecycle_seconds'] for x in selected),
                        peak_resident_bytes_max=max(x['peak_resident_bytes'] for x in selected),
                        **resource_reports[direction])
                summaries.append(summary)
    if digest(executable) != metadata['benchmark_sha256']:
        raise RuntimeError('benchmark executable changed during measurement')
    totals = []
    for codec in codecs:
        selected = [x for x in summaries if x['codec'] == codec]
        raw = sum(x['input_bytes'] for x in selected)
        encoded = sum(x['archive_bytes'] for x in selected)
        total = dict(codec=codec, input_bytes=raw, archive_bytes=encoded,
                     encoded_to_input_ratio=encoded / raw if raw else 0)
        for direction in ('encode', 'decode'):
            seconds = sum(x[direction]['process_seconds_median'] for x in selected)
            total[direction] = dict(
                process_seconds_sum_of_input_medians=seconds,
                mib_per_second=raw / 1024 ** 2 / seconds if seconds else 0,
                peak_resident_bytes_max=max(x[direction]['peak_resident_bytes_max'] for x in selected))
        totals.append(total)
    report = dict(**metadata, measurements=summaries, totals=totals)
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'verified {len(summaries)} profile/input measurements: {output / "report.json"}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--benchmark', type=Path, required=True)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--input', type=Path, action='append')
    inputs.add_argument('--silesia', type=Path,
                        help='verify and measure the twelve local corpus files; no download')
    parser.add_argument('--codec', action='append', help='repeat to select profiles; default: all')
    parser.add_argument('--iterations', type=positive, default=3)
    parser.add_argument('--warmups', type=int, choices=range(11), default=1)
    parser.add_argument('--timeout', type=positive_timeout, default=3600)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--keep-artifacts', action='store_true',
                        help='retain every archive/restored file instead of only reports')
    args = parser.parse_args()
    try:
        measure(args)
    except (OSError, ValueError, RuntimeError, VerificationError,
            subprocess.SubprocessError) as error:
        print(f'benchmark campaign failed: {error}', file=sys.stderr)
        if getattr(error, 'stderr', None):
            print(error.stderr, file=sys.stderr)
        return 1
    return 0


def positive_timeout(text):
    value = float(text)
    if not math.isfinite(value) or value <= 0:
        raise argparse.ArgumentTypeError('timeout must be finite and positive')
    return value


if __name__ == '__main__':
    raise SystemExit(main())
