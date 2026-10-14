# Four-MiB position-distance tANS

## DD-1592: exact four-MiB specialization

This document specializes the family definition in
`lzss-position-distance-tans.md` for `F = 4,194,304` and `N = 22`.
It defines a new stream identity; it does not change the 64KiB or one-MiB
representations. The public selector is
`lzss-position-distance-tans-4m`. The dictionary window and configured frame
maximum are both 4,194,304 bytes. Match lengths remain 3 through 258, and
the public encoder uses fixed-five eligibility followed by deterministic
longest-match and nearest-distance tie breaking.

The stream identity is dictionary `2/10`, context `1/23`, entropy `5/5`,
54 contexts, and 4,636 frequencies. The alphabet sequence is
`[2]*3 + [256]*17 + [9]*3 + [23]*9 + [2]*22`. The first three contexts
represent preceding token kind; literal contexts start at 3 and subsequent
literals use `4 + (previous_literal >> 4)`; matches preserve literal
history. Length contexts are 20 through 22. Distance classes use contexts
23 through 45, and residual distance bits use contexts 32 through 53.
The exact distance class 22 is an isolated grammar boundary with zero
residual bits; it cannot be reached by a nonempty reset frame of at most F
raw bytes.

## Descriptor and tANS rules

The descriptor begins with the 24-byte little-endian prefix used by the
family: decisions u32 at offset 0, payload bytes u32 at 4, table log u8 at
8, final valid bits u8 at 9, flags u16 at 10, context count u16 at 12,
reserved u16 at 14, frequency count u32 at 16, and reserved u32 at 20.
The active mask begins at 24 and occupies seven bytes; its unused two high
bits are zero. Active records follow in context order. Mode 0 is a singleton,
mode 1 is dense with the final frequency implied, and mode 2 is sparse with
ascending symbols and the final frequency implied. The canonical choice is
the shortest record, with mode 0 preferred for one symbol.

Present frequencies are normalized to total 4096 with the specified integer
rounding, minimum-one clamp, largest signed error repair, and deterministic
ties. The maximum descriptor extent is 9,249 bytes. Empty entropy is the
zero-mask descriptor followed by payload `00 00`.

The tANS table uses spread step 2563 modulo 4096, ascending symbols and
ascending occurrences. State values are 4096 through 8191. The length
residual bank is uniform 2048/2048. Distance residual bits use their declared
adaptive context models and are emitted LSB-first. Payload stores the final
state as little-endian u16 followed by the prepended bit sequence. Strict
decoding requires the terminal state 4096, exact decision and bit counts,
zero high padding, and every active model requested.

## Frames and checked limits

The known-size MARC2.0 stream header is 112 bytes and each MRF2 frame header
is 64 bytes. Header identity is dictionary 2/10, context 1/23, entropy 5/5,
window/frame 4,194,304, table log 12, state count 1, contexts 54 and
frequencies 4,636. Reserved, flags, side-data and trailer fields are zero;
dictionary, entropy and context parameter regions remain 16 bytes each.
Every frame resets dictionary and entropy state. Token, event and decision
counts are checked before allocation; raw frames are nonempty and sequence
numbers are exact. Invalid references, unknown identity, impossible model
records, truncation, nonzero padding and trailing data reject before ordinary
output publication. Failed frames may use private scratch but never expose a
raw prefix to the caller.

For T tokens and raw extent R <= F, decisions satisfy
`2T <= D <= min(32T, ceil(26R/3))`, and payload is bounded by
`2 + ceil(12D/8)`. The maximum theoretical frame payload is therefore
`2 + ceil(12 * 11,534,336 / 8) = 17,301,506` bytes; this is a format bound,
not a memory grant. Table storage is `(54 + 1) * 4096` entries and must be
charged separately from descriptor, token, payload and raw scratch. Actual
admission, peak memory, timeout and heavy-test parallelism require pilot
measurements.

## Independent vectors and gates

`tests/position_tans_4m_vectors.py` is an independent mathematical oracle.
It derives inverse transitions by enumerating the forward equations, checks
the descriptor parser, and freezes finite vectors for empty entropy, literal
history, lengths 3/4/258, distances through 4,194,304, all active dense
models, malformed descriptors and invalid payload states. Isolated exact-F
distance vectors are grammar tests only and are not evidence that a reset
frame can produce that reference.

The next milestones are the bounded production core, differential and
failure-invariance tests, public API/CLI, contextual 4MiB comparison, normal
speed/peak measurements, fuzzing, and schema74/84 exchange. Until those gates
pass, this document and its vectors do not admit a public factory or bundle
entry.
