# v1.0 compatibility contract — draft

Status: proposal for review, 2026-11-02. This draft does not change the current
0.8.0 API, format definitions, experimental labels or installed package rules.
The [readiness inventory](v1-readiness.md) records current facts; the
[stage-two plan](design/v1-stabilization-stage-2.md) tracks decisions still open.

## Proposed decisions

| Decision | Recommended policy | What remains before adoption |
| --- | --- | --- |
| D1: public scope | Preserve all102 published selectors and factories. Keep the42-profile baseline distinction; complete the60 additional Format2 profiles' stability/support classification before publishing1.0 promises. | Resolve experimental descriptions per family against the inventory. No automatic promotion or removal is made by this document. |
| D2: decoder compatibility | Retain documented admitted stream identities and legacy compatibility behavior throughout1.x. Unknown IDs, versions, contradictory parameters and malformed streams continue to be rejected. | Enumerate the historical restore-only exceptions and retired/reserved variants; preserving old streams does not mean admitting every historical diagnostic. |
| D3: producer determinism | For an existing selector, exact configuration and explicit boundary operations, retain canonical deterministic output and the frozen v0.8.0 corpus. Optimizations preserve bytes; a different decoder-visible representation needs a distinct documented variant/profile. | Specify the1.x default-change/deprecation rules alongside per-family flag semantics. The limited corpus is a regression gate, not proof for every possible input. |
| D4: C ABI | Keep ABI1 symbols, signatures, public struct layouts, status/error meanings and opaque-handle lifetime. Prefer additive names/types for new capabilities over changing existing layouts in place. | Review exact struct-size checks, reserved fields and future extensibility policy before promising binary compatibility. No header is changed now. |
| D5: resource contract | Preserve checked admission, no unsafe pre-validation allocation, and exact query/create agreement for the selected configuration and ABI. Callers query each configuration and own the declared retained storage/I/O correctly. | Separate stable semantics from variable private allocation/layout costs and alignments; do not promise that every resource number remains forever constant. |
| D6: supported environment | Initially guarantee the release-qualified Windows/MSVC and Ubuntu/Ninja x86-64 configurations, C ABI use and shared/static installed consumption. | Publish exact final toolchain versions/minimums and assess non-x86-64/other compiler support explicitly. Existing source portability is not a successful ARM64 test. |
| D7: package compatibility | For1.x, recommend an ABI-consistent major-version CMake compatibility policy and an explicit shared-library ABI-major policy; retain `marc::shared` / `marc::static`. | Evaluate `SameMajorVersion` and ELF VERSION/SOVERSION against ABI1 and installed-consumer tests. Current `SameMinorVersion` and unversioned shared target remain unchanged. |
| D8: distribution | Keep a source-oriented1.0 release initially, with clear fresh-clone/source-archive instructions and a separately pinned test dependency. | Verify source archive contents, notices and library builds without GoogleTest; signing/prebuilt packages are separate possible commitments. |

These are recommendations, not eight accepted release decisions. Adoption must
record the actual scope and unresolved limitations in the numbered decision
ledger, after the supporting reviews. A compatibility-breaking change needs a
concrete separate scope decision; a1.0 planning task does not authorize one.

## API and streaming details to freeze

### Struct size, reserved fields and feature evolution

Current common validation in `src/c_api/configuration.hpp` requires exact
`struct_size == sizeof(Config)` and ABI1; individual factories also validate
reserved fields and policy. A size-tagged struct is therefore not already a
promise that old functions accept larger future structs. Preserve old call
behavior and layouts. Recommend new opt-in types/functions or an explicitly
specified versioned contract for any evolution that cannot fit the current API.
Changing size validation silently would change admission and error priority.

Future additive APIs must not consume a reserved bit/field without documenting
its meaning, accepted versions and failure behavior. Package/project version
increments do not alter the interpretation of existing fields or IDs.

### Direction, counts, flags and terminal state

Keep encode/decode direction immutable. Report committed input and output
counts separately; zero output alone is not end-of-stream. Retain independent
one-byte/chunking vectors, forward-progress checks and bounded call guards.
Document Flush, ResetBlock, EndInput repetition and terminal behavior per
profile rather than giving every profile an invented shared capability.

The proposed guarantee preserves each transform's documented sticky ended/error
state. Distinguish that from C wrapper argument rejection before dispatch, which
may leave an existing transform untouched. Do not turn all INVALID_ARGUMENT
returns into permanent codec failure or change error-order/position semantics
while editing prose.

### Ownership, admission and publication

Borrowed workspace must outlive the transform; owned paths still account for
the specified external I/O, retention and controls. Query using the current
configuration and enforce the reported alignment/extent. If configuration or
policy changes, query again before creating. Old hard-coded private sizeof
values are not a portable public resource contract.

Library logical admission is distinct from physical resident memory. Preserve
checked arithmetic, expansion limits, metadata/alias validation and failed-frame
withholding. Previously committed frames remain committed. The CLI's whole-file
publication transaction is a separate promise from frame-atomic library output.
Hashes observe committed bytes exactly once at their declared boundary.

Document the required error/output/handle state after constructor failure,
allocation failure, overlapping buffers and malformed final frames using the
existing tests. Keep independent instance state isolated. Propose documenting
single-instance concurrent calls as unsupported unless explicitly synchronized;
separate-instance parallel safety still needs its current source/test audit.

## Format and compatibility boundaries

Project1.0, C ABI1, stream1.0/1.1/2.0 and schema92 are different namespaces.
Format2 does not become a different wire major just because the project becomes
1.0. Preserve explicit little-endian serialization, LSB-first payloads,
zero-padding rules and exact profile/model identities. LZD remains Lempel-Ziv
Double; Adaptive Huffman naming retains the documented FGK variant. Internal
canonical Huffman primitives are not a new public Static Huffman profile.

Admit only the variants the format specification actually supports. The old
Contextual rANS entropy variant2 remains retired/reserved, and the old non-full-
literal4MiB native reference does not become a new public selector. Preserve the
separately documented decoder-compatibility versus producer-byte exceptions for
pre0.8 native Huffman planning revisions. Audit those exceptions explicitly in
the final compatibility corpus; do not silently remove the restore-only cases.

## Support and packaging decision boundaries

The current complete CI evidence qualifies Windows/MSVC and Ubuntu/Ninja x64,
with four shared/static package configurations. The existing Clang C client on
Windows uses an MSVC DLL; this does not establish an independently built Clang
codec or Linux/Clang runtime guarantee. A non-x86-64 claim requires actual
generation/cross-decoding evidence and the relevant library/package checks.
If such evidence is unavailable, assess the release item and explicitly limit
support under the release procedure instead of marking it successful.

For1.x CMake consumption, major compatibility is useful only when matched by
the actual ABI/source promise. Record the choice before changing install rules;
test old/new consumer expectations and shared-only/static-only exports. Keep
DLL filename/export decisions separate from ELF SONAME rules. This draft adds
no built binaries, platform package, install target, version or CI job.

## Adoption gate and next work

Complete the unresolved per-family flags/defaults and campaign/measurement
receipt mapping in the readiness inventory. Then reconcile the public guides,
resolve D1..D8 explicitly and validate any resulting example/package changes.
Only then close the corresponding stage2.2 checklist items. No proposed promise
here is evidence that a future1.0 candidate has passed its final full CI,
sanitizer, measurement, exchange or provenance gate.
