# Specification-driven independent implementation record

This project follows a specification-driven independent implementation
process. It does not claim a formal clean-room development process and provides
no legal guarantee of non-infringement.

## 2026-07-12 — repository foundation

- Authoring method: requirements and architecture supplied in `AGENTS.md`,
  followed by independently authored build, ABI, and core-contract scaffolding.
- References used: C++20 language and CMake build-system documentation at the
  level recorded in `references.md`.
- Implementation sources consulted: none.
- Restricted implementations intentionally consulted: none.
- Design decisions: C++20, portable implementation with MSVC as reference,
  CMake as canonical build, small C ABI, no exceptions across ABI boundaries,
  same sources for static/shared builds, known original size in the baseline.
- Generated-code task description: establish the documented project skeleton,
  core process-result invariant checker, ABI metadata functions, and build
  verification tests without implementing a compression algorithm.
- Similarity review: not applicable; no codec implementation exists.

## 2026-07-12 — serialization and bit primitives

- Authoring method: independently implemented directly from the byte-order and
  bit-order requirements in `AGENTS.md` and the hand-checkable vectors recorded
  in `format.md`.
- References used: none beyond the repository requirements.
- Implementation sources consulted: none.
- Design decisions: bounded spans, checked unsigned arithmetic, stateful
  one-byte LSB-first buffering, zero padding on finish, strict padding checks.
- Generated-code task description: implement portable little-endian load/store,
  checked arithmetic, and partial-buffer LSB-first bit I/O with tests.
- Similarity review: simple direct expressions of the documented representation;
  no external implementation compared.

## 2026-07-12 — decoder limits and frame bounds

- Authoring method: independently derived from the resource-limit requirements
  in `AGENTS.md` before defining a decoder-visible frame layout.
- References used: none beyond the repository requirements.
- Implementation sources consulted: none.
- Design decisions: application policy is separate from format declarations;
  validate cumulative output, individual dimensions, simultaneous buffering,
  and expansion before allocation or decoding.
- Generated-code task description: add conservative decoder defaults and an
  overflow-safe validator for parsed frame bounds with negative tests.
- Similarity review: no external implementation compared.
