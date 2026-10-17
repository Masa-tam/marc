# Native position-distance Blocked Huffman

## DD-1609: additive family and retained exchange baseline

The confirmed starting bundle is schema78 with **88 archives**, not81.
It contains the preserved81-entry older prefix plus the seven completed
position-distance tANS profiles. Keep all88 entries, order, sizes and bodies;
append the following seven selectors, in order, to reach95 archives. Planned
schemas are79 through85. No existing format or public selector changes.

The subsequent [seven-window local checkpoint](../interoperability.md)
at IX-0103 now qualifies all seven additions and95-archive local exchange.
Hosted CI and external four-route95 results remain separate.

| Window bytes | CLI suffix | Dictionary | Context | Entropy | Contexts | Alphabet extent | Descriptor ceiling bytes |
| ---: | --- | --- | --- | --- | ---: | ---: | ---: |
| 65536 | none | 2/8 | 1/28 | 2/3 | 48 | 4570 | 2518 |
| 1048576 | 1m | 2/9 | 1/29 | 2/4 | 52 | 4614 | 2547 |
| 4194304 | 4m | 2/10 | 1/30 | 2/5 | 54 | 4636 | 2561 |
| 8388608 | 8m | 2/11 | 1/31 | 2/6 | 55 | 4647 | 2563 |
| 16777216 | 16m | 2/12 | 1/32 | 2/7 | 56 | 4658 | 2575 |
| 33554432 | 32m | 2/13 | 1/33 | 2/8 | 57 | 4669 | 2578 |
| 67108864 | 64m | 2/14 | 1/34 | 2/9 | 58 | 4680 | 2590 |

These are format reservations, not public admission or completion claims.
Implement and qualify64KiB first, then1/4/8/16/32/64MiB. The selector family
is `lzss-position-distance-blocked-huffman` with the listed suffixes.
Dictionary grammar and fixed-five public encoder eligibility follow the
qualified position-distance family; new context and entropy variants identify
the distinct pooled Huffman representation. Earlier byte-stream and
contextual Blocked Huffman variants retain their exact representations.

## DD-1610: exact64KiB representation

One bounded raw frame is one independently reset typed-event Huffman block.
This extension's block accounting unit is binary decisions, not the baseline
byte-stream Blocked Huffman symbol count. Frame size is configurable1..65536
raw bytes; dictionary window is65536, wire match lengths3..258 and flags zero.
Known original size is mandatory. Empty input contains only the stream header.
No terminal flush or implicit additional frame is introduced.

### Events and context partition

Use48 contexts with alphabets `[2]*3+[256]*17+[9]*3+[17]*9+[2]*16`, extent4570.
Preceding token kind starts0, becomes1 after a literal and2 after a match.
Emit literal/match kind0/1 through context equal to that preceding kind.
Initial literal uses context3; later literals use `4+(previous_literal>>4)`.
Matches preserve previous-literal history. Length class uses context
`20+preceding_kind`: lengths3/4 use class8 and one uniform bit `length-3`;
otherwise set `y=length-4`, class=floor(log2(y)), then class uniform bits of
`y-2^class`, low bit first. Distance class=floor(log2(distance)) uses
context `23+length_class`. Distance extra bit p uses context `32+p`, low bit
first. Distance65536 is an isolated grammar endpoint unreachable inside a
nonempty reset frame of at most65536 bytes. Do not infer history validity
from an isolated entropy vector.

Uniform length bits are literal payload bits with context request -1.
Each counts as one decision. Every modeled symbol also counts as one
decision. Event and decision counts are equal in this extension.

### Pooled and optional context models

Five pooled fields have alphabets `[2,256,9,17,2]`: kind, literal, length
class, distance class and position-distance bit. Contexts0..2/3..19/20..22/
23..31/32..47 map respectively to those fields. Each active context may
override its pooled field with a separate codebook. Pool existence records
an observed field even when every request uses an override. The decoder
must observe every declared field and every declared override by completion;
unused symbols within a valid codebook are permitted. Uniform bits do not
activate a field. Identical pooled/override codebooks are legal input;
the encoder's strict cost rule will not select a redundant override.

Models are either a sole symbol using zero payload bits, or a complete
canonical prefix code with lengths1..15. Reject oversubscribed, incomplete,
zero-symbol or one-nonzero-length multi-symbol tables. Assign conventional
canonical numeric codes in increasing length then symbol order. Reverse
each assigned code within its length before LSB-first writing. Singleton
models consume a decision but no bits; declared counts bound all loops.

