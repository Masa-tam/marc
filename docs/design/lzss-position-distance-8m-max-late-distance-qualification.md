# Maximum-frame late failures and distant matches

DD-1463 / IR-1222 / TVG-1330 / CR-1634. Date: 2026-10-04.
Status: qualification design only; no new codec execution or fixture admission.

## Evidence and boundary

DD-1461 established bounded private owning-adapter behavior and six synthetic
late postconditions. DD-1462 established maximum-frame normal operation and
complete budget/configuration refusals for four recipes. Neither establishes
maximum-frame late postconditions or distant matches. Keep those finite
results intact. Operation remains the speed baseline and safe ownership the
oracle. This unit changes no code, format identity, public registration,
default, strategy selection, benchmark or fuzz campaign.

Use the unchanged private known-size dictionary 2/11, context 1/12, entropy
3/2, 47 contexts and total 32768. Define F = 8388608 uncompressed bytes and
N = 2*F+32. All proposed stream fixtures consist of two identical full frames
and the first 32 bytes of that frame. Frame resets prohibit references into
the previous frame. A raw indexed match consumes at least five bytes;
within a full F-byte frame its distance cannot exceed F-5. The typed grammar
also accepts lengths three/four, which is a separate surface. Distance F
and its class-23 representation cannot be reached by these raw frame tests.
Do not infer their coverage from the configured eight-MiB window.

## Candidate fixtures and prerequisite validation

Retain the DD-1462 cyclic full frame: byte i is i modulo 256. Historical
raw/wire hashes, full composition and actual plans are controls, not grants
for a new driver's surrounding storage.

Define two new distant-match candidates, with M = 5 and M = 258:

| Frame positions | Byte value |
| --- | --- |
| 0 through M-1 | 7 |
| M through F-M-1 | 0 |
| F-M through F-1 | 7 |

The independently reasoned target is a match at raw position F-M, length M,
distance F-M. These proposed distances are 8388603 and 8388350, respectively,
both greater than 4194304 and in distance class 22. This is an expectation,
not an observed token or admitted vector. An earlier token boundary, parser
choice or unexpected composition must stop admission, not be filled in by
an assumed match. No compressed size, token total or hash is prescribed for
these new candidates before actual planning.

Before arming any fault, query actual token/event/payload plans for each
sequence and prior-raw boundary. Allocate planning storage only after its
complete prospective reservation fits. Require every token field to validate,
exact token counts, cumulative expansion equal to the frame length, and a
record of literal/match counts, distances and lengths. Require the target
match exactly at F-M in each full candidate frame. Independently expand the
tokens with bounded overlap-copy semantics and compare every raw byte.
Use the unchanged decoder on complete ordinary operation/safe/adapter wires;
require equal bytes and hashes. Confirm the distance-class/extra-bit mapping
from the actual tokens, without modifying models to force the desired class.
Plan the tail independently; do not reuse a full-frame composition assertion.

## New maximum-frame test seams

The existing late-fault seam tests expected_sequence against prior/64.
Preserve it unchanged. Add separate test-only seam header/implementation
derived solely from those first-party contracts. Its controller must select
the exact prior raw boundary, raw length, expected sequence and frame size.
Validate them against the supplied context and known-size remaining length
with checked arithmetic. Do not substitute F for the old divisor in existing
files or infer sequence from a magic constant. Owner-only calls also supply
their explicit contexts; absence/mismatch invalidates the test receipt.

Delegate to the real helper first. Arm only after ordinary planning, wire
and decoder validation. Inject exactly once after the real selected helper
has succeeded, recording stage arrival, original success, byte counts and
original descriptor/layout fields. Preserve the real selected-mode semantics:

| Mode | Postcondition change | Required selected-stage arrivals |
| --- | --- | --- |
| 1 | Successful range result becomes internal_error | range 1, prefix 0, reparse 0 |
| 6 | Successful reparsed layout sequence increases by one | range 1, prefix 1, reparse 1 |
| 0 | No mutation, isolated positive control | range 1, prefix 1, reparse 1 |

The sequence increment is checked. Overflow, failure before the selected
stage, multiple injections or missing arrivals fails qualification. Modes
2 through 5 retain only their earlier bounded evidence; this selected
maximum-frame matrix does not qualify all six at maximum size.

Rename only the owner's three late-helper dependencies in its isolated
translation unit. Rename only the adapter's six allocator delegates in its
isolated translation unit. Helpers, coordinator and real exact delegate
stay ordinary. Use a separate maximum-frame allocation observer if needed
for dynamic full snapshots, preserving the earlier bounded seam unchanged.
It must actually allocate/delete independent exact typed blocks and must
never return aliases or alter codec input. Initialize newly obtained byte
output blocks with a sentinel before handing them to the owner; this permits
checking candidate publication on real release. Record each block's type,
capacity and allocation identity in the new observer, rather than guessing
types from the earlier observer's pointer/byte-only records. Bind ordinary and isolated links,
per-object substitutions, symbol definitions/references and exact binaries.
No seam enters production or public allocator configuration.

## Planned finite matrix

Two optimized routes and a fully address/undefined-instrumented route with
leak detection disabled must run the following untimed checks. A check is
a matrix row, not an allocator callback, frame or process launch. Process
grouping is an implementation choice to be reported separately.

