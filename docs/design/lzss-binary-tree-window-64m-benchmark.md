# LZSS BinaryTree Exact 64 MiB comparison experiment

## 1. Purpose and scope

This document defines the reproducible evidence gate for the candidate 64-MiB
Contextual LZSS family. It compares public `HashChain Exact` and `BinaryTree
Exact` at sixteen- and sixty-four-MiB windows while holding the frame boundary,
input Corpus, parser rules, compiler build, and measurement procedure fixed.

The experiment changes no codec, profile, format identity, ABI, default,
selector, decoder, or interoperability archive. Its timing and token-reduction
results are descriptive. Exact token equivalence, bounded resource use, and
complete evidence are success conditions; a particular speed or compression
ratio is not.

The previous 16-MiB experiment cannot supply the comparison baseline because
it used a 16-MiB frame. This experiment remeasures the 16-MiB window with a
64-MiB frame so both windows observe identical reset boundaries. Every verified
Silesia member is smaller than 64 MiB, but the runner still validates the
reported frame count rather than relying on that observation implicitly.

## 2. Fixed matrix

The complete experiment uses the canonical twelve-member verified Silesia
Corpus and exactly this configuration:

```text
frame bytes                     67,108,864
window bytes                    16,777,216; 67,108,864
strategies                      hash-chain-exact; binary-tree-exact
timed iterations                1
maximum internal buffered bytes 2,147,483,648
planned records                 12 * 2 * 2 = 48
```

Canonical order is manifest member order, ascending window, then HashChain
before BinaryTree. Each record is a separate benchmark process. The runner
performs no network access, download, extraction, or Corpus generation and
starts no benchmark until all twelve members pass the repository's fixed size
and SHA-256 manifest.

## 3. Exact memory policy

For `F = 67,108,864`, the current checked 64-bit calculators require:

```text
HashChain workspace  = 524,288 + 4F
                     = 268,959,744 bytes
HashChain aggregate  = F + workspace
                     = 336,068,608 bytes

BinaryTree workspace = 29F
                     = 1,946,157,056 bytes
BinaryTree aggregate = F + workspace
                     = 2,013,265,920 bytes

explicit policy      = 2,147,483,648 bytes (2 GiB)
BinaryTree headroom  = 134,217,728 bytes
```

The executable remains authoritative: it calculates the selected workspace,
checks input plus workspace against the explicit policy, and rejects overflow,
unsupported native extents, short or misaligned storage, and aggregate excess
before processing. The runner requires the report to contain the exact
workspace above and the common 2-GiB policy. One byte below each aggregate is
covered by executable tests without attempting the full Corpus run.

The experiment requires a 64-bit process. It does not lower operating-system
memory safety, request parallel records, or promise that every host can commit
the BinaryTree workspace. Allocation failure is a failed record, not grounds
for silently reducing the frame or substituting a strategy.

## 4. Benchmark executable boundary

The existing explicit-limit command is sufficient and remains byte-compatible:

```text
marc_lzss_match_finder_benchmark --frames-limited \
  <hash-chain-exact|binary-tree-exact> <member> 1 \
  67108864 <16777216|67108864> 2147483648
```

The diagnostic untimed pass and diagnostic-free timed pass remain separate.
They must agree on input bytes, frame count, and token count. The report must
include mode, strategy, input/frame/window sizes, frame and token summaries,
SHA-256 token fingerprint, selected limit, exact workspace, diagnostic
counters, histogram, and finite nonnegative elapsed time.

No new benchmark mode is needed. Focused executable tests add the 64-MiB exact
workspace values, the 2-GiB equality boundary, one-byte-short rejection, and
invalid or overflowing argument rejection without reading the Silesia Corpus.

## 5. Dedicated runner and schemas

Add `tools/run_silesia_binary_tree_64m_experiment.py` with independent schemas:

```text
marc-silesia-binary-tree-64m-experiment-v1
marc-silesia-binary-tree-64m-checkpoint-v1
```

