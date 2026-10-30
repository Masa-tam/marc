# Stage-one refactoring: source roles and remaining work

Audit date: 2026-10-31. Source snapshot: `58b12eda`, after the public rANS
owner/frame/stream consolidation. This is a local source-role audit, not the
stage-one completion or hosted-validation receipt. The controlling scope and
checkpoint policy remain in [the plan](v1-refactoring-stage-1.md).

## Inventory and method

Read tracked files, the generated MSVC solution/project dependencies and CTest
commands, explicit CMake declarations, quoted project includes and retained
Python oracle imports. Follow project references and first-party include edges;
do not classify a path as obsolete merely because its name contains `candidate`,
`prepared`, `reference` or `scratch`. A compile-list entry establishes build
membership, not proof that every function is reached by every public profile.
Public-route adoption and retirement decisions retain DD-1630/DD-1631 evidence.

| Tracked area | v0.8.0 paths | Audit snapshot paths | Disposition |
| --- | ---: | ---: | --- |
| src | 1477 | 1419 | Production primitives/profile implementations and explicit test references. |
| tests | 1296 | 1155 | Retained contract, vector, differential, malformed and allocation tests. |
| fuzz | 164 | 162 | 125 harness cpp files and 37 checked-in seed inputs. |
| benchmarks | 204 | 8 | One common driver, five support headers, two explanatory READMEs. |
| tools | 92 | 43 | CLI/adapters, maintained validation/measurement tools and independent oracles. |

These are raw tracked-path counts, including the corpus documentation; the
plan's initial203 non-corpus benchmark count excluded that README. User-owned
ignored Silesia inputs, caches, build artifacts and local results are excluded.
Counts are observations, not deletion quotas.

The ordinary MSVC configuration has422 targets and4172 CTests. Its shared and
static libraries compile the same680 cpp files. All695 tracked src cpp files
are accounted for:680 production compilation units and15 test-only units below.
Their generated consumers and project references are present. The production
include closure contains1388 first-party files; every tracked src header is
reachable from the ordinary target/include graph. No removed experiment path
remains in active CMake declarations, and no tracked src cpp is unregistered.

A fresh Clang22.1.3/Ninja Release configuration with benchmarks, diagnostics and
fuzz enabled generates566 targets. All125 retained fuzz harness cpp files are
registered and the optional exporter is present. This is configuration/dependency
evidence; no new sanitizer/fuzz campaign or full Clang codec runtime is claimed.

## Test-only implementation roles

The following15 cpp files are absent from the production compile list. Their
existing tests/fuzz consumers justify retention; sharing them into a production
helper would weaken the independent comparison boundary.

| Group | Files under src | Current consumers and reason |
| --- | --- | --- |
| Older4MiB rANS control profile (8) | `context/lzss_position_rans_4m_tokens.cpp`; `entropy/position_rans_4m_{format,encoder,decoder}.cpp`; `frame/lzss_position_rans_4m_{format,frame,stream_decoder,owned}.cpp` | Format/token/frame/stream/owner tests, ANS pool-applicability controls and optional stream fuzzing. The public4MiB profile uses the distinct full-literal implementation. |
| Independent8MiB dictionary reference (1) | `dictionary/lzss_position_distance_8m_reference.cpp` | Reference/indexed tests compare deterministic match selection against the retained reference. |
| Older1MiB owner wrappers (2) | `frame/lzss_position_distance_1m_{prepared_owned_encoder,finder_scratch_owned_encoder}.cpp` | Prepared/finder-scratch stream and allocation tests. Adopted underlying prepared/model/finder primitives remain production dependencies. |
|4MiB reference support (3) | `frame/lzss_position_distance_4m_{owned_encoder,owned_decoder,five_prefix_owned_encoder}.cpp` | `marc_position_distance_4m_reference_support` links production primitives once and supplies retained frame/reference tests. |
|8MiB operation-stream companion (1) | `frame/lzss_position_distance_8m_stream_encoder.cpp` | Stream/owning/prepared/public-encoder differential and replay tests. The adopted prepared-owning route remains in production. |

These classifications confirm the prepared/reference adoption map in the plan;
they do not relabel test-only wrappers as shipped APIs or remove a decoder oracle.

## Maintained tool graph

All43 tracked tool files have current roles:

- One CLI implementation,26 explicit profile headers and six shared support
  headers provide public setup, pumping, borrowed storage and file transactions.
- `run_public_benchmark.py`, `verify_silesia_corpus.py` and
  `benchmark_process_audit.py` provide repeatable measurement, external-input
  verification and process accounting for the common102-profile driver.
- `ci_validation_gate.py` preserves exact-SHA complete-run selection and offline
  history-policy tests. This audit changes no CI triggers or coverage.
- `generate_position_adaptive_huffman_vectors.py` and
  `position_distance_rans_diagnostic.py` provide independent vector/model evidence.
  Current rANS differential/vector tests import the Python diagnostic directly.
- `position_distance_rans_export.cpp` is an optional trusted bounded-frame bridge
  from native modeled tokens to that mathematical diagnostic. It is not a
  benchmark or public codec entry point. `MARC_BUILD_ENTROPY_DIAGNOSTICS` defaults
  OFF and requires the static library. The Python tests do not require executing
  the exporter; the native/manual bridge has its own verification below.
- Three `run_position_blocked_huffman_{64k,1m,4m}_fuzz.py` runners retain bounded
  local public-profile fuzz/replay workflows. Dedicated hosted fuzz jobs stay absent.

The common benchmark is the sole remaining measurement executable. Its five
support headers share process/verification, setup, file, session and physical-RSS
mechanics. Historical experiment descriptions remain in the two benchmark
READMEs; no active experiment JSON manifest or retired runner is left there.