| Fixture and context | Isolated rows per route | Ordinary companion rows per route |
| --- | ---: | ---: |
| Cyclic direct owner, fresh/replacement, modes 0/1/6 | 6 | 2 |
| Cyclic adapter, targets first/second/tail, single/staged, modes 0/1/6 | 18 | 6 |
| M=5 adapter positive single; M=258 adapter positive single/staged | 3 | 3 |
| M=258 adapter replacement, single/staged, mode 6 | 2 | 0 |
| Total planned | 29 | 11 |

The last two fault rows use the matching M=258 single/staged ordinary and
isolated positive controls from the preceding row. Planned total is 120
checks over three routes, of which 54 are injected faults and 66 positive
controls. These are acceptance targets, not completed results. No existing
192/450/24 results are added to this count. If a candidate fails prerequisite
validation or complete admission, preserve evidence and revise the explicit
matrix before injection; do not silently drop rows and claim completion.

Direct-owner fresh context is sequence zero/prior zero. Replacement first
encodes and acknowledges a valid full frame, then uses sequence one/prior F.
Adapter targets are sequence 0/1/2 with raw lengths F/F/32 and prior 0/F/2F.
Single-call cases pass all remaining input with EndInput. Staged cases drain
the stream header, consume/drain each preceding frame, then pass exactly the
target input. Pre-target drains include zero and one-byte output probes plus
bounded larger chunks; they are selected split cases, not exhaustive splits.
Repeat EndInput on an unconsumed suffix. Positive controls finish the entire
stream. Guard every call against out-of-range counts and zero-count Progress.

## Failure atomicity and publication checks

Before a direct-owner replacement, snapshot all initialized fields of both
old token arrays and every byte of all three old byte buffers, retaining
their complete actual capacities. Compare every token's kind/literal/distance/
length rather than native struct padding. Record the publication pointer,
length, pending state and all fourteen layout fields. A refusal must preserve
these values; candidate tokens/frame may have been written internally, but
candidate publication must remain untouched and all five candidate blocks
must be genuinely released. Fresh refusal preserves the empty owner state.
Snapshot storage and comparison controls remain charged until destruction.

The adapter exposes no private owner metadata. Its independent allocator
observer can compare initialized old block contents and retained capacities;
it must not claim access to private pending/layout fields. Capture snapshots
before candidate allocation without borrowing the owner's budget. Require
the earlier-valid wire prefix exactly, the entire unused output/sentinel
suffix unchanged, and zero failed-call output in staged cases. Record exact
returned core code, byte/bit position and actual consumed/produced counts.
The inspected unchanged owner maps these continuation refusals to
limit_exceeded; a different observed result blocks the assertion and requires
source-bound investigation, rather than rewriting the oracle to accept it.

For target j, define B_j = 112 + sum of complete frame sizes before j from
the independently validated ordinary wire. Single-call failure output must
equal B_j and consumed input equals the end of target raw input. Staged
failure emits zero additional bytes. Error position is prior raw 0/F/2F,
bit zero. Sticky EndInput and invalid-flags calls retain code/position and
zero counts. Never decode a failed prefix as a complete stream. After actual
destruction require zero live receipts and actual deletes equal allocations;
clearing a receipt before real deletion is not acceptable evidence.

## Complete prospective and actual admission

Diagnostic policy is 1073741824 logical bytes, not a default recommendation,
RSS measurement, hidden allocator overhead or instrumented-runtime bound.
Obtain all sizes from actual compiled types and actual capacities. Use checked
sums/products/offsets, including prefix counters; avoid the prior diagnostic's
finite unsigned-counter shortcut. Include raw/index owners, whole input/output
view capacities and tails, reference wires, decoder output/workspaces, guards,
plans, controller records, dynamically allocated full snapshots, and the
complete nested late/allocator-seam working charges. Allocate snapshots only
after their full prospective capacity is admitted and keep them charged.

For exact generation j use G_j = 2*sizeof(Token)*T_j + 3*P_j + 160;
raw/index base is F + sizeof(uint32_t)*(65536+F). Replacement counts the
complete old G and candidate G simultaneously until actual release. Original
operation workspaces include both full event arrays, even when a tail is
small. Planning storage is removed from later phases only after destruction.
At every phase account for complete external retained owners plus the exact
helper's local views/controls, preserving conservative duplication in the
owning adapter. Never transfer grants or discount storage because it is
logically drained. Charge separately the new seam's nested real-helper work;
the owner's existing grant is not permission for new instrumentation.

Compute joint prospective maxima before large outputs/workspaces, validate
actual capacities after allocation, and validate the actual old/candidate
peak against independent allocation records. Operation, owner, adapter,
snapshot and decoder phases are separate maxima with genuinely ended
lifetimes; an enum or branch alone does not release storage. This design
prescribes no numeric new-fixture reservation, fit, wire hash or speed result.

## Execution gate and next unit

DD-1464 / IR-1223 / TVG-1331 / CR-1635 adds new standalone driver/seams,
plans actual fixtures and qualifies this matrix. Preserve every existing
source, test, benchmark and artifact, including intermediate failed results.
Bind source hashes, flags, isolated object/symbol mapping, real-release
receipts, per-row results and final actual counts. Save a successful selected
process snapshot before execution; it is not a system-wide exclusion lock.
Review ordinary/isolated and cross-route equality, first-party provenance,
append-only public records and all preserved hashes. Public admission,
registration, CLI/exchange and release similarity review remain separate.
