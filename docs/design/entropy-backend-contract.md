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
