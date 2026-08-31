# LZSS contextual 64 MiB window candidate

Status: Format 2 dictionary/context identity reserved. No backend triple,
public profile, decoder admission, CLI name, or interoperability archive is
assigned by this document.

## Purpose

This document defines the next candidate typed-token LZSS Contextual resource
family after the completed 16-MiB profiles. The candidate raises the reference
frame, window, and maximum match distance to 67,108,864 bytes while retaining
minimum match length 5, maximum match length 258, greedy longest-match
selection, nearest-distance tie breaking, overlap copying, and per-frame
dictionary reset.

Sixty-four MiB is the next deliberate fourfold boundary. It extends distance
class 24 through class 26 and follows the existing 64-KiB, one-MiB, four-MiB,
and sixteen-MiB progression. It is also the largest common fourfold profile
whose conservative payload ceilings for all five current entropy backends fit
the existing 32-bit frame payload fields. A later 256-MiB profile would make
the existing Adaptive Huffman `267F/8` ceiling exceed `UINT32_MAX` and therefore
requires a separate format-width or payload-proof decision.

The larger window does not justify automatic limit expansion, strategy
selection, cross-frame history, or reinterpretation of an existing stream.
HashChain Exact and BinaryTree Exact remain encoder-local and byte-identical.

## Candidate decoder-visible identity

Format 2 reserves the next exact pair as:

```text
dictionary algorithm ID 2, dictionary variant 6
context-model algorithm ID 1, context variant 5
```

Variants 6 and 5 must occur together. Every crossed pair is contradictory and
must be rejected before allocation or publication. The reservation assigns
the pair's decoder-visible meaning but admits no complete entropy triple. Each
entropy backend retains its existing algorithm and variant identity and needs
an independent validator, payload, workspace, lifecycle, fuzz, and
interoperability proof before publication.

## Candidate dictionary and context layout

The dictionary parameter region is:

```text
minimum_match_length = 5
5 <= maximum_match_length <= 258
1 <= window_size <= 67,108,864 bytes
```

The reference profile uses a 67,108,864-byte frame and window. Because the
dictionary resets at each frame, the greatest reachable distance in that
reference frame is `67,108,864 - minimum_match_length`. A direct primitive
test may use a larger caller-supplied frame to prove the inclusive distance
ceiling; it does not imply cross-frame history.

Context IDs and field selection remain unchanged. Only the eight distance
alphabets grow from 25 to 27 symbols:

| Context IDs | Count | Alphabet | Meaning |
|---:|---:|---:|---|
| 0..2 | 3 | 2 | token kind |
| 3..19 | 17 | 256 | Literal value |
| 20..22 | 3 | 8 | match-length class |
| 23..30 | 8 | 27 | match-distance class |

The flattened model contains exactly 4,598 entries:

```text
3*2 + 17*256 + 3*8 + 8*27 = 4,598
```

For distance `D`, `distance_class = floor(log2(D))` and
`distance_extra = D - 2^distance_class`. Distance 67,108,864 uses class 26 and
26 zero bypass bits. One Match has at most three modeled symbols, seven
length-extra bits, and 26 distance-extra bits, or 36 decisions for at least
five raw bytes. The checked common bounds become:

```text
token_count <= F
event_count <= 2F
decision_count <= 8F
decision_count <= 36*token_count
```

The `8F` ceiling replaces `7F` only for the candidate pair. At
`F = 67,108,864`, `8F = 536,870,912` and `36F = 2,415,919,104`; both fit the
existing unsigned 32-bit count fields. Earlier layouts retain their frozen
counts, alphabets, and bytes.

## Backend format ceilings

The candidate context changes only model dimensions and the common decision
bound. Existing entropy grammars remain unchanged. The independently derived
64-MiB ceilings are:

| Backend | Model or descriptor bound | Payload ceiling | Complete-frame ceiling |
|---|---:|---:|---:|
| Dynamic Range | 4,598 model entries | `16F+5` = 1,073,741,829 | `16F+85` = 1,073,741,909 |
| rANS | 9,185 descriptor bytes; 126,976 decode entries | `16F+8` = 1,073,741,832 | `16F+9,257` = 1,073,751,081 |
| tANS | 9,189 descriptor bytes; 131,072 table entries | `12F+2` = 805,306,370 | `12F+9,255` = 805,315,623 |
| Blocked Huffman | 2,606 descriptor bytes; 35 decode tables | `15F` = 1,006,632,960 | `15F+2,670` = 1,006,635,630 |
| Adaptive Huffman | 9,227 nodes; 4,598 symbol indices | `267F/8` = 2,239,758,336 | `267F/8+80` = 2,239,758,416 |

Every ceiling remains below `UINT32_MAX`. These calculations reserve no
identity and must be repeated in checked code rather than copied as unchecked
allocation arithmetic.

