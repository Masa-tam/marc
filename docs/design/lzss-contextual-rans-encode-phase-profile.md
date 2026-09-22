# LZSS contextual rANS encode-phase diagnostic

Status: measurement contract, checked accumulator, optional codec hooks,
identity-gated diagnostic benchmark, and fixed pilot and full-member runners
implemented; both campaigns completed.

## Question and scope

DD-1162 retains the current 262,144-bucket HashChain policy but does not
identify the next encode bottleneck. Measure the time distribution of the
existing public `lzss-contextual-rans-4m` encode path before selecting another
search or entropy optimization. This is a private diagnostic, not a new codec,
format variant, API, ABI, runtime policy, or CI speed threshold. Use only the
locally supplied, verified Silesia Corpus; do not download or redistribute it.

## Execution path to measure

`LzssContextualRansFrameStreamingEncoder::prepare_frame()` invokes the
matched frame encoder. In `encode_frame()`, `plan_frame()` first materializes
typed LZSS tokens, then calls `plan_lzss_contextual_rans_tokens()`. That plan
validates tokens, builds the contextual model, and makes a reverse rANS pass
with no output to determine payload size. Next,
`encode_lzss_contextual_rans_tokens()` calls the same token plan again and
then makes the output-producing reverse pass. Frame descriptor/header
serialization and streaming input/output copies surround those operations.
Thus the model-building and size-planning path is executed twice per frame.
This is a code-path observation, not yet a claim about its time share or a
reason to remove validation.

For a single measured encode invocation, use disjoint accumulated durations:

1. `tokenize`: typed-token production including match search.
2. `first_plan`: the first contextual model/size plan, including validation.
3. `second_plan`: the plan repeated inside output encoding.
4. `reverse_write`: the output-producing reverse rANS pass.
5. `frame_finish`: descriptor and frame-header serialization.
6. `other`: checked preflight, stream-header work, buffer copies, draining,
   and any uninstrumented work. Compute this only as the same invocation's
   measured total minus its disjoint timed stages; reject a negative residual
   rather than silently clamping it.

The first and second plans must not be merged or silently called an entropy
write cost. Do not add isolated microbenchmark timings to a separate public
codec timing and call the result an end-to-end profile. The profiling clock
and guards introduce overhead; report that limitation, and preserve raw
per-iteration values rather than only percentages.

## Implementation contract

Add an optional private, per-instance timing accumulator at internal frame
and context boundaries. A null accumulator is the normal production path:
no clock reads, global mutable counters, additional scratch/workspace
allocation, or changes to encoded bytes. The streaming encoder object itself
may grow by one private pointer; its heap allocation is not part of the
caller-supplied workspace query. Use `std::chrono::steady_clock` and checked
accumulation.
Keep the timing accumulator outside the public C ABI and public headers.
The diagnostic benchmark must run the actual streaming encoder with the
same configuration and buffer sizes as the existing public benchmark; it
must not rebuild tokens or invoke a substitute codec in order to time a
stage. Time one complete encode invocation, including all frames. On a
failed encode, report failure, not a partial phase summary.

Use one untimed public encode/decode round trip and compare the complete
diagnostic archive size and SHA-256 with the public archive before accepting
any timing. The diagnostic build must use the same source revision, MSVC x64
Release compiler/options, codec parameters, and hard limits as the public
comparison. A mismatch invalidates the result. Record executable hash,
compiler, flags, source revision, Corpus identity, input and output lengths,
frame count, and exact queried workspace with each report.

## Validation and staged measurement

1. Unit-test the timing accumulator with a deterministic fake clock or
   explicitly supplied durations, including overflow rejection and the
   partition invariant. Smoke-test empty input, one short frame, and a
   multi-frame synthetic input without the external Corpus. Verify timed
   and untimed archives match and that a null accumulator leaves the normal
   path unchanged. The mock tests must never require Silesia files.
2. Run an isolated pilot on `xml`, `x-ray`, and `mr`, selected before timing:
   the previous A/B run showed a small, a large, and a slightly negative
   HashChain response, respectively. Use independent processes and at least
   three measured invocations per member after an untimed correctness pass.
   Report each iteration and a median per member. Do not treat this selected
   pilot as a Corpus-wide performance claim.
3. If the pilot passes identity and accounting checks, freeze a separate
   all-member manifest and restartable runner before reading the remaining
   timings. A future full result may guide a new optimization hypothesis,
   but requires a separate design decision before production code changes.

No stage timing may change decoder behavior, existing output, workspace hard
limits, HashChain selection, or the fixed BM-0087 result.

## Stage 1 status

The private `LzssContextualRansEncodePhaseTiming` accumulator and synthetic
duration tests are implemented. They define the five named stage counters,
checked per-stage addition, checked cross-stage sum, reset, and a summary
that publishes `other` only when the disjoint stage sum fits inside the same
invocation's total. Invalid phase IDs, negative durations, overflow, and an
overfull partition leave caller-visible state unchanged. This stage does not
read a clock, instrument the codec, or produce a performance result.

