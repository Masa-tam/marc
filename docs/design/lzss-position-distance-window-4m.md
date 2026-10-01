# Position-distance LZSS: transition to a 4 MiB window

Status: development plan under DD-1360. Exact variant allocation, final bounds
and public admission remain future implementation work.

## Objective

Use the externally qualified 1 MiB implementation at
`1099a84be8dae4b1b821e7ccef2cd3ad58272546` as the retained reference and move the
next position-distance development effort to a 4 MiB frame/window. Follow the
existing 1, 4, 16 and 64 MiB progression. BM-0191's reproducible 8.85% to 9.25%
whole-encoder reduction is a useful stable endpoint for the current work; it
does not establish that all further 1 MiB optimization opportunities are exhausted.

The initial target is a separately identified position-distance Dynamic Range
profile. Existing contextual 4 MiB coding is a comparison profile, with its own
minimum match length and models. It does not replace the planned position-
distance grammar. No new numeric identity or application name is reserved here.

## Representation work

Retain the current literal/length grammar, minimum match length 3, maximum
match length 258, longest-match/nearest-distance rules and per-frame reset.
A 4,194,304-byte distance ceiling requires classes 0 through 22 and at most
22 distance-extra bits, compared with classes 0 through 20 at 1 MiB.

Under the same model construction, contexts 15 through 23 would have alphabet
23, and distance bit-position contexts 24 through 45 would have alphabet 2.
This yields 46 contexts and 2,588 flattened frequency entries: the current
2,566 plus eighteen ordinary distance entries and four binary entries.
These are derived design inputs; exact grammar acceptance, class-22 zero-extra
handling, descriptor validation and canonical termination must be specified
before codec implementation. A fixed frame/window profile still restricts
references to already reconstructed bytes and prohibits crossing a reset.

Derive event/decision counts and payload bounds independently for the widened
class range. Do not assume that changing a window constant validates all old
bounds. Document little-endian fields, LSB-first extra-bit order, identity-crossing
rejection and the new decoder-visible representation before implementing it.

## Memory and transferable implementation knowledge

The current workspace plan retains raw bytes, a conservative serialized frame,
one token per raw byte, two modeled operations per raw byte, three finder
link arrays and three fixed head tables. Its frame-dependent terms grow
linearly. A direct transfer is expected to need roughly 300 MiB at a four-MiB
frame, plus bounded model/owner state; exact requirements must come from the
new checked workspace query. This estimate is not a measured allocation or
physical peak-memory claim. Payload and aggregate ceilings exceed the existing
generic defaults and need an explicit profile resource policy.

Transfer prepared operation mapping, the five-prefix search candidate and
expired-finder payload scratch with their lifetime and failure checks. Reassess
finder head width and actual scratch eligibility at the larger window. The
rejected unconditional 18-bit-head admission at 1 MiB remains evidence; neither
its rejection nor a fixed head width proves the best 4 MiB choice. Wider history
can change collision and cache costs. Scratch capacity may select fallback even
when a frame is otherwise valid; count eligibility rather than assuming it.

## Development sequence and evidence

1. Define an additive format variant, checked limits and hand-checkable distance
   vectors, including distances around 1 MiB and 4 MiB and final short frames.
2. Implement bounded decoder validation and a clear reference path, preserving
   failed output and withholding failed frames at every public boundary.
3. Transfer validated optimized mechanisms with exact token/operation/archive
   comparisons against the new reference, plus malformed, chunking, allocation
   and reset tests. Retain the 1 MiB implementation and previous artifacts.
4. Evaluate all twelve corpus members through complete encoders and decoders,
   reporting archive size, encode/decode time and workspace separately. Compare
   position-distance 1 MiB and existing contextual 4 MiB. Frame/window/model
   differences mean these are profile comparisons, not isolated window effects.
5. Use full suites, sanitizers, bounded fuzzing and revision-specific external
   exchange qualification before public completion. A new archive identity must
   append to the existing inventory without changing its frozen prefix.

Larger windows may improve compression on distant repetitions while increasing
encoding cost. The corpus results, rather than window size alone, determine
whether this profile should progress to 16 MiB.


## Exact reservation and preflight stage

