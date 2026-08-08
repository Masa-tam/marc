# LZSS typed-token protocol

Status: experimental design for the `0.2.x` line. This protocol does not alter
or replace any format-version-1 stream.

## Purpose and boundary

The protocol separates LZSS parsing from byte serialization. An LZSS encoder
produces typed `Literal` and `Match` events for one bounded outer frame. A
context model consumes those events directly; it does not parse the canonical
two-byte and nine-byte representation used by LZSS variant 1.

```text
raw frame -> LZSS parser -> typed token sink -> context model
context model -> typed token source -> LZSS reconstructor -> raw frame
```

The boundary is internal C++ infrastructure. It introduces no public C type,
native-structure serialization, allocator callback, or exception. A later C
surface may expose only opaque workspace byte counts and alignment.

## Token vocabulary

The protocol has exactly two events:

```cpp
enum class LzssTokenKind : std::uint8_t {
    Literal = 0,
    Match = 1,
};

struct LzssTypedToken {
    LzssTokenKind kind{};
    std::uint8_t literal{};
    std::uint32_t distance{};
    std::uint32_t length{};
};
```

These fields describe values, not an ABI layout. Implementations must not
serialize the object representation. A `Literal` uses `literal` and requires
`distance == 0` and `length == 0`. A `Match` uses `distance` and `length` and
requires `literal == 0`.

Variant 2 retains variant 1's deterministic greedy parse and nearest-distance
tie break. Its initial experimental limits are deliberately narrower:

- `minimum_match_length == 5`;
- `5 <= maximum_match_length <= 258`;
- `1 <= window_size <= 65,536`;
- dictionary history starts empty and resets at every outer frame;
- no token or match crosses an outer-frame boundary.

## Encoder-side contract

The producer reports token consumption separately from sink acceptance:

```cpp
struct TokenResult {
    std::size_t raw_consumed{};
    std::size_t tokens_produced{};
    StreamStatus status{};
    StreamError error{};
};
```

The sink may accept zero tokens only when it reports `NeedOutput` or an error.
`Progress` with both counts zero is forbidden. `EndInput` resolves the final
pending parse before the producer ends. Repeated calls after completion return
`EndOfStream`.

The reference implementation may buffer one raw frame before parsing. It must
query and validate raw-frame, token, dictionary-history, and aggregate limits
before accepting untrusted sizes. At most one token is produced per raw byte.

## Decoder-side contract

The consumer validates a complete bounded token frame before publishing any raw
byte. Validation checks:

- known token kind and zero unused fields;
- Literal and Match field ranges;
- distance against configured window and already reconstructed frame history;
- length against configured and local limits;
- checked output extent and exact declared raw-frame size;
- exact token count and absence of trailing events.

Overlap copying is bytewise from `output_position - distance`, identical to
LZSS variant 1. A malformed token frame publishes none of that frame, while
previously completed frames remain committed.

## Canonical diagnostic transcript

Typed events are not serialized between the dictionary and context layers.
For deterministic hashing, diagnostics, and tests, a canonical transcript is
defined independently of native layout:

- Literal: byte `00`, then the literal byte;
- Match: byte `01`, little-endian `uint32` distance, then little-endian
  `uint32` length.

This transcript deliberately matches the LZSS variant-1 token bytes, but it is
not the entropy input of the typed pipeline. A hash target must name
`TypedTokenTranscript` explicitly; it must not call this boundary an ordinary
dictionary byte stream.

## Reset and ownership

Token and parser state reset only at an outer-frame boundary. `Flush` does not
close a partial frame. A typed-token block cannot cross a frame. All token
arrays, parser tables, raw staging, and reconstruction staging are bounded and
caller owned or transform owned at construction; input cannot resize them.