## Stage 2 status

The private timing type now lives in the context layer so both context and
frame encoders can use it without a dependency from context back to frame.
An optional pointer is passed from the streaming encoder through the frame
encoder to the contextual rANS encoder. Null remains the normal production
setting and does not read a clock. Non-null timing records token production,
the two existing plans, the output-producing reverse pass, and frame
serialization in the same streaming encode call. The empty, two-frame
one-byte-buffer, and HashChain single-frame tests compare timed output with
the established untimed oracle and check the accounting partition. No
benchmark timing or Corpus-wide claim has been produced yet.

## Stage 3 status

The private `marc_lzss_contextual_rans_phase_benchmark` executable runs a
public C API encode/decode round trip and verifies a complete archive SHA-256
before using the same streaming encoder with an untimed and then optional
timed accumulator. It rejects a size or SHA-256 mismatch for each invocation
and reports disjoint raw nanoseconds, the residual, input identity, frame
count, and the queried encoder workspace. The tool is built only with the
static library because its timing hook is deliberately not public ABI.
Its README smoke test and a two-frame `xml` identity check passed locally.
The `xml` run was a correctness check, not the predeclared three-member pilot:
it had one measured invocation and no median. The pilot runner still needs
to bind executable SHA-256, source revision, compiler/options, and Corpus
identity to every report and launch independent measured processes. Neither
a bottleneck conclusion nor a change to the encoder follows from this stage.

## Stage 4 status

The pilot runner fixes `xml`, `x-ray`, `mr` and three one-iteration child
processes per member. Each child performs its own untimed public round trip
and untimed private archive identity check before its single measured encode.
The runner independently verifies the complete local Silesia Corpus and each
child's input SHA-256, frame count, workspace sum, stage partition, and
cross-process archive identity. The ignored JSON report binds the source
revision, diagnostic executable SHA-256, MSVC compiler/version, Release
cache flags, generated target project SHA-256, member identities, raw
per-process timings, and per-member medians. Fixture-only runner tests do not
depend on the Corpus. The runner requires a clean source tree and does not
interpret a median as a regression threshold. Pilot measurements remain
pending until the committed runner is executed.

## Stage 5 status

The fixed selected pilot completed all nine independent processes at clean
revision `f4987173ec9e6812718dd6818be020cba8b83967`; BM-0088 records
the environment, raw-result digest, medians, and interpretation. Every child
passed the public round trip, private archive identity, and stage partition
checks; each member's archive identity was stable across three processes.
The 3-member pilot suggests that typed-token production and match search
dominate on `xml` and `mr`, while both contextual plans are material on
`x-ray`. These selected inputs cannot establish a Corpus-wide distribution
or justify removing validation or the second plan yet. The next measurement
stage remains a separately frozen all-member manifest and restartable runner.

## Stage 6 all-member contract

DD-1164 and `silesia-contextual-rans-phase-full-v1.json` freeze a distinct
36-record campaign: twelve published Silesia members in their canonical
order, each with three independent one-iteration child processes. Pilot
records are excluded. The runner must verify the entire local Corpus and
the MSVC x64 Release diagnostic build, then bind the manifest's exact bytes,
source revision, executable and generated target project hashes, compiler
and flags, Corpus metadata, and path identity before measuring. Save each
validated record atomically. On resume, require a canonical prefix and
revalidate each stored report, phase partition, input identity, and stable
per-member complete-archive identity. A changed or malformed checkpoint
must fail closed. An optional run quota may stop after a fixed number of new
records for a dry run; an incomplete checkpoint is not a full result.
Report raw attempts and per-member medians without a pass/fail speed gate.
Only after all 36 records pass may the full result be interpreted. The
instrumented clock and the fixed 4 MiB policy remain unchanged.

## Stage 7 implementation status

The separate full-member v1 manifest and runner implement the 36-record
contract. Fixture-only tests cover strict manifest matching, canonical
checkpoint prefixes, changed execution identity, malformed reports, archive
drift, atomic checkpointing after each accepted record, quota/resume, and
completed replay without a new child process. The actual campaign completed
all 36 Corpus records at clean revision `574cac57f62bb1675260d72ad45f458ac7116c67`.
Every child passed archive and phase-partition checks; completed replay
validated the stored result without launching a new child. BM-0089 records
the identities, per-member median-total invocation shares, and limits of
interpretation. Eight of twelve members spent over 90% of the median-total
invocation in typed-token production, whereas the two contextual plans
remained material for `x-ray`, `sao`, `ooffice`, and `osdb`. This stage does
not separate match-search time from other token-production work and makes
no production-code or runtime-policy change.

## Stage 8 follow-up

DD-1166 and the [token-production breakdown contract](lzss-contextual-rans-tokenize-breakdown.md)
define the next diagnostic boundary inside the measured `tokenize` phase.
This follow-up is a new measurement and must retain BM-0089 as a separate
historical result.
