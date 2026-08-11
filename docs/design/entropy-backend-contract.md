# Entropy-backend contract

Status: experimental design for the `0.2.x` line. This contract permits later
backend substitution without changing the typed-token or context-model layers.

## Role and block boundary

An entropy backend maps one bounded frame's modeled-operation sequence to a
payload and performs the exact inverse. It does not interpret LZSS tokens or
select contexts.

```text
modeled operations <-> entropy backend <-> descriptor and payload bytes
```

The reference boundary is block oriented because rANS, tANS, and Blocked
Huffman require bounded planning or reverse traversal. Streaming adapters may
collect and drain one immutable complete block. No modeled block crosses an
outer frame.

## Planning and coding interface

```cpp
struct EntropyPlan {
    std::uint32_t event_count{};
    std::uint32_t decision_count{};
    std::uint32_t descriptor_bytes{};
    std::uint32_t payload_bytes{};
};

EntropyResult plan(
    std::span<const ModeledOperation> operations,
    EntropyPlan& plan_out);

EntropyResult encode(
    std::span<const ModeledOperation> operations,
    const EntropyPlan& plan,
    std::span<std::byte> descriptor_out,
    std::span<std::byte> payload_out);

EntropyResult begin_decode(
    std::span<const std::byte> descriptor,
    std::span<const std::byte> payload);

EntropyResult decode_symbol(
    std::uint16_t expected_context,
    std::uint16_t expected_alphabet,
    std::uint32_t& value_out);

EntropyResult decode_bypass(
    std::uint8_t expected_bit_count,
    std::uint32_t& value_out);

EntropyResult finish_decode(
    std::uint32_t expected_event_count,
    std::uint32_t expected_decision_count);
```

Planning is write free. Encoding must reproduce the plan exactly and leaves all
serialized output unchanged if capacity or validation fails. Decode setup
validates descriptor, payload, and aggregate limits. Each request validates its
expected context, alphabet, bit count, and arithmetic state. Finalization checks
the event and decision counts and exact payload exhaustion. Returned values feed
only private context/token staging; the outer frame remains the caller-visible
publication boundary.

The context model, not serialized input, supplies the expected next operation
kind, context ID, and alphabet. A backend decoder returns only the next value;
it cannot select a different context or alphabet from the stream.

## Contextual Dynamic Range variant 2

Entropy algorithm ID 3, variant 2 reuses variant 1's unsigned interval,
normalization, delayed-carry, and five-shift termination arithmetic. Its
difference is model ownership:

- each Symbol context has an independent adaptive order-0 frequency table;
- every table begins with frequency one for every symbol in that context's
  fixed alphabet;
- a used table updates only after its symbol is coded;
- when a table total reaches 32,768, every frequency becomes
  `max(1, (frequency + 1) / 2)` and the total is recomputed;
- BypassBits are coded one bit at a time, least-significant bit first, with a
  fixed cumulative `(0,1)` or `(1,1)` over total 2 and do not update a model.

All 31 Symbol contexts are allocated at construction from the selected context
model's fixed schema. No serialized byte controls context count, alphabet size,
or allocation. The frame starts with all tables reset.

Variant 2 uses one 16-byte descriptor:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | decision count | Symbol events plus individual bypass bits |
| 4 | 4 | payload size | exact payload bytes; at least 5 |
| 8 | 2 | context count | `31` for `LzssFieldContext` variant 1 |
| 10 | 2 | flags | zero |
| 12 | 4 | reserved | zero |

The reference decoder owns fixed storage for the 4,518 frequency entries and
31 totals. After `begin`, every request is checked against the fixed context
schema and the descriptor decision budget before arithmetic state advances.
Any setup, request, arithmetic, truncation, or finalization error is sticky;
the requested value remains unchanged.

The Format 2 preflight and backend share one internal descriptor and constants
definition, so context count, table entries, and model-total limits cannot
drift between framing and arithmetic validation.

