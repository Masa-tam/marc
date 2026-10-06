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
