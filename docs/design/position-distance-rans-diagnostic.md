# Position-distance rANS diagnostic

Status: private experiment, not a public codec or MARC stream variant.

DD-1511 compares one immutable 1 MiB LZSS token sequence across entropy
backends. The primary sequence uses dictionary variant 9, minimum match 3,
maximum match 258. A separate minimum-match-5 sequence permits the existing
contextual rANS variant-2 model to encode exactly the same tokens. These two
sequences must never be presented as an isolated entropy comparison.

The experiment retains the position-distance field grammar: three kind
contexts, nine literal contexts, three length contexts, nine distance-class
contexts, and twenty binary distance-bit-position contexts. Length extras
remain uniform. Distance extras emit least-significant position first. Static
per-frame frequencies replace adaptive frequency updates; this is an explicit
experimental representation change, not compatibility with Dynamic Range.

Each context normalizes to 4096: floor proportional allocation with a minimum
of one for observed symbols, then add to the largest signed proportional error
(lower symbol wins ties), or subtract from the smallest error among frequencies
above one (higher symbol wins ties). Unobserved symbols remain zero. An unused
context has no record. Byte-renormalized scalar rANS uses a 64-bit state,
lower bound 2^31, reverse encoding, an eight-byte little-endian final state,
and reversed emitted renormalization bytes. Uniform bits are equiprobable
binary rANS decisions; they require no descriptor record.

Diagnostic PDRX serialization, all integers little-endian:

* magic `PDRX`, version u8 = 1;
* raw size, token count, decision count, model size, payload size: five u32;
* SHA-256 of reconstructed raw bytes: 32 bytes;
* model: active-context mask u64, then ascending active-context records;
* each record: nonzero-symbol count u16, ascending (symbol u8, frequency u16);
* payload: final state u64 followed by renormalization bytes.

The header is 57 bytes; the sparse-only model is at most 7794 bytes. This
simple descriptor is deliberately not claimed to be the production compact
descriptor. Compare payload and descriptor separately; PDRX container totals
and production MARC archive totals have different overheads.

Before allocation or decode, require raw size <= 1048576, token count <= raw
size, decisions <= 32 * raw size, model <= 7794, payload <= 8 + 2 * decisions,
and exact total extent. Validate mask, symbol ordering, nonzero frequencies,
and each active sum of 4096. Decode requests come from the field grammar,
never from serialized context selectors. Validate lengths, history, output
extent, exact decisions, exhausted payload and terminal state. Rebuild the
model from decoded decisions and require canonical re-encoding and digest
agreement. Decode returns raw bytes only after every check; failure publishes
no prefix. The transactional wrapper changes a caller-owned bytearray only
after complete successful decode or encode. This is a finite-buffer diagnostic,
not proof of the production partial-buffer lifecycle or allocator behavior.

Work is bounded by a single frame. Python object overhead and diagnostic
decision materialization are not production codec memory estimates. Report
actual process peaks separately if measured. No public API, algorithm ID,
CLI selector, baseline matrix or exchange inventory changes in this phase.

The native token exporter emits `PDOP`, then per frame six u32 fields: raw
size, token count, adaptive range payload size, existing contextual rANS
descriptor size, its payload size, and minimum match. Records are followed by
raw bytes and tokens (kind u8, literal u8, distance u32, length u32). Sizes for
the contextual control are zero when minimum match is three. This is trusted
local diagnostic input, bounded and checked by the Python reader. Empty source
has no frames; isolated empty PDRX has zero counts, empty mask and state 2^31.

Build the exporter with `MARC_BUILD_ENTROPY_DIAGNOSTICS=ON` and static library
enabled; the target is `marc_position_distance_rans_export`. Invoke it with a
bounded input path, a new export path, and minimum match `3` or `5`. Decode and
compare an export using `python -B tools/position_distance_rans_diagnostic.py
EXPORT`. The default build does not build the exporter. The Python regression
is registered as `marc_position_distance_rans_diagnostic` when an interpreter
is available. Native baseline entropy block limits count decisions rather
than raw bytes; the exporter uses a local 32 Mi-decision ceiling and 256 MiB
aggregate ceiling without changing public defaults.


## DD-1512 compact model extension

The comparison baseline is contextual rANS with the same minimum-match-five
tokens. Dynamic Range compression does not gate this extension. Future native
throughput and directional peak memory must be measured separately; neither
a Python timer nor a table entry count proves an algorithmic advantage.

PDRX version 2 retains the 57-byte header, normalization and payload layout.
Its model begins with a six-byte little-endian active-context mask; unused
high four bits must be zero. Ascending active contexts have one mode byte:

* Mode 0: one symbol u8, its frequency implicitly 4096.
* Mode 1: u16 frequencies for alphabet symbols except the last; last frequency
  is 4096 minus the preceding sum. Zeros are allowed.
* Mode 2: nonzero count u16, then ascending observed symbols. All but the last
  symbol have (symbol u8, frequency u16); last has only symbol u8 with frequency
  inferred as 4096 minus the preceding sum.

A single observed symbol always selects mode 0. Otherwise mode 1 costs
1 + 2*(alphabet-1), mode 2 costs 1 + 3*nonzero_count; choose smaller, ties
choose mode 1. Active models must sum to 4096. Sparse frequencies are positive.
The parser reconstructs models privately, then reserializes and requires exact
canonical equality, including mode choice. Empty model is a zero mask. Maximum
model extent is 5094 bytes; all old raw/token/decision/payload bounds apply.
The final canonical container check uses the received explicit version.

`encode` defaults to version 1 to preserve previous diagnostic bytes. Select
version 2 explicitly through its argument or CLI `--model-version 2`. Decode
accepts both explicit versions. This diagnostic is still finite and private;
no statement of streaming/public completion follows from model compaction.


For same-token component comparisons, charge the same sixteen bytes of
non-model entropy metadata that the existing contextual rANS descriptor
contains. Materialize a derived diagnostic descriptor with prefix
(decisions u32, payload size u32, table log u8=12, flags u8=0, context count
u16=44, frequency entries u32=2566), then the PDRX model. This prefix is not
added to PDRX bytes, whose header already carries sizes. Report its derived
descriptor plus payload separately from both raw model bytes and container
bytes. It remains a private measurement envelope, not public stream admission.