The private reference encoder accepts a complete bounded
`ModeledOperation` sequence. A write-free planning pass validates every fixed
context, alphabet, symbol, bypass width, unused field, decision count, local
limit, and final payload extent while running the exact arithmetic coder. The
materialization pass repeats that deterministic finite computation only after
exact output capacity and operation/output non-aliasing are established. It
writes only the planned payload extent and publishes the descriptor only after
the second pass reproduces the plan exactly.

The decoder bring-up bypass vector uses these six decisions:

```text
Symbol(context 0, alphabet 2, value 1)
Symbol(context 20, alphabet 8, value 2)
BypassBits(bit_count 2, value 2)       # physical bits 0, 1
Symbol(context 25, alphabet 17, value 1)
BypassBits(bit_count 1, value 0)
```

From reset models, variant-1 arithmetic and termination produce payload
`00 A4 3C 3C 38 00`. This is five modeled events and six entropy decisions.

## Retired Contextual rANS variant 2 identity

This section records the withdrawn fixed-descriptor experiment. Entropy
identity `4/2` remains permanently reserved, but current encoders do not emit
it and current decoders reject it before descriptor parsing. The implementation
details below are retained as provenance, not as a supported backend contract.

Entropy algorithm ID 4, variant 2 retains variant 1's scalar unsigned 64-bit
state, `table_log=12`, normalized total 4,096, lower bound `L=2^31`, byte
renormalization, and final-state-first payload layout. It changes model
ownership and the coded alphabet:

- every one of the 31 Symbol contexts owns one independent static normalized
  model over that context's fixed alphabet;
- a context used by the frame is normalized from only its Symbol operations,
  using variant 1's exact integer rule and numeric-symbol tie breaks;
- an unused context has all normalized frequencies zero;
- each bypass bit is a decision in the same rANS state under the fixed binary
  model `0:2048, 1:2048` and contributes no serialized frequency;
- one state covers the complete frame and resets to `L` at every outer-frame
  boundary.

The encoder counts and normalizes all Symbol contexts before coding. It then
traverses modeled operations in reverse order. Symbol operations use their
context table. Bypass operations traverse their significant bits from the
highest index down to zero, so the decoder recovers each value least-
significant bit first while walking operations forward. Zero-bit bypass
operations remain omitted by the context model.

Variant 2 uses one fixed 9,052-byte descriptor:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | decision count | Symbol operations plus individual bypass bits |
| 4 | 4 | payload size | exact payload bytes; at least 8 |
| 8 | 1 | table log | exactly 12 |
| 9 | 1 | flags | zero |
| 10 | 2 | context count | exactly 31 |
| 12 | 4 | frequency entry count | exactly 4,518 |
| 16 | 9,036 | normalized frequencies | 4,518 little-endian uint16 values |

Frequency slices occur in ascending context order and symbols occur in
ascending numeric order within each fixed alphabet. Each slice must either be
all zero or sum exactly to 4,096. The canonical encoder assigns nonzero
frequencies only to symbols observed in that context. A decoder request must
select a nonzero slice and a nonzero symbol. Finalization rejects a nonzero
context slice that was never requested, an unused zero slice that was
requested, an invalid or trailing renormalization byte, or a terminal state
other than exactly `L`.

The payload begins with final state `x` as little-endian uint64 and continues
with the completed renormalization region, exactly as variant 1. Planning is
write free and uses the conservative bound of two bytes per entropy decision
plus eight state bytes; exact planning may produce less. A reference decoder
may build 31 separate 4,096-slot tables, but must charge the complete
126,976-entry maximum to its entropy-table limit before exposing any table or
decoded value.

For raw byte `A`, `LzssFieldContext` emits Symbol `(context 0, value 0)` and
Symbol `(context 3, value 65)`. Each used context is a one-symbol model with
frequency 4,096, so the payload is the eight-byte state
`00 00 00 80 00 00 00 00`.

The private format boundary now implements this fixed descriptor independently
of state decoding. Its parser publishes only after all 4,518 entries, slice
sums, fixed fields, expected counts, payload bound, and caller limits pass.
Its serializer validates first and commits one complete temporary byte array.
The alphabet and flattened-offset constants are owned by the shared
`LzssFieldContext` schema; Dynamic Range and rANS consume that schema directly
without backend-named compatibility aliases.