DD-1361 and the appended 4 MiB position-distance section in `docs/format.md`
reserve `2/10 + 1/11 + 3/2` and define the complete decoder-visible model,
counts and header rules. This supersedes the planning-stage lack of a numeric
identity. Private grammar/state/preflight validation is the current stage;
Range payload decoding, encoding, public selection and performance qualification
remain subsequent work.

The scalar operation-coding stage (DD-1362, TVG-1229) adds a private Range
encoder/decoder with independent payload vectors, widened distance/model tests
and canonical termination checks. It preserves concrete preflight state charges
and does not admit public selectors. Next, implement token mapping and bounded
transactional/private-scratch decoding with reconstruction/history checks, then
complete reference frame coding before optimized transfer and measurement.

DD-1363 adds the private token bridge, complete validation, transactional output
and discardable single-pass scratch. TVG-1230 checks real histories above one MiB
and exact four-MiB reconstruction using the existing typed-token reconstructor.
This completes token-layer reference integration; reference frame serialization,
bounded frame lifecycle and publication checks remain the next stage.

DD-1364 and TVG-1231 complete private reference frame helpers for selected tokens,
including checked serialization, reconstruction and failed-frame output privacy.
Next, connect bounded reference token selection from raw input and qualify frame
boundaries before a streaming owner or optimized finder/prepared/scratch transfer.
These helper tests do not establish public lifecycle or performance qualification.

DD-1365 and TVG-1232 connect bounded reference raw-input selection to complete
private frame encoding and reconstruction. Exhaustive and nearest-first
three-byte-prefix selectors preserve longest-match and nearest-tie rules with
fixed eligibility. Full/final raw-frame positions, memory/capacity checks and
publication privacy are tested. The next stage is a bounded private streaming
owner with arbitrary input/output splits and whole-stream publication checks;
public selection and optimization/performance qualification remain later work.

DD-1366 and TVG-1233 add bounded private owned and borrowed streaming transforms
with explicit workspace queries. They preserve deterministic frame bytes across
arbitrary input/output splits and validate each complete frame before publication.
The reference lifecycle is now testable end to end. Next, build a private complete
profile diagnostic and measure compression ratio, encode/decode throughput and
memory separately before transferring finder/model optimizations or considering
public profile admission. Historical external qualification remains the one-MiB
profile; private four-MiB tests do not replace it.

DD-1367, TVG-1234 and BM-0193 complete the initial private complete-profile
diagnostic. Both compiler builds preserve complete bytes and restoration for
all twelve inputs in each of three profiles; exclusive encode/decode observations
record ratio, throughput, query thresholds and whole-process peaks separately.
The four-MiB reference archive total is smaller than the compared public totals,
while its unoptimized encode path needs further investigation. This is not an
isolated window comparison or repeated speed qualification. Next, split reference
selection and frame-coding time before transferring exact finder optimizations;
retain exhaustive/reference differentials and failed-frame publication rules.
Public profile admission and revision-specific external qualification remain
separate, and the qualified one-MiB profile is unchanged.

DD-1368, TVG-1235 and BM-0194 split the unchanged reference path into token
selection and selected-token frame coding. Both compiler builds match all frozen
complete archives and restore 57 frames; the exclusive initial phase screen
identifies selection as the larger measured phase. Selection includes validation,
reset/search and token materialization, so its share is not pure dictionary time.
Next, transfer the retained exact five-prefix finder under differential selected-
token/full-byte, bounded-workspace and failure-publication tests. Preserve the
single-prefix/exhaustive reference paths and require repeated exclusive timing
before accepting a speed claim. Public four-MiB admission remains later work.

DD-1369 and TVG-1236 add a private exact five-prefix finder/candidate and raw-frame
trial. Small exhaustive and wide-distance differentials, full/final frames and
capacity/alias/failure tests pass. All selected corpus tokens, frozen complete
frames and restored bytes agree on both compiler builds. Reference selection,
scalar coding, existing owners and public interfaces remain unchanged. Next,
measure reference and trial with identical phase paths and repeated exclusive
controls before considering private owner integration and complete-stream timing.
This correctness stage does not establish an optimization speedup or public
four-MiB qualification.

