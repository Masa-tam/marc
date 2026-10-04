# Prepared stream adoption assessment and isolated late-failure test gate

DD-1456 / IR-1215 / TVG-1323 / CR-1627. Date: 2026-10-04.
Status: assessment and execution design only; fault injection remains unconfirmed.

## Evidence and decision

Recompute the preserved BM-0211 six-sample medians separately for each of
two optimized builds and twelve cases. All 24 matched process and lifecycle
ratios prepared/safe are below one, and prepared/operation above one.
Lifecycle ratios range 0.59851..0.61964 and 0.59820..0.63573 for safe,
and 1.19518..1.42221 and 1.27372..1.43315 for operation, in builds A/B.
This is analysis of existing measurements, not another benchmark campaign.
No combined mean, best sample, decoder improvement or general workload claim.

Retain operation as the speed baseline, prepared as a private memory-oriented
candidate, and safe ownership as an independent oracle. No public adoption,
automatic selection, CLI, default, profile, ID or format changes follow.
Prepared is faster than the safe owner but is not a speed replacement for
operation. In every measured case its complete logical reservation is lower
than operation; its allocation peak equals the safe owner's and its logical
reservation is 640 bytes greater than the safe owner's.

For the eight-MiB random recipe, operation/prepared allocation peaks are
805584602/268713690 bytes: saving exactly 536870912 bytes (512 MiB).
Complete logical reservations are 864339542/327473646 bytes: difference
536865896 bytes. Logical reservations and allocation ledgers are separate
from resident memory, hidden allocator overhead and framework resources.
These values include the benchmark's retained owners and full call views.
They do not prove public admission at a smaller configured limit. Input size
alone cannot choose a path: actual tokens, events, payloads, retained owners,
adjacent generations and all configured limits determine admission. Future
selection would need its own validated policy and qualification.

## Source-isolated seam

DD-1447 requires a test-only source-isolated seam before late range/prefix
injection. Preserve all existing owner, coordinator, helpers, tests and
benchmarks. Do not expose Prepared, add a factory, forge a plan, add an
injection constructor or change the private issuance boundary.

Build a separate diagnostic owner object directly from the unchanged
src/frame/lzss_position_distance_8m_prepared_storage_owner.cpp. Apply three
dependency-name substitutions ONLY to this one translation unit:

| Original dependency | Isolated test dependency |
| --- | --- |
| encode_lzss_position_distance_8m_token_range | test_encode_lzss_position_distance_8m_token_range |
| serialize_lzss_position_distance_8m_frame_prefix | test_serialize_lzss_position_distance_8m_frame_prefix |
| preflight_lzss_position_distance_8m_frame_prefix | test_preflight_lzss_position_distance_8m_frame_prefix |

The shims have exactly the original signatures and noexcept specifications.
Each calls the original unchanged helper first. Compile the shim translation
unit and all helper translation units without substitutions, so delegation
cannot recurse into a shim. Replace the owner object in the isolated link;
never link both owner objects or duplicate definitions. The coordinator stays
unchanged. No substitution enters a library, production target, existing test
target, benchmark, CMake source list or build-wide compiler definition.
Record source hashes, per-object options and link inputs. Check the object
symbol/reference mapping and prove the ordinary companion link references
the original dependencies. Compilation/link isolation is a future check,
not established by this document.

Use a bounded scoped test controller with a thread-local active pointer,
restored by RAII. Execute tests serially in one thread; no asynchronous input
mutation, allocator side effects, dangling spans or const_cast. The pointer
is test-only and cannot enter a production object. Disarmed shims delegate
without changing results. A matching controller is armed for exactly one
frame, by range counts' output_already_committed/raw size and prefix context's
sequence/prior. Prove original success before every injection, one injection
only, and exact stage/delegate counts. An early failure or missed hook FAILS
the test rather than counting as successful fault coverage.

## Six planned late faults

| Mode | Injection after original success | Expected continuation refusal |
| --- | --- | --- |
| R1 | Set range details.error to internal_error after payload write | range_error before prefix call |
| R2 | Increase descriptor.payload_size by one with checked arithmetic | inconsistent_counts before prefix call |
| P1 | Set serializer error to validation_error after the full prefix write | prefix_error before reparse call |
| P2 | Change prefix bytes_written from 80 to 79; leave prefix bytes valid | successful reparse then inconsistent_counts |
| P3 | Toggle bit zero of prefix byte zero after full prefix write | actual reparse rejects invalid_magic; prefix_error |
| P4 | After successful original reparse, increase parsed header.sequence by one with checked arithmetic | same_layout mismatch; inconsistent_counts |

