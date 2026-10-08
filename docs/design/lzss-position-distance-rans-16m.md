# Sixteen-MiB position-distance rANS definition and vectors

## DD-1542: sixteen-MiB representation before implementation

This is milestone1 of DD-1536. Its exact reserved tuple is dictionary2/12,
context1/18 and entropy4/9. Maximum frame/window F is16,777,216 bytes,
lengths3..258. Adopt the full previous-literal partition and scalar rANS
representation of DD-1532 with these explicit sixteen-MiB substitutions:

- Alphabets: [2]*3+[256]*17+[9]*3+[25]*9+[2]*24, totaling56 contexts and
  4658 frequency entries. Kind0..2, literals3..19, lengths20..22,
  distance classes23..31 and residual bits32..55 have the DD-1536 meaning.
- Descriptor metadata is16 bytes; its seven-byte mask uses all56 bits.
  Records start at23. In particular, byte22 bit7 is valid rather than an
  eight-MiB reserved bit. The compact single/dense/sparse record grammar
  and deterministic normalization retain DD-1532. Minimum descriptor is23
  bytes and maximum is9283; the all-context dense vector reaches9283.
- Distance class24 represents only distanceF, with24 zero residual bits.
  Its isolated field vector is valid grammar but cannot be a valid match
  in a nonempty reset frame of at mostF bytes. Reachable maximum-frame
  recipes instead use distanceF-3/length3 or distanceF-258/length258.
- Decisions are bounded by min(34T,10F), payload by20F+8 bytes, and complete
  serialized frame capacity by20F+9355 bytes, including64-byte frame header
  and9283-byte descriptor. The128KiB fixed working allowance is separate.
  Check every arithmetic operation and allocation before committing state.
- The112-byte stream header replaces dictionary variant at14 with12,
  entropy variant at18 with9, context variant at98 with18, context count
  at82 with56, frequency count at84 with4658 and window at64 withF.
  Configured frame at20 remains bounded byF. All other DD-1532 fields,
  known-size termination,64-byte frame header, resets, zero reserved fields,
  strict trailing rejection and failed-frame quarantine remain unchanged.

Independent Python vectors freeze empty descriptor/state/stream header,
the valid highest mask bit, literal history across a match, all sixteen
literal buckets, length classes, distance classes and the largest dense
descriptor. The test parses compact records separately, decodes the scalar
rANS decisions forward and checks terminal state/extent. It also checks200
seeded models and200 decision payloads. These qualify the mathematical
vectors only: native implementation, malformed-input and failure guarantees,
public API/CLI, memory admission, measurements, fuzzing and schema69 are
subsequent milestones. No public selector or memory default is admitted here.

See [family definitions](lzss-position-distance-rans-large-windows.md) and
[format](../format.md) for the retained framing and compact record rules.

## Private entropy-core checkpoint

The separately named finite C++ descriptor validator, model builder,
reverse scalar encoder and forward decoder are implemented from the
first-party eight-MiB core with the explicit DD-1542 layout substitutions.
All56 mask bits are accepted; the corresponding eight-MiB high-bit rejection
is absent. Static offset checks require residual start4610 and end4658.
Independent differential fixtures compare200 exact descriptor roundtrips,
200 normalized models and payloads, all24 frozen decision vectors and the
maximum descriptor. Native unit checks retain failed destination/count and
symbol invariants, failed-begin state preservation, bounded truncation,
canonical records, strict final state/extent and sticky errors.
Only standalone private test targets use this code. Typed-token integration,
frame quarantine, owning allocation lifecycle and every public/measured/
fuzz/exchange gate remain pending; this does not complete milestone2.
