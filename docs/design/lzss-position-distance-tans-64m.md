# Sixty-four-MiB position-distance tANS

## DD-1608: exact sixty-four-MiB specialization

The native position-distance tANS specialization uses `F = 67,108,864` and
`N = 26`. Its identity is dictionary `2/14`, context `1/27`, and entropy
`5/9`. The alphabet sequence is `[2]*3 + [256]*17 + [9]*3 + [27]*9 + [2]*26`,
with 58 adaptive contexts and 4,680 frequencies. The descriptor keeps the
24-byte prefix, uses an eight-byte active mask, and has a maximum extent of
9,334 bytes. States are 4096 through 8191, table log is 12, and payload bits
are LSB-first after the little-endian final-state delta.

Frames reset dictionary and entropy state. For token count `T`, raw extent `R`,
and decision count `D`, the validator requires `2T <= D <= min(34T, 10R)`.
Descriptor, payload, padding, state, history, and sequence validation precede
publication. Failed entropy, token, or reconstruction work remains in private
scratch and leaves ordinary destinations unchanged.

The independent oracle derives 237 vectors, including the 64 MiB distance
boundaries and a dense descriptor at the declared 9,334-byte limit. Clang core,
token, frame-format, frame, stream, vector, and oracle-differential tests pass
locally. The public API resource query, fixed-capacity process path, CLI
selector, deterministic small-input round trips, malformed-input rejection,
and unpublished failed-frame check now pass locally. Benchmark, fuzz runtime,
exchange, contextual measurements, and release qualification remain open.