The private decode-table boundary now materializes the reference layout. Its
caller supplies at least 126,976 `RansDecodeEntry` elements. Context `c` always
owns entries `[c * 4096, (c + 1) * 4096)`, including inactive contexts, whose
entries remain zero. Each active range entry records the selected symbol and
that symbol's cumulative start and frequency. The builder validates and takes
a private copy of all descriptor frequencies before touching output, so a
malformed descriptor, insufficient table limit, insufficient output, or
descriptor/output alias cannot expose a partial table. The completed span and
31 active-context flags are published together only after construction.

The private scalar decoder owns no table allocation. `begin` validates the
descriptor, exact payload extent, initial boundary state, and caller table
capacity before building those fixed tables and publishing its running state.

`decode_symbol` requires the context's fixed alphabet and an active model;
`decode_bypass` accepts 1 through 16 bits and recovers them least-significant
bit first under the fixed 2,048/2,048 split. Both consume the same state and
enforce the descriptor decision budget before committing a caller value.

`finish` requires exact event and decision counts, requires every serialized
nonzero context model to have been requested, requires state exactly `L`, and
then requires exact payload exhaustion. Errors are sticky, repeated finish is
an explicit lifecycle error, and a new successful `begin` resets the instance.
The decoder exposes no public profile and does not reconstruct typed tokens.

The private LZSS bridge now supplies those decoder requests directly from the
accepted `LzssFieldContextState`. It reconstructs and validates each complete
typed token without first materializing `ModeledOperation[]`. The first pass
validates parameters, declared counts, every token, raw extent, every entropy
decision, model use, and strict rANS completion. Only an identical second pass
writes the caller-owned token span.

Both passes reuse one caller-owned 126,976-entry table span. Preflight rejects
short tables and any overlap between the payload and the written table extent;
materialization additionally rejects payload/token and table/token overlap.
Thus entropy workspace writes cannot corrupt bytes or tokens still needed by
the pass, and a validation or capacity failure leaves all token output
unchanged. Raw reconstruction and frame publication remain later boundaries.

The private frame composition now places this bridge behind an rANS-specific
Format 2 header and descriptor preflight. The caller owns four independent
regions: serialized frame input, 126,976 decode entries, typed tokens, and raw
output. Capacity and all six pairwise overlap checks complete before the first
table write. After strict two-pass token inversion, the existing typed LZSS
reconstructor alone may publish raw bytes. Any failure keeps serialized
consumption at zero. The composition adds no encoder or public profile.

The private operation encoder constructs the matching forward entropy
boundary. It validates all modeled operations first, counts Symbol values per
context, and normalizes each used slice with the same integer-error and numeric
tie rules as scalar rANS variant 1; unused slices stay zero and bypass bits use
only the fixed binary model. Encoding walks operations backward and each
bypass field from its highest bit to bit zero, filling renormalization bytes
backward after the eight-byte final state. Planning writes no payload, exact
encoding rejects operation/payload aliasing, and descriptor publication occurs
only after the complete result validates.

The shared private model builder and reverse writer are also consumed by the
typed-LZSS direct encoder. That bridge supplies the same Symbol and bypass
decisions without storing native modeled-operation records. Its forward count
and backward encode passes must reproduce the reference operation encoder's
decision count, normalized descriptor, exact payload extent, and every payload
byte. The entropy primitives remain unaware of token meaning.

## Contextual rANS variant 3 canonical descriptor

Entropy algorithm ID 4, variant 3 retains variant 2's normalized models,
single scalar state, payload bytes, decision ordering, bypass model, strict
completion, and fixed 126,976-entry reference decode-table ceiling. Only the
descriptor representation changes. It is variable length, with a 20-byte
prefix:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | decision count | identical to variant 2 |
| 4 | 4 | payload size | identical to variant 2 |
| 8 | 1 | table log | exactly 12 |
| 9 | 1 | flags | zero |
| 10 | 2 | context count | exactly 31 |
| 12 | 4 | frequency entry count | exactly 4,518 |
| 16 | 4 | active-context mask | bits 0 through 30; bit 31 is zero |