Build the retained exporter against the qualified MSVC static library and compare
empty/mixed binary fixtures at minimum-match3/5 through model versions1/2. All
eight native-export/mathematical-oracle cases pass. The retained diagnostic CTest
also passes (6.84 seconds). No corpus files are copied, changed or redistributed.

## Codec-family review

| Family | Assessment |
| --- | --- |
| Position-distance Blocked Huffman | Seven windows share explicit owner/frame/stream mechanics (DD-1647–DD-1649). Profile constants, formats, tables and validation contracts remain explicit. |
| Position-distance tANS | Seven windows share tANS-specific owner/frame/stream helpers (DD-1651). Its table bank, payload rounding and admission remain distinct from Huffman/rANS. |
| Public position-distance rANS | Seven windows share rANS-specific owner/frame/stream helpers (DD-1652–DD-1653); older4MiB controls remain separate. |
| Position-distance Adaptive Huffman | Already one profile-dispatched owner/frame/stream implementation for all seven windows. Adding another template layer would not remove cloned implementations. |
| Position-distance Range,16/32/64MiB owned stream | Three329-line cpp files match after profile-name substitution and whitespace normalization. This is a concrete remaining common-mechanics opportunity; retain the plan's family-sharing item open and extract it next. |
| Other Range ownership paths |1/4/8MiB have prepared/fallback, operation/token allocator or distinct allocation/admission contracts. Do not merge them into a universal owner merely because their stream statuses are similar. Keep their adopted/reference roles explicit. |
| Core dictionary/entropy/profile primitives | Shared bit, serialization, checked arithmetic, hashing and stream contracts remain foundational. Different codec/model/token formats and independent reference implementations remain separate; this stage does not replace algorithm bodies with a universal engine. |

The Range review compares14 corresponding frame-component triplets. In addition
to the329-line owned-stream implementations, the277-line frame encoders,
164-line frame decoders and249-line serializers match after profile-name and
whitespace normalization. Review these as subsequent family-specific extraction
scopes, preserving their explicit profile/format callbacks. The21-line owning
delegates are already identity-preserving forwarding wrappers; another helper
would not materially simplify them. The other nine component triplets do not
match this substitution rule and require their contract differences to remain
explicit. A similar filename is not sufficient evidence for sharing them.

## Retired reproduction commands and remaining gates

The [retired command index](v1-refactoring-retired-commands.md) identifies87
measurement/tool targets and68 runners/manifests present in v0.8.0 and absent
from the current tree. Every listed target's source has retired and none is in
the ordinary/optional generated target sets. Existing historical measurements,
design records and provenance retain their original results; old recipes refer
to v0.8.0, not maintained commands. Current measurements use the common benchmark
and [current measurement contract](../benchmarks.md).

This audit closes the remaining phase-two role/retired-command classification.
It does not close phase six: Range16/32/64MiB owner/frame/serializer sharing still
needs code and qualification. After that work, refresh affected role/inventory evidence and
audit all six phases against executable evidence. Complete Windows/Linux,
linkage/package and102-profile hosted validation at the integrated candidate
remains required under DD-1650. Do not request separate hosted runs for this
documentation audit or the next ownership helper.


## Subsequent ownership qualification

DD-1655/TVG-1674/CR-2053 qualify the16/32/64MiB Range owning-stream bridge and
coordinator extraction locally. One stateless private helper becomes a production
include dependency; concrete allocation interfaces, layouts and call-controls
remain unchanged. The counts/source comparisons above intentionally identify
the original58b12eda snapshot. Refresh the affected inventory/include closure at
final integration. Frame encoder/decoder and serializer sharing and remaining
contract/completion review stay open; this update does not claim a hosted result.

## Detailed follow-up to the frame survey

The original14-triplet survey contains five name-normalized matches and nine
nonmatches. The earlier prose/TVG-1673 count of eight nonmatches was an arithmetic
error; the nine-component count above corrects it without changing source evidence.

Ignoring include ordering, comments and whitespace, then binding existing callback-name aliases and explicit
profile bounds/context counts identifies five further comparable responsibilities:
`compact_frame_encoder`, `compact_owner`, `compact_prefix`, `owning_adapter` and
`token_frame_encoder`. The adapter difference is include ordering; the other
differences include native callback suffixes, window bounds, context counts48/49/50,
decision/payload bounds and entropy-table entries2610/2621/2632. All three complete
sources match after these explicit bindings. These are subsequent extraction
candidates, not completed work or an authorization to normalize format values.

Keep the remaining four responsibilities explicit: C encoder factories contain
public profile initialization/limits; decoder factories and stream decoders
distinguish typed-token16MiB storage from packed-byte32/64MiB storage and their
helper/lifetime charges; the small basic preflights state decoder-visible profile
IDs, limits and memory representation. Their shared C-API boundary infrastructure
already exists. Keep21-line owning delegates as concrete identity forwards.
After the pending five comparable paths are qualified, refresh inventory evidence
and audit the remaining distinctions and full completion requirements again.


## Subsequent operation-frame and serializer qualification

DD-1656/TVG-1675/CR-2054 qualify16/32/64MiB operation-frame encode/decode and
serialization sharing locally. The nine concrete headers stay unchanged;18
actual local data layouts and nine working expressions agree in both compilers.
All42 selected tests and132 frozen public traces pass. Three more stateless
headers join the production include graph. Preserve the dated inventory above
and refresh it at integration. The five additional comparable paths identified
in the detailed follow-up and full completion/hosted gates remain open.
