# Eight-MiB prepared encoder integration scope

DD-1458 / IR-1217 / TVG-1325 / CR-1629. Date: 2026-10-04.
Status: source-bound integration design; no public adoption.

## Decision and evidence

Keep the operation encoder as the speed baseline, the safe owner as an
independent oracle, and prepared ownership as the memory-oriented candidate.
BM-0211's 24 matched lifecycle medians place prepared/safe below one and
prepared/operation above one. For the eight-MiB random recipe the allocation
peak falls from 805584602 to 268713690 bytes; complete logical reservation
falls from 864339542 to 327473646 bytes. These are measured recipe-specific
ledgers, not resident-memory measurements or admission guarantees.

DD-1457 establishes 168 isolated checks and 24 ordinary companion controls
on cyclic full-64 and tail-32 frames. Its six synthetic postcondition faults
exercise the owner's commit boundary. They do not establish match-bearing
late-fault coverage, maximum-frame injection, real-helper failure atomicity,
new fuzz coverage or public compatibility. Existing broader tests retain
their original scope and source bindings; this design reruns none of them.

## Existing boundaries checked

The public header declares separate position-distance factories for 64 KiB,
one MiB and four MiB. The CLI recognizes those same three selectors. No
eight-MiB position-distance C factory or CLI selector exists. The generic
typed-context profile enum concerns field-context variants and is not a
registration mechanism for the private eight-MiB position-distance path.

The private preflight requires dictionary variant 11, context algorithm 1,
context variant 12, 47 contexts and range total 32768. Its parser also checks
dictionary algorithm 2 and entropy algorithm/variant 3/2. Frame and window
ceilings are eight MiB. Preserve this existing representation and decoder;
an implementation storage strategy must not allocate a new format identity.

The prepared coordinator's API takes a concrete original_size. It computes
frame lengths from that value and reaches awaiting_end at that exact size;
there is no unknown-size protocol in this path. Do not reinterpret a large
integer as an unknown-size sentinel. Flush is neutral, ResetBlock unsupported,
and callers repeat EndInput with an unconsumed suffix. Sticky terminal
results and immutable encode direction remain part of the contract.

## Proposed internal integration boundary

First qualify match-bearing late faults using the existing isolated seam.
Then design and qualify a private owning adapter around the unchanged
prepared coordinator, with a concrete bounded allocator and explicit known
size. The adapter owns the allocator longer than the transform and destroys
the transform before the allocator. It must not expose the owner's private
Prepared type or allow callers to issue plans, mutate receipts or bypass
validation. Keep operation, safe and prepared as separately named internal
paths with equivalent wire output under the same configuration.

No automatic strategy selector is admitted. An explicit internal prepared
choice is meaningful only with a fully admitted budget. File length alone
cannot predict tokens, modeled events, payload size or adjacent generation
coexistence. Refusal remains refusal: no silent fallback, retry, budget raise
or release of the old publication to make a candidate fit. A later public
policy would require a separate specified API and compatibility review.

Account for the adapter, opaque handle if any, allocator object, callback
working storage, observer/hash controls, raw/index owners, current and
candidate generations, and complete call-buffer capacities. All allocations
remain charged until actual release, including drained old publication and
spare capacity beyond a supplied view. Reconcile prospective admission with
actual returned capacities before use. Arithmetic overflow, oversized return,
overlap, allocation refusal and ledger mismatch must fail safely. Never reuse
an earlier helper grant to pay for the adapter or its controls.

The private maximum-frame random diagnostic needed about 327.5 million
logical bytes with its retained owners. That is not a proposed default:
different adapters and retained buffers change the total. Obtain adapter
requirements from actual types and conservative bounds before allocation;
test the exact configured ceiling and one byte below. Do not advertise an
eight-MiB path as fitting any public default until this calculation and its
qualification exist.

## Gates and acceptance evidence

| Stage | Required evidence | Scope still excluded |
| --- | --- | --- |
| Match-bearing late faults | Deterministic repeated-pattern fixture with independently confirmed literal and match tokens; all six modes after real-helper success; fresh/replacement owner and first/second/tail coordinator; ordinary companion; full old-generation snapshots and real-release ledger | Maximum-frame injection and arbitrary tokens |
| Private owning adapter | Source-bound allocator lifetime and complete budget model; refusal/overlap/capacity cases; empty and known-size mismatch; partial input/output, neutral Flush, unsupported ResetBlock and sticky terminals; operation/safe/prepared wire equality and unchanged decoder round trip | Public ABI, CLI and unknown size |
| Maximum-frame qualification | Representative literal-heavy and match-heavy full/tail frames; measured actual peak and logical admission at the selected policy; allocation failures and selected late-stage failures; no failed-frame output | Universal workload or resident-memory claim |
| Public promotion proposal | Concrete C configuration, requirements and ownership API; exact error translation and full-buffer boundary guard; separately reviewed profile-local limits; static/shared consumers and applicable sanitizer/fuzz tests | Automatic selection unless separately specified |
| CLI and exchange promotion | Explicit selector, existing selectors preserved; file transaction failure preserves destinations; revision-bound archive inventory, CI and external cross-route verification | Changes to previously admitted archives or defaults |

For any failure, preserve old owner publication, layout, length and pending
state. A stream call may consume failing raw bytes and publish earlier valid
bytes; counts must stop output at the prior validated boundary. Staged failure
publishes zero bytes. Sticky error code and position repeat with zero counts,
including an invalid-flags follow-up. Destructor tests must observe actual
release before asserting zero receipts. Ordinary-helper failure atomicity
remains an independent obligation, separate from synthetic seam tests.

Public promotion needs an explicit implementation gate after these results;
the current design adds no source registration, public declarations, selector,
default, profile, format change or exchange archive. Previously admitted
release artifacts remain unchanged. Future release binaries must be tied to
the admitted revision before external verification.

## Next bounded unit

DD-1459 / IR-1218 / TVG-1326 / CR-1630 should qualify a new standalone
match-bearing late-failure fixture against the unchanged seam and helper
sources. Establish actual literal/match composition before arming faults;
do not infer it from repetition or reuse cyclic-only coverage. Bind ordinary
and isolated object symbols, source hashes, allocation receipts and outputs
in two optimized routes and a fully instrumented route. Preserve all prior
tests and evidence. Only observed passes close this additional gap. Private
adapter implementation and later promotion remain subsequent work.