Records for set mask bits follow in ascending context order. No record exists
for an inactive context, whose full frequency slice is implicitly zero. An
active context with alphabet size `A` and `K` nonzero symbols uses exactly one
of these canonical forms:

- Dense (`mode=0`): the mode byte followed by `A-1` little-endian uint16
  frequencies for symbols `0..A-2`. The final frequency is
  `4096 - sum(explicit)`.
- Sparse (`mode=1`): the mode byte, one byte `K-1`, then the first `K-1`
  entries as increasing `(symbol:uint8, frequency:uint16-le)` pairs, followed
  by the final increasing `symbol:uint8`. Its frequency is
  `4096 - sum(explicit)`.

Every stored or inferred nonzero frequency in sparse form is positive. Dense
form may contain zero values but its reconstructed slice must contain at least
one nonzero value. Sparse symbols are strictly increasing and below `A`.
The canonical encoder chooses sparse exactly when `3K < 1 + 2(A-1)`; equality
selects dense. A decoder rejects the noncanonical alternative after rebuilding
the slice. It also rejects an empty active mask, bit 31, unknown modes,
truncation, overflow, a sum greater than or equal to 4,096 before an inferred
sparse frequency, a dense sum greater than 4,096, mask/slice contradiction,
or trailing descriptor bytes.

The smallest general descriptor is 23 bytes for one one-symbol sparse context.
Encoding every context densely is the exact maximum:
`20 + 3*3 + 17*511 + 3*15 + 8*33 = 9,025` bytes. Descriptor length comes from
the enclosing frame header and is checked before parsing. Variant 3 does not
reuse variant 2's fixed 9,052-byte identity and does not alter its decoder.
The private descriptor parser and serializer implement this representation in
isolation. They reconstruct into fixed local storage, validate canonical size
and decoder limits, require exact input consumption, and publish only after
the complete descriptor succeeds. The descriptor module itself admits neither
rANS state nor frames; state admission is the separate boundary below.

The private scalar decoder provides one canonical begin operation over an
exact descriptor span, frame-declared decision and payload sizes, exact
payload, decoder limits, and caller-owned fixed table storage. A parse failure
reports its format category and leaves the decoder in sticky
`invalid_descriptor` state without touching table storage. Successful input
uses the same table layout, Symbol/bypass transitions, completion checks, and
reuse policy as variant 2. Format-specific validation charges 23 through 9,025
descriptor bytes before the shared model/table core; it never substitutes the
fixed 9,052-byte charge.

The canonical implementation uses unqualified Contextual rANS names at its
descriptor, scalar decoder, typed-token bridge, frame, streaming, fuzz, and
public boundaries. There is no descriptor-object decoding entry and no
compact-qualified compatibility alias. The dense/sparse record primitive
retains its generic compact-model name because Contextual tANS shares that
representation rule.

## Contextual tANS variant 2

Entropy algorithm ID 5, variant 2 retains tANS variant 1's `table_log=12`,
table size `L=4096`, live-state interval `[L,2L)`, spread step 2,563,
LSB-first additional bits, and exact terminal state `L`. It changes model
ownership while retaining one state for the complete modeled frame:

- every used Symbol context owns an independent static normalized model and
  deterministic spread/transition table;
- normalization uses rANS/tANS variant 1's exact integer rule and numeric-
  symbol tie breaks independently within each fixed context alphabet;
- unused Symbol contexts have no record and an implicit all-zero model;
- each bypass bit selects one implicit fixed binary tANS table normalized as
  `0:2048, 1:2048`, and contributes no serialized frequency;
- all context tables, the bypass table, and the state reset at every outer
  frame.

Encoding traverses modeled operations in reverse. A Symbol uses its context's
inverse transition. A bypass field traverses its selected bits from the
highest index down to zero, so forward decoding reconstructs the value least-
significant bit first. The completed payload begins with little-endian uint16
`x-L`, followed by the logical decoder-order bit sequence packed LSB first.

