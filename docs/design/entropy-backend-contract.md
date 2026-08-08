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

## Backend substitution

A later rANS, tANS, or Huffman backend may consume the same operation sequence,
but it receives a distinct entropy variant and decoder-visible descriptor
layout whenever its bytes differ. It may serialize per-context tables and a
separate bypass-bit region, provided exact order, normalization, padding,
workspace limits, and frame-atomic validation are specified first.

Backend substitution never changes the dictionary variant or context-model
variant. Compression ratio, encode throughput, decode throughput, descriptor
overhead, and peak workspace can therefore be compared along one explicit axis.
