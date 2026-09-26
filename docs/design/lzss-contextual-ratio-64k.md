# 64-KiB LZSS Contextual compression-ratio study

Status: baseline modeled-event and complete-payload diagnostic complete; no
format or public-API change (2026-09-24).

## Question and baseline

The near-term question is whether `lzss-contextual-dynamic-range` can improve
compression on the external Silesia `mozilla` member while retaining its
64-KiB frame/window resource profile. Beating the maintainer's reported gzip
result is a stretch goal, not an assumed consequence of any one change.
Selection must consider the complete Silesia corpus, encode/decode time and
queried peak workspace, not just `mozilla` bytes.

The locally held `mozilla` input has 51,220,480 bytes and SHA-256
`657fc3764b0c75ac9de9623125705831ebbfbe08fed248df73bc2dc66e2a963b`.
With the working tree at revision `be380bb8edfea819bb61286964429a508dd202a3`,
the existing MSVC Release CLI with SHA-256
`f1720f1f9f2c582f84e863b7272761ac3b2bd257c1267477d75c2a72d3629a26`
reproduced 20,085,366 bytes with:

```text
marc encode --codec lzss-contextual-dynamic-range mozilla output.marc
```

The archive SHA-256 is
`95eec4f4450a991c75af5dc805c3bde02cafb20d4f21197a55145f2338cd8317`.
The matching `marc decode --codec lzss-contextual-dynamic-range` output had
the original input SHA-256. These are local observations; generated archives
and corpus files remain ignored and are not release fixtures. The executable
predates the release-publication documentation commit; there was no executable
source change between the release tag and this observation.

The maintainer reported these byte counts for the same named corpus member.
The external commands used `-9v` for gzip, bzip2, and lzma. Exact command
lines, tool versions, and output-container details have not yet been frozen,
so external numbers are provisional comparison targets:

| Compressor/profile | Output bytes | Origin |
| --- | ---: | --- |
| bzip2 | 17,914,392 | maintainer report |
| gzip | 18,994,139 | maintainer report |
| lzma | 13,365,111 | maintainer report |
| marc Contextual Dynamic Range, 64 KiB | 20,085,366 | report and local repeat |
| marc, 1 MiB | 19,068,790 | maintainer report |
| marc, 4 MiB | 18,792,234 | maintainer report |
| marc, 16 MiB | 18,576,393 | maintainer report |
| marc, 64 MiB | 18,473,921 | maintainer report |

The reported gzip gap at 64 KiB is 1,091,227 bytes; strictly beating that
specific output would require reducing marc by at least 1,091,228 bytes.
The 16-to-64-MiB window change saves only 102,472 bytes here, so a still
wider window is not the first experiment. The 64-MiB profile holds this whole
input in one frame. Different external transforms and default presets mean
these figures do not establish that a short-match change alone can close the
gap.

## First hypothesis: parser cost does not match typed entropy cost

The current typed-token parser calls `lzss_match_is_beneficial`, shared with
the serialized-byte LZSS parser. That predicate accepts a match only when
its length exceeds `9 / 2`, the canonical Match/Literal byte-size ratio.
Every existing Contextual profile additionally fixes minimum match length 5
and maximum 258. The context backend instead codes token kind, literal,
length and distance fields separately. Therefore 3- and 4-byte matches are
not examined or emitted even when their actual encoded cost might be below
the cost of their literals. The opportunity count below establishes that
such matches exist, not that their encoding would be profitable.

The maximum length 258 is also allowed by DEFLATE (RFC 1951), so it is not
the first explanation for marc losing to the reported gzip output. A change
to match length or benefit policy must not silently reinterpret existing
dictionary/context variants or alter frozen schema-57 archive bytes.

## Staged experiment

1. Freeze the external comparison: record exact command lines, tool versions,
   output formats, input SHA-256, output sizes and hashes. Preserve the
   current marc archive as the 64-KiB control.
