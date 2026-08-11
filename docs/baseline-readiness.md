# Baseline readiness

This document separates locally verified implementation readiness from release
evidence that must be produced by external CI, additional architectures, and
representative measurements. It is a status index, not a replacement for the
completion criteria in `AGENTS.md` or the exact representations in
`docs/format.md`.

## Local implementation matrix

`Ready` means the profile has an exact format and validator, bounded one-shot
and streaming encode/decode paths, a public C ABI, CLI and benchmark adapters,
a bounded decoder fuzz target, and a public-ABI completion matrix covering
determinism, chunking, terminal behavior, and malformed final-frame handling.
`In progress` means a public profile exists but one or more of those local
readiness boundaries remain pending.

| Required codec | Public CLI profile | Local status | Interoperability schema 31 |
|---|---|---|---|
| LZ77 | `lz77` | Ready | Included |
| LZSS | `lzss` | Ready | Included |
| LZ78 | `lz78` | Ready | Included |
| LZW | `lzw` | Ready | Included |
| LZD (Lempel-Ziv Double) | `lzd` | Ready | Included |
| LZMW | `lzmw` | Ready | Included |
| Blocked Huffman | `blocked-huffman` | Ready | Included |
| Adaptive Huffman (FGK) | `adaptive-huffman` | Ready | Included |
| Dynamic Range Coder | `dynamic-range` | Ready | Included |
| rANS | `rans` | Ready | Included |
| tANS | `tans` | Ready | Included |

The internal canonical Huffman primitives are not a standalone public codec.
Their bounded frequency, length construction, canonical assignment,
serialization, decode-table, padding, and malformed-table behavior is covered
by component tests and exercised through Blocked Huffman.

## Additional public profiles

| Profile | Purpose | Local status | Interoperability schema 31 |
|---|---|---|---|
| `lz77-blocked-huffman` | First composed dictionary/entropy pipeline | Ready | Included |
| `lzss-blocked-huffman` | Second composed dictionary/entropy pipeline | Ready | Included |
| `lz78-blocked-huffman` | Third composed dictionary/entropy pipeline | Ready | Included |
| `lzw-blocked-huffman` | Fourth composed dictionary/entropy pipeline | Ready | Included |
| `lzd-blocked-huffman` | Fifth composed dictionary/entropy pipeline | Ready | Included |
| `lzmw-blocked-huffman` | Sixth composed dictionary/entropy pipeline | Ready | Included |
| `lz77-adaptive-huffman` | First Adaptive Huffman composition | Ready | Included |
| `lzss-adaptive-huffman` | Second Adaptive Huffman composition | Ready | Included |
| `lz78-adaptive-huffman` | Third Adaptive Huffman composition | Ready | Included |
| `lzw-adaptive-huffman` | Fourth Adaptive Huffman composition | Ready | Included |
| `lzd-adaptive-huffman` | Fifth Adaptive Huffman composition | Ready | Included |
| `lzmw-adaptive-huffman` | Sixth Adaptive Huffman composition | Ready | Included |
| `lz77-dynamic-range` | First Dynamic Range composition | Ready | Included |
| `lzss-dynamic-range` | Second Dynamic Range composition | Ready | Included |
| `lz78-dynamic-range` | Third Dynamic Range composition | Ready | Included |
| `lzw-dynamic-range` | Fourth Dynamic Range composition | Ready | Included |
| `lzd-dynamic-range` | Fifth Dynamic Range composition | Ready | Included |
| `lzmw-dynamic-range` | Sixth Dynamic Range composition | Ready | Included |
| `lz77-rans` | First rANS composition | Ready | Included |
| `lzss-rans` | Second rANS composition | Ready | Included |
| `lz78-rans` | Third rANS composition | Ready | Included |
| `lzw-rans` | Fourth rANS composition | Ready | Included |
| `lzd-rans` | Fifth rANS composition | Ready | Included |
| `lzmw-rans` | Sixth rANS composition | Ready | Included |
| `lz77-tans` | First tANS composition | Ready | Included |
| `lzss-tans` | Second tANS composition | Ready | Included |
| `lz78-tans` | Third tANS composition | Ready | Included |
| `lzw-tans` | Fourth tANS composition | Ready | Included |
| `lzd-tans` | Fifth tANS composition | Ready | Included |
| `lzmw-tans` | Sixth tANS composition | Ready | Included |
| `checksum-raw` | Version 1.1 per-frame CRC-32C framing profile | Ready | Included |

`lzmw-tans` now has bounded complete-frame validation and decoding, exact
planning and deterministic encoding, bounded streaming transforms, a public C
lifecycle, completion matrix, fixed-memory fuzz boundary with permanent
regressions, a transactional CLI selector, and a verification-first benchmark.
Schema 31 now supplies local generation, exact-order verification,
byte-identical re-encoding, reordered-manifest rejection, and schemas 1
through 30 compatibility. External cross-platform exchange remains release
evidence rather than a local readiness requirement. That exchange has now
passed in all four directions at revision
`903181080556c3bb511ad4a2e5275837ebda48e7` for all 42 archives across the
recorded Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers.

`lzd-tans` is the completed preceding admission composition. It has bounded
complete-frame validation, private raw decoding,
transactional caller-output publication, an exact write-free planner, a
deterministic serialized encoder, and bounded streaming transforms. The
streaming decoder commits only completely validated frames, and an internal
profile calculator supplies their coupled workspace requirements. A pure C11
query and factory now expose both directions through the shared library. The
public-only completion matrix proves required binary classes, deterministic
chunking, stable terminals, and malformed-final-frame atomicity. A fixed-memory
dual-decoder fuzz target now covers complete-frame and public incremental
parsing, with permanent atomic regressions for truncation, saturated frame
extents, and invalid tANS metadata. A transactional CLI selector now uses only
the public requirements query and lifecycle. A dependency-free benchmark now
verifies a complete public-ABI round trip before reporting ratio, throughput,
and all workspace regions. Schema 30 now supplies local generation, exact-order
verification, byte-identical re-encoding, reordered-manifest rejection, and
schemas 1 through 29 compatibility. External cross-platform exchange remains
release evidence rather than a local readiness requirement. That exchange has
now passed in all four directions at revision
`827ddf085efb40c7d8f9bc27628977053179d84c` for all 41 archives across the
recorded Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers.

Schema 31 contains forty-two archives: the frozen forty-one-entry schema-30 set
followed by the LZMW tANS profile. Schemas 1 through 30
remain frozen at seven, eight, thirteen, fifteen, sixteen, seventeen, eighteen,
nineteen, twenty, twenty-one, twenty-two, twenty-three, twenty-four,
twenty-five, twenty-six, twenty-seven, twenty-eight, twenty-nine, thirty,
thirty-one, thirty-two, thirty-three, thirty-four, thirty-five, thirty-six,
thirty-seven, thirty-eight, thirty-nine, forty, and forty-one profiles;
their meanings are fixed by their version and codec-set rules.

## Public-profile evidence matrix

Every `Yes` below names an implemented and test-covered repository boundary.
`Completion` is the public C ABI matrix covering required data classes,
deterministic output, one-byte and mixed chunking, repeated terminal calls,
and transactional rejection of a malformed final frame. Interoperability is
kept separate because it requires artifacts produced outside the local build.