### Descriptor and model records

Prefix24 bytes: decision_count u32 at0, payload_size u32 at4, final_valid_bits
u8 at8, max_code_length u8=15 at9, field_count u8=5 at10, field_active_mask
u8 at11, context_count u16=48 at12, reserved u16=0 at14, alphabet_extent
u32=4570 at16 and flags u32=0 at20. Integers are little endian. The six-byte
override mask follows at24. Reject mask bits beyond the48 contexts and
field-mask bits beyond4. Records follow for active pooled fields in field
order, then declared overrides in context order. An override requires its
field to be declared active.

Each record starts with one mode byte:

- Mode0: one u8 sole symbol, less than the selected alphabet. No lengths.
- Mode1: ceil(alphabet/2) bytes of lengths, even symbol in the low nibble,
  odd symbol in the high nibble. Unused final high nibble must be zero.
- Mode2: present_count u16, then ascending u8 symbol/u8 positive-length
  pairs. Count is2..alphabet; no duplicate or out-of-range symbol is valid.

Mode0 is mandatory for one present symbol. For multiple symbols use dense
mode1 when `1+ceil(alphabet/2) <= 3+2*present_count`; otherwise use mode2.
Reject noncanonical mode choice, invalid lengths, tables, unused mask bits,
truncated records and trailing descriptor bytes. Descriptor extent is30..2518
bytes. The ceiling is `24+6+sum(1+ceil(A/2))` over the five field alphabets
and all48 context alphabets; it is an admission bound, not a claim that a
particular frame uses every possible model. Zero decisions require no active
fields/overrides, zero payload and zero final-valid bits. An isolated stream
of uniform bits is valid without any model.

### Payload, determinism and planning

Payload concatenates modeled codes and uniform length bits in forward event
order. It has no coder state prefix or trailer. Zero payload requires
final_valid_bits0; otherwise the field is1..8. All unused high bits are zero.
Require exactly the declared decisions, exact consumed valid bits, all field
and override usage, successful token/history validation and exact raw extent.
Reject nonzero padding, trailing bits/bytes and premature input termination.

The encoder counts bounded frame symbols in pooled and per-context models,
and uses the repository's independently implemented length-limited canonical
Huffman primitive at maximum15. Its deterministic package ordering is weight,
minimum contained symbol, leaf before package, then creation order; leaves
are introduced by ascending symbol, packages by level then adjacent-pair
order. One present symbol chooses the zero-bit representation. Build all
pooled models; choose an observed context override only when its exact
payload-bit saving is **strictly greater** than eight times its serialized
record bytes. A tie retains the pool. Uniform bits are excluded from these
models. Canonical record selection remains the rule above. Optimization
policy is encoder-visible; the decoder admits all structurally valid models.

### Stream and frame envelope

Use the position-distance family's explicitly serialized112-byte MARC2.0
stream and64-byte MRF2 frame envelope. Stream magic is MARC; u16 offsets
4/6/8/10 are2/0/64/1; offsets12/14/16/18 are2/8/2/3. Frame_size u32 is at20;
u32 offsets28/32/48 are16; original_size u64 is at40. Dictionary parameters
at64/68/72/76 are65536/3/258/0. Entropy bytes80/81 are15/5, u16 at82 is48,
u32 at84 is4570. Context u16 offsets96/98 are1/28. Every other field is zero.

Frame magic is MRF2, u16 header extent64 at4, flags0 at6, sequence u64 at8.
Raw/token/event/decision counts are u32 at16/20/24/28; payload/descriptor/
side-data/trailer bytes are u32 at32/36/40/44. Side data, trailer and48..63
are zero. Descriptor then payload follow. Frame raw extents exactly partition
the known original size, in zero-based sequence order. Every frame resets
dictionary, literal history, pooled models and overrides. No empty frame,
unknown size, auto-detection or new integrity trailer is introduced.

For tokensT and raw bytesR, require `1<=T<=R<=frame_size<=65536`,
`2T<=D<=min(26T,7R)`, event_count=D and payload_bytes<=ceil(15D/8).
The full-frame payload ceiling is860160 bytes and frame ceiling862742 bytes.
Check all arithmetic, limits, serialized extents and complete codebooks before
allocation or decoding. Reject unknown tuples and contradictory counts.
Ordinary descriptor/token/raw destinations commit only on success; private
scratch may contain a discarded prefix. A frame starts draining only after
entropy, history and reconstruction all succeed. Failed frames never publish;
prior valid frames remain available. CLI keeps its existing temporary-file
commit contract. Steady-state no-progress and terminal-status rules remain
those of the core API.

