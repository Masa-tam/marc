# Eight-MiB position-distance Blocked Huffman

## DD-1616: exact eight-MiB specialization

This format specializes the family definition in
`lzss-position-distance-blocked-huffman.md` for window and maximum frame
8,388,608 bytes. Its selector is
`lzss-position-distance-blocked-huffman-8m`. The identity is dictionary2/11,
context1/31 and entropy2/6. Match lengths are3..258; known original size,
fixed-five eligibility, longest matching and nearest-distance tie breaking are
required. Earlier selectors and generated bodies are unchanged.

Each nonempty frame is an independently reset typed-event Huffman block.
Frame size is configurable1..8,388,608 raw bytes. The event grammar is the
same field grammar as the preceding family variants: preceding token kind,
literal value, length class and distance class are modeled. Length residuals
are LSB-first uniform bits; distance residual bit p uses binary Huffman context
32+p, low bit first. Contexts are 0..54 with alphabets
`[2]*3 + [256]*17 + [9]*3 + [24]*9 + [2]*23`; pooled fields are
`[2,256,9,24,2]`. The alphabet extent is4647. Distance class23 is the
isolated grammar endpoint and cannot be emitted by a nonempty reset frame whose
raw extent is at most the window.

The descriptor uses the 24-byte little-endian prefix
`<IIBBBBHHII`: decision count, payload size, final valid bits, maximum code
length15, five fields, field mask, context count55, reserved0, alphabet extent
4647 and flags0. Seven override-mask bytes follow; the upper bit of the final
byte and field-mask bits5..7 are zero. Pooled records precede context override
records in field/context order. Mode0 is a singleton, mode1 is packed dense
lengths and mode2 is sorted sparse symbol/length pairs. Singleton and dense vs
sparse selection is canonical, code lengths are complete1..15, and canonical
codes are reversed only at the LSB-first physical writer. Descriptor extent is
31..2563 bytes; all bounds and model usage are checked before payload decode.

The 112-byte MARC2.0 header declares the identity above, frame/window
8,388,608, dictionary minimum3/maximum258, maximum code length15, five fields,
55 contexts and extent4647. All unspecified fields, flags, side data and
trailers are zero. Each 64-byte MRF2 frame header carries raw/token/event/
decision counts and descriptor/payload sizes. Frames exactly partition the
known original size; empty input is header-only. Failed descriptors, tokens,
history and raw reconstruction remain private until the complete frame is
validated, and a failed frame publishes no raw prefix.

For T tokens, R raw bytes and D decisions,
`1 <= T <= R <= frame_size`, `2T <= D <= min(33T,9R)` and payload is at most
`ceil(15D/8)`. The full-frame checked bounds are D<=75,497,472,
payload<=141,557,760 and serialized frame<=141,560,387 bytes. These are wire
validation bounds, not memory defaults or physical peak claims. Resource
defaults require fresh normal measurements.

## Qualification checkpoint

The independent first-party oracle and frozen vectors generate196 positive and
44 negative cases, including every distance class through8,388,608, all55
contexts, full dense descriptor ceiling, complete fifteen-bit codes, exact
header identity and malformed metadata/truncation. Production implementation,
API/CLI, measurements, contextual comparison, fuzz, exchange and final local
audit remain unstarted for this window. No completion or hosted result is
claimed.

## Independent format qualification checkpoint

TVG-1512 freezes196 positive and44 negative mathematical cases. The oracle
checks the55-context partition,4647 extent,2563-byte descriptor ceiling,
distance classes through8,388,608, the33T/9R decision bounds and exact header
identity. No production source, API, CLI, measurement, exchange or fuzz work
is claimed by this checkpoint.

### Exact envelope and model completion rules

Use DD-1610's exact canonical model records, package ordering and strict
override saving rule. A cost tie retains the pool. Context groups0..2,
3..19,20..22,23..31 and32..54 map to fields0..4; table slots0..4 are pools
and5..59 overrides. Every declared field and override must be used at exact
completion. Empty entropy is31 zero-model descriptor bytes with no payload
and final_valid_bits0; otherwise payload padding is zero and valid bits1..8.

The112-byte stream header has u16@4/6/8/10=2/0/64/1,
u16@12/14/16/18=2/11/2/6, frame_size u32@20, u32@28/32/48=16,
known original_size u64@40, dictionary u32@64/68/72/76=8388608/3/258/0,
u8@80/81=15/5, context_count55 u16@82, alphabet_extent4647 u32@84,
and context u16@96/98=1/31. All other bytes are zero, including52..63.
MRF2 has magic@0, extent64 u16@4, flags0 u16@6, sequence u64@8,
raw/token/event/decision u32@16/20/24/28 and payload/descriptor/side/trailer
sizes u32@32/36/40/44. Side/trailer and bytes48..63 are zero. Descriptor
precedes payload. Sequences start0; dictionaries and models reset per frame.
All offset arithmetic and allocations must be checked before use.

