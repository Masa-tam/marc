# LZSS HashChain end-to-end A/B experiment

Status: design fixed; runner and full-Corpus measurement not yet implemented.

## Purpose and scope

BM-0085 checked the 262,144-bucket production HashChain in a complete codec
pipeline on two Silesia members. This experiment extends that check to all
twelve locally supplied Silesia members. It measures the effect of the
encoder-only bucket-cap change on the public `lzss-contextual-rans-4m` route;
it does not choose a new cap, change the stream format, or replace the earlier
144-record match-finder selection result. A descriptive performance result
must not become a CI speed threshold.

The A build is the immediate pre-promotion source revision
`64f79321ec20169f2cc55787b0f637dc7075ea57` (65,536-bucket standard
HashChain). The B build is the promoted source revision
`fe11a20b0c5d3e79101f7567c97b70cf97ea28da` (262,144-bucket standard
HashChain). Use isolated clean
build trees, the same compiler and architecture, MSVC x64 Release, and the
same explicit `/O2 /Ob2 /DNDEBUG` Release flags. The existing benchmark
source and codec configuration must be identical. Do not compare a later
source revision or silently substitute the explicit legacy benchmark route.

## Fixed matrix and correctness gate

- Corpus: the twelve members in `tools/verify_silesia_corpus.py`, in its
  manifest order, verified against their recorded lengths and digests before
  starting. The corpus remains external and ignored; no download occurs.
- Codec: `lzss-contextual-rans-4m`, using the repository benchmark's normal
  configuration and one measured encode/decode iteration per child process.
- Records: one A and one B process per member (24 benchmark records). Alternate
  A/B execution order by member index to reduce systematic order bias. Each
  process performs its existing untimed round-trip check before measurement.
- Each member also requires one A and one B CLI archive encode under the same
  public codec configuration. Record complete-archive byte counts and SHA-256
  digests; both must agree exactly. Remove temporary archives only after their
  digests and byte counts have been checkpointed. The benchmark's encoded byte
  count must agree with the corresponding CLI archive count, or the result is
  invalid. If the benchmark and CLI configuration cannot be proven identical,
  stop rather than treating a digest comparison as evidence.
- Any process failure, round-trip failure, archive mismatch, malformed report,
  nonpositive time, or missing required value invalidates the affected member
  and the final all-member claim. Do not turn such a failure into a slow timing
  result or silently skip it.

The measured benchmark output includes encode/decode seconds, input and
encoded bytes, and queried encoder/decoder workspace. Archive identity is
checked independently by the CLI. Decode time is reported, but an encoder
search-policy change cannot by itself establish a decoder speedup.

## Preflight and execution identity

Before any timed child, verify the two checkout revisions, clean source
trees, benchmark/CLI executable hashes, compiler identity and target x64,
CMake generator/configuration, and effective Release compile flags. In
particular, inspect both CMake caches and their generated MSBuild projects
or captured compiler invocations: an empty or missing optimization setting,
or unequal effective `/O2 /Ob2 /DNDEBUG` settings, is an error. The initial
unoptimized A build discarded in BM-0085
is not admissible. Build both binaries after configuration and require the
preflight to bind those binaries to the inspected build trees; a label saying
"Release" alone is not evidence of optimization.

The future runner must use a separate, versioned manifest and result schema.
Do not modify or rerun `silesia-hash-chain-bucket-scaling-v1.json` under its
old `hash-chain-exact` meaning. The new identity includes the manifest hash,
both source revisions, executable hashes, compiler/flags, corpus identities,
codec, iteration count, per-child command, OS/architecture, and result schema.
Reject a resumed checkpoint if any identity field differs.

Run one child at a time. Set an explicit per-child timeout in the manifest
large enough for the slowest verified member; a timeout is a failed record,
not an invitation to extrapolate. Checkpoint one complete member-side result
atomically after each successful benchmark and archive check. Write a new
temporary checkpoint, flush it, and replace the prior checkpoint only after
the full record validates. Resume at the first missing record, never rerun or
overwrite a valid record implicitly, and reject duplicate/conflicting keys.
Allow a bounded `max-new-records` development mode to test resume behavior
without launching the full matrix. Keep results and temporary archives under
ignored paths outside the tracked corpus tree.

## Report and interpretation

For every member and side, retain the raw benchmark report, archive digest,
input/encoded sizes, encode/decode times, exact queried workspace, child
duration, and execution identity. If process working set is sampled, record
sample interval and observed maximum separately; a sampled maximum is not a
guaranteed OS peak and must not replace exact workspace requirements.

Report member-paired A/B encode-time and decode-time ratios, archive identity,
and workspace deltas. Aggregate throughput is
`sum(input_bytes) / sum(encode_seconds)` for each side; compute decode
throughput separately. Aggregate compression ratio is
`sum(encoded_bytes) / sum(input_bytes)`. Also show the median member encode-time
ratio and worst member, but never average per-member throughput ratios as if
they were a byte-weighted aggregate. With one timed iteration per side and
non-identical process scheduling, timing is descriptive evidence, not a
universal speedup claim. A repeat campaign would need a new fixed manifest.

## Implementation gates

1. Commit the manifest, runner, and mock-only tests without a full-Corpus run.
   Tests cover flag mismatch, identity mismatch on resume, incomplete/duplicate
   records, archive mismatch, timeout, atomic checkpoint replacement, and
   aggregate arithmetic. No test should require the external Corpus.
2. Run preflight and a bounded two-member dry run, then inspect the exact
   record and resume behavior. Do not use dry-run timings in the final report.
3. Run the fixed 24-record matrix. Preserve the canonical ignored result and
   checkpoint hashes, report failures as failures, and document the result
   separately in `docs/benchmarks.md` and the clean-room record.

No gate changes the encoder, decoder, public API, ABI, stream bytes, format,
profile, or hard limits.
