"""Offline history/attempt regressions for scheduled CI deduplication."""
from pathlib import Path
from contextlib import redirect_stdout
import io
import os
import sys
import tempfile
import unittest
from unittest.mock import patch
import urllib.error

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from ci_validation_gate import decide, main, MARKER_JOB


SHA = 'a' * 40
BASE = '/repos/owner/repo/actions'


def run(**changes):
    return dict(dict(id=10, head_sha=SHA, workflow_id=7, event='schedule',
                     status='completed', conclusion='success', run_attempt=2), **changes)


def marker(**changes):
    return dict(dict(name=MARKER_JOB, status='completed', conclusion='success'), **changes)


class History:
    def __init__(self, runs=None, jobs=None):
        self.runs = runs if runs is not None else {1: []}
        self.jobs = jobs if jobs is not None else {1: []}
        self.calls = []

    def __call__(self, path):
        self.calls.append(path)
        if path == BASE + '/runs/99':
            return dict(workflow_id=7)
        page = int(path.rsplit('page=', 1)[1])
        if '/attempts/' in path:
            return dict(jobs=self.jobs[page])
        return dict(workflow_runs=self.runs[page])


class GateTests(unittest.TestCase):
    def decision(self, history, event='schedule'):
        return decide(event, SHA, '99', 'owner/repo', history)[0]

    def test_push_pr_and_manual_always_run_without_lookup(self):
        for event in ('push', 'pull_request', 'workflow_dispatch'):
            with self.subTest(event=event):
                history = History({1: [run()]}, {1: [marker()]})
                self.assertTrue(self.decision(history, event))
                self.assertEqual(history.calls, [])

    def test_unverified_commit_runs(self):
        self.assertTrue(self.decision(History()))

    def test_successful_complete_schedule_or_manual_is_reused(self):
        for event in ('schedule', 'workflow_dispatch'):
            with self.subTest(event=event):
                history = History({1: [run(event=event)]}, {1: [marker()]})
                self.assertFalse(self.decision(history))
                self.assertIn('/runs/10/attempts/2/jobs?', history.calls[-1])

    def test_unsuccessful_incomplete_wrong_commit_or_workflow_is_not_reused(self):
        cases = [dict(conclusion=c) for c in ('failure', 'cancelled', 'skipped', 'timed_out', None)]
        cases += [dict(status='in_progress'), dict(status='queued'), dict(head_sha='b' * 40),
                  dict(workflow_id=8), dict(id=99), dict(event='push'), dict(event='pull_request')]
        for change in cases:
            with self.subTest(change=change):
                history = History({1: [run(**change)]}, {1: [marker()]})
                self.assertTrue(self.decision(history))
                self.assertFalse(any('/attempts/' in p for p in history.calls))

    def test_regular_or_skipped_nightly_success_is_not_complete_evidence(self):
        for jobs in ([], [marker(name='Test (regular)')],
                     [marker(conclusion='skipped')], [marker(conclusion='cancelled')],
                     [marker(conclusion='failure')], [marker(status='in_progress')]):
            with self.subTest(jobs=jobs):
                self.assertTrue(self.decision(History({1: [run()]}, {1: jobs})))

    def test_success_is_still_reusable_after_a_later_failure(self):
        self.assertFalse(self.decision(History(
            {1: [run(id=11, conclusion='failure'), run()]}, {1: [marker()]})))

    def test_run_pagination(self):
        self.assertFalse(self.decision(History(
            {1: [run(event='push')] * 100, 2: [run()]}, {1: [marker()]})))

    def test_job_pagination(self):
        self.assertFalse(self.decision(History({1: [run()]},
            {1: [marker(name='Other job')] * 100, 2: [marker()]})))

    def test_api_or_response_errors_run_conservatively(self):
        for error in (urllib.error.HTTPError('https://example.invalid', 403, '', {}, None),
                      urllib.error.URLError('offline'), ValueError('invalid JSON'),
                      KeyError('run_attempt'), TypeError('incomplete response')):
            with self.subTest(error=error):
                def unavailable(_):
                    raise error
                self.assertTrue(self.decision(unavailable))

    def test_manual_entrypoint_writes_action_outputs_without_network(self):
        with tempfile.TemporaryDirectory(prefix='ci-gate-test-', dir=Path.cwd()) as directory:
            output = Path(directory) / 'output'
            summary = Path(directory) / 'summary'
            with patch.dict(os.environ, dict(GITHUB_EVENT_NAME='workflow_dispatch',
                    GITHUB_SHA=SHA, GITHUB_RUN_ID='99', GITHUB_REPOSITORY='owner/repo',
                    GITHUB_OUTPUT=str(output), GITHUB_STEP_SUMMARY=str(summary))), \
                    patch('urllib.request.urlopen') as network, redirect_stdout(io.StringIO()):
                main()
            network.assert_not_called()
            self.assertEqual(output.read_text(), 'run=true\n')
            self.assertIn('never deduplicated', summary.read_text())


if __name__ == '__main__':
    unittest.main()
