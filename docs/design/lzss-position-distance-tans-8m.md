# Eight-MiB position-distance tANS

## DD-1602: exact eight-MiB specialization

This document specializes the family definition in
`lzss-position-distance-tans.md` for `F = 8,388,608` and `N = 23`. It
defines a distinct stream identity and leaves all earlier position-distance
tANS representations unchanged. The intended public selector is
`lzss-position-distance-tans-8m`. Its dictionary window and configured frame
maximum are both 8,388,608 bytes. Match lengths remain 3 through 258; an
eventual public encoder uses fixed-five eligibility, exact longest matching,
and nearest-distance tie breaking.

The exact identity is dictionary `2/11`, context `1/24`, entropy `5/6`,
55 contexts, and 4,647 frequencies. Its alphabet sequence is
`[2]*3 + [256]*17 + [9]*3 + [24]*9 + [2]*23`. Contexts 0 through 2 encode
the preceding kind; literal contexts begin at 3, with later literals using
`4 + (previous_literal >> 4)` and matches preserving literal history. Length
contexts are 20 through 22. Distance classes use contexts 23 through 46 and
distance residual bits use contexts 32 through 54. Class 23 has a required
zero residual and cannot be emitted by a nonempty reset frame of at most F
bytes; it remains a grammar boundary for validation.

## Descriptor, state, and frame rules

The descriptor has the family 24-byte little-endian prefix: decision count
u32 at 0, payload size u32 at 4, table log u8 at 8, final valid-bit count u8
at 9, zero flags u16 at 10, context count u16 at 12, zero reserved u16 at 14,
frequency count u32 at 16, and zero reserved u32 at 20. The active mask is
seven bytes at offset 24. Its upper one bit is zero. Active models follow in
context order: singleton mode 0, dense mode 1 with a final implied frequency,
or sparse mode 2 with ascending symbols and a final implied frequency.
Singleton is mandatory for one symbol; otherwise choose dense when it is no
larger than sparse, and sparse otherwise. The descriptor is at most 9,270
bytes.

Present frequencies normalize to 4096 using the family integer rule and its
specified tie breaks. The tANS table uses the family 4096-state automaton,
spread step 2563, ascending symbols and occurrences. States range from 4096
through 8191. Length residuals use the 2048/2048 uniform model; distance
residuals use their adaptive contexts, low bit first. Payload starts with the
little-endian `final_encoder_state - 4096`, then its prepended LSB-first bits.
The empty entropy representation is the zero-mask descriptor and `00 00`
payload. Strict decoding requires terminal state 4096, exact decision and bit
counts, zero high padding, and use of every active model.

The known-size MARC 2.0 header is 112 bytes and MRF2 frame header is 64 bytes.
The header declares the identity above, an 8,388,608-byte window and maximum
frame, table log 12, state count 1, 55 contexts, and 4,647 frequencies.
Reserved, flags, side data, and trailer fields remain zero. Every nonempty
frame resets dictionary and entropy state. Failed candidates remain private;
no raw prefix from a failed frame may be published.

For token count T, raw extent R no greater than F, and decision count D,
`2T <= D <= min(33T, 9R)`. Payload is bounded by
`2 + ceil(12D/8)`, yielding a theoretical maximum of 113,246,210 bytes at a
full frame. This is a wire-format bound, not an allocation or admission
grant. Workspace limits, timeouts, and parallelism require measured evidence.

## Qualification status

`tests/position_tans_8m_vectors.py` is the independent mathematical oracle
for the identity, normalization, descriptor grammar, inverse transitions,
boundary lengths, and distances through 8,388,608. This specification and its
vectors do not admit an internal implementation, public API, CLI selector,
benchmark, fuzz target, or exchange archive. Those remain separate gates.
