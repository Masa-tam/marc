# Private owning adapter for the prepared eight-MiB encoder

DD-1460 / IR-1219 / TVG-1327 / CR-1631. Date: 2026-10-04.
Status: implementation design, no adapter implemented or admitted.

## Scope and construction

Wrap the unchanged LzssPositionDistance8mPreparedStreamEncoder in a private
core::Transform. Use explicit known-size configuration; preserve dictionary
2/11, context 1/12, entropy 3/2, 47 contexts and model total 32768. The
operation encoder remains the speed baseline and safe ownership the oracle.
No strategy selection, fallback, new format, public C handle or CLI is added.

The proposed adapter owns a bounded receipt allocator, which delegates typed
allocation and destruction to the existing ExactStreamAllocator. It owns
the prepared coordinator in disengaged in-place storage, constructed only
after configuration and fixed-reservation validation. Keep both objects at
stable addresses; delete copy and move operations. The allocator member is
constructed first and destroyed last. Explicitly destroy the coordinator
before checking zero live receipts and before destroying the allocator.

The coordinator destructor currently releases index and raw in its body,
then member destruction releases the current generation while its bridge
and underlying allocator still live. Do not replace this ordering or claim
that drained generations were released early. Invalid construction leaves a
sticky adapter error and zero counts; partially constructed coordinators
must still be destroyed through the same lifetime path.

Proposed configuration contains the existing stream header and limits,
external retained bytes, and maximum input/output view capacities in bytes.
There is no unknown-size sentinel, implicit default budget, injectable user
allocator, allocator callback or owner-publication accessor. A nonallocating
query reports fixed logical reservations and raw/index initial requirements;
it does not promise that every future frame fits. Stack, caller-owned and heap
instances use the same full sizeof(adapter) charge. Any caller heap owner or
additional framework state must be included in external retained bytes.

## Receipt allocator

Use a fixed array of twelve typed receipts: raw and index plus at most five
current and five candidate blocks. Each receipt records pointer, element
capacity, byte count and type. Include counters and the exact allocator
delegate inside controls().data/bytes covering the complete allocator object;
controls and working_bytes remain stable while the coordinator is alive.
Do not report the whole enclosing adapter as allocator controls: it contains
the coordinator and would contradict the existing disjointness checks.

Before delegating, check nonzero element count, checked count*sizeof(T),
a free receipt slot and the local complete reservation against the limit.
The coordinator and owner continue to perform their original prospective
frame/generation admission; the receipt allocator does not issue a plan or
replace that validation. Delegate exactly the requested count. A successful
return must have nonnull storage and exactly that capacity; no rounding,
pooling, grant reuse, early deletion or hidden retry. Check disjointness with
all live blocks and allocator controls before entering the receipt.

The existing exact delegate is the sole allocation source. Qualification may
isolate delegation in a test-only translation unit; malformed returns must
be independently owned and genuinely releasable, never aliases to live storage
or forged pointers. An impossible internal receipt/type/release mismatch is
a violated internal contract, not malformed caller input; do not advertise
recovery from arbitrary broken allocators. Normal release invokes typed real
destruction before clearing its receipt and decreasing the live count.
Empty blocks are harmless. Diagnostic ledger inspection is read-only.

## Complete logical accounting

Let A be sizeof(adapter), H its conservative process/query controls, W the
unchanged coordinator working_bytes(), C the complete allocator object,
K its callback/delegate working charge, E external retained bytes, and I/O
the declared full maximum input/output capacities. All sums and products
are checked before allocation. Obtain A/C/H/K from actual compiled types and
the delegate contract; do not substitute old driver or benchmark constants.

Use conservative external charge X = A + H + E + I + O when constructing
the coordinator. Its unchanged live budget is W + C + K + X + raw_bytes +
index_bytes + current_candidate_bytes + actual_input_view + actual_output_view.
This deliberately counts embedded coordinator/allocator storage again in A
and call views again in I/O. Record this conservative duplication; do not
subtract overlapping grants or claim the sum is unique physical memory.
It ensures all enclosing state and call-owner tails remain charged without
changing the original helper's issuance boundary.

