# Sixty-four-MiB position-distance rANS definition and vectors

## DD-1552: sixty-four-MiB representation before implementation

This is milestone1 of DD-1536 for dictionary2/14, context1/20 and entropy4/11.
Maximum frame/window F is67,108,864 bytes; match lengths are3..258. Retain
DD-1532's full previous-literal partition, scalar rANS state, normalization
and canonical compact record rules with these explicit substitutions:

- Alphabets [2]*3+[256]*17+[9]*3+[27]*9+[2]*26 total58 contexts and4680
  frequencies. Kind0..2, literal3..19, length20..22, distance class23..31
  and residual bit32..57 retain the family meanings. Matches preserve the
  previous literal. Residual frequency offsets start4628 and end4680.
- The descriptor has16 metadata bytes and an eight-byte active mask at16.
  Records begin at24. Byte23 bits0..1 are valid; bits2..7 are reserved zero
  and strictly rejected. Minimum descriptor is24 bytes, maximum9326,
  reached by the independently frozen all-context dense description.
- Distance class26 permits only distanceF and26 zero residual bits.
  Isolated field fixtures qualify grammar, not reachable frame history.
  A nonempty reset frame of at mostF cannot reference distanceF. Separate
  reachable recipes use distanceF-3/length3 and distanceF-258/length258.
- Decisions are bounded by min(36T,10F), payload by20F+8 bytes and complete
  serialized frame capacity by20F+9398 bytes, including the64-byte frame
  header and9326-byte descriptor. The128KiB fixed working allowance remains
  separate. Checked arithmetic precedes allocation and state publication.
- The112-byte stream header has dictionary variant14 at14, entropy variant11
  at18, context variant20 at98, count58 at82, frequency count4680 at84 and
  windowF at64. Configured frame at20 is bounded byF. Other DD-1532 fields,
  known original size, reset rules, zero reserved fields and strict trailing
  rejection remain unchanged. No failed frame is drained to public output.

Independent Python vectors freeze24 positive decision fixtures, the empty
112-byte stream header, empty descriptor/state and9326-byte dense descriptor.
They test the highest valid mask bit and reject every reserved mask bit,
all literal buckets, literal history across matches, length boundaries and
isolated distance-class transitions. A separately written forward compact
parser and scalar inverse verify model records, decision bytes, terminal
state and exact payload extent. Two hundred seeded models and200 payloads
add bounded mathematical checks using seed1552. No production codec is
used by this test.

This milestone admits no public factory, CLI selector, exchange entry or
memory default. Native differential and failure guarantees, owning/public
integration, full contextual corpus comparison, measured resources, fuzzing
and schema71 are later milestones. Existing representations are unchanged.

See [the family definition](lzss-position-distance-rans-large-windows.md),
[the retained scalar rules](lzss-position-distance-rans-4m-full-literal.md)
and [the format](../format.md).

## Private finite entropy-core checkpoint

The fixed context layout, bounded canonical descriptor parser/serializer,
reverse scalar encoder and forward decoder implement DD-1552's geometry.
Descriptor parse and serialization commit caller destinations only after
success. Failed decoder initialization preserves the preceding decoder,
and a failed symbol read preserves its output argument. Reverse encoding
writes only explicitly private scratch; it supplies no frame publication.
Limits are checked before descriptor use. No public factory, CLI selector,
owning resource default or exchange entry is added by this checkpoint.

TVG-1415 compares independent generated models and payloads, including
the frozen mathematical fixtures and the maximal descriptor, against the
native parser, scalar encoder and decoder. Token/frame/stream/owning
integration and failed-frame quarantine require the following checkpoint;
passing this finite core alone does not complete milestone2.

## DD-1553: private token, frame, stream and owning integration

Typed-token variant14 admits the64MiB short-length grammar and leaves older
variants unchanged. The field cursor uses27 distance classes and rejects
nonzero residual in class26. Ordinary token and frame destinations are
transactional. The separate private scratch operation decodes tokens in
one pass and may retain a prefix on failure; its owner discards that scratch.
Raw reconstruction follows successful token validation. Stream draining
begins only after complete frame validation, preserving failed-frame
quarantine, including a later failure after an earlier successful frame.

The owning encoder uses the first-party five-prefix nearest-longest finder
with eligibility3 or5; later public integration fixes5. MaximumF private
queries are3020826222 bytes encode and2214733558 decode. Serialized capacity
is1342186678 bytes and finder capacity806092800 bytes. These are checked
capacities, not process peaks. The4GiB private test allowance is not a public
default; public admission requires actual directional measurements.

TVG-1416 exercises independent token/frame bytes, split-stream reset
boundaries, fixed-five owning bytes, allocation rollback, pointer overlap,
limits and late failure quarantine. API/CLI, full contextual corpus
comparison, measured public defaults/resources, instrumented fuzz and
schema71/final81 remain unfinished. Local passing checkpoints are committed
without waiting for later stages or the other windows.

Both native builds pass all nine private integration/regression tests.
Independent token/frame differentials each pass211 cases; split-stream
checks pass22 streams/62 frames, including the full-window-to-short-reset
transition. Fixed-five owning bytes match forF+1 input and two automatic
frames. Fixture bytes and private resource queries agree across builds.
This completes private milestone2 only; the later public and final gates
listed above remain unfinished.

## DD-1554: public API and CLI capacity trial

The public136/64-byte config/resource types expose canonical initialization,
preallocation queries and immutable-direction creation. Public matching
fixes eligibility5. Retain allocation rollback, ordinary success-only
destinations, private failed-scratch discard and failed-frame quarantine.
The canonical CLI selector is lzss-position-distance-rans-64m, with65536-byte
calls and whole-file temporary commit. A3072MiB capacity trial is distinct
from process peaks and awaits actual directional/resource admission.

With65536-byte input/output calls and65536 external retained controls,
public queries are3021027038 bytes encode and2214934374 decode. The trial
grant is3221225472 bytes. This query fit does not substitute for measured
peaks, full corpus comparison, sanitizer qualification or final admission.
TVG-1417 covers lifecycle, static/shared ABI and CLI boundaries; subsequent
benchmark/fuzz and schema71/final81 gates remain unfinished.

Both builds pass all four public API/static C ABI/shared C ABI/CLI tests,
and all six earlier position-distance rANS API regressions. Seven CLI
boundary archives are identical across builds; all212 truncations of the
short archive, metadata/state/trailing mutations and retained destination
cases pass. Exact public query rows also agree across builds. This completes
public milestone3 only; capacity admission and final qualification remain
pending as stated above.

## Complete local benchmark/fuzz qualification

BM-0231 qualifies all twelve corpus members and48 isolated directional rows,
with one warmup and three samples per row and fresh normal public process
peaks. Candidate59366141 bytes are below contextual60568740 bytes, with
every member smaller. Observed encode time is1.095024790% shorter and
decode time53.456937565% longer; individual encode sample spans preclude a
stable acceleration claim. Report capacity queries separately from peaks.
DD-1555 retains3072MiB after complete query/resource qualification. Together
with FZ-0086's five fully instrumented10k campaigns this completes local
milestone4. Final fixed-runtime/source81 qualification, four-profile local
completion and maintainer push/hostedCI/external gates remain unfinished.