2. Add a private, bounded diagnostic that reports baseline Literal/Match
   counts, match-length distribution, matched bytes, and frame counts. Measure
   exact 3- and 4-byte opportunities at positions visited by the current
   greedy parser separately from all-position opportunities. Check a short
   independent matcher against exhaustive search on small synthetic inputs.
3. Count modeled symbol and bypass-bit categories. Do not present a sum of
   independent per-field bit costs as the actual range-coded payload size:
   arithmetic coding shares state across events. Use complete experimental
   payload sizes for any compression claim.
4. Only if diagnostics show a useful opportunity, prototype minimum lengths
   3 and 4 behind a private new decoder-visible variant. Specify the new
   length-value mapping, limits, model/descriptor validation, deterministic
   parse, and malformed-input handling before implementing its encoder.
   Existing profiles and schema-57 bytes remain untouched.
5. Then compare greedy, limited lookahead, and cost-aware match selection as
   separately named encoder policies. A policy that changes canonical output
   needs its own documented selection and deterministic tests, even if the
   decoder representation is shared. Do not equate the nine-byte diagnostic
   token transcript with entropy cost.

## Stage 2: exact short-prefix opportunity count

The private `marc_lzss_short_match_diagnostic` executable reads one input file
in independent 65,536-byte frames. Its fixed-capacity index records the nearest
prior equal 3-byte and 4-byte prefix at every frame position. It uses the
production exact HashChain typed-token encoder with the current 5..258 match
contract and reports both all-position and parser-visited counts. The index
was checked against exhaustive search on bounded synthetic inputs. It does
not change archive bytes, model state, public APIs, or the match finder.

On the local `mozilla` input identified above:

| Measure | Count |
| --- | ---: |
| Frames | 782 |
| Baseline literal tokens | 14,711,301 |
| Baseline match tokens | 3,065,042 |
| Baseline matched bytes | 36,509,179 |
| All positions with a 3-byte prefix match | 37,298,509 |
| All positions with a 4-byte prefix match | 31,922,000 |
| Parser-visited positions with a 3-byte prefix match | 6,710,314 |
| Parser-visited positions with a 4-byte prefix match | 4,024,782 |
| Baseline literal positions with a 4-byte prefix match | 959,740 |
| Baseline literal positions with a 3-byte but no 4-byte prefix match | 2,685,532 |

The two final rows are disjoint and together cover 3,645,272 baseline literal
positions. They are opportunities under the existing parse, not independent
replacement matches: accepting one would skip later positions and alter
model history. Prefix equality alone does not establish an encoded-bit saving.
In particular, the all-position counts include locations inside existing
matches and must not be read as a candidate token count. See BM-0099 for
distance and existing-match length distributions.

## Stage 3: modeled events and complete baseline payload

The diagnostic now passes those production tokens through the real 64-KiB
field-context mapper and Dynamic Range payload planner. It counts modeled
symbols and bypass decisions separately, then sums *complete* frame payload
sizes. The model resets per frame exactly as it does in the encoder; the
reported payload is not a sum of independently estimated field costs.

For the same `mozilla` input, the baseline comprises 17,776,343 token-kind,
14,711,301 literal, 3,065,042 length-class, and 3,065,042 distance-class
symbols. It also has 5,467,985 length and 25,892,152 distance bypass bits.
The 44,284,147 modeled operations represent 69,977,865 arithmetic decisions.
The complete Range payload is 20,022,694 bytes. Adding the 112-byte stream
header and 782 frame-header-plus-descriptor pairs of 80 bytes yields
20,085,366 bytes, exactly the measured baseline archive. A tracked single-
frame and generated two-frame smoke test each independently compare this
prediction with the CLI archive size.

These counts identify which fields dominate *decision count*, not their
compressed-bit contribution. In particular, 25.9 million distance bypass
decisions are coded with equal binary probabilities, but the count alone is
not a measured 3.2-MiB opportunity: alternative parsing changes token kinds,
lengths, distances, and subsequent model state together. The next comparison
must use the separately reserved
[short-match representation](lzss-contextual-short-match-64k.md) and its
complete encoded payloads, including format overhead. That reservation is
decoder-visible documentation only: no parser, encoder, decoder, CLI selector,
or public profile admits the candidate yet.

