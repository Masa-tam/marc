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
