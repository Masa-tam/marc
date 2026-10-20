"""Skip scheduled work only after proven complete validation of this commit."""
import json
import os
from pathlib import Path
import time
import urllib.parse
import urllib.request


MARKER_JOB = 'Complete validation succeeded'


def should_run(event, sha, current_run_id, repository, get):
    # Manual execution is deliberately unconditional, including full reruns.
    if event != 'schedule':
        return True, 'Push/PR/manual execution is never deduplicated.'
    base = f'/repos/{repository}/actions'
    current = get(f'{base}/runs/{current_run_id}')
    workflow_id = current['workflow_id']
    query = urllib.parse.urlencode(dict(head_sha=sha, status='success', per_page=100))
    for page in range(1, 11):
        runs = get(f'{base}/workflows/{workflow_id}/runs?{query}&page={page}')['workflow_runs']
        for run in runs:
            if (str(run['id']) == str(current_run_id) or run['head_sha'] != sha
                    or run['workflow_id'] != workflow_id
                    or run['event'] not in ('schedule', 'workflow_dispatch')
                    or run['status'] != 'completed' or run['conclusion'] != 'success'):
                continue
            # Pin the attempt: do not reuse a successful job from an older,
            # partially rerun attempt whose complete result is now different.
            attempt = run['run_attempt']
            for job_page in range(1, 11):
                jobs = get(f'{base}/runs/{run["id"]}/attempts/{attempt}/jobs'
                           f'?per_page=100&page={job_page}')['jobs']
                if any(job['name'] == MARKER_JOB and job['status'] == 'completed'
                       and job['conclusion'] == 'success' for job in jobs):
                    return False, f'Complete validation already succeeded in run {run["id"]}, attempt {attempt}.'
                if len(jobs) < 100:
                    break
        if len(runs) < 100:
            break
    return True, 'No successful complete validation was confirmed for this commit.'


def decide(event, sha, current_run_id, repository, get):
    try:
        return should_run(event, sha, current_run_id, repository, get)
    except (OSError, ValueError, KeyError, TypeError):
        # An unavailable API, permission failure or incomplete response is not
        # evidence of success. Run validation rather than suppressing it.
        return True, 'History lookup unavailable; run validation conservatively.'


def main():
    env = os.environ
    deadline = time.monotonic() + 60

    def get(path):
        if time.monotonic() >= deadline:
            raise TimeoutError('History lookup time budget exhausted')
        request = urllib.request.Request(env['GITHUB_API_URL'] + path, headers={
            'Accept': 'application/vnd.github+json',
            'Authorization': 'Bearer ' + env['GH_TOKEN'],
            'X-GitHub-Api-Version': '2026-03-10',
            'User-Agent': 'marc-ci-validation-gate',
        })
        with urllib.request.urlopen(request, timeout=15) as response:
            return json.load(response)

    run, reason = decide(env['GITHUB_EVENT_NAME'], env['GITHUB_SHA'],
                         env['GITHUB_RUN_ID'], env['GITHUB_REPOSITORY'], get)
    with Path(env['GITHUB_OUTPUT']).open('a', encoding='utf-8') as output:
        output.write(f'run={str(run).lower()}\n')
    with Path(env['GITHUB_STEP_SUMMARY']).open('a', encoding='utf-8') as summary:
        summary.write(f'Validation decision: {reason}\n')
    print(reason)


if __name__ == '__main__':
    main()