Variant 2 uses a variable descriptor with this 24-byte prefix:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | decision count | Symbol operations plus individual bypass bits |
| 4 | 4 | payload size | exact bytes; at least 2 |
| 8 | 1 | table log | exactly 12 |
| 9 | 1 | final valid bits | 0 iff payload size is 2; otherwise 1..8 |
| 10 | 2 | flags | zero |
| 12 | 2 | context count | exactly 31 |
| 14 | 2 | reserved | zero |
| 16 | 4 | frequency entry count | exactly 4,518 |
| 20 | 4 | active-context mask | bits 0 through 30; bit 31 is zero |

Active-context records follow in ascending context order and use contextual
rANS variant 3's exact canonical dense/sparse forms and selection inequality.
The smallest general descriptor is 27 bytes and the all-dense maximum is 9,029
bytes. Descriptor length comes from the enclosing frame header and parsing
must consume it exactly.

For `D` decisions, payload size is at most `2 + ceil(12D / 8)`. Decoding
charges 32 complete 4,096-entry transition tables before construction: 31
possible Symbol contexts plus the implicit bypass table. It rejects a
requested inactive context, an unrequested active context, an invalid state
transition, insufficient bits, extra valid bits, nonzero high padding, a
terminal state other than `L`, or any count mismatch.

For raw byte `A`, `LzssFieldContext` emits Symbol `(context 0, value 0)` and
Symbol `(context 3, value 65)`. Each is a one-symbol model with frequency
4,096. Both transitions leave state `L` unchanged and emit no bits, so the
payload is `00 00` and final valid bits is zero.

The private format boundary now implements this descriptor independently of
tANS state or table construction. Prefix validation enforces decision and
payload bounds, final-valid-bit consistency, fixed identities, zero flags and
reserved bytes, frame-supplied counts, the 131,072-entry table ceiling, and
aggregate buffering. A shared private compact-model primitive owns only the
dense/sparse record rules used by both this variant and contextual rANS
variant 3; each backend retains distinct prefix and limit validation. Parsing
and serialization publish only after the complete descriptor succeeds.

## Contextual Blocked Huffman variant 2

Entropy algorithm ID 2, variant 2 consumes the same modeled-operation sequence
with static length-limited canonical Huffman tables rebuilt at every outer
frame. Four pooled tables correspond to token kind, literal, length class, and
distance class. An optional table for context ID `C` overrides only its pooled
table. Every Symbol decision selects the override when mask bit `C` is set;
otherwise it selects the field table. Bypass values enter the same payload as
raw LSB-first bits in operation order.

The variable descriptor begins with 16 bytes:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | decision count | exact Symbol decisions plus bypass bits |
| 4 | 4 | payload size | exact bytes; zero is permitted |
| 8 | 4 | override mask | context bits 0..30; bit 31 zero |
| 12 | 1 | final valid bits | zero iff payload is empty; otherwise 1..8 |
| 13 | 1 | maximum code length | exactly 15 |
| 14 | 1 | field-active mask | exactly `0x03` or `0x0f` |
| 15 | 1 | flags | zero |

`0x03` activates token-kind and literal tables for an all-Literal frame;
`0x0f` additionally activates the inseparable length and distance tables when
the frame contains a Match. Active field records follow in numeric field order,
then override records follow in ascending context order. An override requires
its field bit. Every record uses its inferred alphabet and one canonical form:

- Single (`mode=0`): four bytes `(mode, 0, symbol:uint16-le)`. The symbol is
  below the alphabet and consumes zero payload bits.
- Sparse (`mode=1`): four-byte prefix `(mode, K-1, 0:uint16-le)`, followed by
  `K` increasing `(symbol:uint8, code_length:uint8)` pairs.
- Dense (`mode=2`): the same prefix followed by `ceil(A/2)` bytes. Symbol
  `2i` uses the low nibble and symbol `2i+1` the high nibble; the unused high
  nibble for odd `A` is zero.

Canonical tables have `K >= 2`, lengths 1..15 for present symbols, zero for
absent symbols, and a complete non-oversubscribed code space. Sparse is used
exactly when `2K < ceil(A/2)`; equality selects dense. Canonical numeric codes
are assigned normally and reversed within their length before the common
LSB-first writer. Single records are not one-bit canonical tables.