Do not modify the 16-MiB result or checkpoint schemas. Common parsing,
aggregation, manifest verification, and atomic JSON helpers may be factored
into a repository-owned module only when the existing runner's behavior and
tests remain unchanged.

For every member/window Exact pair, the runner requires equality of:

```text
token_count
literal_count
match_count
matched_bytes
token_fingerprint_sha256
```

It also requires `token_count == literal_count + match_count` and
`input_bytes == literal_count + matched_bytes`, exact workspace and policy,
complete query histograms, finite time, and the expected frame count. A
partial, duplicated, out-of-grid, reordered, contradictory, or non-Exact set
must not produce the final result.

The final result stores identity, verified manifest, fixed configuration, 48
canonical records, per-window/strategy aggregates, BinaryTree-to-HashChain
throughput ratios, per-member wins, matched-byte coverage, and the 16-to-64-MiB
token-count reduction. Window-to-window token equality is not required because
the larger window deliberately changes match opportunities.

## 6. Checkpoint and bounded execution

After each completely validated record, replace the checkpoint atomically in
the same directory. Flush file contents before replacement. The checkpoint
identity includes:

```text
schema and full Git revision
benchmark absolute path and SHA-256
runner and dependent source SHA-256 values
Corpus absolute path and complete verified manifest
fixed matrix, iteration count, policy, and exact workspace expectations
platform, Python, compiler, generator, architecture, and build label
```

Restored records pass through the normal validator. Any identity change,
unknown key, duplicate, corrupt report, command mismatch, or Exact-pair
contradiction rejects resume.

`--max-new-points N` requires a checkpoint, forbids final output, and stops
successfully only after saving at most `N` new complete records. Value zero
performs identity, checkpoint, manifest, and progress validation without
starting a benchmark. The value is not part of checkpoint identity and may
change between invocations. Only an unrestricted invocation with `--output`
after all 48 records publishes canonical result JSON.

Because one HashChain point may be long-running, checkpointing cannot promise
progress inside a benchmark process. Interruption leaves the last completed
record valid and reruns only the interrupted point. The runner launches one
process at a time and provides no automatic retry or strategy substitution.

## 7. Required tests

1. Freeze the 64-MiB frame, two windows, two strategies, 2-GiB limit, exact
   workspaces, and 48-record matrix.
2. Preserve existing benchmark modes and the 16-MiB runner byte-for-byte.
3. Validate complete reports, reconstruction identities, exact fingerprints,
   histograms, finite times, exact workspace, and expected frame count.
4. Reject wrong modes, policies, workspaces, commands, summaries, fingerprints,
   windows, strategies, member order, and record counts.
5. Use a fake benchmark to prove canonical order, aggregation, Exact-pair
   mismatch rejection, checkpoint resume, identity mismatch, corrupt records,
   zero-point validation, and bounded batches without Corpus-sized allocation.
6. Prove executable equality and one-short aggregate boundaries without
   allocating the full workspace.
7. Run all unit tests under MSVC, ClangCL, and CI; keep the real Corpus opt-in.
8. Store checkpoints and results only under ignored local result directories.

## 8. Staged execution

1. Commit this design, provenance, and test contract.
2. Add focused executable boundary tests if the current generic calculator
   coverage does not already freeze the 64-MiB values.
3. Implement the dedicated runner and dependency-free fake-runner tests.
4. Pass both toolchains and CI without executing the real Corpus matrix.
5. Execute the 48 points in bounded batches and publish one canonical local
   result after complete validation.
6. Record the result and review whether the measured benefit justifies format
   reservation and shared primitive implementation.

Steps one through four are complete. The dedicated runner implements the two
independent v1 schemas, canonical-prefix checkpoint recovery, exact workspace
and paired-token validation, bounded batches, and final 48-record publication.
Dependency-free fake-runner tests cover the full grid and resume path without
allocating the planned workspace or requiring the external Corpus. Real Corpus
execution remains the next explicit, opt-in evidence stage.

Even a strong BinaryTree result does not change the HashChain initializer
default or select a strategy automatically. Even a weak compression gain does
not invalidate the candidate mathematically; it informs whether its large
resource envelope is worth publishing.
