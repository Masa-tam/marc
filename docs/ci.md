# CI validation scopes

Pushes and pull requests run the regular Windows/MSVC and Ubuntu/Ninja suites,
the installed-package matrix, sanitizer fuzz smoke jobs, and interoperability
bundle generation. Benchmark smoke tests and deterministic CLI fuzz campaigns
remain enabled. The regular suites exclude only the explicit `ci-deep` label.
Successful regular CI is not evidence that the complete suite passed.

The same workflow checks nightly at 18:17 UTC whether the exact commit already
has successful complete validation. Only that confirmed success suppresses
the nightly build, tests, package, fuzz and bundle jobs. Unverified, failed,
cancelled or still-running validations do not suppress work. An unavailable
history API also causes validation to run conservatively. A short history
check job still runs on a skipped night.

It can also be started from Actions using **Run workflow** with
`full_validation` enabled (the default). Manual runs always execute, even for
an already validated commit. Push/PR regular runs are also unconditional.
Select the exact candidate ref and
record its commit and run ID. Release qualification requires complete success
on that commit, together with package, fuzz and exchange gates. A nightly run
of a different commit does not satisfy that requirement.

The history check requires a successful run of this same workflow and exact
SHA, plus the successful **Complete validation succeeded** marker job in
that run's recorded attempt. That marker depends on both platform suites,
both fuzz jobs and the entire installed-package matrix, and only runs for
complete scope. Regular successes and skipped-night successes cannot serve
as complete evidence. Older runs without the marker are conservatively
revalidated. A prior confirmed success remains reusable even if a separate
later run fails; manual execution can always force a fresh result.

The gate uses the read-only GitHub
[workflow runs](https://docs.github.com/en/rest/actions/workflow-runs#list-workflow-runs-for-a-workflow)
and [run-attempt jobs](https://docs.github.com/en/rest/actions/workflow-jobs#list-jobs-for-a-workflow-run-attempt)
APIs with pagination. Only the planning job requests `actions: read`.
The history policy has ten offline regression tests, covering success scope,
failed/cancelled runs, exact commit/workflow matching, run attempts,
pagination, API failures, and manual execution/output. They passed locally,
alongside the generated CTest partition and documentation checks. Hosted API
authorization and dispatch remain to be confirmed after push.

`cmake/MarcCiTestLabels.cmake` contains the reviewed inventory of long tests.
The initial inventory uses tests taking at least 60 seconds on either hosted
platform in the 2026-10-20 logs. CLI fuzz campaigns are an explicit exception
and remain regular. New tests default to regular; they must be measured before
being moved. Both parts of the split 64-MiB boundary campaign are deep tests.
No test is disabled, and no original case, recipe, or assertion is removed.

The test `marc_ci_test_partition` checks the generated CTest inventories:
regular and deep must be nonempty, disjoint, and cover the complete suite. It
also checks that fuzz/benchmark tests are not moved to deep, that both 64-MiB
campaign sections are registered, and that past-schema compatibility remains
in complete validation. Build all targets before executing either suite.

For example, after configuring and building the Release preset:

```console
ctest --preset windows-msvc-release --parallel 2 --label-exclude "^ci-deep$" --no-tests=error
ctest --preset windows-msvc-release --parallel 2 --label-regex "^ci-deep$" --no-tests=error
ctest --preset windows-msvc-release --parallel 2 --no-tests=error
```

Use `ninja-release` for Ubuntu. The first command runs regular validation; the
second runs only deep tests; the last runs all tests. CTest's JSON inventory
and label listing can be inspected without running codecs:

```console
ctest --preset ninja-release --show-only=json-v1
ctest --preset ninja-release --print-labels
```

The 64-MiB CLI script accepts `--section boundaries` and `--section stress`.
The former retains boundary, far-distance oracle, malformed-input,
transactional output, identity, and CLI usage checks. The latter retains both
full incompressible frames, restoration, and byte-identical re-encoding.
Calling the script without `--section` still runs both. Each invocation saves
its own logs and qualification JSON, including its section. The two CTests
share the existing `marc_large_window` resource lock.

The initial scope change keeps the measured two-worker CTest limit and existing
serial/lock properties. Raising concurrency, introducing compiler caches, or
reducing build targets is a separate measured change; the scope change alone
does not establish a particular elapsed time. Compare subsequent hosted runs
against the initial Windows build/test durations (about 38/117 minutes) and
Ubuntu durations (about 20/143 minutes). Bundle generation time is additional.
Bundles from a regular run require complete validation at the same revision
before release qualification; their schema and external verification contract
are unchanged.

The 2026-10-21 scope change was checked with configuration generation, the
partition audit, documentation validation, five retained API/CLI/HashTap and
benchmark smoke tests, and both complete 64-MiB script sections. The original
21 assertion expressions and nine stress statements were also compared with
the previous script's syntax tree. These checks passed; execution of the
revised Actions workflow and its hosted elapsed times remain unconfirmed.