For `D` decisions, payload size is at most `ceil(15D/8)`. Symbol codes and raw
bypass bits remain interleaved in modeled-operation order. The final partial
byte has exactly the declared low valid bits and zero unused high bits. Strict
completion consumes every declared operation and valid payload bit, requests
every serialized override at least once, and rejects an invalid path,
truncation, extra bits, nonzero padding, or an unused override.

The canonical encoder builds pooled tables from complete field histograms even
when overrides are selected. It selects an active context override only when
its symbol-bit saving is strictly greater than eight times its complete record
size; a tie remains pooled. Descriptor parsing validates representation and
limits but cannot infer profitability from code lengths alone; deterministic
encoder tests own that rule.

The smallest nonempty descriptor is 24 bytes: its 16-byte prefix plus two
single field records. The all-dense maximum is 2,561 bytes. Decoder planning
charges at most 35 bounded Huffman tables before construction and retains the
complete-frame token and raw publication transaction.

The entropy decoder is a forward state machine. `begin` validates the complete
descriptor and exact payload extent, rejects nonzero high padding, and builds
only non-Single canonical tables into caller-owned workspace. A Symbol request
must supply the exact schema context and alphabet. It selects the context
override when present or the context's pooled field table otherwise. A Single
selection returns its stored symbol without consuming a bit; every other
selection consumes one complete canonical code. A bypass request accepts 1
through 16 bits and reconstructs its value least-significant bit first from the
same cursor.

Each Symbol counts as one event and one decision. Each bypass field counts as
one event and one decision per bit. Failed requests do not publish their output
value. `finish` requires the caller's event and decision counts to equal the
descriptor budget, every serialized override to have been requested, and the
bit cursor to equal the exact valid-bit extent. It rejects premature input,
invalid paths, extra valid bits, repeated completion, and requests before
`begin`. Pooled tables may remain unrequested because the encoder retains the
complete pooled histograms even when all requests for a field select overrides.

The typed-LZSS adapter owns context evolution, class inversion, and token
validation. It reconstructs each token in a write-free validation pass, checks
the declared token/event/decision/raw-size tuple and all dictionary limits,
then repeats the deterministic pass into caller-owned token storage. It derives
the exact number of non-Single decode tables from the descriptor; an all-Single
frame therefore accepts empty table workspace. Payload, used table workspace,
and published token ranges must be pairwise disjoint. No token is published
unless the complete first pass, entropy completion, and raw-size check succeed.

The complete-frame decoder validates the `2/2` stream configuration and the
64-byte Format 2 frame header before locating variable data. It cross-checks
sequence, expected raw extent, token/event/decision bounds, descriptor and
payload sizes, zero feature extents, caller limits, and the exact descriptor.
Only a fully preflighted `header || descriptor || payload` region may enter
token inversion. Serialized input, used decode tables, typed tokens, and raw
output are pairwise disjoint. Raw reconstruction begins only after complete
token validation; `serialized_consumed` is published only after raw success.

The private streaming decoder incrementally collects the fixed headers and one
bounded complete frame, invokes that transaction, and drains only validated raw
bytes. Because the active Huffman table count is descriptor-dependent, exact
table capacity and aggregate buffered memory are checked after descriptor
preflight and before any decode-table construction. An all-Single frame keeps
the zero-table-workspace property. `EndInput` remains latched while final raw
bytes drain; truncation, trailing bytes, unsupported flags, and workspace/output
aliasing become sticky transform errors.

The operation encoder is the inverse entropy boundary. It gathers bounded
pooled/context histograms, builds canonical length-limited models, and admits an
override only for strict net bit savings after its canonical record cost. It
writes canonical codes and bypass fields through one forward LSB-first cursor;
Single models consume no bit. Planning completes before payload publication and
the descriptor is published only after successful encoding.

The typed-LZSS adapter uses the same builder and writer without materializing
modeled operations. Its first token pass validates context-dependent model
choices and fixes the descriptor; its second pass regenerates the identical
contexts and writes the payload. Writer completion cross-checks event, decision,
and exact valid-bit extents before descriptor publication.

