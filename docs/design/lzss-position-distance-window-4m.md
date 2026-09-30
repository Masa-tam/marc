# Position-distance LZSS: transition to a 4 MiB window

Status: development plan under DD-1360. Exact variant allocation, final bounds
and public admission remain future implementation work.

## Objective

Use the externally qualified 1 MiB implementation at
`1099a84be8dae4b1b821e7ccef2cd3ad58272546` as the retained reference and move the
next position-distance development effort to a 4 MiB frame/window. Follow the
existing 1, 4, 16 and 64 MiB progression. BM-0191's reproducible 8.85% to 9.25%
whole-encoder reduction is a useful stable endpoint for the current work; it
does not establish that all further 1 MiB optimization opportunities are exhausted.

The initial target is a separately identified position-distance Dynamic Range
profile. Existing contextual 4 MiB coding is a comparison profile, with its own
minimum match length and models. It does not replace the planned position-
distance grammar. No new numeric identity or application name is reserved here.

## Representation work

Retain the current literal/length grammar, minimum match length 3, maximum
match length 258, longest-match/nearest-distance rules and per-frame reset.
A 4,194,304-byte distance ceiling requires classes 0 through 22 and at most
22 distance-extra bits, compared with classes 0 through 20 at 1 MiB.

Under the same model construction, contexts 15 through 23 would have alphabet
23, and distance bit-position contexts 24 through 45 would have alphabet 2.
This yields 46 contexts and 2,588 flattened frequency entries: the current
2,566 plus eighteen ordinary distance entries and four binary entries.
These are derived design inputs; exact grammar acceptance, class-22 zero-extra
handling, descriptor validation and canonical termination must be specified
before codec implementation. A fixed frame/window profile still restricts
references to already reconstructed bytes and prohibits crossing a reset.

Derive event/decision counts and payload bounds independently for the widened
class range. Do not assume that changing a window constant validates all old
bounds. Document little-endian fields, LSB-first extra-bit order, identity-crossing
rejection and the new decoder-visible representation before implementing it.

## Memory and transferable implementation knowledge

The current workspace plan retains raw bytes, a conservative serialized frame,
one token per raw byte, two modeled operations per raw byte, three finder
link arrays and three fixed head tables. Its frame-dependent terms grow
linearly. A direct transfer is expected to need roughly 300 MiB at a four-MiB
frame, plus bounded model/owner state; exact requirements must come from the
new checked workspace query. This estimate is not a measured allocation or
physical peak-memory claim. Payload and aggregate ceilings exceed the existing
generic defaults and need an explicit profile resource policy.

Transfer prepared operation mapping, the five-prefix search candidate and
expired-finder payload scratch with their lifetime and failure checks. Reassess
finder head width and actual scratch eligibility at the larger window. The
rejected unconditional 18-bit-head admission at 1 MiB remains evidence; neither
its rejection nor a fixed head width proves the best 4 MiB choice. Wider history
can change collision and cache costs. Scratch capacity may select fallback even
when a frame is otherwise valid; count eligibility rather than assuming it.

## Development sequence and evidence

1. Define an additive format variant, checked limits and hand-checkable distance
   vectors, including distances around 1 MiB and 4 MiB and final short frames.
2. Implement bounded decoder validation and a clear reference path, preserving
   failed output and withholding failed frames at every public boundary.
3. Transfer validated optimized mechanisms with exact token/operation/archive
   comparisons against the new reference, plus malformed, chunking, allocation
   and reset tests. Retain the 1 MiB implementation and previous artifacts.
4. Evaluate all twelve corpus members through complete encoders and decoders,
   reporting archive size, encode/decode time and workspace separately. Compare
   position-distance 1 MiB and existing contextual 4 MiB. Frame/window/model
   differences mean these are profile comparisons, not isolated window effects.
5. Use full suites, sanitizers, bounded fuzzing and revision-specific external
   exchange qualification before public completion. A new archive identity must
   append to the existing inventory without changing its frozen prefix.

Larger windows may improve compression on distant repetitions while increasing
encoding cost. The corpus results, rather than window size alone, determine
whether this profile should progress to 16 MiB.


## Exact reservation and preflight stage

DD-1361 and the appended 4 MiB position-distance section in `docs/format.md`
reserve `2/10 + 1/11 + 3/2` and define the complete decoder-visible model,
counts and header rules. This supersedes the planning-stage lack of a numeric
identity. Private grammar/state/preflight validation is the current stage;
Range payload decoding, encoding, public selection and performance qualification
remain subsequent work.

The scalar operation-coding stage (DD-1362, TVG-1229) adds a private Range
encoder/decoder with independent payload vectors, widened distance/model tests
and canonical termination checks. It preserves concrete preflight state charges
and does not admit public selectors. Next, implement token mapping and bounded
transactional/private-scratch decoding with reconstruction/history checks, then
complete reference frame coding before optimized transfer and measurement.

DD-1363 adds the private token bridge, complete validation, transactional output
and discardable single-pass scratch. TVG-1230 checks real histories above one MiB
and exact four-MiB reconstruction using the existing typed-token reconstructor.
This completes token-layer reference integration; reference frame serialization,
bounded frame lifecycle and publication checks remain the next stage.