## Supported 64-bit workspace proof

For a full frame, current supported object extents give 805,306,368 bytes of
typed tokens, 268,959,744 bytes of HashChain storage, and 1,946,157,056 bytes
of BinaryTree storage. The resulting aggregate requirements are:

| Backend | HashChain encode | BinaryTree encode | Decode |
|---|---:|---:|---:|
| Dynamic Range | 4,362,600,533 | 6,039,797,845 | 1,946,157,141 |
| rANS | 2,215,126,057 | 3,892,323,369 | 1,946,928,169 |
| tANS | 1,946,952,743 | 3,624,150,055 | 1,678,255,143 |
| Blocked Huffman | 2,148,010,606 | 3,825,207,918 | 1,879,194,082 |
| Adaptive Huffman | 3,381,290,224 | 5,058,487,536 | 3,112,330,476 |

The Adaptive Huffman encoder totals include the four-byte alignment before
the selected finder. Runtime queries remain authoritative and must use native
extents, canonical alignment, checked addition and multiplication, and the
selected finder calculator. Full-profile support therefore requires a 64-bit
`size_t`; unsupported layouts fail before publishing workspace requirements.

The proposed explicit profile policies are 8 GiB for Dynamic Range and
Adaptive Huffman and 4 GiB for rANS, tANS, and Blocked Huffman. Each policy
admits both public Exact strategies, while the workspace query reports only
the selected strategy's actual allocation. Applying the profile is the
caller's explicit authorization to raise these local hard limits; stream
metadata never raises them. Initializers, all older profile helpers, and the
library-wide 128-MiB default remain unchanged. A caller may tighten any
returned limit and re-query; exact aggregate succeeds and one byte short must
fail atomically.

## Evidence gate

Before reserving or implementing the candidate format pair, add a fixed,
checkpointed Silesia experiment with a 64-MiB frame and window, both public
Exact strategies, one timed iteration, and an explicit aggregate limit large
enough for BinaryTree. The Corpus remains external and verified before any
benchmark process starts. Exact token summaries and fingerprints must agree;
throughput and token reduction are descriptive evidence, not correctness
gates. The runner must support bounded batches because HashChain candidate
growth may make the experiment long-running.

The experiment cannot prove the inclusive 64-MiB distance because individual
Corpus members may be smaller. A bounded overlap-built primitive vector must
independently prove class 26, 26 bypass bits, maximum distance, and rejection
by every older context.

## Required validation before admission

- hand-check distance classes 24, 25, and 26 and their bypass bits;
- prove the 4,598-entry context layout and exact dictionary/context pairing;
- accept `8F` and `36T` at equality and reject one above without overflow;
- prove every descriptor, payload, complete-frame, and workspace ceiling at
  equality and one byte short on the supported 64-bit layout;
- reject full-profile requirements on layouts that cannot represent every
  returned region and aggregate;
- preserve HashChain/BinaryTree Exact token and completed-stream identity;
- reject all reciprocal crossings with the four published profile pairs
  before token or raw publication;
- retain all published archive bytes and schema-52 compatibility;
- keep fuzz input, output, and process counts fixed-memory when selecting the
  large identity; and
- append an interoperability archive only after one backend's complete public
  lifecycle is admitted.

## Staged work

1. Fix this shared design, provenance, and test contract.
2. Define and execute the bounded 64-MiB Silesia comparison.
3. Review the measured benefit and memory cost without treating performance as
   a correctness gate.
4. Reserve dictionary variant 6 and context variant 5 in `docs/format.md`.
5. Implement shared dictionary/context constants, checked layouts, validators,
   and hand vectors.
6. Admit Dynamic Range from private decoder validation through public C, CLI,
   benchmark, bounded fuzzing, and one new interoperability archive.
7. Admit rANS, tANS, Blocked Huffman, and Adaptive Huffman separately, each
   with its own memory proof and one schema append.

No stage may reinterpret an existing identity, infer limits from an untrusted
stream, select a match finder automatically, or claim completion from a
one-shot round trip.

Stages one through four completed on 2026-09-01. The fixed 48-point Silesia
matrix validated every Exact strategy pair and measured a 414,783-token
aggregate reduction, or 1.293%, when moving from a 16-MiB to a 64-MiB window
under the same 64-MiB frame. BinaryTree won seven of twelve members at each
window and was 8.053 times HashChain in aggregate throughput at 64 MiB, at the
cost of 1,946,157,056 bytes of finder workspace. The evidence is sufficient to
retain the candidate as an explicit high-memory profile. Format 2 now reserves
only the inseparable `2/6 + 1/5` pair; every backend triple and public surface
remains unavailable until its later stage. The evidence is not a basis for a
new default or automatic strategy selector.