No default memory grant, physical peak, watchdog or heavy-test parallelism is
inferred from tANS/rANS. Measure first. Use the existing bounded canonical
primitives; do not add standalone Static Huffman as a public codec.

## Completion gates

At this milestone only the64KiB format and independent finite vectors are
being qualified; no new production factory, CLI, benchmark result, sanitizer
receipt or bundle entry is admitted. Later windows remain reservations.
Require internal validators/reference codec, differential and failure tests,
split/guarded public API and CLI, full-window cases, instrumented decoder
fuzz, actual same-window contextual comparison, encode/decode timing and
measured peak memory before each window's completion. Try existing contextual
APIs with matching window/frame for8/32MiB and verify actual identities and
restoration; source plausibility is insufficient.

Record every member and total, misses and response, and speed/memory tradeoffs.
Dynamic Range size is not an adoption condition. Keep independent local
commits for passing milestones and explicitly unfinished gates. Final95 needs
unchanged88-entry prefix, historical-schema compatibility, prelaunch negative
admission and local self/cross execution. Only after all local gates does the
maintainer push once and provide CI and95-archive external four-route results.

### 64KiB bounded entropy-core milestone

The internal descriptor validator/parser/serializer and supplied-codebook
scalar writer/reader now exist. The frozen140 positive vectors agree byte for
byte with this independent C++ path, and all57 malformed vectors reject.
Ordinary descriptors, serialized outputs and result sizes commit only on
success. Scalar encoding completes a full sizing/symbol/usage preflight before
writing ordinary payload bytes. Decode tables are private scratch; each symbol
destination commits only on successful read, while token/raw storage must
still remain private until entropy finish and later history validation.

The reader begins with one complete finite payload, not a public partial-buffer
transform. Coding/validation errors are sticky; overlapping regions reject
before state/scratch writes. Successful finish is idempotent and later reads
reject. Scalar tables reserve53 slots, with active-model entry admission at
511 nodes per declared model. All supplied regions are charged plus a16384-byte
bounded canonical/decode-construction temporary allowance. Descriptor work
charges its object,2518-byte scratch, exact wire and declared payload sizes.
These are conservative internal accounting policies, not native-stack/RSS
measurements or public default grants. Exact and one-short budgets are tested.

TVG-1473 records scalar differential, limits, alias, fifteen-bit fallback and
ordinary-destination tests. Frequency counting, pooled/override planning,
typed-token/frame integration, public API/CLI, streaming chunking, fuzzing,
measurements and exchange are still unfinished. No failed-frame publication
or complete codec claim is inferred solely from these scalar tests.

### 64KiB bounded frequency/planning milestone

An incremental encoder-side model builder now counts at most458752 decisions
(the7*65536 format bound), checks context/symbol validity before counting,
and builds the specified length-limited pooled and context candidates. It
does not retain the event sequence. Strict bit-cost comparison includes the
exact override record extent; an equal-cost candidate retains its pool.
Planning results commit only on success; counting failures are sticky.
Repeated finish is deterministic, and a successful planning snapshot does not
end the accumulator. Its explicit workspace charge covers the accumulator,
candidate/result descriptors and a131072-byte bounded package-construction
temporary allowance plus2518-byte format scratch. This is not a public grant,
measured native stack size or process peak.

TVG-1474 records hand-weighted and cost-boundary examples, complete equal-
frequency tables, a skewed distribution reaching the15-bit limit, a field
whose requests all use overrides, uniform-only payload and maximum zero-bit
decision admission. The directed unit and197 frozen differential cases also
pass ASan/UBSan with every reachable production translation unit instrumented;
this is not randomized fuzz coverage. Next integration must keep generated
events bounded without unnecessarily materializing a full decision array;
the supplied-model two-pass scalar codec remains the clear reference.
The private forward writer is now implemented and checked against the same
140 independent wires. It accepts decisions incrementally into private
payload/table scratch and commits the ordinary size only after exact counts,
bits and model usage validate. TVG-1475 covers sticky failures, overlap and
capacity checks, guards, repeat finish and maximum zero-bit decisions without
an event array. Its workspace charge sums full provided regions plus the
same16384-byte temporary allowance as the scalar core; this is not a physical
peak or a public memory grant.

