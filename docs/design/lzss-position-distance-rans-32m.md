# Thirty-two-MiB position-distance rANS definition and vectors

## DD-1548: thirty-two-MiB representation before implementation

This is milestone1 of DD-1536 for dictionary2/13, context1/19 and entropy4/10.
Maximum frame/window F is33,554,432 bytes; match lengths are3..258. Retain
DD-1532's full previous-literal partition, scalar rANS state, normalization
and canonical compact record rules with these explicit substitutions:

- Alphabets [2]*3+[256]*17+[9]*3+[26]*9+[2]*25 total57 contexts and4669
  frequencies. Kind0..2, literal3..19, length20..22, distance class23..31
  and residual bit32..56 retain the family meanings. Matches preserve the
  previous literal. Residual frequency offsets start4619 and end4669.
- The descriptor has16 metadata bytes and an eight-byte active mask at16.
  Records begin at24. Byte23 bit0 is valid; bits1..7 are reserved zero and
  strictly rejected. Minimum descriptor is24 bytes, maximum9305, reached
  by the independently frozen all-context dense description.
- Distance class25 permits only distanceF and25 zero residual bits.
  Its isolated field fixtures qualify grammar, not reachable frame history.
  A nonempty reset frame of at mostF cannot reference distanceF. Separate
  reachable recipes use distanceF-3/length3 and distanceF-258/length258.
- Decisions are bounded by min(35T,10F), payload by20F+8 bytes and complete
  serialized frame capacity by20F+9377 bytes, including the64-byte frame
  header and9305-byte descriptor. The128KiB fixed working allowance remains
  separate. Checked arithmetic precedes allocation and state publication.
- The112-byte stream header has dictionary variant13 at14, entropy variant10
  at18, context variant19 at98, count57 at82, frequency count4669 at84 and
  windowF at64. Configured frame at20 is bounded byF. Other DD-1532 fields,
  known original size, reset rules, zero reserved fields and strict trailing
  rejection remain unchanged. No failed frame is drained to public output.

Independent Python vectors freeze24 positive decision fixtures, the empty
112-byte stream header, empty descriptor/state and9305-byte dense descriptor.
They test the highest valid mask bit and reject every reserved mask bit,
all literal buckets, literal history across matches, length boundaries and
isolated distance-class transitions. A separately written forward compact
parser and scalar inverse verify model records, decision bytes, terminal
state and exact payload extent. Two hundred seeded models and200 payloads
add bounded mathematical checks. No production codec is used by this test.

This milestone admits no public factory, CLI selector, exchange entry or
memory default. Native differential and failure guarantees, owning/public
integration, full contextual corpus comparison, measured resources, fuzzing
and schema70 are later milestones. Existing representations are unchanged.

See [the family definition](lzss-position-distance-rans-large-windows.md),
[the retained scalar rules](lzss-position-distance-rans-4m-full-literal.md)
and [the format](../format.md).

## Private finite entropy-core checkpoint

The separately named descriptor validator, normalized model builder,
reverse scalar encoder and forward decoder implement DD-1548's layout.
Static checks fix residual start4619/end4669,57 alphabets and58 offsets.
Records start at24 and every reserved high bit of byte23 is rejected before
model interpretation; the highest defined bit remains valid.
Independent native differentials qualify200 descriptor reserializations,
200 normalized model/payload inversions, all24 frozen decision fixtures
and the maximum9305-byte descriptor. Native tests cover every truncation
of that descriptor, canonical records, destination/count invariants,
failed-begin state preservation, unchanged failed-read symbols, terminal
state/extent and sticky errors. Existing sixteen-MiB finite-core tests pass.
An initial adaptation error in the literal alphabet was detected by static
offset assertions, corrected and followed by successful validation.
Only standalone private test targets use this code. Typed-token integration,
frame quarantine and owning allocation lifecycle remain pending, so this is
a checkpoint within milestone2. Public/CLI, measured/fuzz and schema70 gates
remain pending; no memory default or current public runtime changes here.

## DD-1549: private token, frame, stream and owning integration

Add internal short-length typed-token variant13 for distanceF; all older
variant numbers and their behavior remain intact. The field cursor uses
26 distance classes and rejects nonzero residual in class25. Ordinary token
and frame destinations are transactional. The separate private scratch
operation decodes tokens in one pass and may retain a prefix on failure;
its owner must discard that scratch. Raw reconstruction follows successful
token validation. Stream draining begins only after complete frame validation.

The owning encoder uses the first-party five-prefix nearest-longest finder
with eligibility3 or5; subsequent public integration will fix eligibility5.
Resource queries include owner, retained raw/token/serialized/finder buffers
and the separate fixed working allowance. At maximumF, private queries are
1,510,876,761 bytes encode and1,107,437,281 decode. Serialized capacity is
671,098,017 bytes and finder capacity403,439,616 bytes. These are checked
capacities, not OS peaks. A2GiB private test allowance is not a public default.
Public resource admission requires later actual measurements.

Independent token/frame differentials each pass211 cases (200 generated,
11 full-window). Split-stream checks pass22 streams/62 frames, including
full-window-to-short-reset transition. Independent fixed-five owning bytes
match forF+1 input and two automatic frames. Native lifecycle, chunking,
late failure/quarantine and sixteen-MiB regressions pass. This completes
private milestone2 only. Public API/CLI, full contextual corpus comparison,
measured defaults/resources, instrumented fuzz and schema70 remain pending.
The full four-window goal remains active; passing private work is committed
locally without waiting for the other windows.

## DD-1551: measured public capacity admission

The public API/static/shared ABI/CLI milestone and instrumented fuzz
checkpoint precede full twelve-member compression and isolated 48-row
directional qualification. BM-0227 records 59597025 versus 60783907 bytes,
all twelve members smaller, encode time -11.316549641% and decode time
+35.197529728% against the physical 32MiB contextual control. Normal public
driver peaks are 857104384 bytes encode and 453640192 decode; these are
process observations with a different scope from capacity accounting.

Admit the unchanged 1536MiB capacity default after exact public driver
queries of 1511077577/1107638097 bytes, with declared 65536-byte calls and
65536 external/control bytes, fit the grant. Retain the fixed-five policy,
success-only destination commits, discarded failed private scratch and
failed-frame quarantine. This qualification changes no wire format or
numeric initialization. The local schema70/80 exchange checkpoint remains
separate from final fixed-runtime/source80 qualification. Those final gates,
64MiB/final81 and maintainer push/hostedCI/external validation remain pending.

## Final local qualification

IX-0077 closes the remaining local source/runtime gate at implementation
revision7c66f0258f3980af43509f2c670065b30dc3708e. Both qualified public
runtimes reproduce the independent private archive bytes for all twelve
members and restore every input. Both builds pass public API/static C ABI/
shared C ABI/CLI tests. The source-bound80-archive exchange passes two local
self and two local cross routes while preserving the exact79-archive prefix.
Earlier negative/history evidence is retained after unchanged-script and
archive verification, rather than claimed as a new run. Together with
BM-0227 and FZ-0085 this completes all five32MiB local milestones. The
four-window goal remains active:64MiB, final81 and maintainer push/hosted CI/
external four-route validation are unfinished.