The owner currently translates these continuation refusals to
core::ErrorCode::limit_exceeded. Test that exact observable error rather than
inventing new public categories. The trace must distinguish the six stages
despite the shared owner error. Shims deliberately violate successful helper
postconditions inside a private candidate. This tests the owner's defensive
commit boundary; it does not prove the original helper's own failure
atomicity, simulate an external malformed stream or establish fuzz coverage.
Original helper failure-atomicity tests remain separate and unchanged.

## Bounded matrix and contracts

Use frame size 64 bytes, full cyclic byte frames and a final cyclic 32-byte
tail, with stream original size 160. Dictionary window remains eight MiB,
variant 11, contextual variant 12, 47 contexts, total 32768. Set diagnostic
max_frame_size and max_block_size to 64, maximum compressed payload to 65536
bytes and internal-buffer ceiling to one MiB. These are proposed test limits,
not passed admission claims. If actual admission fails, retain evidence and
stop; revise explicitly before a fresh qualification, never silently raise
the ceiling, reuse a grant or exclude an owner.

Plan 12 owner failure checks: six modes times fresh/acknowledged replacement.
Plan 36 coordinator failure checks: six modes times first/second/short-tail
frame times single-call/staged publication. Add eight disarmed positive
controls: two owner contexts and six coordinator combinations. Thus 56 checks
per instrumented build. Separately run eight corresponding positive checks
in a companion built with the ordinary owner object. Three build routes
(two optimized, one fully address/undefined-instrumented with leak detection
disabled) imply 168 isolated checks plus 24 companion controls, all planned.
No actual new check, benchmark label or fuzz label is assigned at this gate.

Fresh owner refusal: zero bytes_validated, empty publication, unchanged
initial layout/length/pending, five candidate blocks actually released and no
live receipts. Replacement refusal: acknowledge the successful old frame,
retain its full generation and snapshots, then fail its fresh candidate.
Publication pointer, full bytes, length, all layout fields and pending state
remain unchanged; all five original blocks remain live and unchanged, five
candidate blocks are really freed, and destruction later returns receipts to
zero. Compare initialized token fields explicitly rather than relying on
structure padding. Raw/index planning scratch may change; do not claim it
belongs to the owner's unchanged-publication contract.

Single-call coordinator refusal may publish the header and earlier VALID
frames and consume the failing frame's raw bytes in that same call. Require
reported counts to match the known valid prefix and accepted raw position,
output bytes to match the ordinary companion prefix exactly, and every byte
after output_produced to retain its sentinel. Never require the whole call to
produce zero when it legitimately committed earlier frames. Error position
is the prior validated raw length (0/64/128), not the newly accepted length.
For staged calls, drain header and earlier frames first; the failing call must
produce zero and preserve its entire output. Exercise zero and one-byte
output capacity while draining only validated bytes, with bounded call guards.
Sticky error repeats the identical code/position with zero counts and no
writes, including a subsequent invalid-flags call. Earlier valid frames may
be checked with the unchanged frame consumer; a partial failed stream is not
a complete stream or an EndOfStream success.

At stream failure, fresh live storage is raw/index only; replacement retains
raw/index and the five old-generation blocks. Destroying the coordinator and
owner must really free all allocations before zero-receipt assertions.
Record complete current/candidate capacity coexistence, full call views,
guards, deletion ordering and before/after ledger snapshots.

Charge the whole test controller, active-pointer/RAII controls, fixed trace,
all wrapper parameters/results/call controls and conservative nested helper
working storage in separately retained bytes. Do not reuse the old owner's
working-byte grant to pay for shims. Charge all raw/index/wire/snapshot owners
at full capacity, allocator receipts, oracle controls, observer/hash controls,
and spares until actual destruction. Check arithmetic and prospective/actual
admission before allocation; derive sizes from the actual compiled types.
Logical and allocation ledgers remain distinct; no RSS or universal-fit claim.

## Completion and next unit

DD-1456 completes assessment and the source-isolated execution design only.
Late range/prefix injection remains UNCONFIRMED. No code, new fault test,
new timing, new fuzz, public integration or external campaign is completed.
Next DD-1457 / IR-1216 / TVG-1324 / CR-1628 implements and qualifies the
isolated seam and bounded matrix. Preserve normal companion and fault evidence
separately, including all failures. Require source/symbol isolation, disarmed
wire equivalence, exact fault arrival, failed-frame privacy, unchanged old
publication and real-release ledgers before closing the gap. Any integration
decision still requires its own later gate and applicable validation.