### 64KiB private typed-token milestone

The token planner and encoder now visit the validated token sequence in
forward order, reusing the bounded model builder and private writer without
retaining a decision array. Ordinary descriptor/payload spans are written
only after the entire private encoding and descriptor serialization succeed.
The decoder reconstructs tokens once into private scratch, checking history
and declared raw bounds before each assignment. The ordinary wrapper copies
only after exact raw/decision counts and entropy finish have succeeded.
An error may leave an accepted private prefix, which is never a publication
count. These finite token helpers do not yet expose a raw frame or a public
partial-buffer transform. Empty isolated token input does not permit empty
wire frames.

Workspace admission includes the full capacity of every provided span and
the bounded simultaneous model/table temporaries, with separate planning,
encoding and decoding allowances. Exact/one-short quotas are tested and do
not represent measured physical peaks or public profile grants. TVG-1476
records48 independent valid wires, independent validation of all48 production
wires, five history/length failures, full-frame literal/overlap restoration
and directed sanitizer qualification. Raw-frame quarantine, public streaming
API/CLI, randomized fuzzing, measurements and exchange remain unfinished.

### 64KiB native envelope and private raw-frame milestone

Native stream/frame validators, explicit serializers and prefix/full extent
preflight now exist. Prefix admission validates the complete descriptor before
payload buffering; it does not claim entropy termination or history validity.
Huffman payloads may be empty when modeled decisions consume zero bits, and
do not have ANS state words. Full finite frame decode reconstructs tokens
once into private scratch, completes entropy/history checks, then reconstructs
private raw using the bounded typed reconstructor. Ordinary raw copies only
after every step succeeds. The scratch entry remains private even on success
until its future stream controller explicitly permits draining.

Frame requirements report the minimum private decoder footprint: exact input
extent, token storage, raw storage,53 decode-table slots and bounded nested
token/frame work. Caller destinations and controls are additional; actual
frame calls charge their complete provided regions. These logical admission
figures are not physical peaks or public grants. TVG-1477 records independent
raw restoration, descriptor-first parsing, transactional failures, exact
resource boundaries, final partial frames and reset history isolation. Raw
match selection, public chunked streaming/API/CLI, randomized fuzzing, corpus
measurements and exchange remain unfinished.

### 64KiB raw and bounded streaming milestone

The internal owned encoder now uses the existing exact indexed short-prefix
match finder with minimum eligible length5 and retains a bounded raw frame,
tokens, finder, canonical tables, private payload, private serialization and
pending finalized frame. The supplied-scratch decoder buffers the header,
validates the complete descriptor, collects the bounded payload, and validates
private tokens/raw before entering draining. No process call allocates.
Flush does not alter the specified frame partition; ResetBlock is unsupported.
Known output length alone does not imply EOS: explicit EndInput is required.
Errors are sticky; repeated EOS remains EOS. Failed frames never drain, while
prior validated frames remain available.

Owned requirements admit all buffers/object storage and bounded nested work
before allocation. The payload capacity is ceil(105*frame_size/8), and pending
frame capacity adds64+2518 bytes. Encoding separately retains a private copy.
Minimum header admission also checks a full decode-table bank; this nonallocated
floor is distinguished from encoder-owned buffers. TVG-1478 records split,
allocation-failure, exact-budget and495 independent raw-wire checks. Public
factories/CLI, randomized fuzzing, physical peaks, corpus comparisons and
exchange qualification remain unfinished.

## DD-1611:64KiB public API and resource boundary

The additive C API is `marc_lzss_position_distance_blocked_huffman_config_init`,
`marc_lzss_position_distance_blocked_huffman_resource_requirements` and
`marc_lzss_position_distance_blocked_huffman_create`. It selects only the
specified native tuple and immutable encode/decode direction. Encoding requires
the known original size. Frame capacity is1..65536; match eligibility stays5
while decoding accepts3..258. Default limits are65536 raw frame bytes,458752
decisions per block,860160 compressed payload bytes and27083 entropy entries
(53*511). Entry admission uses the actual active model count at frame prefix
time. A smaller entry cap does not change the preallocated table-span size.