The allocator's local preallocation check uses R = W + C + K + X + I + O,
then requires R + all_actual_live_block_bytes + requested_bytes <= limit.
This is a secondary conservative check, not a transfer of the owner's
continuation grant. W must cover the existing sequential helper maximum and
K must cover nested delegate controls. Assert that relationship from the
actual unchanged definitions during implementation review. If it cannot be
established, revise this explicit design before allocation, rather than
guessing the missing charge. Core frame-bound validation remains required.

For original size N, raw capacity is min(frame_size,N). For nonempty input,
index capacity is 65536 + raw_capacity entries with four bytes per entry.
For empty input the coordinator requests neither allocation; the fixed
state still counts. Check prospective initial storage before either callback.
At frame replacement count both full generations until actual release. The
full publication capacity counts after its bytes have drained. Actual
capacity refusal remains subject to the existing core error translation;
an allocator returning empty is observed as out_of_memory, not a new public
error category. No local check may silently increase the budget or retry.

The query provides fixed and initial minima and explicitly reports deferred
per-frame admission. Do not derive worst-case frame fit from BM-0211 random
measurements or DD-1459's 394-byte generations. Maximum-frame qualification
and a proposed default limit are separate work. Logical totals exclude RSS
and hidden system allocator overhead, as in existing diagnostics.

## Process boundary

Before delegation, require input/output extents within the declared caps and
check their disjointness with each other and the entire adapter. The core
still checks raw/index/current/candidate/control overlap. Do not perform a
pairwise check between the enclosing adapter and its own internal members.
External retained owners must remain alive and correctly charged; declaring
a capacity is not permission to pass a dangling view. Oversized or overlapping
calls become sticky invalid_argument at the prior accepted input position,
zero counts and no output writes. Track accepted position only by checked
addition of actual delegated input_consumed; core-originated errors preserve
their exact code and position. Terminal results must be consulted first so
subsequent invalid calls cannot replace a sticky error or ended result.

Delegate flags and process results without introducing a second flush policy.
Flush is neutral; ResetBlock and unknown flags remain unsupported. Repeat
EndInput on an unconsumed suffix. The adapter must preserve the core behavior
for early/late known-size termination and empty input, and must not return
Progress with zero counts. Failed frames never drain. A failing call may
publish earlier validated bytes and consume failing raw bytes; staged failure
writes zero. Sticky error/EndOfStream returns zero counts thereafter.

## Implementation qualification matrix

| Area | Required independent checks |
| --- | --- |
| Construction/query | Invalid header/limits, checked overflow, empty input with zero allocations, exact initial reservation and one byte below, no callback before complete prospective admission, partially allocated initial failure and real destruction |
| Allocator | Exact typed requests, twelve-slot bound, stable controls, full capacities and coexistence, each discovered allocation failure in fresh/replacement/tail paths, real deletion before receipt clearing, zero receipts after destruction |
| Boundary guard | Overlapping input/output, adapter overlap, allocator/live-block overlap, oversized declared views, unchanged sentinels, sticky wrapper/core errors and sticky ended calls with invalid follow-up flags |
| Stream | Empty, one-byte, cyclic literal-heavy and confirmed match-bearing inputs; full/full/tail; one-byte and varied input/output splits; zero output, neutral Flush, unsupported ResetBlock, short and excess known-size input, EndInput suffix behavior |
| Oracles | Operation/safe/prepared adapter full wire equality, independently parsed boundaries and unchanged decoder round trip; count and error-position equality where delegation occurs |
| Fault boundary | Ordinary companion and isolated helper postcondition faults, exact stage arrival, old generation/layout preservation, no failed-frame output, complete enclosing and test-seam charges |

Discover callback counts from untimed positive runs and inject each actual
allocation point, without assuming the old seventeen-call fixture applies.
Keep test-only delegate/late-fault seams out of production objects. Qualify
two optimized routes and a fully address/undefined-instrumented route, with
source hashes, per-object flags, link inputs, receipts and results. No clocks,
new benchmark/fuzz labels or public source registration are needed here.

## Next unit

DD-1461 / IR-1220 / TVG-1328 / CR-1632 implements this private adapter,
query and bounded standalone qualification in new files. Existing helper,
owner, coordinator, tests, benchmarks and CMake registrations remain
unchanged. Any missing budget relationship or unexpected core behavior must
be recorded and resolved explicitly. Passing that unit admits only the
private adapter; maximum-frame and public API/CLI/exchange promotion still
require their own evidence and revision-bound release gate.
