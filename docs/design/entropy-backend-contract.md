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

## Contextual rANS variant 2

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

## Contextual rANS variant 3 compact descriptor

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

The private scalar decoder now provides a compact begin operation over an
exact descriptor span, frame-declared decision and payload sizes, exact
payload, decoder limits, and caller-owned fixed table storage. A parse failure
reports its compact format category and leaves the decoder in sticky
`invalid_descriptor` state without touching table storage. Successful input
uses the same table layout, Symbol/bypass transitions, completion checks, and
reuse policy as variant 2. Format-specific validation charges 23 through 9,025
descriptor bytes before the shared model/table core; it never substitutes the
fixed 9,052-byte charge.

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
