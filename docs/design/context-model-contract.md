# Context-model contract

Status: experimental design for the `0.2.x` line. Context-model identifiers and
variants are decoder-visible and never reinterpret an existing stream variant.

## Role

A context model is an invertible state machine between typed dictionary tokens
and modeled operations. It selects a fixed context and alphabet for every
symbol but does not perform entropy arithmetic.

```text
typed tokens <-> context model <-> modeled operations
```

The model may use only already accepted tokens and its own bounded state. It
must not inspect future raw input, entropy bytes, or private dictionary history.
Encoder and decoder reset it at the same outer-frame boundary.

## Modeled-operation vocabulary

```cpp
enum class ModeledOperationKind : std::uint8_t {
    Symbol,
    BypassBits,
};

struct ModeledOperation {
    ModeledOperationKind kind{};
    std::uint16_t context_id{};
    std::uint16_t alphabet_size{};
    std::uint32_t value{};
    std::uint8_t bit_count{};
};
```

For `Symbol`, `bit_count` is zero and `value < alphabet_size`. For
`BypassBits`, `context_id` and `alphabet_size` are zero, `bit_count <= 16`, and
unused high value bits are zero. This is an internal value contract, not a
serialized struct.

The producer and consumer report token and operation counts independently.
They obey the core no-zero-progress rule and retain terminal status. A complete
frame is deterministic regardless of caller chunking.

## LZSS field-context model variant 1

Context-model algorithm ID 1, variant 1 is named `LzssFieldContext`. It holds
only the previous token kind and the most recent Literal value. Both begin in a
distinct `Start` state and reset for every outer frame. A Match does not change
the remembered Literal.

Context identifiers and alphabets are fixed:

| Context IDs | Count | Alphabet | Selection |
|---:|---:|---:|---|
| 0..2 | 3 | 2 | token kind; Start, previous Literal, previous Match |
| 3..19 | 17 | 256 | literal; no previous Literal, then previous Literal high nibble 0..15 |
| 20..22 | 3 | 8 | length class; same previous-token states |
| 23..30 | 8 | 17 | distance class; selected by length class 0..7 |

Token kind symbol 0 means Literal and 1 means Match. State is sampled before
the current token and updated only after all of that token's operations have
been accepted.

For a Literal, emit the token-kind symbol followed by its byte in the selected
literal context.

For a Match with length `L` and distance `D`:

```text
length_value = L - 5 + 1
length_class = floor(log2(length_value))
length_extra = length_value - 2^length_class

distance_class = floor(log2(D))
distance_extra = D - 2^distance_class
```

Emit, in order:

1. Match token-kind symbol;
2. `length_class` in the selected length context;
3. `length_extra` as `length_class` LSB-first bypass bits;
4. `distance_class` in context `23 + length_class`;
5. `distance_extra` as `distance_class` LSB-first bypass bits.

The fixed LZSS variant-2 limits make length classes 0..7 and distance classes
0..16. Zero-count bypass operations are omitted. The decoder performs the exact
inverse checked arithmetic before publishing a typed token.

## Bounds and malformed input

Each Literal produces two modeled events and two entropy decisions. A Match
produces at most five modeled events and 26 entropy decisions when every bypass
bit is counted separately. A frame therefore satisfies
`event_count <= 2 * raw_frame_size` and uses the conservative checked bound
`decision_count <= 6 * raw_frame_size`. The implementation also enforces
configured token, event, decision, context-state, and aggregate-workspace
limits.

Reject an unknown model ID or variant, an unexpected context, a mismatched
alphabet, an out-of-range class, excess or missing bypass bits, checked-
arithmetic failure, an operation count mismatch, or a reconstructed token that
violates the typed-token protocol. Failure is frame atomic.

The reference decoder implements this boundary as a two-pass private inverse.
The first pass validates the complete modeled-operation frame and reconstructs
each token only as a local value; the second pass materializes caller-owned
typed-token staging after capacity and non-aliasing checks. Validation reports
the first failing operation and token indices plus the accepted decision and
raw prefixes, while publishing no partial token sequence.

The reference encoder uses the symmetric two-stage policy. Its write-free plan
first validates the complete typed-token frame and calculates exact operation
and decision counts. Only sufficient, bounded, non-aliasing operation staging
permits deterministic materialization; an invalid token, count mismatch,
policy failure, or short output leaves the entire operation span unchanged.

The private contextual Dynamic Range decoder may consume this contract
directly rather than allocating a modeled-operation array. It MUST derive each
request from the same prior-token state, validate the reconstructed token
before updating that state, and preserve the exact event and decision counts.
The reference bridge performs a write-free complete pass before a second
materialization pass, so entropy, token, count, raw-size, capacity, limit, and
alias failures publish no partial typed-token sequence.
