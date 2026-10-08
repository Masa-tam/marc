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
