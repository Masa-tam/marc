# v1.0 readiness: current inventory and evidence

Status: initial audited inventory, 2026-11-02. Definition snapshot: `7421ccec`.
This is stage2.1 of the [stabilization plan](design/v1-stabilization-stage-2.md),
not a new release-readiness declaration. Compatibility proposals are separate in
[the draft contract](v1-compatibility.md). Current0.x behavior and labels remain.

## Evidence scope

- The102 names and order below match the frozen v0.8.0/schema92 manifest and
  the current CLI/common benchmark source inventories. No implicit alias or
  historical retired selector is counted as an extra public profile.
- Dictionary/context/entropy IDs are read from those independently preserved
  archives and checked against the public bindings and format definition.
  Format1 has no context ID field; a dash does not allocate a new ID.
- Fresh read-only initializer/query inspection of the existing MSVC DLL returns
  the same224 configuration/query records,111 ABI layouts and public-header
  hash as the frozen baseline. No new codec execution or complete CI is claimed.
- `fa925da4` is the independently confirmed complete implementation candidate:
  [run41131424716](https://github.com/Masa-tam/marc/actions/runs/41131424716),
  attempt1, both platform complete suites and four package configurations.
  IX-0121/TVG-1678 record both102-stream bundles identical to v0.8.0 and their
  four Windows consumer routes. A Windows Clang client uses the MSVC DLL.
- The maintainer reports successful push/CI through `7421ccec`; the report does
  not name a run or complete scope. No implementation changed between the
  qualified candidate and this inventory's definition snapshot.

## Reading the inventory

`C binding` omits the common `marc_` prefix. Its configuration type is
`marc_<binding>_config`; initialization is `marc_<binding>_config_init`.
An appended `p=N` requires `marc_<binding>_config_apply_profile`: for contextual
profiles N=0..4 means64KiB/1/4/16/64MiB, and for native Adaptive Huffman N=0..6
means64KiB/1/4/8/16/32/64MiB. The unsuffixed CLI name denotes64KiB.

- **W**: caller-owned encoder/decoder workspace. Use `_workspace_requirements`
  and `_create`; retain the exact required regions through destruction. Handles
  remain library-owned. Region shapes/alignment are query-specific.
- **O**: internally owned encoder/decoder storage. Use `_resource_requirements`
  and `_create`; supplied I/O/retention capacities and checked aggregate policy
  still matter. Logical grants are not process resident-memory ceilings.
- **R**: internally owned Range encoder via `_resource_requirements` and
  `_create_encoder`; separate borrowed decoder via `_decoder_config_init`,
  `_decoder_workspace_requirements`, `_create_decoder` and its own
  `marc_<binding>_decoder_config`. Pass the five regions through the size/ABI-
  tagged `marc_<binding>_decoder_buffers` structure, not five positional arguments.
  Decoder workspace consists of five queried
  regions;16MiB typed records differ from32/64MiB byte-record storage. Query
  the particular profile rather than deriving a layout from another window.

`Init frame` is the encoder C initializer/profile-helper frame in bytes, not
the maximum frame admitted by every decoder, a memory grant, or a claim that
CLI overrides are identical. `Q` is the bare empty-encoder query status:
`OK` succeeds with original_size=0; `CONFIG` is the observed
`MARC_STATUS_INVALID_ARGUMENT` (100) for the four split Range initializers,
which require caller-supplied policy fields before admission. Neither means an actual default-size allocation or
round-trip was performed by this inspection.

**B** is the42-profile baseline. **C/P** are additional contextual/native
Position-distance Format2 profiles. Existing experimental/outside-baseline
descriptions are preserved; their1.0 stability classification is pending a
recorded contract decision. Every row inherits the evidence limitations below.
There are70 W rows,28 O rows and four R rows; these count API ownership shapes,
not separate implementations or verified allocation campaigns.

## Exact102-profile inventory

| # | CLI selector | C binding / profile | Format; D / C / E | Init frame | Storage; Q | Class |
| ---: | --- | --- | --- | ---: | --- | --- |
| 1 | `checksum-raw` | `checksum_raw` | 1.1; 0/0 / - / 0/0 | 1048576 | W; OK | B |
| 2 | `lz77` | `lz77` | 1.0; 1/1 / - / 0/0 | 1048576 | W; OK | B |
| 3 | `lz77-blocked-huffman` | `lz77_blocked_huffman` | 1.0; 1/1 / - / 2/1 | 1048576 | W; OK | B |
| 4 | `lzss` | `lzss` | 1.0; 2/1 / - / 0/0 | 1048576 | W; OK | B |
| 5 | `lz78` | `lz78` | 1.0; 3/1 / - / 0/0 | 1048576 | W; OK | B |
| 6 | `lzw` | `lzw` | 1.0; 4/1 / - / 0/0 | 1048576 | W; OK | B |
| 7 | `lzd` | `lzd` | 1.0; 5/1 / - / 0/0 | 1048576 | W; OK | B |
| 8 | `lzmw` | `lzmw` | 1.0; 6/1 / - / 0/0 | 1048576 | W; OK | B |
| 9 | `blocked-huffman` | `blocked_huffman` | 1.0; 0/0 / - / 2/1 | 1048576 | W; OK | B |
| 10 | `adaptive-huffman` | `adaptive_huffman` | 1.0; 0/0 / - / 1/1 | 1048576 | W; OK | B |
| 11 | `dynamic-range` | `dynamic_range` | 1.0; 0/0 / - / 3/1 | 1048576 | W; OK | B |
| 12 | `rans` | `rans` | 1.0; 0/0 / - / 4/1 | 1048576 | W; OK | B |
| 13 | `tans` | `tans` | 1.0; 0/0 / - / 5/1 | 1048576 | W; OK | B |
| 14 | `lzss-blocked-huffman` | `lzss_blocked_huffman` | 1.0; 2/1 / - / 2/1 | 1048576 | W; OK | B |
| 15 | `lz78-blocked-huffman` | `lz78_blocked_huffman` | 1.0; 3/1 / - / 2/1 | 1048576 | W; OK | B |
| 16 | `lzw-blocked-huffman` | `lzw_blocked_huffman` | 1.0; 4/1 / - / 2/1 | 1048576 | W; OK | B |
| 17 | `lzd-blocked-huffman` | `lzd_blocked_huffman` | 1.0; 5/1 / - / 2/1 | 1048576 | W; OK | B |
| 18 | `lzmw-blocked-huffman` | `lzmw_blocked_huffman` | 1.0; 6/1 / - / 2/1 | 1048576 | W; OK | B |
| 19 | `lz77-adaptive-huffman` | `lz77_adaptive_huffman` | 1.0; 1/1 / - / 1/1 | 65536 | W; OK | B |
| 20 | `lzss-adaptive-huffman` | `lzss_adaptive_huffman` | 1.0; 2/1 / - / 1/1 | 65536 | W; OK | B |
| 21 | `lz78-adaptive-huffman` | `lz78_adaptive_huffman` | 1.0; 3/1 / - / 1/1 | 65536 | W; OK | B |
| 22 | `lzw-adaptive-huffman` | `lzw_adaptive_huffman` | 1.0; 4/1 / - / 1/1 | 65536 | W; OK | B |
| 23 | `lzd-adaptive-huffman` | `lzd_adaptive_huffman` | 1.0; 5/1 / - / 1/1 | 65536 | W; OK | B |
| 24 | `lzmw-adaptive-huffman` | `lzmw_adaptive_huffman` | 1.0; 6/1 / - / 1/1 | 65536 | W; OK | B |
| 25 | `lz77-dynamic-range` | `lz77_dynamic_range` | 1.0; 1/1 / - / 3/1 | 65536 | W; OK | B |
| 26 | `lzss-dynamic-range` | `lzss_dynamic_range` | 1.0; 2/1 / - / 3/1 | 65536 | W; OK | B |
| 27 | `lz78-dynamic-range` | `lz78_dynamic_range` | 1.0; 3/1 / - / 3/1 | 65536 | W; OK | B |
| 28 | `lzw-dynamic-range` | `lzw_dynamic_range` | 1.0; 4/1 / - / 3/1 | 65536 | W; OK | B |
| 29 | `lzd-dynamic-range` | `lzd_dynamic_range` | 1.0; 5/1 / - / 3/1 | 65536 | W; OK | B |
| 30 | `lzmw-dynamic-range` | `lzmw_dynamic_range` | 1.0; 6/1 / - / 3/1 | 65536 | W; OK | B |
| 31 | `lz77-rans` | `lz77_rans` | 1.0; 1/1 / - / 4/1 | 65536 | W; OK | B |
| 32 | `lzss-rans` | `lzss_rans` | 1.0; 2/1 / - / 4/1 | 65536 | W; OK | B |
| 33 | `lz78-rans` | `lz78_rans` | 1.0; 3/1 / - / 4/1 | 65536 | W; OK | B |
| 34 | `lzw-rans` | `lzw_rans` | 1.0; 4/1 / - / 4/1 | 65536 | W; OK | B |
| 35 | `lzd-rans` | `lzd_rans` | 1.0; 5/1 / - / 4/1 | 65536 | W; OK | B |
| 36 | `lzmw-rans` | `lzmw_rans` | 1.0; 6/1 / - / 4/1 | 65536 | W; OK | B |
| 37 | `lz77-tans` | `lz77_tans` | 1.0; 1/1 / - / 5/1 | 65536 | W; OK | B |
| 38 | `lzss-tans` | `lzss_tans` | 1.0; 2/1 / - / 5/1 | 65536 | W; OK | B |
| 39 | `lz78-tans` | `lz78_tans` | 1.0; 3/1 / - / 5/1 | 65536 | W; OK | B |
| 40 | `lzw-tans` | `lzw_tans` | 1.0; 4/1 / - / 5/1 | 65536 | W; OK | B |
| 41 | `lzd-tans` | `lzd_tans` | 1.0; 5/1 / - / 5/1 | 65536 | W; OK | B |
| 42 | `lzmw-tans` | `lzmw_tans` | 1.0; 6/1 / - / 5/1 | 65536 | W; OK | B |
| 43 | `lzss-contextual-dynamic-range` | `lzss_contextual_dynamic_range p=0` | 2.0; 2/2 / 1/1 / 3/2 | 65536 | W; OK | C |
| 44 | `lzss-contextual-rans` | `lzss_contextual_rans p=0` | 2.0; 2/2 / 1/1 / 4/3 | 65536 | W; OK | C |
| 45 | `lzss-contextual-tans` | `lzss_contextual_tans p=0` | 2.0; 2/2 / 1/1 / 5/2 | 65536 | W; OK | C |
| 46 | `lzss-contextual-blocked-huffman` | `lzss_contextual_blocked_huffman p=0` | 2.0; 2/2 / 1/1 / 2/2 | 65536 | W; OK | C |
| 47 | `lzss-contextual-adaptive-huffman` | `lzss_contextual_adaptive_huffman p=0` | 2.0; 2/2 / 1/1 / 1/2 | 65536 | W; OK | C |
| 48 | `lzss-contextual-dynamic-range-1m` | `lzss_contextual_dynamic_range p=1` | 2.0; 2/3 / 1/2 / 3/2 | 1048576 | W; OK | C |
| 49 | `lzss-contextual-rans-1m` | `lzss_contextual_rans p=1` | 2.0; 2/3 / 1/2 / 4/3 | 1048576 | W; OK | C |
| 50 | `lzss-contextual-tans-1m` | `lzss_contextual_tans p=1` | 2.0; 2/3 / 1/2 / 5/2 | 1048576 | W; OK | C |
| 51 | `lzss-contextual-blocked-huffman-1m` | `lzss_contextual_blocked_huffman p=1` | 2.0; 2/3 / 1/2 / 2/2 | 1048576 | W; OK | C |
| 52 | `lzss-contextual-adaptive-huffman-1m` | `lzss_contextual_adaptive_huffman p=1` | 2.0; 2/3 / 1/2 / 1/2 | 1048576 | W; OK | C |
| 53 | `lzss-contextual-dynamic-range-4m` | `lzss_contextual_dynamic_range p=2` | 2.0; 2/4 / 1/3 / 3/2 | 4194304 | W; OK | C |
| 54 | `lzss-contextual-rans-4m` | `lzss_contextual_rans p=2` | 2.0; 2/4 / 1/3 / 4/3 | 4194304 | W; OK | C |
| 55 | `lzss-contextual-tans-4m` | `lzss_contextual_tans p=2` | 2.0; 2/4 / 1/3 / 5/2 | 4194304 | W; OK | C |
| 56 | `lzss-contextual-blocked-huffman-4m` | `lzss_contextual_blocked_huffman p=2` | 2.0; 2/4 / 1/3 / 2/2 | 4194304 | W; OK | C |
| 57 | `lzss-contextual-adaptive-huffman-4m` | `lzss_contextual_adaptive_huffman p=2` | 2.0; 2/4 / 1/3 / 1/2 | 4194304 | W; OK | C |
| 58 | `lzss-contextual-dynamic-range-16m` | `lzss_contextual_dynamic_range p=3` | 2.0; 2/5 / 1/4 / 3/2 | 16777216 | W; OK | C |
| 59 | `lzss-contextual-rans-16m` | `lzss_contextual_rans p=3` | 2.0; 2/5 / 1/4 / 4/3 | 16777216 | W; OK | C |
| 60 | `lzss-contextual-tans-16m` | `lzss_contextual_tans p=3` | 2.0; 2/5 / 1/4 / 5/2 | 16777216 | W; OK | C |
| 61 | `lzss-contextual-blocked-huffman-16m` | `lzss_contextual_blocked_huffman p=3` | 2.0; 2/5 / 1/4 / 2/2 | 16777216 | W; OK | C |
| 62 | `lzss-contextual-adaptive-huffman-16m` | `lzss_contextual_adaptive_huffman p=3` | 2.0; 2/5 / 1/4 / 1/2 | 16777216 | W; OK | C |
| 63 | `lzss-contextual-dynamic-range-64m` | `lzss_contextual_dynamic_range p=4` | 2.0; 2/6 / 1/5 / 3/2 | 67108864 | W; OK | C |
| 64 | `lzss-contextual-rans-64m` | `lzss_contextual_rans p=4` | 2.0; 2/6 / 1/5 / 4/3 | 67108864 | W; OK | C |
| 65 | `lzss-contextual-tans-64m` | `lzss_contextual_tans p=4` | 2.0; 2/6 / 1/5 / 5/2 | 67108864 | W; OK | C |
| 66 | `lzss-contextual-blocked-huffman-64m` | `lzss_contextual_blocked_huffman p=4` | 2.0; 2/6 / 1/5 / 2/2 | 67108864 | W; OK | C |
| 67 | `lzss-contextual-adaptive-huffman-64m` | `lzss_contextual_adaptive_huffman p=4` | 2.0; 2/6 / 1/5 / 1/2 | 67108864 | W; OK | C |
| 68 | `lzss-position-distance-dynamic-range` | `lzss_position_distance_dynamic_range` | 2.0; 2/8 / 1/9 / 3/2 | 65536 | W; OK | P |
| 69 | `lzss-position-distance-dynamic-range-1m` | `lzss_position_distance_dynamic_range_1m` | 2.0; 2/9 / 1/10 / 3/2 | 1048576 | W; OK | P |
| 70 | `lzss-position-distance-dynamic-range-4m` | `lzss_position_distance_dynamic_range_4m` | 2.0; 2/10 / 1/11 / 3/2 | 4194304 | W; OK | P |
| 71 | `lzss-position-distance-dynamic-range-8m` | `lzss_position_distance_dynamic_range_8m` | 2.0; 2/11 / 1/12 / 3/2 | 8388608 | R; CONFIG | P |
| 72 | `lzss-position-distance-dynamic-range-16m` | `lzss_position_distance_dynamic_range_16m` | 2.0; 2/12 / 1/13 / 3/2 | 16777216 | R; CONFIG | P |
| 73 | `lzss-position-distance-dynamic-range-32m` | `lzss_position_distance_dynamic_range_32m` | 2.0; 2/13 / 1/14 / 3/2 | 33554432 | R; CONFIG | P |
| 74 | `lzss-position-distance-dynamic-range-64m` | `lzss_position_distance_dynamic_range_64m` | 2.0; 2/14 / 1/15 / 3/2 | 67108864 | R; CONFIG | P |
| 75 | `lzss-position-distance-rans-1m` | `lzss_position_rans_1m` | 2.0; 2/9 / 1/10 / 4/4 | 1048576 | O; OK | P |
| 76 | `lzss-position-distance-rans` | `lzss_position_distance_rans` | 2.0; 2/8 / 1/9 / 4/5 | 65536 | O; OK | P |
| 77 | `lzss-position-distance-rans-4m` | `lzss_position_distance_rans_4m` | 2.0; 2/10 / 1/16 / 4/7 | 4194304 | O; OK | P |
| 78 | `lzss-position-distance-rans-8m` | `lzss_position_distance_rans_8m` | 2.0; 2/11 / 1/17 / 4/8 | 8388608 | O; OK | P |
| 79 | `lzss-position-distance-rans-16m` | `lzss_position_distance_rans_16m` | 2.0; 2/12 / 1/18 / 4/9 | 16777216 | O; OK | P |
| 80 | `lzss-position-distance-rans-32m` | `lzss_position_distance_rans_32m` | 2.0; 2/13 / 1/19 / 4/10 | 33554432 | O; OK | P |
| 81 | `lzss-position-distance-rans-64m` | `lzss_position_distance_rans_64m` | 2.0; 2/14 / 1/20 / 4/11 | 67108864 | O; OK | P |
| 82 | `lzss-position-distance-tans` | `lzss_position_distance_tans` | 2.0; 2/8 / 1/21 / 5/3 | 65536 | O; OK | P |
| 83 | `lzss-position-distance-tans-1m` | `lzss_position_distance_tans_1m` | 2.0; 2/9 / 1/22 / 5/4 | 1048576 | O; OK | P |
| 84 | `lzss-position-distance-tans-4m` | `lzss_position_distance_tans_4m` | 2.0; 2/10 / 1/23 / 5/5 | 4194304 | O; OK | P |
| 85 | `lzss-position-distance-tans-8m` | `lzss_position_distance_tans_8m` | 2.0; 2/11 / 1/24 / 5/6 | 8388608 | O; OK | P |
| 86 | `lzss-position-distance-tans-16m` | `lzss_position_distance_tans_16m` | 2.0; 2/12 / 1/25 / 5/7 | 16777216 | O; OK | P |
| 87 | `lzss-position-distance-tans-32m` | `lzss_position_distance_tans_32m` | 2.0; 2/13 / 1/26 / 5/8 | 33554432 | O; OK | P |
| 88 | `lzss-position-distance-tans-64m` | `lzss_position_distance_tans_64m` | 2.0; 2/14 / 1/26 / 5/9 | 67108864 | O; OK | P |
| 89 | `lzss-position-distance-blocked-huffman` | `lzss_position_distance_blocked_huffman` | 2.0; 2/8 / 1/28 / 2/3 | 65536 | O; OK | P |
| 90 | `lzss-position-distance-blocked-huffman-1m` | `lzss_position_distance_blocked_huffman_1m` | 2.0; 2/9 / 1/29 / 2/4 | 1048576 | O; OK | P |
| 91 | `lzss-position-distance-blocked-huffman-4m` | `lzss_position_distance_blocked_huffman_4m` | 2.0; 2/10 / 1/30 / 2/5 | 4194304 | O; OK | P |
| 92 | `lzss-position-distance-blocked-huffman-8m` | `lzss_position_distance_blocked_huffman_8m` | 2.0; 2/11 / 1/31 / 2/6 | 8388608 | O; OK | P |
| 93 | `lzss-position-distance-blocked-huffman-16m` | `lzss_position_distance_blocked_huffman_16m` | 2.0; 2/12 / 1/32 / 2/7 | 16777216 | O; OK | P |
| 94 | `lzss-position-distance-blocked-huffman-32m` | `lzss_position_distance_blocked_huffman_32m` | 2.0; 2/13 / 1/33 / 2/8 | 33554432 | O; OK | P |
| 95 | `lzss-position-distance-blocked-huffman-64m` | `lzss_position_distance_blocked_huffman_64m` | 2.0; 2/14 / 1/34 / 2/9 | 67108864 | O; OK | P |
| 96 | `lzss-position-distance-adaptive-huffman` | `lzss_position_distance_adaptive_huffman p=0` | 2.0; 2/8 / 1/35 / 1/3 | 65536 | O; OK | P |
| 97 | `lzss-position-distance-adaptive-huffman-1m` | `lzss_position_distance_adaptive_huffman p=1` | 2.0; 2/9 / 1/36 / 1/4 | 1048576 | O; OK | P |
| 98 | `lzss-position-distance-adaptive-huffman-4m` | `lzss_position_distance_adaptive_huffman p=2` | 2.0; 2/10 / 1/37 / 1/5 | 4194304 | O; OK | P |
| 99 | `lzss-position-distance-adaptive-huffman-8m` | `lzss_position_distance_adaptive_huffman p=3` | 2.0; 2/11 / 1/38 / 1/6 | 8388608 | O; OK | P |
| 100 | `lzss-position-distance-adaptive-huffman-16m` | `lzss_position_distance_adaptive_huffman p=4` | 2.0; 2/12 / 1/39 / 1/7 | 16777216 | O; OK | P |
| 101 | `lzss-position-distance-adaptive-huffman-32m` | `lzss_position_distance_adaptive_huffman p=5` | 2.0; 2/13 / 1/40 / 1/8 | 33554432 | O; OK | P |
| 102 | `lzss-position-distance-adaptive-huffman-64m` | `lzss_position_distance_adaptive_huffman p=6` | 2.0; 2/14 / 1/41 / 1/9 | 67108864 | O; OK | P |

## Contracts and evidence by group

The definition sources are [marc.h](../include/marc/marc.h),
[format](format.md), [C API](c-api.md), [CLI](cli.md) and the source bindings in
`benchmarks/support/position_profiles.hpp` and `benchmarks/marc_benchmark.cpp`.
The matrix does not replace these specifications. Read current family sections
with the later completion records; old “pending” text is not always current.

| Group | Existing behavior/evidence | Evidence boundary or follow-up |
| --- | --- | --- |
| B:42 baseline | Existing public completion/one-byte/malformed/HashTap tests and format validators; baseline matrix in baseline-readiness. Caller-owned bounded storage. | Known-size encoding and committed-count process rules apply. Per-profile flag/default and decoder-limit summaries still need reconciliation; do not infer an identical flag policy from this count. |
| C:25 contextual | Five backend families x five explicit resource profiles; shared C profile helpers and borrowed workspaces. Independent context/model/vector and public conformance tests. | Every selector fixes its exact tuple. Historical rANS variant2 is retired/reserved, not a required extra decoder. Review each helper/default and frozen historical restore-only policy. |
| P Range:7 | Three borrowed paths and four split owning-encoder/borrowed-decoder paths; large-window core/frame/owner/public/CLI conformance. Stage-one TVG-1674..1676 cover shared16/32/64MiB mechanics. | Empty initializer success is not general admission. Typed versus packed record limits and caller IO/retention accounting stay explicit. |
| P rANS:7 | Owned public profiles, including the distinctly named1MiB C family and full-literal4..64MiB variants. Stage-one TVG-1671..1672 cover owner/frame/stream preservation. | Older4MiB non-full-literal support remains an independent test reference, not an eighth public row. |
| P tANS:7 | Owned profiles with independent table/transition/vector/public tests. Stage-one TVG-1670 covers seven-window consolidation. | ANS-specific state/table/admission rules remain distinct from Range/Huffman. |
| P Blocked Huffman:7 | Owned profiles; bounded canonical primitives, independent frame/vector/ownership tests. TVG-1664..1668 and stage-one complete gate. | Pre0.8 producer revisions have explicit restoration/re-encoding distinctions; preserve those historical rules. |
| P Adaptive Huffman:7 | One owned public C family with seven helper-selected profiles. Native FGK/field-history/byte-vector/public tests; IX-0119..0121. | Format2 native Adaptive variants3..9 are distinct from baseline FGK and contextual variant2. |

### Runtime and safety facts still being normalized

Encoding requires a known original size, direction is fixed at construction,
and codecs are bounded by caller limits. Input consumption and output production
are independent. Only committed bytes belong to a hash boundary. Frame-based
decoding withholds a failed frame, while earlier committed frames are not rolled
back; the CLI has its separate whole-file transaction. Destruction and input/
output/workspace lifetimes remain part of the C contract.

Native-family guides document non-terminal Flush, unsupported ResetBlock and
sticky completion/errors; this inventory does not automatically generalize them
to every baseline call path. Finish the per-family flag/decoder-default review,
including EndInput suffix repetition and allocation-failure/alias behavior,
before closing the second2.1 checklist item. No flag or validation order is changed.

### Sanitizer evidence

The retained125 harness cpp files cover public and internal/reference boundaries;
125 is not the number of independent public codecs or a count of campaigns run
now. Baseline and contextual stream harnesses are described in [fuzzing](fuzzing.md).
Native sources include `lzss_position_distance_*`, `lzss_position_rans_*`,
`lzss_position_tans_*`, `lzss_position_blocked_huffman_*` and
`lzss_position_adaptive_huffman_stream_fuzz.cpp`; several cover multiple windows.
FZ-0094..0099 and later family readiness/implementation records include directed
large-frame and bounded campaign evidence. The native Adaptive campaign is
recorded separately from its initial deferral. Dedicated hosted fuzz jobs remain
absent. Stage2 must still bind exact campaign SHA/instrumentation/bounds and
coverage to every row; stage3 decides justified repeats. Source availability
and a successful complete non-sanitizer CI are not a new ASan/UBSan campaign.

### Measurements and provenance

The common benchmark supports every listed selector. BM-0275 qualifies its
102-profile driver and a checksum-raw Silesia integration, not a repeated
representative measurement of every codec. Older family measurements remain
valid at their recorded revisions/settings (including BM-0250..0264 native
Huffman/pool studies and BM-0266..0273 native Adaptive comparisons).
Keep archive ratio, directional timings, logical grants and physical peaks
distinct. Exact per-row measurement/campaign SHA binding and the representative
matrix are open inventory work, not an assertion that historical results vanished.
The [source-role audit](design/v1-refactoring-stage-1-audit.md) retains independent
oracles and retired-tool classifications. This inventory imports no external code.

## Remaining decisions and confirmed gaps

| ID | Kind | Current disposition / next evidence |
| --- | --- | --- |
| R1 | Confirmed documentation inconsistency | README/C API/readiness summaries reflect different older inventories or ownership scopes. Reconcile in2.3; history stays intact. |
| R2 | Policy decision | All60 additional Format2 profiles need an explicit1.0 stability/support classification; keep current labels meanwhile. |
| R3 | Policy decision |1.x ABI, struct evolution, byte/default compatibility and package/SONAME promises need adoption; draft decisions are not executable changes. |
| R4 | Evidence gap | Non-x86-64 release evidence is not established here. Existing x86-64 CI and a Windows Clang client do not close it. Assess or limit support explicitly. |
| R5 | Unfinished inventory | Bind historical sanitizer/measurement receipts to exact profile scope and SHA; avoid blanket “all current” or “all missing” labels. |
| R6 | Unfinished contract review | Complete flags, decoder defaults/limits and lifetime/publication comparison for all families before checking2.1 ownership/behavior complete. |
| R7 | New release gate | A future1.0 tag needs complete validation/package/exchange at its own exact candidate after final version/changelog edits. Stage-one success is not that release. |
| R8 | Closed historical work | Stage-one removal/commonization and fa925da4 hosted/exchange gate are complete. Old local pending statements do not reopen those tasks. |
