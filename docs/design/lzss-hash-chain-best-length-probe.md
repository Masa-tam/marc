# HashChain best-length probe experiment

Status: private matcher pilot/full campaigns (BM-0093/BM-0094) and selected
whole-codec pilot (BM-0095) completed, 2026-09-23. All-member whole-codec
evaluation remains pending. This document does not authorize a production
matcher or format change.

## Motivation and boundary

BM-0091 isolates `find_match` as the dominant part of token production on
many 4 MiB contextual rANS inputs. BM-0092 finds that `mr` visits about
5.16 billion HashChain candidates, almost all with a matching five-byte
prefix, while a global Binary Tree replacement has large regressions on
`sao` and `x-ray` and requires about 6.44 times the finder workspace.

Test a narrower, exact candidate rejection inside the existing HashChain
search. The baseline matcher, public strategy enum, encoder configuration,
frame format, decoder, and default route remain unchanged while the idea
is evaluated. The experiment must use the same workspace and bucket/link
construction as the production HashChain; no additional per-input table or
unbounded allocation is permitted.

## Candidate rule and correctness argument

At a valid query position, the baseline visits candidates from newest to
oldest and only replaces `best` when a candidate has a *strictly longer*
match. Its equal-length tie therefore retains the nearest distance. Let
`L = best.length`. Before the ordinary byte-by-byte comparison of an older
candidate, the private variant MAY compare the single byte at offset `L`:

```text
if L > 0 and L < maximum_length and
   input[position + L] != input[candidate + L]:
    skip this candidate's full comparison
```

If those bytes differ, that candidate cannot match more than `L` bytes,
regardless of whether its earlier prefix matches. It cannot replace the
current best. If they agree, run the unchanged full comparison and normal
best update. A best match of `maximum_length` already ends the baseline
search; the probe never reads at that offset. Because `candidate < position`
and `position + L < input.size()`, `candidate + L` is also in bounds. This
argument applies to overlap-copy matches because both bytes are read from
the supplied input, not from an evolving decoder history. A five-byte
hash collision is harmless: a probe hit still receives the full comparison.

This proof depends on the existing newest-to-oldest traversal and strict
`length > best.length` update. A later tie-break or traversal change must
invalidate the proof and its tests before reusing the probe. The variant
must preserve full token sequence and canonical token fingerprint for
every input, not merely round-trip output.

## Private implementation contract

- Keep the current production `LzssHashChainMatchFinder` behavior unchanged.
  Use an explicit, compile-time private variant for the probe; do not add a
  runtime branch to the production inner loop. If shared code is factored,
  verify baseline token identity and diagnostic counters after refactoring.
- Count a visited candidate before probing. Add private diagnostic counts
  for probe comparisons and probe-pruned candidates. The existing prefix
  match/mismatch counts classify only candidates that reach the full scan;
  for this variant the invariant is `candidates = prefix_matches +
  prefix_mismatches + probe_pruned`. Count the probe byte comparison in
  total byte comparisons, but do not mislabel it as a full-scan extension.
  Preserve the baseline statistic semantics and validation unchanged.
- The probe performs no allocation, clock read, unbounded retry, or decoder
  operation. Its failure surface is limited to the existing bounded query
  and checked diagnostic counters.
- Extend only the private match-finder benchmark with a distinct strategy
  label and report fields. Its verification pass collects statistics and
  token fingerprint; its separate timing pass disables statistics. Reject
  a mismatch against the baseline before interpreting time.

## Test and measurement sequence

The selected pilot is frozen in
`benchmarks/experiments/silesia-hash-chain-best-length-probe-v1.json`.
It runs three independent attempts per member/strategy, ordered by member,
attempt, then baseline/probe. It checkpoints every validated record and
requires identical build/source/manifest/Corpus identity on resume.
The output reports per-member medians and raw records; selected inputs
cannot justify a production policy or an all-Corpus conclusion.

After BM-0093, the separate
`benchmarks/experiments/silesia-hash-chain-best-length-probe-full-v1.json`
freezes all twelve members in Corpus order, with three baseline/probe pairs
per member (72 independent processes). All other measurement and resource
conditions match the pilot. It records each member's medians and slowdowns;
aggregate speedup divides the sums of baseline/probe member median times.
The full campaign cannot reuse the pilot checkpoint or authorize a default
change. Any regression must be retained and considered before whole-codec
evaluation; no post-observation input selection or threshold tuning is allowed.

The private benchmark accepts `hash-chain-best-length-probe-exact` in
`--frames`, `--frames-limited`, and `--synthetic` modes. It verifies the
baseline token fingerprint and candidate count before timing, reusing the
same workspace. Both verification passes are outside the statistics-disabled
timing pass. Reports add `hash_chain_best_length_probe_comparisons` and
`hash_chain_best_length_probe_pruned_candidates`; baseline reports set both
to zero. The verification overhead is not whole-codec throughput.

1. Add hand-checkable candidate vectors and baseline/private token equality
   tests before activating the variant in the benchmark. Include no-best,
   probe miss, probe hit followed by an earlier mismatch, later longer
   match, nearest-distance tie, overlapping match, maximum length, frame
   end, window boundary, and wrapped link storage.
2. Compare complete token sequences and fingerprints over deterministic
   binary/repetitive inputs and varied frame splits. Require identical
   candidate visitation order and exact results; only comparison-work
   counters may differ. Fuzz or exhaustive bounded tests may add evidence,
   but never replace fixed regression vectors.
3. On a clean pinned MSVC x64 Release build, measure the existing HashChain
   and private probe on selected `mr`, `sao`, and `x-ray` under identical
   4 MiB frame/window and hard limits, with independent process attempts.
   Record input/build/revision identities, workspace, token fingerprint,
   candidates, probe-pruned fraction, byte comparisons, and isolated
   statistics-disabled time. Treat this as a pilot, not a timing gate.
