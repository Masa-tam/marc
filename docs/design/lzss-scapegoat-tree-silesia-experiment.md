# LZSS Scapegoat Tree Silesia Experiment

## 1. Purpose and status

This document freezes the first Corpus experiment that compares the private
Scapegoat Exact match finder with the existing AVL BinaryTree Exact and
private Red-Black Exact finders. It is an implementation experiment, not
admission of Scapegoat or Red-Black to a public codec selector, ABI, stream
format, profile, or default.

The experiment asks one narrow question: under the same bounded one-MiB frame
workload, does deterministic Scapegoat rebuilding improve useful work or
throughput relative to AVL and Red-Black while preserving the exact LZSS
parse? All three strategies are rerun from the same revision and executable;
the earlier Red-Black experiment is historical evidence, not a timing
baseline for this experiment.

## 2. Fixed matrix

The dedicated runner MUST use only these values:

| Property | Fixed value |
| --- | --- |
| Corpus | all 12 independently verified local Silesia members |
| Frame size | 1,048,576 input bytes |
| Window sizes | 65,536; 262,144; 1,048,576 bytes |
| Strategies | `binary-tree-exact`; `red-black-tree-exact`; `scapegoat-tree-exact` |
| Measured iterations | 1 |
| Processes | one member/window/strategy per process |
| Total records | 108 |

The runner provides no arguments that can alter this matrix. It performs no
network access and does not put the Corpus, checkpoint, or result files under
source control.

## 3. Canonical order and exact-result gate

For every member/window point, AVL runs first, Red-Black second, and Scapegoat
third. AVL is the baseline for both following candidates. Each candidate MUST
agree with AVL on:

- token count;
- literal count;
- match count;
- matched byte count;
- lowercase SHA-256 fingerprint of canonical typed tokens.

Each report MUST reconstruct the input extent from literals and matched bytes,
account for every query in its depth histogram, report finite nonnegative
time, report a nonnegative strategy-specific calculator workspace, and reject
diagnostic-counter overflow. A mismatch invalidates the experiment; timing
must never excuse a different parse.

## 4. Measurement and interpretation

The benchmark executable keeps its separation between an untimed diagnostic
pass and a timed pass with a null statistics pointer. Results aggregate input,
time, token coverage, strategy counters, query-depth histograms, and maximum
calculator workspace for every strategy/window pair.

The comparison reports Red-Black-to-AVL and Scapegoat-to-AVL throughput and
workspace ratios. It retains AVL's lifetime maximum-height statistic and the
exact final heights measured for Red-Black and Scapegoat. These height fields
have different scopes and MUST NOT be presented as equivalent maxima.

Scapegoat reporting additionally retains depth violations, ancestor steps,
subtree and whole-tree rebuilds, total and maximum rebuilt nodes, and maximum
structural node visits in one update. Query work and mutation work remain
separate. These counters and all timing values are diagnostic observations,
not conformance constants.

## 5. Checkpoint contract

The 108 independent processes may be executed in bounded batches. A
checkpoint freezes:

- result and checkpoint schema identifiers;
- Git revision;
- resolved benchmark path and SHA-256;
- resolved Corpus path and complete verified manifest;
- runner, shared-runner, and verifier SHA-256 values;
- the fixed experiment configuration;
- platform, Python, compiler, generator, build type, architecture, and build
  label.

A checkpoint is accepted only when that complete identity is unchanged.
Records form a canonical prefix of member, then window, then AVL, Red-Black,
and Scapegoat order. A candidate record without its AVL baseline, a duplicate,
a changed command, or any out-of-grid record is rejected. Once both candidates
for a point exist, each is rechecked against the baseline while loading the
checkpoint. Each completed process is saved atomically before the next begins.

## 6. Admission boundary

Successful completion permits documenting observed trade-offs and making a
separate admission decision. It does not add Scapegoat or Red-Black to
`LzssMatchFinderStrategy`, the C ABI, CLI codec selection, profiles, frame
encoders, interoperability archives, or stream metadata.

Public admission requires Exact identity at every point, no hard-limit or
single-update-work failure, and a separately documented reason that the
observed throughput, workspace, and latency trade-off benefits a defined use
case. Aggregate speed alone cannot hide a pathological single-update spike.
An inconclusive or unfavorable result leaves Scapegoat private without making
the experiment a failure.

## 7. Private file-frame seam

The internal match-finder benchmark accepts `scapegoat-tree-exact` through
ordinary `--frames` processing. It reuses the bounded frame reader, checked
Scapegoat workspace calculator, Exact parser, diagnostic validation, token
summary and fingerprint, and counter-free timed pass already exercised by the
synthetic route. It remains rejected by `--frames-limited`; the dedicated
experiment uses only the fixed default limit and matrix above.

The repository README smoke fixture makes HashChain, AVL, Red-Black, and
Scapegoat agree on the complete token summary and fingerprint. It also checks
the Scapegoat report's comparison, rebuilding, bounded single-update,
final-height, query-depth, and finite timing fields. This seam produces no
archive and is not a public codec selector.

## 8. Runner implementation

`tools/run_silesia_scapegoat_tree_experiment.py` implements this contract with
distinct result and checkpoint schemas. It verifies the local Corpus before
launch, fixes the complete 108-record order, runs one benchmark process per
record, validates the complete strategy-specific report, applies both Exact
identity gates, and atomically saves every accepted canonical-prefix record.
Its checkpoint identity covers the revision, benchmark executable and digest,
Corpus manifest and path, runner/helper sources, environment, and fixed
configuration.

`--max-new-points` permits bounded resumable batches without weakening the
matrix. A three-process local connection smoke completed the first canonical
AVL/Red-Black/Scapegoat triplet for `dickens` at 65,536 bytes and saved
`3/108`. The full 108-process experiment remains intentionally unclaimed until
the completed result has been produced and reviewed.
