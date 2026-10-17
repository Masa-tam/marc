"""Audit an appended exchange bundle against a retained, unmodified baseline."""
import argparse
import hashlib
import json
from pathlib import Path


def audit(baseline, candidate, additions):
    old = json.loads((baseline / 'manifest.json').read_text(encoding='utf-8-sig'))
    new = json.loads((candidate / 'manifest.json').read_text(encoding='utf-8-sig'))
    count = len(old['archives'])
    assert new['archives'][:count] == old['archives'], 'Existing entry order or representation changed'
    assert len(new['archives']) == count + additions, 'Unexpected number of additions'
    assert new['schema_version'] == old['schema_version'] + additions
    assert new['codec_set'] == f"marc-cli-v{new['schema_version']}"
    assert new['input'] == old['input'], 'Input descriptor changed'
    assert list(new['input'].items()) == list(old['input'].items()), 'Input property order changed'
    for before, after in zip(old['archives'], new['archives']):
        assert list(before.items()) == list(after.items()), 'Entry property order changed'
    checked = []
    for entry in [old['input']] + old['archives']:
        name = entry['file']
        original = (baseline / name).read_bytes()
        current = (candidate / name).read_bytes()
        assert original == current, f'Existing bytes changed: {name}'
        digest = hashlib.sha256(current).hexdigest()
        assert len(current) == entry['bytes'] and digest == entry['sha256']
        checked.append({'file': name, 'bytes': len(current), 'sha256': digest})
    return {'passed': True, 'baseline_archives': count,
            'candidate_archives': len(new['archives']), 'verified_existing_files': checked}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--additions', type=int, required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.baseline, args.candidate, args.additions)
    # Preserve any prior evidence rather than silently replacing it.
    with args.receipt.open('x', encoding='utf-8') as receipt:
        json.dump(result, receipt, indent=2)
        receipt.write('\n')
    print(f"Preserved {result['baseline_archives']} archives and the input byte-for-byte")
