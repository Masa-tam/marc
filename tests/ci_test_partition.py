"""Audit CTest selections without executing the selected tests."""
import argparse
import json
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ctest', required=True)
    parser.add_argument('--build', required=True)
    parser.add_argument('--config', default='Release')
    args = parser.parse_args()

    def inventory(*selection):
        result = subprocess.run(
            [args.ctest, '--test-dir', args.build, '-C', args.config,
             '--show-only=json-v1', *selection],
            check=True, capture_output=True, text=True, timeout=120)
        return {test['name']: test for test in json.loads(result.stdout)['tests']}

    complete = inventory()
    regular = inventory('--label-exclude', '^ci-deep$')
    deep = inventory('--label-regex', '^ci-deep$')
    if not regular or not deep:
        raise RuntimeError('Both regular and deep selections must contain tests')
    if set(regular) & set(deep) or set(regular) | set(deep) != set(complete):
        raise RuntimeError('Regular and deep must partition the complete inventory')
    for name in deep:
        if 'fuzz' in name or 'benchmark' in name:
            raise RuntimeError(f'Fuzz/benchmark coverage moved out of regular CI: {name}')
    for name, section in (
            ('marc_cli_position_distance_64m_boundaries', 'boundaries'),
            ('marc_cli_position_distance_64m_stress', 'stress')):
        command = deep[name]['command']
        if command[-2:] != ['--section', section]:
            raise RuntimeError(f'Wrong 64 MiB campaign selection: {name}')
    if 'marc_interoperability_schema_compatibility' not in deep:
        raise RuntimeError('Past-schema compatibility must remain in complete validation')
    print(json.dumps(dict(complete=len(complete), regular=len(regular),
                          deep=len(deep), deep_tests=sorted(deep)), indent=2))


if __name__ == '__main__':
    main()