Admission requires byte-exact round trips, split-buffer determinism, strict
malformed-stream rejection, bounded workspace queries, sanitizer coverage,
and a measured size/speed/memory comparison across all twelve Silesia members.
Report the aggregate and worst-member regressions; a `mozilla` win alone is
insufficient. The reported gzip size is an aspirational reference, not a test
assertion or a promise to match a different compressor architecture.

## Stage 4: fixed-token distance-extra-bit diagnostic

BM-0119 identifies distance bypass as a large, not yet characterized cost.
Keep the context-7-selected 0/3/4 winner tokens fixed, as in BM-0117. Do not
change parsing, literal partition, Range arithmetic, frame boundaries or
public bytes. Replay only the distance extra bits in a benchmark diagnostic.

Compare these three bounded controls, reset at each frame:

- Equal binary probabilities: exactly one information bit per bypass bit.
- Position-only: 16 binary models, one per numeric bit position.
- Class and position: 17 by 16 binary models, indexed by decoded distance
  class and numeric bit position. Unused combinations remain unused.

Distance class `c = floor(log2(distance))` is already decoded before its
`c` extra bits. Class zero has no bypass operation. Consume bits LSB-first;
predict each bit before updating its own model. Initialize frequencies to
`[1,1]`, add one to the observed frequency, and when the total reaches 32768,
replace each frequency with `(frequency + 1) / 2`. No future data, decoded raw
byte, extra dictionary search or recursive structure is needed for prediction.
Treat class 16 consistently, including its constrained legal distance range.

For each candidate report adaptive information, per-frame empirical binary
information, zero/one counts and modeled-bit count. Empirical scores are
diagnostic only and must not drive predictions. The uniform control must
exactly reproduce `retained_cost_distance_bypass_bits`; length bypass remains
unchanged and must never enter these counters. Validate operation shapes and
association with a preceding distance-class symbol before indexing arrays.
Require width to match class, reject an orphan/duplicate bypass, and leave
input tokens and operations untouched. Use fixed-capacity arrays only.

Test hand-calculated sequences, LSB ordering, field isolation, class zero and
16, reset behavior, rescaling and malformed operations. Then screen mozilla
and all twelve corpus members, reporting regressions as well as totals. A
favorable diagnostic requires a separately specified private format and
actual bounded encode/decode measurement before adoption. This stage reserves
no IDs and changes no existing codec. Do not expand parser policy combinations
merely to pursue BM-0118's small reselection benefit.

## Stage 5: private position-adaptive distance representation

BM-0121 supports testing actual binary coding: the position-only diagnostic
improves all twelve file totals, while the larger class/position bank loses
to uniform coding on two. First implement the smaller candidate, not both
banks or a framewise selector. The exact private identity and integer rules
are reserved in [the format](../format.md) as 2/8 + 1/9 + 3/2.

Keep the context-8 field mapping and append sixteen binary models. The
dictionary emits the same typed tokens. The context boundary retains grouped
extra operations; a grammar-aware adapter identifies length versus distance
and passes the latter to an explicit distance-extra backend operation.
Do not allow the generic bypass method to guess the field from its width.
In particular, width one is legal for both short-length escape and distance.
Reject orphan, duplicate or interrupted extra fields before coding them.
The decoder knows the field from the current token/class and requests the
same explicit operation. Context IDs 24..39 are backend bit models, not new
independent token events or permission for arbitrary operation injection.

Implement in bounded stages:

1. Add identity/layout and descriptor validation plus hand-checkable vectors.
   Keep every published parser rejecting the private tuple and crossed IDs.
2. Add bounded model state and strict decoder interval replay. Preserve event
   versus decision accounting and failure-before-frame-publication behavior.
3. Add reference planning/encoding over validated operations, with explicit
   overlap/capacity checks and concrete workspace accounting. Retain grouped
   operations so the existing five-operations-per-input-byte cap suffices.
4. Integrate private frames; test reset, chunk/capacity boundaries, malformed
   fields, count limits, termination and model rescaling. Pin canonical payload
   vectors independently of encoder/decoder agreement and retain old vectors.
