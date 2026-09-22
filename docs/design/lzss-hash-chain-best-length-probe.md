# HashChain best-length probe experiment

Status: private matcher and benchmark connected, 2026-09-23; selected-input
measurement pending. This document does not
authorize a production matcher or format change.

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