The initial8MiB aggregate grant follows full-frame internal resource admission
and normal zero/periodic/pseudorandom resource probes. It is logical admission,
not process peak/resident memory. Resource queries separately expose private
payload, serialized staging, table bank, owned buffers, owner and fixed working
charges, nonallocated metadata floor and public/external charges. Minimum
aggregate is max(private admission, configured block ceiling) plus public/
external charge. Call capacities default65536 bytes each; retained caller
storage must be declared separately. Larger declared retention can require a
larger grant even though the raw window is unchanged.

Failed resource queries retain their output. Creation sets a null handle on
refusal except config/handle overlap, which leaves the overlapping storage
unchanged. Every construction allocation is reversible. Calls beyond declared
capacities reject before consuming or producing bytes. Guarded one-byte and
zero-output calls retain the core status contract; failed frames never drain.
Public C static/shared checks and independent raw-wire checks qualify this
API stage separately from CLI, fuzz, corpus measurements and exchange.

### 64KiB CLI connection

The exact selector connects to the same public factory through bounded65536-
byte I/O arrays and a declared65536-byte retained control charge. It inherits
the8MiB logical grant and private frame validation gate. The ordinary file is
published only after transform EOS, strict input completion, sink close and
successful temporary-file rename. A pre-existing destination or temporary is
not overwritten. TVG-1480 records deterministic reset boundaries, independent
short grammar, all small-stream truncations and a valid first frame followed
by a private-token terminal mismatch in the second frame. The file target
remains absent even if the lower transform accepted the first frame. This
checkpoint does not complete fuzz, corpus/peak or exchange qualification.

### 64KiB fuzz and complete corpus measurement checkpoint

FZ-0097 completes the five bounded10000-unit coverage-guided campaigns with
audited production instrumentation and no sanitizer/invariant finding.
BM-0250 records all12 same-window contextual comparisons, actual identities,
restoration, normal timing and separate physical peaks. The aggregate size
target is met; four per-member misses and slower/higher-memory cases remain
visible, with both selectors retained. This checkpoint leaves exchange
extension, historical-schema/self/cross compatibility and the final64KiB
audit open. Later windows remain reservations until those gates pass.

### 64KiB local qualification closure

IX-0097 closes the exchange and historical/self/cross gates with89 archives:
both producers preserve the original88 bodies and entry representations;
all four local routes restore and re-encode exactly. Schemas1 through79 pass
conversion/execution,125 admission negatives reject before codec launch,
and the final17-test qualification passes. The retained-artifact audit checks
the actual bodies and restorations, beyond console success alone. The fixed
Windows runtime retains its qualified bytes. Together with BM-0250 and
FZ-0097, this completes the64KiB local profile and permits1MiB format work.
The four member-specific size misses and mixed speed/memory results remain
documented; local completion does not imply universal compression benefit.
The remaining six profiles and final95 local/hosted/external gates stay open.

### One-MiB local qualification closure

The [one-MiB specialization](lzss-position-distance-blocked-huffman-1m.md)
completes its local gates at IX-0098: exact format/vectors, ordinary/private
decoders, failure quarantine, public API/CLI, all12 paired comparisons and
normal peaks, bounded instrumented fuzz,90-archive exchange and historical
schemas1..80. The final17-test suite and retained-artifact audit pass. Both
producers preserve the starting88 and preceding89 archive entries and bytes.
The size target is met on all12 materials with a small aggregate gain, while
decode time and memory costs remain explicit. Two of seven local profiles are
complete; four-MiB work may proceed. Five larger windows and the final95 local,
hosted CI and external four-route gates remain open.

### Seven-window local qualification

[IX-0103](../interoperability.md) closes the local format/implementation,
failure/resource, API/CLI and exchange gates for64KiB,1MiB,4MiB,8MiB,16MiB,
32MiB and64MiB. Schema85 retains the original88 entries and appends seven,
with all95 verified through four local self/cross routes and every historical
schema1..85. The family audit binds all56 production units to qualified
milestones and rechecks current public logical resource admission.

Earlier completed comparisons remain intact, including the64KiB member-specific
size misses and the measured speed/memory tradeoffs. By explicit request, fuzz
and further ratio/speed/physical-peak benchmarks were omitted for the rest of
this goal. Unmeasured results are not passes or claims that the contextual
comparison target was met. User batch push, hosted CI and external four-route
confirmation of95 archives remain open; local qualification does not supply
those results.