5. Measure complete actual frames on the same fixed tokens as BM-0121, with
   context 8 as control, strict token/raw recovery and identical framing costs.
   Report every member's size, coding timings and supplied/required workspace.
   Only afterward consider candidate reselection or public admission.

The binary bank adds 32 frequency entries to 2490, and sixteen contexts to 24.
Workspace query must include totals, pending-field state and replay storage,
not merely frequencies. The diagnostic's log2 savings are neither an output
size guarantee nor a speed prediction. Keep length extras uniform and avoid
changing literal state, parsing, frame size or any non-distance model so the
first measured difference has a single cause.

## Context-9 integration readiness after frame optimization

Review date: 2026-09-26; implementation through `e39f81c2`. The exact private
identity remains dictionary 2/8 + context 1/9 + entropy 3/2. This review does
not admit it through the public parser or replace an existing codec.

### Evidence and its limits

| Area | Established evidence | Not established |
| --- | --- | --- |
| Size | BM-0122/0123: mozilla 18,542,748 accounted bytes; all twelve Silesia totals improve over context 8 (69,166,828 versus 70,732,714 bytes in aggregate) | A publicly emitted context-9 archive or a reproduced, identical-environment gzip comparison |
| Decode speed | BM-0125/0126: specialized binary-model payload decoding improves over its retained generic implementation | Whole-stream or CLI decoding speed |
| Encode speed | BM-0127/0128: binary specialization; BM-0129/0130: two-run frame encoding reduces summed paired frame time by 29.73% to 29.96% | Whole-CLI gains, additive optimization percentages, or parity with the published baseline |
| Correctness | Fixed vectors, frame round trips, reference comparisons, model rescaling, bounds and publication faults; full 3,860-test validation | Incremental context-9 streaming, dedicated stream fuzz coverage or cross-platform artifact verification |
| Memory | Bounded caller-owned frame workspaces and checked aggregate limits | Process peak RSS or a public context-9 workspace/configuration contract |

The accounted mozilla size is below the user's reported gzip -9v size of
18,994,139 bytes. Treat that as a promising target comparison, not a completed
CLI interoperability or performance result. Measurements retain fixed tokens
chosen by the existing experiment; context-9-specific candidate reselection
is not required for initial integration and must be evaluated separately.

### Ordered remaining work

1. Add a private strict one-shot stream decoder for the exact tuple. Reuse the
   complete-frame decoder; validate the canonical 112-byte stream header,
   sequence, reset semantics, raw extent, final short frame and exact end.
   A header-only empty stream is valid. Reject crossed identities, truncation,
   trailing bytes and hard-limit violations without publishing raw output.
   Use validation then reconstruction over stable caller-owned input and bounded
   reusable frame/token storage, following the existing private stream contract.
   This is not an incremental decoder and does not relax public admission.
2. Assemble private streams from caller-owned typed-token frame spans, then
   integrate bounded raw-input tokenization without changing the search/tie
   rules. Check exact whole-stream sizes, empty/final-frame behavior, overlap,
   limits and delayed stream-header publication. Keep prepared objects local
   to each frame; never retain a borrowed operation span across caller returns.
3. Add dedicated stream malformed-input and bounded fuzz coverage, then measure
   real emitted archives and whole encode/decode paths with explicit timing and
   memory boundaries. Keep the paired frame benchmarks as diagnostic controls.
4. Design incremental encoder/decoder state, workspace query, profile/config
   contract and C API/CLI naming. Preserve existing defaults and old bytes;
   require one-byte and arbitrary split-buffer equivalence, limit and hash tests.
   Public format admission is an explicit later change, not a parser shortcut.
5. Extend installed-consumer tests, examples, interoperability inventory and CI
   artifacts; complete Windows/Linux cross-tests before calling the codec public
   and complete. A release/version decision remains with the maintainer.

The immediate next implementation is step 1, not all five steps at once. Its
test vectors must be independently assembled header/frame combinations,
including failure in a later frame with sentinel whole-output preservation.
No new format identity, larger window, context selector or candidate policy is
part of this integration review.