DD-1370, TVG-1237 and BM-0195 compare reference and five-prefix selection through
the same candidate/scalar-frame diagnostic, with duplicate reference controls,
rotated ordering and three complete exclusive passes. Every selected token,
frozen complete frame and restored raw byte remains exact. The finder requires
34,078,720 additional policy bytes; oracle/input/archive buffers are separate
diagnostic overhead. Use the repeated per-input evidence and control spread
when deciding private owner integration; complete-owner speed and memory still
need their own validation. Public admission and external qualification remain
separate, with the qualified one-MiB profile unchanged.

All 36 input/pass comparisons beat both controls. Additive corpus phase time
falls by 73.764% to 73.929% against the control mean, while
maximum input/pass control spread is 1.364%.
This repeated phase evidence supports private owner integration next, retaining
the original reference and bounded failure/publication tests. Complete-owner
throughput and physical-memory effects must be measured after integration;
this phase result does not establish those gains or public admission.

DD-1371 and TVG-1238 add a separate private five-prefix encode query, borrowed
streaming encoder and owned wrapper. Existing reference query/owner and decoder
remain unchanged. Partial-buffer/flush/finish/alias/budget tests preserve complete
frame publication; a later failure leaves only earlier frames published. Both
compiler builds reproduce all twelve frozen streams and restore 57 frames.
BM-0196 measures complete native owners with duplicate controls and rotated
exclusive passes, including creation and destruction. Policy budgets and physical
process peaks remain separate. Public profile admission and external qualification
remain later stages; the qualified one-MiB profile is unchanged.

The repeated complete-owner comparison records 75.904% to 78.576%
aggregate encode reduction against the control mean. The trial beats both
controls in 36 of 36 input/pass comparisons, loses to both in 0
and lies between in 0; maximum control spread is 24.850%.
Retain all observations and the larger memory budget. Future finder/model work
must preserve exact complete streams and failure publication; public admission
requires separate qualification.

All 36 comparisons beat both controls, but time varies across passes and some
duplicate controls differ substantially. Their cause has not been isolated.
Next split native owner creation, process/frame preparation and destruction
through a separate diagnostic before attributing the variability or transferring
prepared-model/scratch optimizations. Keep the reference and all observations;
the repeated relative gains are evidence for this private owner, not a precise
hardware-independent performance guarantee or public admission.


DD-1372 and BM-0197 add a separate native owner call-phase diagnostic before
further optimization. Retain the previous diagnostic, private reference and trial
owners, exact complete streams and failure contracts. Disjoint create/process/
destroy intervals and known-boundary process classification localize elapsed
variation; they do not establish an internal or operating-system cause. Deeper
prepared-model or finder-scratch changes need their own exact differential and
repeated comparisons. Public admission remains a separate boundary.

The completed phase diagnostic puts 95.189% to 99.969% of encode time in
preparation calls and locates the five largest observed repetition differences
almost entirely there. Current duplicate-control spread is at most 1.464%; the
earlier large spread is not reproduced or explained. Preparation internals are
the next optimization target, with a separately qualified prepared-model trial
as a bounded first-party transfer candidate.


DD-1373 qualifies a separate prepared token-mapping/frame/raw trial against the
four-MiB scalar reference. One-use borrowed state fits the existing model-state
bound and dies before scalar range coding or reference fallback. All indexed
reference tokens, frozen corpus frame bytes and restoration match. Streaming
owners remain unchanged; owner integration and repeated throughput/memory
comparison are the next separate steps. No speedup or public admission is inferred
from this private correctness stage.


DD-1374 integrates the prepared raw helper into a separate private streaming owner,
preserving scalar controls, checked budgets and failed-frame privacy. Complete
streams match frozen archives. BM-0198 observes 0.646% to 1.729% aggregate
encode reduction with equal policy budgets and 32/36 wins against both controls.
The trial misses the required improvement across every member/pass comparison.
Retain the scalar five-prefix owner as the reference; the prepared owner remains
a separate private candidate. No existing factory is changed.
Range preparation and finder scratch reuse need separate trials; public admission
and external qualification remain later boundaries.


## DD-1381 adoption readiness audit