### Qualification policy

At the user's request, no further fuzz execution is performed during this
goal. Directed unit, independent differential, malformed-input, failure
retention and sanitizer tests remain required. Record fuzz as omitted by
request rather than as passed. Token/history and failed-frame publication
tests follow the entropy core; the scalar entropy core alone establishes
neither valid LZ history nor frame publication safety.

### Independent-vector audit correction

The preceding196 positive wires are preserved in their original order. That
checkpoint stored44 negative labels rather than replayable negative wires and
did not individually exercise all upper contexts or the hand fifteen-bit
code. The strengthened frozen set contains252 positives and62 actual negative
descriptor/payload cases. Its default check compares against the frozen JSON;
it no longer overwrites expected values during ordinary test execution.

### Bounded entropy-core qualification

TVG-1514 qualifies the descriptor validator/parser, scalar reference coder,
deterministic pooled/override planner and private incremental writer. All252
independent wires match and all62 malformed cases are rejected. All252
production plans are decoded independently and normal/instrumented bytes
agree. Exact/one-short budgets, aliases, late errors, strict model usage,
zero-bit singleton completion, highest context54 and class23, padding and
sticky lifecycle guards pass. The finite75,497,472-decision zero-bit block
passes without allocating a maximum payload. Normal8/8 and six directly
instrumented ASan/UBSan translation units pass independent artifact audit.
No fuzz execution was performed. Token/history, frame quarantine, public
API/CLI, resource defaults, measurements and exchange remain open.

### Private-token and history qualification

TVG-1515 qualifies single-pass private token scratch and success-only ordinary
copy after exact entropy/count/history validation. Every reference is checked
before scratch assignment. All70 independent valid wires and7 invalid
history/length cases pass; independently decoded production wires and normal/
instrumented bytes agree. Tests include distance8388605, full-frame literal
and overlap token sequences, exact/one-short grants, aliases, cuts and late
terminal rejection. Normal5/5 and ten directly instrumented ASan/UBSan units
pass independent artifact audit. Full-literal decode admission is205832806
logical bytes; overlap admission is8388608 because the shared block/grant
floor dominates. These are finite test charges, not public defaults or RSS.
No fuzz execution is performed. Raw reconstruction, failed-frame publication,
stream owners, public API/CLI, normal measurements and exchange remain open.

### Frame and stream publication qualification

TVG-1516 qualifies exact stream/frame envelopes, complete descriptor preflight,
private raw reconstruction and the streaming publication controller. All70
independent positive streams/frames and7 invalid frames pass;69 production
frames are decoded independently and normal/instrumented bytes agree. Fixed
reserved bytes52/53, maximum metadata without payload allocation, a full
8388608-byte zero-payload literal frame, every small split, one-byte I/O,
exact/one-short quotas and late-second-frame publication guards pass.
An invalid descriptor is rejected before consuming/storing its payload.
Normal4/4 and two14-unit direct ASan/UBSan closures pass independent artifact
audit. Fuzz is omitted by request. Owners, normal resource precursors, public
API/CLI, comparisons/peaks and schema82 exchange remain open.

### Owned transforms and logical resource qualification

TVG-1517 qualifies bounded raw owners with the existing eight-MiB fixed-five
longest/nearest finder. All499 independently parsed raw fixtures agree across
normal and directly instrumented paths. Constructor rollback, allocation-free
process calls, exact/one-short admission, all small splits, full-window
partitions and failed-frame quarantine pass. Full-frame logical requirements
are635644855 encode and250956718 decode bytes. Report actual owned buffers,
owner controls, working storage and metadata floors separately; these queries
are neither measured process peaks nor public defaults. Normal3/3 and a
17-unit direct ASan/UBSan closure pass independent artifact audit. Fuzz remains
omitted by request. Normal resource precursors must precede default selection;
public API/CLI, comparisons/peaks and schema82 exchange remain open.

## DD-1617: planned public resource and process boundary

Add `marc_lzss_position_distance_blocked_huffman_8m_config_init`,
`marc_lzss_position_distance_blocked_huffman_8m_resource_requirements` and
`marc_lzss_position_distance_blocked_huffman_8m_create`, with additive
`_8m_config`/`_8m_resources` types preserving the previous field layouts.
Default frame/window8388608, wire minimum3/maximum258, encoder eligibility5,
decision ceiling75497472, payload141557760, table entries30660 (60*511),
distance8388608, total output2^40, expansion ratio1024 and slack8388608.
Declare input/output capacities65536 each; retained caller storage defaults0.