The private complete-frame encoder composes raw LZSS tokenization with that
direct adapter. Its planning pass fixes and validates the complete
`64-byte header || descriptor || payload` extent before publication and charges
raw input, used token storage, and the serialized frame to the aggregate
workspace limit. These three ranges are pairwise disjoint. Encoding publishes
the header last, after the payload and canonical descriptor agree with the
plan; short output and every preflight error therefore leave the destination
unchanged.

The private streaming encoder retains exactly one raw outer frame and one
serialized complete frame around the typed-token workspace. It emits the
stream header before accepting or publishing frame bytes, invokes the exact
complete-frame transaction only when the raw extent is complete, and drains
that result before collecting its successor. `Flush` preserves a partial
frame; final input remains latched through output starvation. Contextual
Blocked Huffman requires no caller-owned encoder-table workspace.

The private profile computes those workspaces without inspecting input data.
Its encoder ceiling is the fixed header plus the 2,561-byte maximum descriptor
plus `ceil(6F * 15 / 8)` payload bytes for raw frame size `F`; its only typed
encoder view is `F` LZSS tokens. Decoder views contain the conservative maximum
35 Huffman tables followed by aligned token storage. All raw, serialized, view,
and publication extents are charged to the aggregate caller limit.

## Contextual Adaptive Huffman variant 2

Entropy algorithm ID 1, variant 2 applies one independent FGK tree to every
Symbol context in `LzssFieldContext` variant 1. Context `c` owns exactly the
fixed alphabet declared by the shared 31-context schema; a serialized stream
cannot enlarge that alphabet or select a different tree.

Each tree starts as one NYT node with number `2A`, where `A` is that context's
alphabet size. Splitting NYT number `n` retains `n` for the new internal node,
assigns `n-2` to the new NYT left child and `n-1` to the new symbol right
child, then performs the same leader selection, swap exclusions, weight
increments, and parent traversal as Adaptive Huffman FGK variant 1. A tree has
capacity `2A+1` nodes. Tree weights are unsigned 32-bit values; the Format 2
frame limits keep every context below overflow, so variant 2 performs no
mid-frame rescaling. All 31 trees reset at every outer-frame boundary.

An existing Symbol emits its current root-to-leaf path. A new Symbol emits the
NYT path followed by `ceil(log2(A))` raw value bits, least-significant bit
first. The decoder rejects raw values at or above `A` and a NYT value already
present in that context. This uses one, three, five, or eight raw bits for the
current alphabets 2, 8, 17, and 256. Paths and raw values share the common
forward LSB-first cursor. BypassBits also enter that cursor least-significant
bit first but never inspect or update an FGK tree.

Variant 2 uses one fixed 16-byte descriptor:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | decision count | Symbol events plus individual bypass bits |
| 4 | 4 | payload size | exact payload byte extent; nonzero |
| 8 | 2 | context count | exactly 31 |
| 10 | 1 | final valid bits | 1 through 8 |
| 11 | 1 | flags | zero |
| 12 | 4 | reserved | zero |

Each Symbol counts as one event and one decision regardless of the number of
FGK path or NYT raw bits it emits. Each bypass field counts as one event and
one decision per bit. `finish` requires the frame-supplied event count and the
descriptor decision count, exact valid-bit consumption, zero unused high bits,
and a valid tree after every completed Symbol. Failed requests publish no
value and do not mutate a tree.

For the one-Literal operation sequence `Symbol(0,2,0),
Symbol(3,256,65)`, both contexts begin at NYT. The first raw value contributes
one zero bit and the second contributes the eight LSB-first bits of `0x41`, so
the payload is `82 00` with one valid bit in the final byte. This vector uses
no context-model inference inside the entropy backend.

The private format boundary now parses and serializes the fixed descriptor
atomically and enforces the context, decision, payload, final-bit, flag,
reserved-byte, and caller-limit rules above. The private bounded tree accepts
caller-owned node and symbol spans, consumes exactly `2A+1` nodes and `A`
symbol slots, derives initial order `2A`, and implements the specified FGK
paths and updates for alphabets 2 through 256. Neither component yet reads or
writes a payload bit or owns the array of 31 trees.

