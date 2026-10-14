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
and unpublished failed-frame check pass locally. The local schema-78 bundle
contains 88 archives, preserves the preceding order, and passed self
verification, 125 manifest/header rejection cases before codec launch, and
the schema-1 through schema-78 compatibility audit. The benchmark smoke also
passed, but its recorded small fixture is 1,125 bytes for position-distance
versus 1,104 bytes for the contextual control, so the smaller-than-contextual
target is not met by that fixture and no corpus-wide claim is made.

The benchmark smoke emits deterministic position/contextual measurements with
encode and decode timing plus queried workspace bytes. The fuzz target and
compile smoke are wired; sanitizer runtime execution is a separate GNU-driver
qualification step and is not claimed by this milestone. Cross-producer,
hosted-CI, external-route, full-corpus measurement, and similarity review
remain open release gates.
The updated Clang 22.1.3 build re-ran the ten 64 MiB CLI, benchmark-smoke,
core, token, frame-format, frame, stream, API, vector, and differential tests;
all ten passed in 54.38 seconds.