| Public profile | Format + validator | Streaming | C ABI | CLI | Benchmark | Bounded fuzz | Completion | Schema 31 |
|---|---|---|---|---|---|---|---|---|
| `lz77` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzss` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lz78` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzw` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzd` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzmw` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `blocked-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `adaptive-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `dynamic-range` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `rans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `tans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lz77-blocked-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzss-blocked-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lz78-blocked-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzw-blocked-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzd-blocked-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzmw-blocked-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lz77-adaptive-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzss-adaptive-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lz78-adaptive-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzw-adaptive-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzd-adaptive-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzmw-adaptive-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lz77-dynamic-range` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzss-dynamic-range` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lz78-dynamic-range` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzw-dynamic-range` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzd-dynamic-range` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzmw-dynamic-range` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `checksum-raw` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lz77-rans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzss-rans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lz78-rans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzw-rans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzd-rans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzmw-rans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lz77-tans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzss-tans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lz78-tans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzw-tans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzd-tans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |
| `lzmw-tans` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |

## Current validation baseline

All forty-two profiles in the current composition matrix satisfy the local
`Ready` definition above and are present in interoperability schema 31. The
internal canonical Huffman primitives remain support components rather than a
separate public profile.

The optimized Release configuration currently enumerates 2,355 tests under
both MSVC/Visual Studio 2026 and ClangCL on Windows x64. These suites cover the
common implementation, public C ABI, CLI, benchmarks, fuzz compile-smoke and
permanent regressions, installed-package behavior, documentation structure,
and interoperability schema compatibility.

The established four-direction schema-31 exchange at revision
`903181080556c3bb511ad4a2e5275837ebda48e7` verifies all forty-two archives
across Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang x86-64
producers. The remaining evidence gaps are listed below.

## Remaining release evidence

The following items remain open even though local codec implementation is
ready:

- repeat interoperability generation and cross-decoding on at least one
  non-x86-64 architecture;
- record representative encode throughput, decode throughput, compression
  ratio, and peak workspace results rather than relying on benchmark smoke;
- run longer sanitizer fuzz campaigns and convert every finding into a
  permanent regression test.

Unknown-size input, allocator callbacks, authentication, archive metadata,
solid grouping, BWT-family transforms, and profiles beyond the complete
baseline matrix remain future extensions. They are not baseline-readiness
failures. The [composition matrix](composition.md) records the complete baseline
and the deferred generation path for any future extension.

## Readiness evidence history

The following numbered records preserve implementation admission, CI, audit,
and cross-platform evidence in Git introduction order. Candidate and pending
wording describes the state at that historical point; the matrices and current
baseline above are authoritative now.

### BR-0001

The 2026-07-18 local audit verified both shared-only and static-only installs in
fresh Visual Studio 2026 build trees. A separately configured pure-C consumer
found each installed CMake package, linked the sole exported target, and
completed its public-ABI round trip. The installed trees contained the public
header, CMake config and version files, license and third-party notices,
documentation, logo, and example sources.

The Windows and Ubuntu CI configurations explicitly enable benchmarks, so all
public adapters are compiled and their smoke tests run in clean CI builds. The
four installed-package matrix entries explicitly disable tests, examples,
tools, and benchmarks, isolating shared-only and static-only library packages
from top-level convenience targets. The selected GitHub-hosted Visual Studio
2026 image and Action major versions were checked against their official
upstream availability before publication. The final audit retained
`actions/checkout@v6` and updated artifact publication to
`actions/upload-artifact@v7`; Dependabot remains responsible for subsequent
Action and submodule update proposals.

### BR-0002

The 2026-07-18 review covered tracked first-party implementation, tests, build
files, public headers, and documentation; the pinned GoogleTest submodule was
treated as separately licensed test infrastructure. It checked provenance
entries, license markers, algorithm terminology, public-profile claims, format
versions, unfinished-work markers, and wording that could imply legal,
security, compatibility, or production-readiness guarantees.

No unexplained third-party copyright or copyleft marker was found in first-party
source, and no implementation was compared with external codec source. Shared
algorithm names, mathematical terms, and cited paper/standard terminology are
accounted for by the references record. The audits corrected historical wording
that described published profiles as future work and synchronized the README
inventory with all eighteen public profiles. The result documents repository
provenance and internal consistency; it is not a legal guarantee of
non-infringement or a claim of long-term 0.x compatibility.

### BR-0003

Public GitHub Actions
[run 29647453799](https://github.com/Masa-tam/marc/actions/runs/29647453799)
completed successfully for pushed revision
`c4f831917a43f75ca5c698d19d3674f12803f40b` on 2026-07-18. Its six successful
jobs covered the complete Windows/Visual Studio 2026 and Ubuntu 24.04/Ninja
suites plus shared-only and static-only installed-package consumers on both
operating systems.

The run retained the self-describing
`marc-interoperability-windows-msvc-x64` and
`marc-interoperability-ubuntu-ninja-x64` artifacts through 2026-10-16. This
closes pushed-revision CI generation evidence. It does not by itself claim
cross-decoding between the artifacts or evidence for a second architecture;
those remain explicitly open above.

### BR-0004

An external Ubuntu 26.04/Clang 21 environment under WSL2 subsequently verified
all eighteen Windows/MSVC and Ubuntu 24.04/Ninja archives, then generated an
Ubuntu 26.04 bundle that the local Windows/MSVC executable verified in the
reverse direction. All nineteen binary files in each bundle (`input.bin` plus
eighteen archives) were byte-identical across the three producers. This closes
the current x86-64 operating-system/compiler cross-check; a second architecture
remains open.

### BR-0005

Revision `a4e3d1a5acb7bfc393aca4f2195188cfe0421817` subsequently completed its
pushed Windows/MSVC and Ubuntu 24.04/Ninja CI run and produced schema-8
artifacts. Ubuntu 26.04/Clang 21.1.8 verified all nineteen archives from both
artifacts, generated a third schema-8 bundle, and verified that bundle locally.
The Windows/MSVC executable then verified all nineteen Ubuntu 26.04 archives in
the reverse direction. Because each verifier also requires byte-identical local
re-encoding, this closes the current schema-8 x86-64 Windows/Linux/compiler
cross-check. A second architecture remains open.

### BR-0006

Revision `8a854eaf9c7c6c36cc2d444cc8e1a135935887b2` subsequently completed pushed
CI and produced the Windows/MSVC and Ubuntu 24.04/Ninja schema-9 artifacts.
The previously recorded Ubuntu 26.04/Clang 21.1.8 environment verified all
twenty archives from both artifacts, generated and verified its own
twenty-archive bundle, and supplied that bundle to the Windows/MSVC executable
for reverse verification. Every pass required byte-identical local
re-encoding. This closes the schema-9 x86-64 Windows/Linux/compiler cross-check;
a second architecture and non-WSL Linux kernel remain open.

### BR-0007

`lz78-adaptive-huffman` now has its exact format, checked frame path, bounded
streaming transforms, typed workspace profile, and public C ABI factory. It
now also has a public-ABI completion matrix, bounded fuzz evidence, a
transactional CLI selector, a verified public-ABI benchmark adapter, and local
schema-10 generation/verification coverage. The pushed Windows/MSVC and Ubuntu
24.04 artifacts plus an independently generated Ubuntu 26.04/Clang bundle have
now passed the complete bidirectional external verification contract, so this
profile is `Ready`.

### BR-0008

Revision `bc8faba3043db78a953f18876f153abc847f814d` subsequently completed
pushed CI and produced the Windows/MSVC and Ubuntu 24.04/Ninja schema-10
artifacts. Ubuntu 26.04/Clang 21.1.8 verified all twenty-one archives from both
artifacts, generated and verified its own schema-10 bundle, and supplied that
bundle to the Windows/MSVC executable for reverse verification. Every pass
required byte-identical local re-encoding. This closes the schema-10 x86-64
Windows/Linux/compiler cross-check; a second architecture and non-WSL Linux
kernel remain open.

### BR-0009

Revision `163948c61dd8b90359882bee122f16ab3794787c` subsequently completed
pushed CI and produced the Windows/MSVC and Ubuntu 24.04/Ninja schema-11
artifacts. Ubuntu 26.04/Clang 21.1.8 verified all twenty-two archives from both
artifacts, generated and verified its own schema-11 bundle, and supplied that
bundle to the Windows/MSVC executable for reverse verification. Every pass
required byte-identical local re-encoding. This closes the schema-11 x86-64
Windows/Linux/compiler cross-check; a second architecture and non-WSL Linux
kernel remain open.

### BR-0010

Revision `7078d0ab20f6e0a1aeaa3c43e480ca866bf8a2fa` subsequently completed
pushed CI and produced the Windows/MSVC and Ubuntu 24.04/Ninja schema-12
artifacts. Ubuntu 26.04/Clang 21.1.8 verified all twenty-three archives from
both artifacts, generated and verified its own schema-12 bundle, and supplied
that bundle to the Windows/MSVC executable for reverse verification. Every pass
required byte-identical local re-encoding. This closes the schema-12 x86-64
Windows/Linux/compiler cross-check; a second architecture and non-WSL Linux
kernel remain open.

### BR-0011

`lzmw-adaptive-huffman` has now entered that queue as the sixth Adaptive
composition. DD-344 fixes its four-byte canonical reference boundary, checked
`4F` token and `132F` payload ceilings, adjacent-phrase and expansion-workspace
limits, validation order, and independent 75-byte single-reference frame. Its
first complete-frame validator now entropy-decodes into private reference
staging and validates the entire adjacent-phrase graph and exact raw extent
without publishing raw bytes. A bounded decoder now reconstructs that validated
graph iteratively into separate private raw staging, with raw capacity,
conservative expansion-stack capacity, and aggregate bytes checked before
entropy output. An internal transactional decoder now copies a complete
successful frame to caller-visible output while leaving it unchanged on every
failure. Its exact-frame planner freezes canonical LZMW references before
Adaptive planning; the deterministic encoder reproduces the independent vector
and round-trips generated references without partial destination writes. Its
first bounded streaming encoder now reproduces concatenated exact frames under
one-byte I/O, output starvation, nonterminal `Flush`, and retained `EndInput`.
The matching bounded streaming decoder now rejects every truncation and
trailing byte and publishes no raw bytes from a malformed later frame. It has
an internal bounded profile that derives all direction-specific byte extents
and partitions aligned encoder, phrase, and expansion records from one opaque
region. A bounded C requirements query and immutable-direction factory now bind
those regions without exposing the private record layouts. Its public-ABI
completion matrix now covers required data classes, deterministic one-byte and
mixed chunking, sticky terminal states, and transactional malformed-final-frame
rejection. A bounded dual-path decoder fuzz harness and permanent atomic
malformed regressions are now present. A transactional CLI selector now binds
the same public factory under the fixed 64-KiB reference profile. A verified
public-ABI benchmark now measures that profile after a byte-exact round trip.
Local schema-13 generation, verification, exact-order rejection, and schemas 1
through 12 compatibility are now present. The pushed Windows/MSVC and Ubuntu
24.04 artifacts plus an independently generated Ubuntu 26.04/Clang bundle have
passed the complete bidirectional external verification contract, so this
profile is `Ready`.
`lzw-adaptive-huffman` has now entered that queue with its exact representation,
checked bounds, validation order, and independent hand vector fixed by DD-316.
Its first complete-frame boundary now strictly reconstructs the packed LZW byte
region through Adaptive Huffman and validates code widths, references, `KwKwK`,
final padding, and exact raw extent. A bounded decoder now reconstructs a
validated frame into separate private raw staging while checking its capacity
and aggregate bytes before entropy output. An internal transactional decoder now
copies a complete successful frame to caller-visible output while leaving it
unchanged on every failure. Its exact-frame planner and internal encoder
now freeze canonical packed LZW bytes before Adaptive planning, reproduce the
independent hand vector, and round-trip deterministic multi-code frames. Its
first bounded streaming encoder now reproduces those exact bytes under
one-byte I/O, output starvation, nonterminal `Flush`, and retained `EndInput`.
The matching bounded streaming decoder validates complete frames before raw
draining and covers all truncations, trailing data, later-frame atomicity, and
sticky errors. Its bounded profile now derives all byte and typed-record
workspaces from public-style configuration and validated local limits, with
checked opaque-region partitioning. A public C ABI factory now binds those
regions to the streaming transforms without exposing private record layouts.
Its public-ABI completion matrix now covers required binary classes,
determinism, chunking, sticky terminal behavior, and transactional malformed
final-frame rejection. A bounded dual-path decoder fuzz harness and permanent
atomic malformed regressions and a transactional CLI selector are now present.
A verified public-ABI benchmark adapter and local schema-11 generation and
verification coverage are now present as well. The pushed Windows/MSVC and
Ubuntu 24.04 artifacts plus the independently generated Ubuntu 26.04/Clang
bundle passed the complete bidirectional external verification contract, so
this profile is `Ready`.

### BR-0012

`lzd-adaptive-huffman` is the fifth Adaptive composition. DD-330 fixes its
decoder-visible representation, checked `8*ceil(F/2)` token bound, `33S`
Adaptive payload bound, phrase and expansion-workspace ceilings, validation
order, and independent 77-byte terminal-token frame. Its first complete-frame
validator now entropy-decodes into private token staging and validates the
whole backward phrase graph, terminal form, and exact raw extent without
publishing raw bytes. A bounded decoder now reconstructs a validated frame into
separate private raw staging, with raw capacity, expansion-stack capacity, and
aggregate bytes checked before entropy output. An internal transactional
decoder now copies the complete successful frame to caller-visible output while
leaving it unchanged on every failure. An exact-frame planner and deterministic
encoder now freeze the complete LZD token bytes before Adaptive planning and
reject short serialized output before mutation. A bounded streaming encoder
now reproduces that representation across arbitrary input/output chunking and
retains end-of-input across output starvation. Its matching bounded streaming
decoder validates complete frames before raw draining and rejects truncation,
trailing data, and later-frame corruption transactionally. A bounded profile
now calculates direction-specific byte workspaces and partitions aligned typed
encoder, phrase, and expansion views. A public C requirements query and factory
now bind those regions to the streaming transforms while keeping every typed
layout opaque. Its public-ABI completion matrix now covers required binary
classes, determinism, chunking, sticky terminal behavior, and transactional
malformed-final-frame rejection. A bounded dual-path decoder fuzz harness and
permanent atomic malformed regressions are now present. A transactional CLI
selector now binds the same public factory under the fixed 64-KiB reference
profile. A verified public-ABI benchmark now measures the same profile after a
byte-exact round trip. Local schema-12 generation, verification, exact-order
rejection, and schemas 1 through 11 compatibility are now present. The pushed
Windows/MSVC and Ubuntu 24.04 artifacts plus the independently
generated Ubuntu 26.04/Clang bundle passed the complete bidirectional external
verification contract, so this profile is `Ready`.

### BR-0013

`lz77-dynamic-range` has entered the queue as the first Dynamic Range
composition. DD-359 fixes its canonical LZ77-token boundary, 2^20-byte raw
frame ceiling, checked `16F` token and `2S + 5` range-payload bounds,
transactional validation order, and independent 88-byte single-Literal frame.
Its first bounded complete-frame validator now enforces all declared and
aggregate extents, range-decodes into private token staging, and validates the
complete LZ77 stream and exact raw extent without publishing raw bytes. It
now reconstructs validated tokens, including overlapping matches, into a
separately bounded private raw staging region, then publishes the complete
frame through a transactional caller-visible boundary only after every layer
succeeds. Its exact planner and deterministic encoder now freeze canonical
tokens before range planning and reproduce the independent frame. It is
`Ready`; its bounded streaming encoder preserves exact frame bytes and finish
semantics, and its matching streaming decoder provides atomic complete-frame
publication. Its bounded profile derives all three
direction-specific byte regions, and its bounded public C requirements query
and factory now construct both streaming directions. Its public-ABI completion
matrix covers binary classes, deterministic chunking, terminal stability, and
transactional malformed-final-frame handling. Its bounded decoder fuzz target
now covers complete-frame and incremental parsing with fixed workspaces and a
fixed call ceiling, while deterministic regressions preserve truncation,
extreme-extent, and descriptor-corruption failures. Its explicit CLI selector
now passes binary and empty round trips, overwrite refusal, and transactional
malformed and trailing-data rejection. Its dependency-free benchmark now
verifies the public profile before reporting throughput, ratio, and both
direction-specific workspaces. Local schema-14 generation, verification,
exact-order rejection, and schemas 1 through 13 compatibility are present.
The pushed Windows/MSVC and Ubuntu 24.04 artifacts plus an independently
generated Ubuntu 26.04/Clang bundle have passed the complete four-direction
external verification contract at revision `802c7a1ab913b07ee79a04fa5b3390c061c88966`.

### BR-0014

`lzss-dynamic-range` is a locally completed composition. DD-373 fixes the
canonical variable-length LZSS-token boundary, 2^23-byte format frame ceiling,
checked `2F` token and `2S + 5` range-payload bounds, transactional validation
order, and independent 79-byte single-Literal frame. Its first bounded
complete-frame validator now enforces those extents and aggregate storage,
strictly range-decodes into private token staging, and validates the entire
variable-length LZSS stream with stable token and byte positions. It is now
`Ready` locally: its bounded private decoder counts raw staging in the
aggregate and reconstructs only validated Literal and overlap-Match tokens.
Its transactional complete-frame decoder now publishes only a fully validated
and reconstructed private frame and leaves output unchanged on failure. A
deterministic exact-frame planner and encoder now freeze canonical LZSS tokens
before range planning and reproduce the independent frame without short-output
mutation. Its first bounded streaming encoder now preserves the concatenated
exact-frame representation under arbitrary one-byte chunking, nonterminal
`Flush`, and output starvation while retaining `EndInput`. Its matching
bounded streaming decoder validates and reconstructs a complete frame before
raw draining, rejects impossible extents before body collection, and keeps
malformed later frames atomic. Its bounded workspace profile now derives all
six direction-specific byte regions with checked arithmetic and no exposed
private layouts. Its public C requirements query and factory now bind those
regions to both streaming directions without a views workspace. The completion
matrix now covers required binary classes, deterministic chunking, stable
terminal states, and atomic malformed-final-frame rejection entirely through
the public C ABI. Its bounded dual-decoder fuzz target fixes every workspace
before parsing, and permanent regressions preserve atomic rejection of every
canonical truncation, extreme frame extents, and an invalid range descriptor.
Its explicit CLI selector now uses only the public requirements query and
factory through transactional temporary-file publication. Its dependency-free
benchmark independently queries both direction workspaces, verifies a complete
round trip before timing, and reports ratio, throughput, and caller-reserved
peak memory. Local schema-15 generation, exact-order verification, reordered-
manifest rejection, and schemas 1 through 14 compatibility are present.
Four-direction schema-15 verification at revision
`504af4f6942aee7662bcb51abf9b55289c957d6c` passed for the Windows/MSVC and
Ubuntu 24.04/Ninja artifacts plus an Ubuntu 26.04/Clang-generated bundle,
including reverse verification on Windows/MSVC.

### BR-0015

`lzw-dynamic-range` is the current locally completed composition. DD-402 fixes
the complete LSB-first LZW packed-code boundary, including final dictionary
padding, before one fresh per-frame Dynamic Range model consumes it. For raw
frame size `F` and
maximum code width `W`, it checks `S = ceil(FW/8)` packed bytes and
`P = 2S + 5` range-payload bytes, retains the 2^20-byte raw-frame cap, and
requires range validation before LZW width, reference, `KwKwK`, padding, and
exact-raw-extent validation. Its independently assembled 79-byte raw-`A` frame
is covered by a standalone-component vector test. Its first complete-frame
validator now checks generic, packed, entropy, capacity, and aggregate extents
before strictly range-decoding into private packed staging and invoking the
existing LZW validator. A bounded private decoder now admits and counts raw
staging before entropy output, then iteratively reconstructs the validated
phrase graph without caller-visible publication. Its internal transactional
decoder now checks complete destination capacity before entropy output and
publishes only a successful private raw frame. Its exact-frame planner freezes
canonical packed bytes before range planning and reports the complete frame
extent without serialized output. Its deterministic complete-frame encoder
reproduces the independent 79-byte frame and preserves short destinations.
Its bounded streaming encoder now preserves canonical bytes under arbitrary
input and output starvation and nonterminal `Flush`. Its bounded streaming
decoder validates complete frames before raw draining and preserves frame
atomicity under later corruption. Its internal direction-specific profile now
calculates every caller-owned byte region and safely partitions opaque aligned
LZW records. A bounded C requirements query and transform factory now connect
both streaming directions through those opaque regions. Its public-ABI
completion matrix
now covers determinism, arbitrary chunking, terminal states, and malformed
final-frame atomicity. A bounded dual-path decoder fuzz target and permanent
atomic malformed regressions are now present. Its explicit transactional CLI
selector uses only the public requirements query and factory. The
dependency-free benchmark uses the same profile, requires an untimed
byte-exact round trip, and reports queried directional workspaces. Local
schema-17 generation, exact-order verification, reordered-manifest rejection,
and schemas 1 through 16 compatibility are present. Four-direction external
schema-17 verification at revision
`b4c700aca87fc925aab642cfb6a6b72f3a29c86b` passed for the Windows/MSVC and
Ubuntu 24.04/Ninja artifacts plus an Ubuntu 26.04/Clang-generated bundle,
including reverse verification on Windows/MSVC.

`lz78-dynamic-range` is a locally completed composition. DD-387 fixes
the canonical fixed-width LZ78-token boundary, 2^21-byte format frame ceiling,
checked `8F` token and `2S + 5` range-payload bounds, bounded aligned phrase
validation, transactional publication order, and independent 83-byte
single-Pair frame.
Its first bounded complete-frame validator now enforces those extents, counts
aligned phrase storage in the aggregate, strictly range-decodes into private
token staging, and validates the complete LZ78 phrase graph with stable token
and byte positions. Its bounded private decoder now includes the complete raw
extent in the pre-entropy aggregate policy and iteratively reconstructs only
that validated phrase graph into separate raw staging. Its transactional
complete-frame decoder now publishes only a fully validated and reconstructed
private frame and leaves output unchanged on failure. Its exact-frame planner
now freezes canonical LZ78 tokens and obtains the range payload and complete
frame extents without writing serialized output. Its deterministic exact-frame
encoder now reproduces the independent 83-byte frame without partial writes on
short capacity. Its bounded known-size streaming encoder now preserves exact
one-shot bytes under arbitrary input/output chunking, nonterminal `Flush`, and
retained `EndInput`. Its matching bounded streaming decoder preflights each
declared frame extent before body collection and publishes only a completely
validated and reconstructed frame. Its bounded profile now calculates every
direction-specific byte and aligned record region and partitions opaque typed
storage only after validating its layout. Its public C factory now exposes
that streaming pair through size-tagged configuration and three caller-owned
regions without leaking private record types. It is `Ready`: a
public-ABI completion matrix now covers required binary classes, deterministic
and chunk-independent streams, sticky terminal states, and transactional
malformed-final-frame rejection. Its fixed-memory dual-path decoder fuzz
boundary, permanent atomic malformed regressions, and transactional public-ABI
CLI selector are also present. Its dependency-free public-ABI benchmark
verifies a complete round trip before measurement. Local schema-16 generation,
exact-order verification, reordered-manifest rejection, and schemas 1 through
15 compatibility are present. Four-direction schema-16 verification at
revision `01f746a5bef2225a0b8fa34f3ff9d52b42f13f40` passed for the
Windows/MSVC and Ubuntu 24.04/Ninja artifacts plus an Ubuntu 26.04/Clang-
generated bundle, including reverse verification on Windows/MSVC.

### BR-0016

`lzd-dynamic-range` is the most recently completed composition. DD-417 fixes
the complete eight-byte LZD reference-pair boundary before one fresh per-frame
Dynamic Range model consumes it. For raw frame size `F`, it checks
`S = 8 * ceil(F/2)` token bytes and `P = 2S + 5` range-payload bytes, retains
the 2^20-byte raw-frame cap, and requires range validation before LZD
token-width, backward-reference, terminal-absence, phrase-length, and exact-
raw-extent validation. Its independently assembled 84-byte raw-`A` frame is
covered by a standalone-component vector test. Its first complete-frame
validator now checks generic, token, entropy, caller-capacity, phrase-record,
and aggregate extents before strictly range-decoding into private token staging
and invoking
the existing LZD validator. A bounded private decoder now admits raw and
expansion-stack capacity and aggregate storage before entropy output, then
iteratively reconstructs the validated phrase graph without caller-visible
publication. Its internal transactional decoder now checks complete
destination capacity before entropy output and publishes only a successful
private raw frame. Its exact-frame planner freezes canonical token bytes before
range planning and reports the complete frame extent without serialized
output. Its deterministic complete-frame encoder reproduces the independent
84-byte frame and preserves short destinations. Its bounded streaming encoder
now preserves canonical bytes under arbitrary input and output starvation and
nonterminal `Flush`. Its bounded streaming decoder validates complete frames
before raw draining and preserves frame atomicity under later corruption. Its
internal direction-specific profile now calculates every caller-owned byte
region and safely partitions opaque aligned LZD records. Its small C ABI now
publishes requirements queries and factories without exposing those record
layouts. Its public C ABI completion matrix now proves required data classes,
chunk determinism, sticky terminal states, and malformed final-frame
atomicity. A bounded dual-path decoder fuzz target and permanent atomic
malformed regressions are now present. Its explicit transactional CLI selector
uses only the public requirements query and factory. The dependency-free
benchmark uses the same profile, requires an untimed byte-exact round trip, and
reports queried directional workspaces. Interoperability schema 18 appends it
once after the frozen schema-17 order; local generation, verification,
reordered-manifest rejection, and schemas 1 through 17 compatibility pass.
The four-direction Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
cross-check verifies all twenty-nine archives at revision
`fd11d1c7ef833873a02694da91f9f6d8d378948b`.

### BR-0017

`lzmw-dynamic-range` is the active admission composition. DD-432 fixes the
complete
four-byte LZMW reference boundary before one fresh per-frame Dynamic Range
model consumes it. For raw frame size `F`, it checks `S = 4F` reference bytes
and `P = 2S + 5 = 8F + 5` range-payload bytes, retains the 2^20-byte raw-frame
cap, and requires range validation before LZMW reference alignment, generated-
phrase graph, and exact-raw-extent validation. Its independently assembled
80-byte raw-`A` frame is covered by a standalone-component vector test. Its
first complete-frame validator now checks generic, reference, entropy,
caller-capacity, phrase-record, and aggregate extents before strictly range-
decoding into private reference staging and invoking the existing LZMW
validator. A bounded private decoder now admits raw and expansion-stack
capacity and aggregate storage before entropy output, then iteratively
reconstructs only the validated phrase graph into disposable raw staging. Its
transactional complete-frame decoder additionally checks destination capacity
before entropy work and publishes only a successful private raw frame. Its
exact-frame planner freezes canonical references before range planning and
reports the complete frame extent without serialized output. Its deterministic
complete-frame encoder reproduces the independent 80-byte frame and preserves
short destinations. Its bounded streaming encoder now preserves canonical
bytes under arbitrary input and output starvation and nonterminal `Flush`.
Its bounded streaming decoder validates complete frames before raw draining
and preserves frame atomicity under later corruption. Its internal direction-
specific profile now calculates every caller-owned byte region and safely
partitions opaque aligned LZMW records. Its small C ABI now publishes
direction-specific requirements and factories over three caller-owned regions
without exposing those record layouts. Its public completion matrix proves the
required binary classes, deterministic arbitrary chunking, sticky terminal
states, and frame-atomic malformed-final-frame rejection. Its bounded
dual-decoder fuzz target and permanent malformed regressions are present. Its
transactional CLI selector uses the fixed 64-KiB profile through the public C
factory and passes multi-frame, empty-input, malformed-input, trailing-data,
and overwrite-refusal coverage. Its dependency-free public-C benchmark verifies
a complete byte-exact round trip before reporting ratio, directional
throughput, and all queried workspace extents. Interoperability schema 19
appends it once after the frozen schema-18 order; local generation,
verification, reordered-manifest rejection, and schemas 1 through 18
compatibility pass. The four-direction Windows/MSVC, Ubuntu 24.04/Ninja, and
Ubuntu 26.04/Clang cross-check verifies all thirty archives at revision
`f8d51680a0ef827fa09f5782ad4ced4c335d346e`.

### BR-0018

`lz77-rans` is the completed first rANS composition. DD-447 fixes complete
canonical LZ77 token staging before rANS, independent byte-block boundaries
within an outer frame, checked
`S = 16F`, `K = ceil(S/B)`, `P = S + 8K`, and `528K` descriptor ceilings,
strict entropy-before-dictionary validation, and an independent 592-byte
single-Literal frame. Its first bounded complete-frame validator now admits
all exact extents, token and view capacities, and aggregate workspace before
entropy work. It validates every rANS block before filling private token
staging, then validates complete LZ77 semantics without reconstructing raw
bytes. Its bounded private decoder now preflights and counts separate raw
staging, then reconstructs literals and overlapping matches only from the
validated token region. Its transactional frame decoder preflights complete
caller output and publishes the private raw frame only after success. Its
encoder-side exact-frame planner now freezes canonical tokens, plans every
rANS block without serialized output, and returns all exact frame extents.
Its deterministic complete-frame encoder now emits the exact independent
592-byte vector, handles block boundaries inside tokens, and rejects short
serialized output atomically. Its bounded known-size streaming encoder now
matches the one-shot stream under one-byte input/output chunking and handles
finish, flush, empty input, protocol misuse, and aggregate workspace limits.
Its bounded streaming decoder now validates complete frames privately before
raw drain, includes rANS views in aggregate workspace, and rejects malformed
later frames without publishing their bytes. Its internal bounded profile now
derives every encoder and decoder byte region plus the decoder rANS view count
from canonical configuration and validated local limits, and its requirements
construct the streaming pair directly. Its public ABI v1 configuration,
requirements query, and factory now expose those transforms through three
borrowed opaque regions, with aligned rANS views only for decoding. Its
public-ABI completion matrix now covers required binary classes,
determinism, arbitrary chunking, sticky terminal results, and atomic malformed
final-frame rejection. Its bounded complete-frame plus streaming fuzz target
now fixes all byte and rANS-view workspaces and retains permanent atomic
regressions. Its transactional CLI selector now passes the standard file-level
admission suite exclusively through the public C ABI. Its dependency-free
benchmark verifies an exact public-ABI round trip before reporting ratio,
directional throughput, and queried workspace extents. Local schema-20
generation, exact-order verification, reordered-manifest rejection, and
schemas 1 through 19 compatibility now make the profile `Ready`. External
schema-20 verification subsequently passed in all four established directions
at revision `01e87fe19f5c9c90edd87c9caeb8acf36b413aad` across the
Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers.

### BR-0019

`lz78-rans` is now the active admission composition. Its initial specification
freezes the complete aligned eight-byte LZ78 token region before scalar rANS,
checks `S <= 8F`, `K = ceil(S/B)`, `8K <= P <= S + 8K`, exact `528K`
descriptor bytes, and bounded phrase records, and requires entropy validation
before LZ78 graph validation or raw reconstruction. An independently assembled
592-byte single-Pair frame fixes the first canonical representation. Combined
validation is now implemented internally: it checks complete frame and
workspace extents, validates every entropy block before private token
mutation, and validates the exact LZ78 phrase graph without expanding it.
A bounded decoder now expands that validated graph iteratively into separate
private raw staging after checking raw capacity and aggregate workspace before
entropy output. Transactional frame publication now checks output capacity
before private mutation and copies only after complete reconstruction.
An exact-frame planner and encoder now freeze deterministic LZ78 tokens before
planning every rANS block, enforce encoder-workspace and aggregate limits, and
reproduce the independent 592-byte frame. A bounded known-size streaming
encoder now emits the fixed prefix, buffers one raw frame, and drains immutable
exact-frame bytes under arbitrary output starvation. The matching bounded
streaming decoder now admits the prefix and each complete frame incrementally,
checks all caller-owned decoder regions and their aggregate before body
collection, and drains private raw staging only after complete frame success.
An internal profile calculator now derives direction-specific byte extents,
aligned encoder records, and an opaque decode layout containing rANS views
followed by LZ78 phrase records. A bounded public C requirements query and
factory now bind those three regions without exposing private record layouts.
The public-ABI completion matrix now covers all one-byte values, representative
binary and boundary-length inputs, deterministic multi-frame output under
arbitrary chunking, stable post-end calls, and transactional rejection of a
corrupt, truncated, or trailing final frame. A bounded dual-path decoder fuzz
target now fixes all byte and typed workspaces before input is inspected and
guards byte-derived chunking with a finite call ceiling. Permanent regressions
cover every canonical truncation, saturated frame extents, and a nonzero rANS
reserved byte without publishing the failing frame. Its explicit CLI selector
now uses only the public lifecycle and preserves overwrite refusal,
transactional temporary-output cleanup, strict trailing-data rejection, and
empty-stream round trips. Its benchmark adapter performs an untimed public-ABI
round trip before measuring ratio, directional throughput, and queried
workspace extents. Interoperability schema 22 appends it after the frozen
schema-21 order; local generation, exact-order verification, reordered-
manifest rejection, byte-identical re-encoding, and schemas 1 through 21
compatibility pass. External schema-22 verification subsequently passed in all
four established directions at revision
`2aa51ded63bdeacb0e5b2ec28a21075a867bb353` across Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang producers.

### BR-0020

`lzw-rans` is the completed fourth rANS admission composition. Its specification
freezes the complete LSB-first packed LZW byte region, including final zero
padding, before scalar rANS. It checks `S <= ceil(FW/8)`,
`K = ceil(S/B)`, `8K <= P <= S + 8K`, exact `528K` descriptor bytes, and
bounded dictionary records, and requires entropy validation before LZW code-
stream validation or raw reconstruction. An independently assembled 592-byte
single-code frame fixes the first canonical representation. Combined
validation is now implemented internally: it checks complete frame and
workspace extents, validates every entropy block before private packed-byte
mutation, and validates the exact LZW code graph without expanding it. Raw
reconstruction is now implemented into separately admitted private staging
only after that complete validation; raw capacity and aggregate storage are
checked before entropy output. Transactional complete-frame publication now
preflights caller capacity and copies the declared raw extent once only after
private success. Its write-free exact-frame planner now freezes canonical
packed LZW bytes, plans every rANS block deterministically, and reports the
validated complete-frame extent while counting encoder records in aggregate
storage. Deterministic frame emission now reproduces the independent vector,
replans each block against the frozen extents, and preserves short output. Its
first bounded known-size streaming encoder emits the fixed prefix, buffers at
most one raw frame, and drains each immutable encoded frame before accepting
the next. Its bounded streaming decoder now collects one complete encoded
frame, admits every private region from its header, and publishes only a fully
validated raw frame. Its internal profile calculator now derives the encoder
byte regions and aligned LZW records, plus decoder byte regions and a combined
rANS-view/LZW-phrase layout, using checked worst-case bounds. Public API and
factory coverage now expose those regions through a fixed-width C config and
the common opaque transform lifecycle. Its public-ABI completion matrix now
proves required data classes, deterministic arbitrary chunking, sticky terminal
states, and malformed-final-frame atomicity. A bounded dual-path decoder fuzz
harness and permanent atomic truncation, saturated-extent, and descriptor
regressions are now present. A transactional CLI selector now binds the same
public factory under the fixed 64-KiB reference profile. A verified public-ABI
benchmark now measures that profile after a byte-exact round trip.
Interoperability schema 23 appends it after the frozen schema-22 order; local
generation, exact-order verification, reordered-manifest rejection, byte-
identical re-encoding, and schemas 1 through 22 compatibility pass. External
schema-23 verification subsequently passed in all four established directions
at revision `5397f261fa04ee49832d9f72b09960a156232aad` across Windows/MSVC,
Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers.

### BR-0021

`lzd-rans` is now the active admission composition. Its initial specification
freezes the complete eight-byte LZD reference-pair sequence before scalar rANS,
checks `S <= 8 * ceil(F/2)`, `K = ceil(S/B)`, `8K <= P <= S + 8K`, exact
`528K` descriptor bytes, bounded phrase records, and iterative expansion
storage, and requires complete entropy validation before any LZD graph
validation or raw reconstruction. An independently assembled 593-byte raw-`A`
frame fixes the first canonical representation. Its first complete-frame
validator now admits every encoded and caller-owned workspace extent, validates
all rANS blocks before token mutation, reconstructs the complete private token
region, and applies the LZD phrase-graph validator without raw expansion.
Private raw decoding now preflights and aggregate-counts the full raw and
iterative expansion regions, then reconstructs only that validated graph into
disposable staging. Transactional caller publication now preflights the full
destination and performs one exact copy only after private success, preserving
all output on failure. Its exact-frame planner now fixes canonical LZD token
bytes before per-block rANS planning, enforces the combined workspace policy,
and reports the checked complete frame extent without serialized output.
Its deterministic frame encoder now reproduces the independent vector,
round-trips phrase-generating multi-block frames, and rejects short serialized
output without publication. Its bounded known-size streaming encoder now
matches concatenated exact frames under one-byte I/O, preserves nonterminal
`Flush` and sticky `EndInput`, and rejects workspace, aggregate, and protocol
failures deterministically. Its matching bounded streaming decoder now admits
complete frames before body collection and raw drain, preserves one-byte and
sticky-end behavior, and keeps malformed later frames transactional. Profile
calculation now derives all encoder byte and aligned-record regions from the
trusted profile and all decoder byte, rANS-view, phrase, and iterative expansion
regions from local limits. Checked opaque partitioning directly constructs the
bounded streaming round trip. Its public C requirements query and
immutable-direction factory now bind those exact regions through a size-tagged
fixed-width config, while all
rANS views, phrases, expansion references, and offsets remain opaque. Public
completion now covers the required binary classes, deterministic one-byte and
mixed chunking, repeated terminal results, and frame-atomic rejection of a
corrupt, truncated, or extended final frame through that C ABI alone. A bounded
dual-path decoder fuzz target now fixes all serialized, raw, entropy-view,
phrase, expansion, and call-count ceilings before processing arbitrary input;
strict truncation, reserved descriptor bytes, and saturated frame extents are
permanent regressions. Its explicit transactional CLI selector now obtains all
three workspaces from the public C query and rejects destination collisions,
malformed input, truncation, and trailing bytes without partial publication or
temporary-file residue. Its public-C benchmark now retains the odd-byte
half-reference in its checked stream ceiling, verifies a complete round trip
before timing, and reports ratio, both throughputs, and queried workspace.
Interoperability schema 24 appends its unchanged CLI archive once after the
frozen schema-23 order. Local generation, exact-order verification, reordered-
manifest rejection, byte-identical re-encoding, and schemas 1 through 23
compatibility pass. Four-direction external schema-24 verification passed at
revision `dad3638da2acb449afca969176194bf8323309f5` across Windows/MSVC,
Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers.

### BR-0022

`lzmw-rans` has completed admission. Its initial specification
freezes the complete four-byte LZMW reference sequence before scalar rANS,
checks `S <= 4F`, four-byte alignment, `K = ceil(S/B)`,
`8K <= P <= S + 8K`, exact `528K` descriptor bytes, bounded phrase records,
and iterative expansion storage, and requires complete entropy validation
before any LZMW graph validation or raw reconstruction. An independently
assembled 592-byte raw-`A` frame fixes the first canonical representation.
Its first complete-frame validator now preflights generic, reference, entropy,
typed-record, and aggregate extents, validates every rANS block before changing
reference staging, reconstructs the complete private reference sequence, and
applies the ordinary LZMW graph validator without raw expansion. Private raw
decoding now preflights and aggregate-counts the full raw region and
conservative iterative expansion region, shrinks the active stack to the
validated graph, and reconstructs into disposable staging. Transactional
caller publication now preflights the full destination and performs one exact
copy only after private success, preserving all output on failure. The
encoder-side exact-frame planner now freezes the canonical reference sequence
in private staging, plans every rANS
block over those fixed bytes, and reports checked exact frame and aggregate
workspace extents without writing serialized output. Deterministic frame
encoding now reproduces the independent 592-byte vector, emits generated
multi-block frames byte-identically, transactionally round-trips them, and
rejects short destinations before publication. Its known-size streaming
encoder now reproduces concatenated one-shot frames under one-byte
input/output, preserves terminal intent across drains, keeps flush nonterminal,
and enforces caller-owned and
aggregate storage bounds. The matching streaming decoder now admits every
complete frame and all workspaces before body collection, reconstructs
privately, and publishes
only validated raw frames. It rejects truncation, trailing bytes, protocol
errors, and later-frame corruption without publishing bytes from the failing
frame.
The internal profile calculator now derives conservative encoder and decoder
storage, validates the coupled opaque-view layout, and directly constructs a
bounded streaming round trip. Public C requirements and factory admission
are now available with pure-C lifecycle, capacity, alignment, reserved-field,
and binary round-trip coverage. Its public-ABI completion matrix now covers all
one-byte values, representative binary and frame-boundary inputs, deterministic
one-byte and mixed chunking, sticky terminal states, and final-frame atomicity.
A bounded dual-path decoder fuzz harness and permanent truncation,
saturated-extent, and reserved-descriptor regressions are now present. Its
transactional CLI selector uses only the public C lifecycle and leaves no
requested output or temporary residue on malformed or trailing input. Its
dependency-free benchmark now verifies an untimed exact round trip before
reporting complete-stream ratio, directional throughput, and every queried
workspace region under the checked `80 + 4N + 2200K` capacity ceiling.
Interoperability schema 25 now appends the unchanged CLI archive once after the
frozen schema-24 order. Local generation, exact-order verification, reordered-
manifest rejection, byte-identical re-encoding, and schemas 1 through 24
compatibility pass. Four-direction external schema-25 verification passed at
revision `bc4cfa45fc8787d5ec9277894bda0b10df0ef638` across Windows/MSVC,
Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers. All local and current
cross-platform admission evidence is therefore present.

### BR-0023

`lz77-tans` is the next admission candidate and the first reserved tANS
composition. DD-537 fixes complete canonical LZ77 token staging before tANS,
independent byte-block boundaries within an outer frame, checked `S <= 16F`,
`K = ceil(S/B)`, exact `528K` descriptor bytes, the per-block
`2 + ceil(12n/8)` payload ceiling, strict entropy-before-dictionary validation,
and an independent 587-byte single-Literal frame. Its first bounded complete-
frame validator now admits all exact extents, token and view capacities, and
aggregate workspace before entropy work. It validates every tANS block before
filling private token staging, then validates complete LZ77 semantics. Its
private decoder additionally admits the complete raw staging extent and
aggregate workspace before reconstructing validated literals and overlapping
matches. Its transactional complete-frame wrapper preflights caller output and
publishes once only after private reconstruction succeeds. It is now a public
C ABI profile; CLI selector, benchmark, fuzz target, completion claim, and
interoperability entry remain absent. The encoder-side write-free
planner now freezes canonical LZ77 staging
and computes every exact tANS block and complete-frame extent; the serialized
writer now emits the generic header, all descriptors, and all payloads only
after complete planning and output-capacity admission. The known-size bounded
streaming encoder now preserves identical bytes across arbitrary chunks using
caller-owned raw, token, and serialized-frame storage. A matching streaming
decoder now collects, validates, and privately reconstructs complete frames
before draining them, with frame-atomic rejection of malformed later input.
The internal profile calculator now derives a canonical header and all
direction-specific workspace extents using the exact blockwise tANS payload
ceiling and local limits. A size-tagged C configuration, workspace query, and
factory now bind the existing streaming pair while keeping tANS views private.
Its public-ABI completion matrix now proves required binary classes,
deterministic encoding, one-byte and mixed chunk independence, sticky terminal
results, and frame-atomic rejection of corruption, truncation, and trailing
data. Its fixed-memory dual-decoder fuzz target now exercises complete-frame
and incremental parsing, with permanent atomic regressions for truncation,
saturated lengths, and invalid tANS metadata. Its transactional CLI selector
now fixes 64-KiB raw and entropy blocks and uses only the public C query and
factory. Its public-ABI benchmark verifies a byte-exact round trip before
reporting ratio, throughput, and both directional workspaces. Interoperability
schema 26 now appends the unchanged CLI archive once and passes local
generation, exact-order verification, re-encoding, reorder rejection, and
schemas 1 through 25 compatibility. External cross-platform verification
passed in all four directions at revision
`5b2aa31ba3333c311ad4086b3438915a6c3ce36d` across Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang producers. All current admission evidence
is present.

### BR-0024

At DD-552, the complete Release suite contains 2,100 tests and passes under
both MSVC/Visual Studio 2026 and ClangCL 22.1.3 on Windows x64. This is strong
local compiler-independence evidence on one architecture. Pushed schema-26 CI
adds Windows/MSVC and Ubuntu 24.04/Ninja coverage plus installed-package
evidence. The established four-direction exchange additionally verifies all
37 archives across those producers and Ubuntu 26.04/Clang 21.1.8. The
remaining release-evidence limits are stated above.

### BR-0025

`lzss-tans` is the completed preceding admission composition. DD-553 fixes
complete LZSS token serialization before tANS, permits entropy blocks to split the
two- or nine-byte token grammar without crossing a frame, and requires full
private token reconstruction before dictionary validation. Its bounds are
`S <= 2F`, `K = ceil(S/B)`, exact `528K` descriptors, and the checked sum of
per-block `2 + ceil(12n/8)` payload ceilings. The independent raw-`A` frame is
587 bytes. Its first bounded complete-frame validator now preflights exact
extents and caller-owned storage, validates every tANS block before any token
mutation, reconstructs the complete private token region, and applies LZSS
grammar and semantic validation. Its private decoder now admits and counts the
complete raw staging extent before entropy work, then reconstructs validated
literals and overlapping matches without caller publication. Transactional
publication now admits the complete caller output before private mutation and
copies the reconstructed frame exactly once. The encoder-side write-free
planner now freezes the canonical
LZSS token region, plans every tANS block, and validates exact frame extents;
the matching writer now admits the complete output before mutation and emits
the header, consecutive descriptors, and consecutive payloads explicitly.
The known-size streaming encoder now drains the 80-byte prefix and complete
prepared frames from bounded caller-owned storage with chunk-independent bytes,
latched finish, and nonterminal `Flush`. Its bounded streaming decoder now
admits each complete encoded frame and all private workspace before collection,
then publishes raw bytes only after transactional tANS and LZSS validation.
Its internal profile calculator now derives canonical stream fields and every
encoder and decoder workspace from known input configuration or local hard
limits. Its public C requirements query and factory now expose both directions
through three opaque borrowed regions while keeping tANS view layout private.
Its public-ABI completion matrix now covers required data classes,
deterministic chunking, sticky terminals, and malformed-final-frame atomicity.
Its dual-decoder fuzz target now fixes all byte and tANS-view storage, bounded
chunk schedules, and a finite call ceiling; permanent regressions cover every
canonical truncation, impossible frame extents, and an invalid descriptor.
Its explicit CLI selector now uses only the public requirements, factory,
process, and destroy lifecycle with transactional output publication. The
dependency-free benchmark now verifies an exact public-C round trip before
timing and reports all directional workspace regions. Schema 27 now appends
the unchanged CLI archive once and passes local generation, exact-order,
re-encoding, reorder-rejection, and schemas 1 through 26 compatibility tests.
Four-direction external verification at revision
`da376a7223f8a8072531271472f40d58b69e3b7a` establishes canonical archives
across the recorded Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
x86-64 producers. All current admission evidence is present.

### BR-0026

`lz78-tans` is the completed third tANS admission. Its reserved boundary fixes
complete aligned eight-byte LZ78 token serialization before tANS, permits
entropy blocks to split a token without crossing a frame, and requires full
private entropy reconstruction before phrase-graph validation. Its bounds are
`S <= 8F`, `K = ceil(S/B)`, exact `528K` descriptors, and the checked sum of
per-block `2 + ceil(12n/8)` payload ceilings. The independent raw-`A` frame is
587 bytes with payload `6B 04 00`. Its first bounded complete-frame validator
now preflights exact extents and all token, view, and phrase workspaces,
validates every tANS block before token mutation, reconstructs the complete
private token region, and applies LZ78 alignment, phrase-graph, and exact raw-
extent validation. Its private decoder now preflights and counts the complete
raw staging extent, then expands the validated phrase graph iteratively without
caller publication. Transactional publication now admits the complete caller
output before private mutation and copies the reconstructed frame exactly once.
The encoder-side write-free planner now freezes canonical LZ78 tokens, plans
every tANS block, counts all encoder workspace, and validates exact frame
extents. The complete-frame writer now admits the exact complete destination
before output mutation, emits the explicit generic header followed by all
descriptors and payloads, and requires repeated block plans and final offsets
to equal the frozen plan. The bounded known-size streaming encoder now drains
the ordinary 80-byte prefix, collects exactly one configured raw frame,
prepares it completely, and drains immutable bytes before workspace reuse.
One-byte capacities, nonterminal `Flush`, latched `EndInput`, and sticky
protocol and workspace failures are covered. The matching streaming decoder
now admits each complete frame and every tANS view, token, phrase, and private-
raw region before body collection, decodes privately, and drains only after
success. A malformed later frame cannot expose its output. The profile
calculator now derives bounded encoder and decoder regions from validated
configuration and hard limits, including aligned mixed tANS-view and LZ78-
phrase storage; its results directly construct the streaming round trip. The C
ABI now exposes the size-tagged config, direction-specific requirements query,
and factory while preserving exact workspace and alignment validation. Its
explicit CLI selector now uses only that public lifecycle, fixes the documented
64-KiB frame/block policy and 4-MiB aggregate bound, and retains transactional
file publication. Its dependency-free benchmark now verifies one exact public
C round trip before timing and reports all three directional workspace
regions. Its bounded dual-decoder fuzz target now fixes every byte, tANS-view,
and LZ78-phrase region, derives chunks only within the capped input, and uses a
finite call ceiling; permanent regressions cover every canonical truncation,
impossible frame extents, and an invalid descriptor. Its public-ABI completion
matrix now covers required binary classes, repeat determinism, one-byte and
mixed chunking, stable repeated terminals, and transactional rejection of a
corrupt, truncated, or trailing final frame. Schema 28 appends its unchanged
CLI archive once after schema 27 and preserves schemas 1 through 27. Its
four-direction external exchange at revision
`3d5001ce7536c425328a597240244551605e8935` verifies canonical output across
the recorded Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
producers.

### BR-0027

`lzw-tans` is the completed preceding admission composition. Its reserved
boundary fixes
the complete LSB-first packed LZW byte region before tANS, permits entropy
blocks to split a variable-width code without splitting a byte or crossing a
frame, and requires complete private entropy reconstruction before LZW
validation. For maximum code width `W`, its bounds are
`S <= ceil(FW/8)`, `K = ceil(S/B)`, exact `528K` descriptors, and the checked
sum of per-block `2 + ceil(12n/8)` payload ceilings. The independent raw-`A`
frame is 587 bytes with packed bytes `41 00` and payload `0C 00 00`. Its first
bounded complete-frame validator now preflights exact extents and all packed,
view, and phrase workspaces, validates every tANS block before packed mutation,
reconstructs the complete private packed region, and applies LZW code-width,
reference, dictionary-growth, raw-extent, packed-exhaustion, and padding
validation. Its private decoder now preflights and aggregate-counts the
complete raw staging extent, then expands the validated LZW graph iteratively
without caller publication. Its transactional wrapper now admits caller
output before private mutation and publishes the declared raw extent once only
after the complete private decode succeeds; short output and malformed tANS or
LZW input leave it unchanged. Its write-free exact-frame planner now freezes
canonical packed LZW bytes, plans every tANS block deterministically, and
reports the validated complete-frame extent while counting encoder records in
aggregate storage. Deterministic frame emission now reproduces the independent
vector, replans each block against the frozen extents, and preserves short
output. Its first bounded known-size streaming encoder emits the fixed prefix,
buffers at most one raw frame, and drains each immutable encoded frame before
accepting the next. Its bounded streaming decoder now collects one complete
encoded frame, admits every private region from its header, and publishes only
a fully validated raw frame. Its versioned C requirements query and factory
now expose the completed streaming pair while retaining all record layouts
inside the implementation. Its public-only completion matrix covers required
binary classes, deterministic chunking, stable terminals, and malformed final-
frame atomicity. Its bounded dual-decoder fuzz target and permanent malformed
regressions cover private and public decode boundaries. Its transactional CLI
selector uses only the public lifecycle and retains atomic output publication.
Its dependency-free benchmark verifies a byte-exact public-C round trip before
measurement and reports every queried workspace. Interoperability evidence
now includes schema-29 local generation, strict order and reorder rejection,
byte-identical re-encoding, and schemas 1 through 28 compatibility. External
four-direction confirmation at revision
`2dcc17c09477958c1f8777a266ecfefbb75217d2` verifies all 40 archives across
the recorded Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
producers.

### BR-0028

No Candidate cell remains in `docs/composition.md`. The final unpublished
pairing, `lzmw-tans`, is now Specified by DD-613 and enters implementation only
through that exact decoder-visible representation and reserved name.
`lzss-rans` is now the active admission composition. DD-462 fixes the complete
variable-length LZSS token boundary before scalar rANS, checked `S <= 2F`,
`K = ceil(S/B)`, `8K <= P <= S + 8K`, exact `528K` descriptor bytes,
entropy-before-dictionary validation, and an independently assembled 592-byte
single-Literal frame. Its first bounded complete-frame validator now admits
all extents, descriptor views, token staging, and aggregate workspace before
entropy processing. It validates every rANS block before mutating token
staging, then applies complete LZSS grammar, reference, overlap, and exact
raw-extent validation without reconstructing raw bytes. Its bounded private
decoder now admits and counts the entire raw staging region before entropy
work, then reconstructs only the completely validated Literal and overlap-
Match sequence into caller-owned disposable storage. Its transactional frame
decoder now preflights the complete destination before entropy processing and
publishes the private raw frame with one copy only after every layer succeeds.
Its write-free exact planner now freezes the canonical variable-length token
sequence, plans every rANS block, applies block-count and aggregate workspace
limits, validates the synthesized frame header, and reports the exact complete
extent. Its deterministic complete-frame encoder now plans fully before output
admission, explicitly serializes the generic header and every descriptor, and
encodes every exact payload subspan. It reproduces the independent vector,
round-trips split Literals and generated Matches deterministically, and leaves
short output unchanged. Its bounded known-size streaming encoder now emits
the canonical prefix and byte-identical one-shot frames under one-byte input
and output chunking, keeps `Flush` non-terminal, and enforces exact input,
workspace, and sticky error contracts. Its bounded streaming decoder now
collects one exact encoded frame, privately validates and reconstructs it,
then drains only committed raw bytes under arbitrary starvation. It strictly
rejects malformed prefixes, frames, truncation, trailing bytes, premature end,
and workspace shortages with sticky errors. Its bounded internal profile now
constructs the fixed stream identity and supplies checked direction-specific
workspace requirements, including descriptor-view count, without exposing
private layouts. Its public ABI v1 configuration, requirements query, and
factory now bind those workspaces as three opaque caller-owned regions and
round-trip through a pure C11 caller. Its public-ABI completion matrix now
covers required data classes, byte determinism, frame boundaries, arbitrary
chunking, sticky terminal states, and malformed-final-frame atomicity. It
now has a fixed-memory dual-boundary fuzz target plus permanent atomicity
regressions. Its CLI selector now uses only the public lifecycle and retains
transactional output and strict trailing-data rejection. Its dependency-free
benchmark verifies an exact public-ABI round trip before reporting ratio,
directional throughput, and queried workspace extents. Interoperability schema
21 appends the profile after the frozen schema-20 order, and its local
generation, exact-order verification, reordered-manifest rejection, and
schemas 1 through 20 compatibility pass. External schema-21 verification
subsequently passed in all four established directions at revision
`110bf3c9f80f5bc3723232c6f027867e4c2e7a2f` across Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang producers.

### BR-0029

The LZMW plus tANS cell now has a checked internal profile calculator in
addition to complete-frame and bounded streaming transforms. It derives all
caller-owned storage and proves that those requirements construct a real
streaming round trip. Later paragraphs record the public lifecycle, completion,
fuzz, CLI, benchmark, and completed schema-31 interoperability evidence; the
cell is locally Ready with its recorded external x86-64 exchange complete.

### BR-0030

The public C lifecycle is now present: its C11 test constructs both directions
from queried storage, proves a binary multi-frame round trip, and rejects short
or misaligned workspace plus nonzero reserved configuration. This addition is
necessary but does not alone mark the cell complete.

### BR-0031

The public-ABI completion matrix is also present. It covers empty and binary
classes, every one-byte value, frame-boundary lengths, deterministic repeated
encoding, one-byte and mixed chunking, sticky terminal results, and a malformed
fourth frame that cannot publish bytes beyond three completed frames. The fuzz
and CLI evidence is recorded below; benchmark and interoperability evidence
remain before `Ready`.

### BR-0032

The decoder fuzz boundary is now present. It caps input, output, every byte and
typed workspace, and total process calls before parsing arbitrary data. Its
compile-smoke passes under both local compilers, while permanent ordinary tests
prove atomic rejection of every canonical truncation, saturated frame extents,
and an invalid tANS descriptor. No open-ended fuzz run is release evidence by
itself; interoperability remains outstanding. The transactional
CLI selector is present and its binary, empty, malformed, trailing-data,
overwrite, and temporary-file regression passes under both local compilers.
The dependency-free benchmark now uses the same public-only profile, verifies
an exact round trip before timing, checks `80 + 6N + 2176K` complete-stream
capacity, and reports both directional three-region workspaces. The remaining
release-evidence step completed in all four directions at revision
`903181080556c3bb511ad4a2e5275837ebda48e7` for all 42 archives.

### BR-0033

The experimental Format 2 `lzss-contextual-dynamic-range` profile now has an
additive size-tagged C ABI configuration, direction-specific workspace query,
and factory over its bounded streaming pair. A pure-C test constructs both
directions from queried storage, proves a binary multi-frame round trip, and
rejects short, misaligned, overlapping, and nonzero-reserved inputs. This is
public lifecycle evidence only: it does not add a forty-third baseline matrix
cell or claim CLI, completion, fuzz, benchmark, or interoperability readiness.

### BR-0034

The experimental Format 2 public lifecycle now has a completion matrix over
empty and binary classes, all one-byte values, 63/64/65-byte boundaries,
repeat determinism, one-byte and mixed chunk schedules, and sticky terminal
results. A corrupt sequence, truncated payload, or trailing byte at the fourth
frame publishes only the three preceding frames. This is completion evidence,
not admission to the baseline matrix, CLI, fuzz, benchmark, or interoperability
bundle.

### BR-0035

The experimental Format 2 decoder fuzz boundary now caps input, every byte and
typed workspace, raw publication, and process calls before parsing arbitrary
data. It drives the private complete-frame decoder after header acceptance and
the public C decoder for every case. Compile-smoke succeeds under MSVC and
ClangCL, while ordinary regressions prove atomic rejection of all canonical
truncations, saturated frame extents, and invalid descriptor reserved data.
No sanitizer campaign, CLI, benchmark, or interoperability readiness is
claimed by this milestone.

### BR-0036

The experimental Format 2 lifecycle now has an explicit
`lzss-contextual-dynamic-range` CLI selector. The command-line adapter uses
only the public configuration, requirements, factory, process, and destroy
surface and preserves atomic temporary-file publication. Its generic
regression covers nonempty and empty round trips, overwrite refusal, malformed
input, strict trailing data, and failure cleanup under both local compilers.
It remains outside the stable 42-profile, benchmark, and interoperability
inventories.

### BR-0037

The experimental Format 2 profile now has dependency-free benchmark wiring.
Its checked `112 + 12N + 85K` capacity, public-only construction, pre-timing
round trip, directional throughput, compression ratio, and three-region
workspace reporting are covered by a separately labeled smoke test under both
local compilers. This is descriptive benchmark evidence, not admission to the
stable 42-profile benchmark matrix or interoperability schema.

### BR-0038

The experimental Format 2 profile is locally admitted as archive 43 in
interoperability schema 32 after the frozen 42-entry schema-31 order. Bundle
generation round-trips every archive; verification enforces exact order,
hashes, foreign decode equality, and byte-identical re-encoding; compatibility
rejects reordered schema 32 and reconstructs schemas 31 through 1. Its
four-direction external exchange at revision
`e9cf0c7d649cf32c9bc3a49bf3db9150370db381` verifies all 43 archives across
the recorded Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
producers.

### BR-0039

A controlled same-input smoke comparison now establishes the first empirical
tradeoff for the experimental architecture. At revision `6b1fd9b`, both MSVC
and ClangCL encode the 4,326-byte README to 3,355 bytes with the Format 1 LZSS
Dynamic Range profile and 2,389 bytes with the Format 2 contextual profile.
The approximately 28.8% reduction in encoded extent is accompanied by a rise
in peak caller-owned workspace from 655,493 to 1,638,485 bytes. This justifies
continued context/backend experiments but is not corpus-wide compression or
throughput evidence.

### BR-0040

The experimental contextual-rANS Format 2 lifecycle now has a public
completion matrix over empty and binary classes, all one-byte values,
63/64/65-byte boundaries, repeat determinism, one-byte and mixed chunk
schedules, and sticky terminal results. A corrupt sequence, truncated payload,
or trailing byte at the fourth frame publishes only the three preceding raw
frames. This is completion evidence, not baseline-matrix, CLI, fuzz,
benchmark, or interoperability admission.

### BR-0041

The experimental contextual-rANS Format 2 decoder now has a fixed-memory
dual-path fuzz boundary. It caps supplied bytes, every byte and native table/
token workspace, raw publication, and process calls before parsing arbitrary
input. Ordinary regressions require atomic rejection of every strict canonical
prefix, saturated frame extents, and nonzero descriptor flags. This is target
construction and compile-smoke evidence, not a sanitizer campaign or CLI,
benchmark, stable-matrix, or interoperability admission.

### BR-0042

The experimental contextual-rANS Format 2 lifecycle now has an explicit
`lzss-contextual-rans` CLI selector. The command-line adapter uses only the
public configuration, requirements, factory, process, and destroy surface and
preserves atomic temporary-file publication. Its common regression covers
nonempty and empty round trips, overwrite refusal, malformed input, strict
trailing data, and failure cleanup under both local compilers. It remains
outside the stable 42-profile, benchmark, and interoperability inventories.

### BR-0043

The experimental contextual-rANS profile now has dependency-free benchmark
wiring. Its checked `112 + 12N + 9,124K` capacity, public-only construction,
pre-timing round trip, directional throughput, compression ratio, and three-
region workspace reporting are covered by a separately labeled smoke test
under both local compilers. This is descriptive benchmark evidence, not
admission to the stable 42-profile benchmark matrix or interoperability schema.

### BR-0044

The experimental contextual-rANS encoder no longer repeats complete frame and
token-count planning above already transactional lower layers. Its reference
LZSS search count falls from six to two passes per frame without changing the
format, limits, queried workspace, or failure publication. README and format-
specification archives remain byte-identical to the pre-change results; the
latter's descriptive MSVC Release encode time falls from 30.462 to 10.053
seconds. The fixed descriptor remains unsuitable as a recommended profile and
requires a distinct compact variant before interoperability consideration.

### BR-0045

Contextual-rANS entropy variant 3 now has a complete reserved descriptor
contract and a hand-checkable one-Literal vector. Its canonical per-context
dense/sparse choice bounds every descriptor at 9,025 bytes while reducing the
vector descriptor from 9,052 to 26 bytes without changing rANS payload bytes.
Variant 2 retains its old identity. This is specification evidence only; no
parser, serializer, public API, CLI, benchmark, or interoperability admission
is claimed.

### BR-0046

The contextual Dynamic Range streaming encoder now relies on its existing
lower-layer transactional preflights instead of repeating complete-frame,
token-count, and operation-count plans. Reference LZSS search falls from six
to two passes per frame, while identical-input README and format-specification
archives retain exact SHA-256 values. Focused tests pass under both compilers;
this is a performance correction with no format, API, workspace, or admission
change.

### BR-0047

Contextual-rANS entropy variant 3 now has an independently testable private
descriptor parser, validator, and serializer. The implementation reconstructs
the complete fixed model before publication, enforces the strict sparse-size
inequality and dense tie rule, admits the exact 23 through 9,025-byte bounds,
and leaves caller state and output unchanged on failure. The corrected
one-Literal vector is 26 bytes: its alphabet-two context is dense on the
three-byte tie and its literal-value context is sparse. All strict prefixes,
trailing data, malformed sparse records, noncanonical alternatives, and local
limits are covered under both local compilers. State, frame, lifecycle,
benchmark, public API, and interoperability admission remain future work.

### BR-0048

The private contextual-rANS scalar decoder now accepts the exact variant-3
compact descriptor span and retains its detailed format error beside the
ordinary sticky decoder result. Successful compact and fixed descriptors
produce identical 126,976-entry reference tables and share all state
transitions and finish checks. Format admission remains separate, so a
26-byte descriptor plus eight-byte payload succeeds under a valid 34-byte
internal-buffer ceiling that correctly rejects variant 2's fixed descriptor.
Seven compact state tests plus the existing table, decoder, and format suites
pass under both local compilers. Typed-token, frame, streaming, encoder format,
public API, benchmark, and interoperability admission remain future work.

### BR-0049

The compact contextual-rANS Format 2 lifecycle now shares one typed public
completion matrix with fixed variant 2. Both distinct ABI-1 families cover
empty and binary classes, all one-byte values, 63/64/65-byte boundaries,
repeat determinism, one-byte and mixed chunk schedules, their exact entropy
variant identities, and sticky terminal results. A corrupt sequence,
truncated payload, or trailing byte at the fourth frame publishes only the
three preceding frames. All 2,577 registered tests, including interoperability
schema compatibility, pass under MSVC. This is completion evidence, not CLI,
compact-fuzz, benchmark, stable-matrix, or interoperability admission.

### BR-0050

The compact contextual-rANS Format 2 decoder now has its own compile-time
selected fixed-memory dual-path fuzz executable. It caps input, every byte and
native table/token workspace, raw publication, and process calls before
driving the variant-3 private complete-frame decoder and compact public C
decoder. The shared permanent matrix requires both variants to reject every
canonical truncation, saturated frame extents, and nonzero descriptor flags
without publication. MSVC compile-smoke and all 2,580 ordinary tests pass; a
ClangCL ASan/UBSan/libFuzzer smoke completed 1,000 bounded cases without a
crash, hang, or sanitizer finding at 44 MiB peak RSS. This is local fuzz
evidence, not CLI, benchmark, stable-matrix, or interoperability admission.

### BR-0051

The compact contextual-rANS lifecycle now has the explicit experimental CLI
selector `lzss-contextual-rans-compact`. It uses only the distinct compact
public configuration, requirements, factory, process, and destroy surface and
emits entropy identity `4/3`; the existing `lzss-contextual-rans` selector is
now regression-checked to remain `4/2`. The transactional regression covers
nonempty and empty round trips, overwrite refusal, malformed input, strict
trailing data, and failure cleanup. All 2,581 ordinary tests pass under MSVC.
This is CLI evidence, not benchmark, stable-matrix, or interoperability
admission.

### BR-0052

The compact contextual-rANS profile now has dependency-free benchmark wiring.
Its checked `112 + 12N + 9,097K` capacity, compact-public-only construction,
pre-timing round trip, directional throughput, complete-stream ratio, and
three-region workspace reporting are covered by a separately labeled smoke.
On the 4,326-byte README it produces 3,006 bytes versus fixed variant 2's
11,081 and contextual Dynamic Range's 2,389; the compact result exactly
confirms the earlier descriptor projection. All 2,626 tests in the benchmark-
enabled configuration pass under MSVC, including 42 stable and three
experimental benchmark smokes. This is descriptive benchmark evidence, not
stable-matrix or interoperability admission.

### BR-0053

The compact contextual-rANS profile is locally admitted as archive 44 in
interoperability schema 33. The schema freezes schema 32's exact 43-entry
order and appends only `lzss-contextual-rans-compact`; fixed-descriptor
variant 2 remains outside the interoperability inventory. Local generation,
exact-order and hash verification, decode/re-encode verification, reordered-
manifest rejection, and reconstruction of every historical schema from 32
through 1 pass. All 2,626 registered tests pass under MSVC in 73.84 seconds,
including `marc_interoperability_schema_compatibility`. External four-
direction exchange at revision
`2c30be4da1a80d01103dac0ee82fb0c4889f3af4` verifies all 44 archives across
the recorded Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
producers.

### BR-0054

Contextual tANS variant 2 now has an independently testable private descriptor
parser, validator, and serializer. It reproduces the 30-byte one-Literal
vector, admits exactly 27 through 9,029 bytes, enforces payload and valid-bit
rules plus the 131,072-entry decoder ceiling, rejects malformed and
noncanonical records, and preserves caller state and output on failure. The
canonical compact-record primitive is shared with compact contextual rANS,
whose exact vectors remain unchanged. State, table, frame, lifecycle, public
API, CLI, benchmark, fuzz, and interoperability admission remain future work.

### BR-0055

Contextual tANS variant 2 now has an independently tested private decode-table
builder. It preflights all active models and the implicit binary bypass model,
then publishes exactly 131,072 caller-owned transitions in fixed regions.
Standalone-equivalence, one-symbol permutations, inactive zeroing, exact
extent, limit rejection, and prewrite atomicity are covered. Live state
decoding, frames, public API, CLI, benchmark, fuzz, and interoperability remain
future work.

### BR-0056

Contextual tANS variant 2 now has an independently tested private state
decoder. It decodes the documented one-Literal vector and a mixed Symbol plus
LSB-first bypass vector, validates caller-owned transitions at use time, and
rejects malformed padding, state, bit extent, schema requests, counts, active
models, and lifecycle calls with sticky errors. Typed reconstruction, frames,
encoder, public API, CLI, benchmark, fuzz, and interoperability remain future
work.

### BR-0057

Contextual tANS variant 2 now has an independently tested private typed-token
decode bridge. It reconstructs the documented Literal vector atomically,
rejects an impossible Match after entropy decoding, enforces declared counts
and LZSS limits, and rejects short or overlapping payload/table/token storage
without token publication. Raw reconstruction, frames, encoder, public API,
CLI, benchmark, fuzz, and interoperability remain future work.

### BR-0058

Contextual tANS variant 2 now has a private Format 2 stream/frame parser and
complete-frame decoder. The exact 96-byte one-Literal frame reconstructs raw
`A`; descriptor, truncation, extent, limit, capacity, alias, and entropy-state
failures preserve raw output. A trailing serialized byte remains unconsumed.
Streaming, encoder, public API, CLI, benchmark, fuzz, and interoperability
remain future work.

### BR-0059

Contextual tANS variant 2 now has a private streaming frame decoder. One-byte
input/output, nonterminal Flush, empty streams, delayed draining, multi-frame
sequencing, later-frame corruption, truncation, trailing data, flags, limits,
workspace shortage, and all caller/private alias classes are covered. Encoder,
public API, CLI, benchmark, fuzz, and interoperability remain future work.

### BR-0060

Contextual tANS variant 2 now has an independently tested private operation
encoder. Exact Literal and Symbol-plus-bypass vectors decode to terminal state,
512 alternating decisions round-trip deterministically, and normalization,
malformed fields, table/payload capacity, all three region overlaps, limits,
and prewrite atomicity are covered. Direct typed-token encoding, frame and
streaming encoders, public API, CLI, benchmark, fuzz, and interoperability
remain future work.

### BR-0061

Contextual tANS variant 2 now has a private direct typed-LZSS encoder. It
validates and models tokens forward, reconstructs their field contexts during
reverse state emission, and exactly matches the materialized-operation
reference without allocating that intermediate array. Literal and mixed-token
vectors, direct decode, capacities, aliasing, limits, and prewrite atomicity
are covered. Complete-frame and streaming encoders, public API, CLI,
benchmark, fuzz, and interoperability remain future work.

### BR-0062

Contextual tANS variant 2 now has a private complete-frame encoder. The raw-
to-token, direct contextual entropy, canonical descriptor, and frame-header
stages reproduce the exact 96-byte Literal frame and deterministically round-
trip mixed input through the existing complete decoder. Token, table, output,
alias, input, stream, and aggregate four-region workspace failures are
covered. Streaming encode, public API, CLI, benchmark, fuzz, and
interoperability remain future work.

### BR-0063

Contextual tANS variant 2 now has a private streaming frame encoder. One-byte
input/output reproduces the exact stream header and consecutive Literal
frames; full-frame emission, partial-frame Flush, retained EndInput, empty
streams, capacities, four-region aggregate limits, input protocol, flags,
private aliases, caller-output aliases, and sticky terminal states are
covered. Public API, profile calculator, CLI, benchmark, fuzz, and
interoperability remain future work.

### BR-0064

Contextual tANS variant 2 now has a private profile and direction-specific
typed-view partitioners. Encoder requirements include raw, exact tokens,
131,072 inverse entries, and conservative serialized frame storage; decoder
requirements derive encoded/raw, 131,072 transitions, and tokens solely from
local limits. Empty streams, bounds, aggregate limits, alignment, forged
requirements, atomic partitioning, stable errors, and profile-constructed
streaming round trips are covered. Public ABI, CLI, benchmark, fuzz, and
interoperability remain future work.

### BR-0065

Contextual tANS variant 2 now has an experimental ABI-1 C lifecycle. Config
initialization, direction-specific exact workspace queries, aligned opaque
typed storage, encode/decode construction, binary round trip, stream identity,
short/misaligned/overlapping regions, reserved fields, size/version tags,
directions, and null arguments are covered by a C11 test linked through the
public library. Completion, CLI, benchmark, fuzz, and interoperability remain
future work.

### BR-0066

Contextual tANS variant 2 now passes the public completion audit. Empty input,
all 256 one-byte values, all-byte sequence, repeated data, mixed binary
patterns, deterministic random data, frame boundaries, and multi-frame input
round-trip deterministically. One-byte and mixed chunk schedules reproduce the
same stream. Corrupted, truncated, and trailing final-frame data preserve the
last raw byte, retain earlier frames, and return a stable repeated error. CLI,
benchmark, fuzz, and interoperability remain future work.

### BR-0067

Contextual tANS variant 2 now retains permanent dual-boundary malformed-input
regressions. Every strict prefix of a generated canonical `ABABX` stream,
all-ones frame extents, and a nonzero descriptor reserved byte fail without
publishing raw output through both the complete-frame helper and public
streaming lifecycle. The bounded harness fixes all input, output, frame,
payload, table, token, aggregate, and call ceilings and compiles warning-clean.
No sanitizer campaign is claimed yet; CLI, benchmark, and interoperability
remain future work.

### BR-0068

Contextual tANS variant 2 has completed its initial bounded Windows sanitizer
smoke. The Clang 22 libFuzzer/ASan/UBSan target processed 1,000 inputs with a
32 KiB maximum input, five-second per-input timeout, 512 MiB RSS limit, and
42 MiB peak RSS without a crash, hang, or sanitizer finding. No corpus or
artifact entered the repository. This is bounded evidence rather than an
exhaustive safety claim; CLI, benchmark, and interoperability remain future
work.

### BR-0069

Contextual tANS variant 2 now has an experimental benchmark profile. Checked
capacity, direction-specific public workspace queries, exact pre-timing round
trip, ratio, encode/decode throughput, and peak caller-owned workspace are
covered by a registered smoke test. The initial same-input result is recorded
as descriptive evidence only; CLI and interoperability remain future work.

### BR-0070

Contextual tANS variant 2 now has an experimental CLI selector. Public-only
configuration, direction-specific workspace construction, file round trip,
exact entropy identity, output commit, and strict trailing-data rejection are
covered. The selector remains outside the stable 42-profile inventory and
requires explicit selection for decoding. Interoperability remains future
work.

### BR-0071

Contextual tANS variant 2 is locally admitted as archive 45 in interoperability
schema 34. Generation, self-verification, exact manifest order, SHA-256,
fixture decode, byte-identical re-encoding, reordered-manifest rejection, and
schemas 1 through 33 compatibility pass locally. External four-direction
Windows/Linux evidence remains pending.

### BR-0072

Contextual tANS interoperability schema 34 now has complete four-direction
external evidence at revision `4929252144e4bfe44fb3ec076f548aa47e4ff111`.
All 45 archives from the Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu
26.04/Clang producers decode and re-encode byte-identically across the tested
Windows and WSL2 Linux x86-64 paths.

### BR-0073

Contextual Blocked Huffman now has an additive experimental ABI-1 lifecycle.
Direction-specific workspace queries retain typed LZSS tokens and bounded
Huffman tables behind opaque caller-owned regions while emitting unchanged
Format 2 dictionary `2/2`, entropy `2/2`. It remains outside CLI, benchmark,
stable-matrix, and interoperability inventories.

### BR-0074

The public lifecycle satisfies its local completion matrix through ABI 1.
Required binary classes, deterministic repeated encoding, three mixed chunk
schedules, stable terminal state, and final-frame corruption, truncation, and
trailing-data atomicity pass under MSVC and ClangCL. Tool and interoperability
admission remain separate.

### BR-0075

The private complete-frame and public streaming decoders now share permanent
malformed-input regressions and a fixed-memory fuzz harness. Every canonical
truncation, saturated frame extents, and nonzero descriptor flags preserve raw
publication atomically. Input, frame, decision, payload, table, output,
aggregate, and call bounds are fixed before arbitrary input is parsed.

### BR-0076

The initial bounded Windows sanitizer smoke completed 1,000 inputs under
Clang 22 libFuzzer/ASan/UBSan with 32 KiB maximum input, five-second per-input
timeout, 512 MiB RSS limit, and 40 MiB peak RSS. No crash, hang, sanitizer
finding, persistent corpus, or artifact occurred. This is bounded execution
evidence; CLI, benchmark, and interoperability admission remain future work.

### BR-0077

Contextual Blocked Huffman now has an experimental benchmark selector backed
only by its public ABI-1 lifecycle. A registered smoke test covers checked
complete-stream capacity, independent direction-specific workspace queries,
exact pre-timing round trip, repeatable measured extents, ratio, throughput,
and peak caller-owned workspace. No metric is a pass threshold, and the
profile remains outside the stable 42-profile, CLI, and interoperability
inventories.

### BR-0078

Contextual Blocked Huffman now has an explicit experimental CLI selector.
Public-only configuration and workspace construction, nonempty and empty file
round trip, exact Format 2 entropy identity `2/2`, overwrite refusal,
malformed-input rejection, strict trailing-data rejection, and failed-output
cleanup are registered. It remains outside the stable 42-profile and
interoperability inventories and is not auto-detected.

### BR-0079

Contextual Blocked Huffman is locally admitted as archive 46 in
interoperability schema 35. Generation, self-verification, exact manifest
order, size and SHA-256, fixture decode, byte-identical re-encoding,
reordered-manifest rejection, and schemas 1 through 34 compatibility are
required locally. External Windows/Linux four-direction evidence remains a
separate post-push milestone.

### BR-0080

The Contextual Blocked Huffman complete-frame decoder is now self-contained
with respect to `std::in_range`: its defining `<utility>` header is included
directly. A repository-wide use-site audit found no second omission, Ubuntu
Clang accepts the translation unit independently, and both local compiler
suites pass without any stream, ABI, limit, or schema change.

### BR-0081

Schema 35 now has complete four-direction external evidence at revision
`7c276151ab428aa9ba0376f8d9ba9a85a9fbd347`. All 46 archives from the
Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers decode and
re-encode byte-identically across the tested Windows and WSL2 Linux x86-64
paths.

### BR-0082

Contextual Adaptive Huffman now has a decoder-visible Format 2 reservation.
The stream identity, parameter and descriptor layouts, 31 context-local FGK
trees, alphabet-specific NYT widths, frame reset, bypass-bit rule, strict
completion conditions, fixed bounds, and one-Literal vector are specified.
No implementation or public surface is claimed, so baseline and schema-35
readiness remain unchanged.

### BR-0083

The reserved Contextual Adaptive Huffman profile now has a private fixed
descriptor boundary and an allocation-free alphabet-bounded FGK tree. Exact
serialization, malformed fields, local limits, atomic failure, all four schema
alphabet sizes, full-tree insertion, reset, and byte-alphabet equivalence with
variant 1 are tested. Payload and frame completion remain future work.

### BR-0084

The Contextual Adaptive Huffman payload can now be decoded at the private
operation boundary with all 31 FGK contexts live in exact caller-owned
storage. Documented and mixed hand vectors, existing and new symbols,
BypassBits, malformed NYT values, truncation atomicity, overlap, local limits,
strict completion, and sticky lifecycle errors are covered. This does not yet
constitute a complete codec profile.

### BR-0085

The private Contextual Adaptive Huffman decoder now reconstructs validated
LZSS typed tokens. Literal and overlapping-match vectors, bypass fields,
invalid distance, count and raw-extent disagreement, truncation, every
workspace shortage and alias class, aggregate limits, and atomic token output
are tested under both local compilers. Raw-byte reconstruction, a complete
frame transaction, encoder parity, streaming, C ABI, tools, and
interoperability are still required before profile admission.

### BR-0086

The reserved Contextual Adaptive Huffman identity now has private stream and
frame parsing, serialization, and preflight. Exact documented bytes,
round-trip headers, every truncated stream/frame prefix, wrong identity and
parameters, dictionary and model bounds, sequence and reserved metadata,
unsupported features, descriptor contradictions, payload limits, trailing
input preservation, and transactional outputs are tested under both local
compilers. Complete-frame decoding and all encoder/public layers remain
pending.

### BR-0087

The private Contextual Adaptive Huffman path now decodes one complete frame to
raw bytes. The documented frame, trailing input preservation, truncation,
every workspace shortage, serialized and model/raw aliases, nonzero entropy
padding, nested diagnostics, token/raw sentinel preservation, and success-only
consumption are tested under both local compilers. A streaming decoder,
encoder parity, C ABI, CLI, benchmark, malformed regression set, fuzzing, and
interoperability admission remain required.

### BR-0088

The private Contextual Adaptive Huffman path now satisfies the streaming
decode lifecycle. One-byte input/output, zero-capacity final drain, empty
input, two-frame model reset, deterministic completion, truncation at
stream-header, frame-header, and frame-body boundaries, trailing data,
nonzero padding, every workspace shortage, representative constructor and
output aliases, aggregate limits,
unsupported flags, and sticky terminal results are tested under both local
compilers. Encoder parity, public C ABI, CLI, benchmark, malformed regression
set, fuzzing, and interoperability admission remain pending.

### BR-0089

The private Contextual Adaptive Huffman operation path can now plan and encode
the reserved payload representation. The documented literal vector, existing
and new symbols, bypass bits, encode/decode model lockstep, deterministic fresh
workspaces, every malformed operation field, short model and payload storage,
operation/model/output aliases, trailing-capacity preservation, entropy-entry,
payload, planning-aggregate, and writing-aggregate limits are tested under
both local compilers. LZSS token encoding and all frame/public layers remain
pending.

### BR-0090

The private Contextual Adaptive Huffman path can now encode validated LZSS
typed tokens directly. The two documented payloads, operation-boundary byte
identity, token-decoder round trip, invalid parameters, invalid and
raw-mismatched tokens, short model and payload storage, every token-related
alias, entropy, payload, aggregate, and total-output limits, trailing-capacity
preservation, deterministic fresh workspaces, and forward lifecycle misuse are
tested under both local compilers. Complete-frame and streaming encoding plus
all public layers remain pending.

### BR-0091

The private Contextual Adaptive Huffman path now encodes one complete raw LZSS
frame. It reproduces the documented 82-byte frame, decodes through the existing
complete-frame consumer, round-trips mixed literal/match input deterministically,
preserves trailing serialized and unused token capacity, rejects short token,
node, symbol, and output storage, rejects raw and output alias classes, and
enforces stream, frame-size, and exact aggregate limits under both local
compilers. Streaming encoding and all public layers remain pending.

### BR-0092

The private Contextual Adaptive Huffman path now satisfies both streaming
directions. The encoder reproduces the documented stream under one-byte
input/output, resets the model across two frames, emits complete frames before
finish, keeps partial frames open on flush, latches final input through frame
and empty-header drain, rejects short workspaces and exact aggregate overflow,
checks constructor and output aliases, enforces input protocol and unsupported
flags, and keeps terminal states sticky under both local compilers. Public C
ABI, CLI, benchmark, malformed regression, fuzzing, and interoperability
admission remain pending.

### BR-0093

The private Contextual Adaptive Huffman profile now derives bounded encoder and
decoder storage without allocator callbacks. Default, short, empty, restricted
decoder, payload, block, entropy-entry, and aggregate cases are tested; aligned
token/node/symbol partitions reject forged metadata, short storage, and
misalignment transactionally. Requirements produced by the calculator build
the streaming encoder and decoder and complete a multi-frame round trip under
both local compilers. Public C ABI, CLI, benchmark, malformed regression,
fuzzing, and interoperability admission remain pending.

### BR-0094

The private Contextual Adaptive Huffman lifecycle is now constructible through
a small additive ABI-1 C family without exposing typed C++ storage. The C11
boundary validates canonical defaults, exact encode/decode requirements,
Format 2 identity, binary round trip, capacity, alignment, pairwise overlap,
configuration tags, reserved fields, directions, and null arguments under
MSVC and ClangCL. All 2,866 registered tests, including
`marc_interoperability_schema_compatibility`, pass under both compilers with
the 240-second per-test limit; static and shared targets build successfully.
Public completion, malformed regression, fuzzing, benchmark, CLI, and
interoperability admission remain pending.

### BR-0095

Contextual Adaptive Huffman now satisfies the local public completion audit
through only its ABI-1 C lifecycle. The audit proves required binary classes,
byte-identical repeated encoding, one-byte and mixed multi-frame chunking,
sticky EndOfStream, and frame-atomic malformed final-frame rejection for
corruption, truncation, and trailing data under MSVC and ClangCL. Private
completion helpers are not part of the proof. All 2,869 registered tests,
including `marc_interoperability_schema_compatibility`, pass under both local
compilers with the 240-second per-test limit. Dedicated malformed regression,
fuzzing, benchmark, CLI, and interoperability admission remain pending.

### BR-0096

Contextual Adaptive Huffman now has permanent dual-boundary malformed-input
regressions. Every canonical strict prefix plus independent stream identity,
reserved-byte, extreme frame count/length, descriptor context/final-bit/flag/
reserved, and nonzero padding mutations preserve private and public raw-output
sentinels. Public failures are sticky malformed-stream results with zero
publication. All 2,874 registered tests, including
`marc_interoperability_schema_compatibility`, pass under MSVC and ClangCL with
the 240-second per-test limit. A bounded fuzz harness and sanitizer smoke,
benchmark, CLI, and interoperability admission remain pending.

### BR-0097

Contextual Adaptive Huffman now has an admitted bounded dual-decoder harness.
All storage and process-call ceilings are compile-time fixed; public workspace
requirements must fit before handle construction, and input bytes affect only
bounded chunk schedules. The translation unit compiles warning-clean under
MSVC and ClangCL, the five permanent malformed regressions still pass, and all
2,874 registered tests including `marc_interoperability_schema_compatibility`
pass under both compilers with the 240-second per-test limit. No mutation or
sanitizer campaign has been executed. Benchmark, CLI, and interoperability
admission remain pending.

### BR-0098

Contextual Adaptive Huffman's admitted bounded harness now has its initial
Windows Clang 22 libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer
smoke. Exactly 1,000 generated inputs completed under a 64 KiB maximum input,
five-second per-input timeout, and 512 MiB RSS limit without a crash, hang, or
sanitizer finding; peak RSS was 40 MiB. The run supplied no persistent corpus
and produced no artifact. This is bounded evidence for the exercised inputs,
not an exhaustive safety claim. Benchmark, CLI, and interoperability admission
remain pending.

### BR-0099

Contextual Adaptive Huffman now has an explicit experimental benchmark
selector backed only by its public ABI-1 lifecycle. One measured Release
iteration over the 4,326-byte README round trips an identical 2,572-byte stream
under MSVC and ClangCL, and both builds report identical direction-specific
workspace extents. The stable benchmark matrix, CLI, and interoperability
schema remain unchanged. All 2,875 registered tests, including
`marc_interoperability_schema_compatibility`, pass under both compilers with
the 240-second per-test limit. CLI and interoperability admission remain
pending.

### BR-0100

Contextual Adaptive Huffman now has an explicit experimental CLI selector.
Its registered test verifies Format 2 entropy identity `1/2`, nonempty and
empty byte-exact round trips, overwrite refusal, malformed and strict-trailing
rejection, and absence of destination or temporary output on failure under
MSVC and ClangCL. The stable profile inventory and interoperability schema
remain unchanged. All 2,876 registered tests, including
`marc_interoperability_schema_compatibility`, pass under both compilers with
the 240-second per-test limit. Interoperability admission remains pending.

### BR-0101

Contextual Adaptive Huffman now occupies archive 47 of interoperability
schema 36 and codec set `marc-cli-v36`, after the frozen schema-35 order. The
focused compatibility test proves generator round trip, exact manifest order,
size and SHA-256 validation, fixture decode, byte-identical re-encoding,
reordered-manifest rejection, and the complete schema-36-through-1 downgrade
chain under MSVC and ClangCL. It completes in 68.10 seconds and 64.79 seconds,
respectively. All 2,876 registered tests, including
`marc_interoperability_schema_compatibility`, pass under both compilers with
the 240-second per-test limit. External four-direction schema-36 verification
remains pending; the stable 42-profile inventory is unchanged.

### BR-0102

Schema 36 now has complete four-direction external evidence at revision
`bdcabd439d9cedb9e58f3dd2a3ac4dcb3526e1a2`. All 47 archives from the
Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers decode and
re-encode byte-identically across the tested Windows and WSL2 Linux x86-64
paths.

### BR-0103

Contextual rANS now exposes only the unqualified public C, CLI, and benchmark
surface, backed by entropy variant 3. The six canonical completion and
malformed-regression cases plus C11 lifecycle, CLI file round trip, and
benchmark smoke all pass under MSVC 19.51.36252 and ClangCL 22.1.3. Both
compilers build the affected library, CLI, benchmark, core-test, and C11
targets warning-clean. This is focused evidence; private fixed-layer removal,
full-suite, sanitizer, schema-37, and performance-comparison evidence remain
pending.
