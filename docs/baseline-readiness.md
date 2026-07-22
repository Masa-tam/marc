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

| Required codec | Public CLI profile | Local status | Interoperability schema 11 |
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

| Profile | Purpose | Local status | Interoperability schema 11 |
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
| `lzd-adaptive-huffman` | Fifth Adaptive Huffman composition | In progress | Not included |
| `checksum-raw` | Version 1.1 per-frame CRC-32C framing profile | Ready | Included |

Schema 11 contains twenty-two archives: the frozen twenty-one-entry schema-10
set followed by the LZW Adaptive Huffman profile. Schemas 1 through 10 remain
frozen at seven, eight, thirteen, fifteen, sixteen, seventeen, eighteen,
nineteen, twenty, and twenty-one profiles; their meanings are fixed by their
version and codec-set rules.

## Public-profile evidence matrix

Every `Yes` below names an implemented and test-covered repository boundary.
`Completion` is the public C ABI matrix covering required data classes,
deterministic output, one-byte and mixed chunking, repeated terminal calls,
and transactional rejection of a malformed final frame. Interoperability is
kept separate because it requires artifacts produced outside the local build.

| Public profile | Format + validator | Streaming | C ABI | CLI | Benchmark | Bounded fuzz | Completion | Schema 11 |
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
| `lzd-adaptive-huffman` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Not included |
| `checksum-raw` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Included |

## Composed-profile admission queue

`lz78-adaptive-huffman` now has its exact format, checked frame path, bounded
streaming transforms, typed workspace profile, and public C ABI factory. It
now also has a public-ABI completion matrix, bounded fuzz evidence, a
transactional CLI selector, a verified public-ABI benchmark adapter, and local
schema-10 generation/verification coverage. The pushed Windows/MSVC and Ubuntu
24.04 artifacts plus an independently generated Ubuntu 26.04/Clang bundle have
now passed the complete bidirectional external verification contract, so this
profile is `Ready`.

Candidate pairings remain
listed in `docs/composition.md`; they enter the queue only after their exact
decoder-visible representation and reserved public name are specified.
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

`lzd-adaptive-huffman` is the fifth Adaptive composition under active
admission. DD-330 fixes its
decoder-visible representation, checked `8*ceil(F/2)` token bound, `33S`
Adaptive payload bound, phrase and expansion-workspace ceilings, validation
order, and independent 77-byte terminal-token frame. It remains outside the
interoperability schema only. Its first complete-frame
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
byte-exact round trip. The interoperability boundary remains.

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
solid grouping, BWT-family transforms, and additional composed profiles remain
future extensions. They are not baseline-readiness failures.
The [composition matrix](composition.md) distinguishes these unpublished
pairings from algorithm incompatibility and records the staged generation path.

## Published CI evidence

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

An external Ubuntu 26.04/Clang 21 environment under WSL2 subsequently verified
all eighteen Windows/MSVC and Ubuntu 24.04/Ninja archives, then generated an
Ubuntu 26.04 bundle that the local Windows/MSVC executable verified in the
reverse direction. All nineteen binary files in each bundle (`input.bin` plus
eighteen archives) were byte-identical across the three producers. This closes
the current x86-64 operating-system/compiler cross-check; a second architecture
remains open.

Revision `a4e3d1a5acb7bfc393aca4f2195188cfe0421817` subsequently completed its
pushed Windows/MSVC and Ubuntu 24.04/Ninja CI run and produced schema-8
artifacts. Ubuntu 26.04/Clang 21.1.8 verified all nineteen archives from both
artifacts, generated a third schema-8 bundle, and verified that bundle locally.
The Windows/MSVC executable then verified all nineteen Ubuntu 26.04 archives in
the reverse direction. Because each verifier also requires byte-identical local
re-encoding, this closes the current schema-8 x86-64 Windows/Linux/compiler
cross-check. A second architecture remains open.

Revision `8a854eaf9c7c6c36cc2d444cc8e1a135935887b2` subsequently completed pushed
CI and produced the Windows/MSVC and Ubuntu 24.04/Ninja schema-9 artifacts.
The previously recorded Ubuntu 26.04/Clang 21.1.8 environment verified all
twenty archives from both artifacts, generated and verified its own
twenty-archive bundle, and supplied that bundle to the Windows/MSVC executable
for reverse verification. Every pass required byte-identical local
re-encoding. This closes the schema-9 x86-64 Windows/Linux/compiler cross-check;
a second architecture and non-WSL Linux kernel remain open.

Revision `bc8faba3043db78a953f18876f153abc847f814d` subsequently completed
pushed CI and produced the Windows/MSVC and Ubuntu 24.04/Ninja schema-10
artifacts. Ubuntu 26.04/Clang 21.1.8 verified all twenty-one archives from both
artifacts, generated and verified its own schema-10 bundle, and supplied that
bundle to the Windows/MSVC executable for reverse verification. Every pass
required byte-identical local re-encoding. This closes the schema-10 x86-64
Windows/Linux/compiler cross-check; a second architecture and non-WSL Linux
kernel remain open.

Revision `163948c61dd8b90359882bee122f16ab3794787c` subsequently completed
pushed CI and produced the Windows/MSVC and Ubuntu 24.04/Ninja schema-11
artifacts. Ubuntu 26.04/Clang 21.1.8 verified all twenty-two archives from both
artifacts, generated and verified its own schema-11 bundle, and supplied that
bundle to the Windows/MSVC executable for reverse verification. Every pass
required byte-identical local re-encoding. This closes the schema-11 x86-64
Windows/Linux/compiler cross-check; a second architecture and non-WSL Linux
kernel remain open.

## Pre-publication CI and package audit

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

## Pre-publication similarity and claims audit

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

## Current validation baseline

At DD-342, the complete Release suite contains 1,391 tests and passes under both
MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64. This is strong local
compiler-independence evidence on one architecture. Public run 29647453799 adds
Windows/MSVC and Ubuntu/Ninja CI plus installed-package evidence; the remaining
release-evidence limits are stated above.