4. Only if the pilot demonstrates useful savings without unacceptable
   regressions, freeze a separate all-member experiment and full-codec
   output/throughput/ratio/memory checks. Decide on any production change
   separately. If the probe rarely rejects `mr` candidates or materially
   slows `sao`/`x-ray`, retain the result as a negative experiment and leave
   the default unchanged.

The same-archive requirement also covers contextual rANS once a production
candidate is considered. This private experiment alone does not establish
such an archive identity or an uninstrumented whole-codec speedup.

### Typed-token integration prerequisite

The private `encode_lzss_typed_tokens_hash_chain_best_length_probe_single_pass`
entry reuses the existing single-pass parser, buffer validation, and production
HashChain workspace calculation. It does not change the public strategy or
the production caller. Reference/canonical-byte equality fixtures also exercise
this entry, with an explicit maximum-match-length 256 boundary test for the
contextual codec (the isolated matcher campaign used 258). A short workspace
must fail without writing tokens. Whole-stream archive identity and throughput
remain a separate next gate; this entry alone does not establish either.

### Private contextual rANS stream route

The internal frame encoder and streaming encoder accept a default-false
`private_best_length_probe` experiment switch. It is not a public config,
strategy enum, profile, or serialized field. Only HashChain exact accepts it;
BinaryTree and nested tokenization timing combinations are rejected. The
probe uses a separate compile-time parser instantiation and the same frame
validation, entropy coding, serialization, and workspace accounting as the
baseline. Production callers omit the switch and retain their existing route.

Regression fixtures compare complete baseline/probe archives for empty,
binary, repetitive, exact/partial/multiple-frame inputs with large buffers
and one-byte input/output buffers, then decode with the ordinary decoder.
These bounded fixtures are not full-Corpus performance evidence. The next
gate is an uninstrumented whole-codec benchmark with archive identity,
round-trip, ratio, and workspace checks before timing is interpreted.

### Uninstrumented whole-codec comparison modes

`marc_lzss_contextual_rans_phase_benchmark` provides two private modes:

```text
--best-length-probe-baseline <input> [iterations]
--best-length-probe-candidate <input> [iterations]
```

Both use the public 4 MiB contextual rANS profile, then independently check
that internal stream configuration and workspace match the public query.
Its current default match range is 5..258; the earlier 256 regression is a
supported parameter boundary fixture, not the benchmark profile default.
The public encoder supplies an untimed archive oracle and the ordinary
public decoder verifies it. Each private trial must reproduce every archive
byte; each timed decode must recover every source byte. Comparisons and
hashes are outside the measured intervals. No phase or token-loop timing
observer is enabled. Both sides time the same internal encoder process call;
the decoder process call is measured separately through the public API.
File I/O, allocation, partitioning, construction, destruction, and post-run
comparisons are excluded from reported encode/decode durations. Required
validation and frame/model work inside `process` remain included.

The versioned report `lzss-contextual-rans-best-length-probe-v1` records input
and archive hashes/sizes, encoded-to-input ratio (zero for empty input),
actual frame/window/match parameters, per-iteration nanoseconds and MiB/s,
and encoder/decoder queried workspace bytes. Peak workspace is the larger
of the two sequential codec workspaces, not process RSS: input/archive oracle,
decoded output, alignment slack, objects and allocator overhead are excluded.
Zero-duration throughput is reported as zero rather than infinity. This
metric must not be described as total benchmark process memory.

The smoke test checks both modes on repository text and empty input, two
iterations, identical archive/config/workspace identities, uninstrumented
report structure, and invalid iteration rejection. It imposes no speed gate.
Existing phase and nested-timing report formats remain unchanged. A frozen
checkpointed Corpus campaign and any production decision remain separate.

### Frozen whole-codec pilot and restart contract

`benchmarks/experiments/silesia-contextual-rans-best-length-probe-v1.json`
freezes `mr`, `sao`, and `x-ray`, three attempts per member, baseline then
probe in each attempt, and one iteration in each independent process.
The eighteen-point grid uses the public 4 MiB profile (match range 5..258),
MSVC x64 Release `/O2 /Ob2 /DNDEBUG`, and a 600-second child timeout.
This is a selected-member pilot, not permission to promote the default.

Run `tools/run_silesia_contextual_rans_best_length_probe.py` once to process
the grid. `--max-new-records N` optionally bounds the number of new points;
the same command resumes after interruption. The default manifest, build,
Corpus and ignored results paths are derived from the repository root.
The runner requires a clean source revision and verifies the supplied Corpus.
Checkpoint identity includes the manifest hash, source revision, build path,
compiler/project/executable identity and Corpus sizes/hashes.

Only a validated canonical prefix is accepted on restart. Every successful
point is checked for exact report fields, input identity, profile parameters,
finite positive timing, consistent throughput/ratio and sequential workspace
accounting. Archive hashes/sizes, ratio and workspace must agree between
strategies and across attempts. Each point is atomically checkpointed;
timeout, child failure or invalid report leaves the last checkpoint intact.
No complete result is published before all eighteen points exist. Completed
replay checks the result against the checkpoint without rerunning children
or rewriting either file. An existing complete result beside an incomplete
checkpoint is an error rather than a reason to overwrite it.

The summary reports per-member encode/decode medians and paired speedups;
all underlying reports remain available. Decode timing uses the unchanged
decoder and identical archives, so differences there must not be attributed
to a new decoding algorithm. The selected campaign is completed in BM-0095;
a full-Corpus whole-codec campaign still requires its own frozen contract
before execution.