The reserved tuple and exact scalar decoder/reference are implemented. Private
five-prefix/finder-scratch ownership preserves complete frozen archives, bounded
workspace and failed-frame privacy. BM-0200/BM-0201 jointly retain 71 wins and
one intermediate result across 72 comparisons, with no losses against both
controls. The repetition alone wins all 36 comparisons; the original exception
and maximum 17.2098-percent control spread remain unresolved evidence. This
audit does not revise the performance condition or admit a public profile.

TVG-1248/FZ-0053 add two bounded private stream fuzz targets. Public admission
still requires the following separately reviewable integration work:

| Boundary | Current evidence | Remaining qualification |
| --- | --- | --- |
| Format and codec | Reserved tuple, reference/decoder, frozen streams, full-window/distance tests | Preserve tuple and canonical validation at public boundaries |
| Memory policy | Checked private encoder/decoder queries; finder charged once | Define public defaults and derive opaque C handle plus actual concrete state/capacities; do not equate owned and C query sizes by assumption |
| C factory | Qualified one-MiB adapter is retained | Add distinct config/query/create with overlap, invalid configuration, budget and failed-output tests |
| CLI/build | Four-MiB implementation remains private | Add additive selection and static/shared/export integration; run complete suites and frozen corpus comparisons |
| Exchange inventory | Qualified one-MiB frozen inventory remains unchanged | Append a new identity with schema/prefix compatibility tests and bundle fixtures |
| Fuzz and publication | Bounded private stream campaigns and deterministic failure tests | Public-boundary fuzzing and full integration regression without failed-frame release |
| External verification | Prior reports cover the qualified one-MiB revision | New hosted CI and revision-specific producer/consumer exchange after integration |

The full four-MiB private encoder query is 315365389 bytes and decoder query is
130029573 bytes; these are fixed workspace policy, not process-memory peaks or
future public C query constants. Generic defaults cannot be silently widened.
No new public API/profile/default/inventory identity is introduced by this audit.


## DD-1382 private C boundary qualification

A distinct private four-MiB configuration now initializes explicit resource
ceilings and queries concrete codec, guard and opaque C handle storage. Creation
charges all supplied capacities, including unused tails; process retains their
complete intervals and protects the opaque handle from input/output aliasing.
The fourteen new C-boundary tests preserve scalar stream bytes and sticky errors,
reject crossed identities and prevent publication of a failed frame.

TVG-1249 records two compiler private suites, sanitizer validation and C17
compile-only header checks. Complete corpus streams and bounded fuzzing through
this C adapter are the next private qualification. The public factory, CLI,
static/shared exports, inventory and external gates from the readiness matrix
remain pending. Existing public defaults and the qualified one-MiB inventory
remain unchanged; private factory names do not constitute public API admission.


## DD-1383 private C corpus and fuzz qualification

TVG-1250 qualifies complete frozen-stream identity and restored raw bytes through
the private C adapter for all twelve verified corpus members and both compiler
routes, with two independent encode/decode schedules. FZ-0054 adds 2000 bounded
instrumented executions with scalar differential, factory guards and independent
failed-frame publication checks. Existing adapter/codec bodies, public defaults
and exchange inventory remain unchanged.

Public integration still needs an explicit implementation/admission decision,
linked standalone C consumption, static/shared/export/CLI full suites, additive
inventory compatibility and revision-specific hosted CI/external exchange.
The historical performance exception and control spread remain unresolved;
successful correctness qualification does not revise the performance condition.


## DD-1384 selected integration baseline and additive boundary plan

Use the five-prefix scalar owner qualified by DD-1372/BM-0197 as the integration
baseline. Finder-scratch remains private because the original complete performance
condition remains unmet; successful correctness tests do not change that rule.
The distinct baseline C adapter preserves the guard/config/decoder contract and
passes boundary, linked C17 and complete corpus tests (TVG-1251).

