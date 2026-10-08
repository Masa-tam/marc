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
