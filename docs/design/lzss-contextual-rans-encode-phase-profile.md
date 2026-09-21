# LZSS contextual rANS encode-phase diagnostic

Status: measurement contract fixed; diagnostic implementation and measurements pending.

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
no clock reads, global mutable counters, changes to allocation, or changes
to encoded bytes. Use `std::chrono::steady_clock` and checked accumulation.
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