| Integration boundary | Concrete next change and required check |
| --- | --- |
| Public C | Add a distinct `marc_lzss_position_distance_dynamic_range_4m_config` and config-init/query/create functions; preserve ABI version/layout policy and old declarations; charge actual state/handle and full supplied capacities |
| Resource defaults | Initialize only the new profile to a four-MiB frame/window, payload bound 75497477 and internal ceiling 512 MiB; keep generic and one-MiB defaults unchanged |
| Static/shared exports | Add required baseline codec sources to library builds; verify installed C consumers and old/new symbols through both linkage modes before claiming public ABI qualification |
| CLI | Add exact `lzss-position-distance-dynamic-range-4m` selection in `tools/marc_cli.cpp`, profile listing, argument/resource validation and round-trip/malformed/error tests; retain existing selections |
| Exchange inventory | Append the new identity after the frozen 69 entries in bundle creation/verification; schema 60 and `marc-cli-v60` must retain schema-59 prefix order and reject reordered/new-invalid manifests while verifying older schemas |
| Final qualification | Complete static/shared/CLI suites and frozen corpus bytes before hosted CI and revision-specific external producer/consumer exchange |

The linked consumer in this stage uses private static support. It does not verify
installed shared exports, a public header, CLI selection or schema 60. None of
those pending changes is introduced here; existing schema 59 and 69 archives
remain unchanged. Historical scratch timing exceptions remain documented.


## DD-1385 public C/static/shared integration result

The selected baseline now has a distinct public C config/init/query/create
family. TVG-1252 qualifies both complete compiler suites, build-tree/installed
static/shared C consumption, exact additive exports, complete frozen corpus
streams and bounded public C fuzzing. Actual config handling, full-capacity
budgets and failed-frame privacy remain unchanged from the qualified proposal.

CLI selection and schema60/additive inventory are the next implementation
boundaries. Generic defaults, old C families, schema59/69 archives and historical
private timing decisions remain unchanged. New hosted CI and revision-specific
external exchange are still required after subsequent integration.


## DD-1386 command-line integration result

The exact `lzss-position-distance-dynamic-range-4m` name now dispatches through
the qualified public C initializer/query/factory with profile-local limits.
TVG-1253 qualifies both complete suites, CLI boundary and file transactions,
all twelve complete frozen streams and original restoration. The scalar codec
bodies, older defaults, private candidate status and schema59 inventory remain
unchanged. No new speed claim is made.

The next boundary is schema60: append exactly one profile/archive after the
existing 69 entries, preserve prior schemas and prefix/order, reject inconsistent
identities and reordered inventory, then qualify the actual revision through
new hosted CI and maintainer external exchange. Earlier one-MiB reports do not
qualify this four-MiB exchange.


## DD-1387 schema-60 local integration result

The exchange generator now appends the explicit four-MiB CLI profile as archive
70, and the verifier accepts schema60 while preserving schemas 1 through 59.
TVG-1254 qualifies both local compatibility routes, unchanged old archive bytes,
identical new archive, reordered/rehashed identity rejection and opposite-compiler
consumption. The exchange fixture remains 8193 bytes; prior window-boundary and
complete-corpus checks supply separate coverage. Codec/API/limit/default and
private candidate admission are unchanged; no new timing or fuzz claim is made.

New hosted CI and maintainer four-direction external exchange on the actual
schema-60 commit are the remaining integration gate. Earlier schema59 reports
do not close it. Further algorithm changes must preserve this qualified baseline
and receive their own differential, safety and performance evidence.


## DD-1389 preparation diagnosis priority

The schema-60 implementation has maintainer-reported CI and four-direction
external qualification in IX-0056. That qualified baseline remains unchanged.
TVG-1256 reconciles earlier diagnostic datasets without taking new timings:
weighted complete-owner preparation 99.5790 to 99.5801 percent, separately
measured dictionary selection 85.8383 to 85.9404 percent. These do not measure
current public-wrapper overhead and have different timing denominators.

Webster, mozilla and nci together account for 61.7613 percent of the sum of
member median owner times. The next private experiment should distinguish
finder reset, lookup-chain traversal, comparisons/extensions and insertion,
plus mapping and frame coding, with complete differential validation before
timing. All twelve inputs remain necessary. Existing scratch/prepared-owner
non-admission and unresolved control variation remain intact; no favourable
repeat overrides the earlier exception. Public APIs, failure/privacy contracts,
format and schema60 inventory are unchanged.
