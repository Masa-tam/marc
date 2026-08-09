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

## Backend substitution

A later tANS or Huffman backend may consume the same operation sequence,
but it receives a distinct entropy variant and decoder-visible descriptor
layout whenever its bytes differ. It may serialize per-context tables and a
separate bypass-bit region, provided exact order, normalization, padding,
workspace limits, and frame-atomic validation are specified first.

Backend substitution never changes the dictionary variant or context-model
variant. Compression ratio, encode throughput, decode throughput, descriptor
overhead, and peak workspace can therefore be compared along one explicit axis.

The first complete Format 2 encoder composes the operation-level reference
encoder without weakening this boundary. It obtains exact event and decision
counts from `LzssFieldContext`, requires the entropy plan to reproduce the
decision count, and serializes the 16-byte descriptor only after the frame
header validates against the stream context. No native operation or token
representation enters the stream.