### Private strict decoder implementation

DD-1270 implements step 1 as `decode_lzss_position_distance_stream`: a strict
one-shot two-pass decoder with caller-owned bounded scratch storage. It validates
all frames before publishing whole-stream raw output. Input must remain stable
and all regions disjoint; scratch can change on failure. Fixed-vector tests
cover empty/multi-frame/final-short streams, truncation, late corruption,
identity, capacity, overlap and limits. Public admission and incremental
streaming remain absent. Step 2 (private typed-token stream assembly) is next.

### Private typed-token stream assembly

DD-1271 adds context-9 stream plan/encode over caller-owned complete token frames.
One bounded operation workspace is reused; all frames are planned before output,
and the canonical header is published last. The strict private decoder verifies
the result. This stage adds no raw-input parser, candidate policy or public entry.
Whole-stream preflight adds a planning pass, so paired frame timing is not a
whole-stream performance claim. Next integrate bounded raw-input tokenization
with an explicit fixed parsing policy before considering candidate reselection.

### Fixed-policy raw-frame connection

DD-1272 introduces one raw-frame adapter before whole raw-stream iteration.
The caller fixes eligibility 3/4/5 and exact reference/indexed search; no size
selector runs. Materialized tokens are retained through frame writing. Indexed
finder storage is charged alongside the frame working set. Differential tests
compare tokens, complete bytes and reconstruction. Whole raw-stream planning
and assembly remain the next step; no new speed/size result is claimed here.

### Fixed-policy raw-stream assembly

DD-1273 connects raw-frame planning and writing over the full caller-owned raw
span, using one reusable token/operation/finder workspace set. All frames are
planned before output and the stream header is committed last. Reference/indexed
streams are byte-identical for a fixed eligibility and agree with typed-token
assembly. Empty input is header-only. This is still private one-shot processing;
whole-stream planning repeats tokenization during writing, so whole-path timing
must be measured separately. Dedicated stream robustness/fuzz validation and
real emitted-archive measurement remain before public admission.

### Private whole-stream measurement harness

DD-1274/1275 add late metadata/termination checks and bounded stream fuzzing.
DD-1276 adds a dedicated emitted-stream benchmark with fixed eligibility and
reference/indexed search. Each saved archive has passed strict whole-stream
reconstruction; reported encode/decode timing includes the complete private
calls. The external sizing pass and supplied buffers are reported separately.
The next measurement is a complete mozilla input with explicitly fixed policy;
earlier selector-based size estimates are diagnostic controls, not an expected
byte count for a different parser policy.

### First emitted mozilla measurement

BM-0131 measures saved private context-9 streams for complete Silesia `mozilla`
with 65,536-byte frames, indexed matching and fixed eligibility 3/4/5. All
three policies reconstruct strictly and produce repeatable archive digests.
Eligibility 3 saves 18,655,833 bytes, compared with 19,048,383 and 19,351,929
for eligibility 4 and 5. Its three-run median whole encode and decode times
are 9.820307400 and 8.008731200 seconds, with planning separately timed at
4.545883400 seconds. The previously reported `gzip -9v` size is 18,994,139
bytes; eligibility 3 is 338,306 bytes smaller on this input, subject to the
cross-environment comparison caveat in BM-0131.

This is an emitted-stream result under a fixed private parser policy, not a
confirmation of earlier selector-based accounted estimates. The measurement
does not establish CLI throughput or peak RSS. Keep public admission gated on
incremental streaming, profile/workspace and C ABI design, installed-consumer
tests and cross-platform interoperability validation.

### Incremental integration contract

DD-1277 records the next-stage
[streaming design](lzss-position-distance-streaming.md). It defines frame-atomic
decoder publication, retained-token encoding with one dictionary search per
frame, checked workspace layout and an additive public family. Existing one-shot
decoding retains its stronger whole-output publication guarantee. The immediate
implementation is a private incremental decoder; public admission follows the
documented validation sequence.