TVG-1518 qualifies seven serial normal processes under exact owner grants:
pattern, fixed-seed noise and overlap encode/decode plus an independent full-
literal decode. All actual identities and source restorations match. The
largest observed ordinary peaks are244523008 encode and124076032 decode
bytes. Owner queries plus separately charged131072 I/O and65536 control bytes
require635841463 encode and251153326 decode logical bytes. Select640MiB
(671088640 bytes) as the planned logical default, leaving35247177 bytes above
the encode trial for additive public controls and declared charges. Actual
API queries and default construction must verify those costs before public
admission. These finite precursors establish neither an RSS ceiling nor an
all-corpus comparison; physical peaks and logical grants remain distinct.

Preserve immutable direction, checked refusal before invalid allocations,
success-only queries, constructor rollback, allocation-free process calls,
declared I/O capacities and private-frame quarantine. Flush remains neutral;
ResetBlock is unsupported. Public API/CLI, actual ABI/default qualification,
all-member same-window contextual comparisons and ordinary peaks, schema82
exchange and final local audit remain open. Fuzz is omitted by user request.

### Public C API qualification

TVG-1519 qualifies the additive configuration/resources/factory boundary,
prior declaration preservation and actual layouts. Both complete default
configurations construct under640MiB. Actual public queries with65536 caller
retention bytes and two65536 call buffers report635845671 encode and251157534
decode bytes, with200816 external bytes; private owner costs remain separate.
Exact/one-short limits, all10/7 constructor faults, aliases, decision floors,
declared capacities, allocation-free processing and failed-frame quarantine
pass. All499 independent raw fixtures agree across owned/public-normal/public-
instrumented paths. Normal9/9,19 direct ASan/UBSan units, prior88/91 runtime
restoration/re-encoding and independent artifact audit pass. CLI, all-member
comparisons/ordinary peaks, schema82 exchange and final audit remain open.
Fuzz is omitted by request.

### Command-line qualification

TVG-1520 qualifies the exact selector
`lzss-position-distance-blocked-huffman-8m` through static and shared commands.
Seven boundaries,211 truncations, independent short/max-length reference
streams and malformed identity/model/padding/size cases pass. A valid first
frame is decoded independently before checking failed-second-frame whole-file
quarantine; existing ordinary and temporary outputs are retained. Both paths
restore and reproduce all91 preceding archive bodies exactly. Normal4/4 and
independent artifact audit pass. Fuzz remains omitted by user request.
All-member same-window comparisons, ordinary peaks, schema82 exchange and
final audit remain open.

### Same-window control and measurement infrastructure

TVG-1521 qualifies normal paired measurement paths on five boundary extents.
The existing contextual sixteen-MiB profile admits an8388608-byte window/frame;
its actual identity remains dictionary2/5, context1/4, entropy2/2. Set maximum
frame/distance8388608, decision limit58720256 and payload limit110100480,
retaining the profile's512MiB logical bound. Actual headers and restoration
qualify this configuration as the same-window control, despite the absence
of a public contextual8MiB selector. Its different field model is retained.
Normal2/2 and independent artifact audit pass. Full twelve-member compression,
speed and ordinary peak results, schema82 exchange and final audit remain
open. Fuzz is omitted by user request.

### Complete paired corpus and ordinary-peak checkpoint

TVG-1522 and [BM-0253](../benchmarks.md) qualify all12 actual same-window
comparisons, repeated bodies, restoration, normal timing and separate physical
peaks. Native total63341198 bytes is3184 bytes smaller than contextual63344382;
all12 members improve. Aggregate encode time is24.0854% lower, decode time
45.7824% higher, and both directional peaks are higher on all12 members.
The matched contextual profile retains its sixteen-MiB field-model identity
while actually configuring an eight-MiB window/frame and eight-MiB limits.
The small size margin and measured costs favor retaining explicit profiles;
these are complete-path observations, not an entropy-only causal claim.
Normal build/source/binary bindings, actual contextual queries, all120 serial
intervals and raw observations pass independent audit. Retain the640MiB
logical default; physical peaks do not redefine admission. Fuzz execution
remains omitted by request. Schema82 exchange, final local profile audit and
later family/hosted/external gates remain open.

### Final local qualification

TVG-1523 and [IX-0100](../interoperability.md) qualify schema82/92 in four
local self/cross routes, exact91-entry and starting88-entry prefixes, every
actual historical restored/re-encoded body through schema82 and375 directed
pre-launch refusals. Independent closure binds all staged correctness and
failure-contract evidence,119 unchanged qualified source/test files,657 normal
source/object pairs, ordinary product/runtime hashes and preserved backups.

The local profile is qualified with fuzz execution omitted by user request.
Completed BM-0253 observations are retained; no future ratio/speed/physical-
peak benchmark is required during this goal under the later maintainer
instruction. Resource admission and failure correctness remain mandatory.
Three larger profiles, final95 local audit, user push, hosted CI and external
four-route95 confirmation remain separate unfinished gates.