The private model bank now partitions caller-owned storage into all 31 exact
tree and symbol slices: 9,067 nodes and 4,518 symbol indices. The operation
decoder owns that bank for one `begin`/request/`finish` lifecycle, traverses
Symbol paths and alphabet-width NYT values, and interleaves BypassBits through
one LSB-first offset. A request commits its value and bit offset only after the
complete operation and tree update succeed. Begin rejects padding, overlap,
short workspace, aggregate memory, and entropy-entry limits; finish validates
counts, exact bit exhaustion, and every tree. Typed-LZSS context inference and
frame decoding remain outside this boundary.

The private LZSS adapter supplies that context inference without changing the
entropy contract. Starting from reset state, it requests token kind, literal,
length class, distance class, and bypass bits from their fixed contexts,
reconstructs the canonical typed token, validates it, then advances state.
The adapter validates the complete sequence without output before repeating
the decode into caller storage. Thus token publication is atomic at the
sequence boundary; the entropy backend still has no knowledge of LZSS tokens.

The private frame-format layer now validates the outer contract before this
backend is entered. It fixes entropy identity `1/2`, the 31-context and NYT
parameter region, the 2^24 raw-frame ceiling, exact descriptor and payload
extents, and the model-entry limit. It does not allocate or initialize a tree;
the validated payload and descriptor remain inputs to the operation decoder.

The complete-frame consumer keeps those entropy workspaces caller-owned and
separate from token and raw storage. It invokes the operation decoder only
after every region is proven disjoint, then reconstructs raw LZSS bytes only
after the full token sequence validates. The entropy backend therefore still
publishes no raw bytes and retains no state beyond one frame.

The private stream transform preserves that frame-local ownership. It holds
the serialized frame and exact 9,067-node, 4,518-symbol, declared-token, and
raw workspaces only until complete-frame validation succeeds and the raw bytes
are drained. It never resumes an FGK tree across frames, never publishes a
partially decoded frame, and does not expose the model layout through a public
ABI.

The private operation encoder is symmetric with the decoder request boundary.
It plans with the same exact model slices, then resets and repeats the forward
operation sequence into an exact zero-filled payload prefix. A Symbol emits
the pre-update tree path and optional NYT value before observing the symbol;
BypassBits never touch a tree. Descriptor publication follows a second-pass
agreement check, while excess output capacity remains untouched.

The LZSS adapter invokes the same forward planner/writer one field at a time.
It owns context inference and class/extra-bit decomposition; the entropy
backend still sees only context, alphabet, symbol, and bypass requests. No
operation array, native token representation, or dictionary byte
serialization enters the entropy boundary.

The complete-frame encoder preserves that separation. Raw LZSS parsing ends at
caller-owned typed tokens; the direct adapter alone converts those tokens into
contextual entropy events. The outer frame layer records only validated counts,
the fixed descriptor, and payload, while resetting the exact model bank for
each planned and written frame.

The streaming producer retains only the serialized result of that complete
transaction while draining. It never advances an FGK tree incrementally across
process calls or frames; chunking affects only raw collection and byte drain,
not entropy decisions or the resulting representation.

The profile boundary describes model storage as typed counts and aligned byte
offsets without exposing it through a public ABI. Encoder and decoder layouts
may differ, but both bind exactly 9,067 nodes and 4,518 symbol indices before a
streaming object is constructed. Forged layout metadata never publishes a
partial view.

## Backend substitution

Backend substitution never changes the dictionary variant or context-model
variant. Compression ratio, encode throughput, decode throughput, descriptor
overhead, and peak workspace can therefore be compared along one explicit axis.

The first complete Format 2 encoder composes the operation-level reference
encoder without weakening this boundary. It obtains exact event and decision
counts from `LzssFieldContext`, requires the entropy plan to reproduce the
decision count, and serializes the 16-byte descriptor only after the frame
header validates against the stream context. No native operation or token
representation enters the stream.
