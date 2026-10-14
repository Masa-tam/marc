
# Thirty-two-MiB position-distance tANS

## DD-1607: exact thirty-two-MiB specialization

The native position-distance tANS specialization uses `F = 33,554,432` and
`N = 25`. Its identity is dictionary `2/13`, context `1/26`, and entropy
`5/8`. The alphabet sequence is `[2]*3 + [256]*17 + [9]*3 + [26]*9 + [2]*25`,
with 57 contexts and 4,669 frequencies. The descriptor keeps the 24-byte
prefix, uses an eight-byte active mask, and has a maximum extent of 9,313
bytes. States are 4096 through 8191, table log is 12, and payload bits are
LSB-first after the little-endian final-state delta.

Frames reset dictionary and entropy state. For token count `T`, raw extent `R`,
and decision count `D`, the validator requires `2T <= D <= min(34T, 10R)`.
Descriptor, payload, padding, state, history, and sequence validation precede
publication. Failed entropy, token, or reconstruction work remains in private
scratch and leaves ordinary destinations unchanged.

The independent oracle derives 236 vectors, including the 32 MiB distance
boundaries. Clang core, token, frame-format, frame, stream, vector, and
oracle-differential tests pass locally. The public API and CLI now have
deterministic round-trip and malformed-input coverage; the updated Clang
build also passes the new 16/32 MiB CLI campaigns. Benchmark smoke, fuzz
compile smoke, and schema77 exchange admission are recorded separately.
Corpus comparison, sanitizer runtime, cross-producer, hosted-CI, external
routes, and release similarity remain open.
