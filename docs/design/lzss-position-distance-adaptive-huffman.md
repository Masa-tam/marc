# Position-distance Adaptive Huffman

## DD-1627: staged native FGK profiles

Implement64KiB,1MiB,4MiB,8MiB,16MiB,32MiB and64MiB in that order. Reuse
first-party bounded FGK tree primitives and the full-literal position-distance
token schedule already specified for native tANS. This is a new representation;
no previous stream, initializer, decoder identity or default is reinterpreted.
Use explicit profile selection before arithmetic, allocation or mutation.

Reserve the following exact Format2.0 identities after checking the existing
registry and native headers. Context algorithm is1 and entropy algorithm is1
(Adaptive Huffman); dictionary algorithm is2. The context/entropy variants are
inseparable from the corresponding dictionary variant and window.

| Window | Dictionary variant | Context variant | Entropy variant | Contexts | Alphabet entries |
| --- | ---: | ---: | ---: | ---: | ---: |
| 64KiB | 8 | 35 | 3 | 48 | 4570 |
| 1MiB | 9 | 36 | 4 | 52 | 4614 |
| 4MiB | 10 | 37 | 5 | 54 | 4636 |
| 8MiB | 11 | 38 | 6 | 55 | 4647 |
| 16MiB | 12 | 39 | 7 | 56 | 4658 |
| 32MiB | 13 | 40 | 8 | 57 | 4669 |
| 64MiB | 14 | 41 | 9 | 58 | 4680 |

Only64KiB is the first implementation milestone. Larger rows reserve distinct
representations and do not claim implementation or resource qualification.
The public selector family is `lzss-position-distance-adaptive-huffman`, then
suffixes `-1m`, `-4m`, `-8m`, `-16m`, `-32m`, `-64m`.

### Context and tree semantics

Reset every tree and dictionary at each independently coded raw frame.
Token-kind contexts0..2 depend on start/previous-literal/previous-match.
Literal context3 means no earlier literal; otherwise use4 plus the high nibble
of the most recent literal, retained across matches. Length-class contexts20..22
depend on the same previous-kind state. Length3/4 use class8 and one uniform
residual bit; length5..258 use class `floor(log2(length-4))` and that many
uniform LSB-first residual bits. Distance-class context is23 plus length class.
Encode `floor(log2(distance))`, then low-to-high residual distance bits using
context32 plus the bit position. The extra-position count is16/20/22/23/24/25/26
respectively. Type and extra-bit alphabets have size2, literals256, length9,
and distance classes17/21/23/24/25/26/27. Length residual bits bypass trees.

Each modeled context has one independent FGK tree. Start with one weight-zero
NYT at order `2*A` for alphabet sizeA. For an unseen symbol, emit the NYT path
then the symbol in `ceil(log2(A))` LSB-first bits; reject unused values and
already-seen symbols following NYT. Replace NYT by weight-one internal node,
new NYT at order-2 on the left and weight-one symbol at order-1 on the right.
Update its former ancestors. For an existing symbol, emit its current path
and update from its leaf. At each update, choose the highest-order equal-weight
node excluding ancestors/descendants and the parent, swap positions/orders,
increment weight and continue at the updated node's parent. Root remains fixed.
Left/right path bits are0/1. Match-length/distance validation precedes emission.
No rescaling occurs within a bounded frame; maximum observations remain below
`UINT32_MAX`. The model is not a transmitted canonical Huffman table.

### Exact entropy representation

Use a16-byte descriptor: little-endian decision count at0, payload byte count
at4, context count at8 (u16), final-valid-bit count at10 (u8), flags at11 (zero),
and reserved zero bytes12..15. A decision is one modeled symbol or one uniform
length-residual bit. Empty standalone entropy has zero count/payload/valid bits;
nonempty entropy has positive payload and final-valid bits1..8. Paths and
numeric fields are physically LSB-first; unused high final-byte bits are zero.
No initial/final coder integer or model description follows the descriptor.
Decoding must consume exactly the declared decisions and valid bits.

### Stream and frame representation

Retain the native112-byte MARC stream-header structure and64-byte frame header.
Header fields are Format2.0, base header64 bytes, flags1, exact identities from
the table, positive raw frame size at most the selected window, known original
size,16-byte dictionary/entropy/context parameter records, and no side data or
trailer. Dictionary parameters are exact window, minimum length3, maximum258,
flags0. Entropy parameters at80 are bytes1(FGK),1(no rescale), u16 context count,
u32 alphabet entries, u32 node entries `2*entries+contexts`, and u32 zero.
Context parameters at96 are algorithm1 and the exact native variant followed
by twelve zero bytes. Reserved base fields retain their specified zero values.

Frames use exact sequence, raw/token/event/decision counts and payload/descriptor
extents; flags, side data and trailers are zero. Descriptor size is exactly16.
Event count equals decision count because bypass bits are individual decisions.
For nonempty rawF and tokensT, require1<=T<=F and2T<=D. The existing short-length
schedule gives upper bounds `D<=min((10+log2(window))*T, B*F)` withB equal to
7/8/9/9/9/10/11 for the seven rows. Require raw extent exactly the next bounded
frame, sequence equal to already-committed bytes divided by frame size, and all
references within reconstructed history and the remaining raw destination.
Empty input has a stream header and no frames. Reject extra trailing bytes.

A conservative payload ceiling is `ceil(267*F/8)`: a worst new type costs3 bits
and a worst new byte literal264 bits; a match's finite field/extra costs are
bounded below that ceiling per its at-least-three raw bytes. At64MiB the ceiling
is2239758336 payload bytes and2239758416 complete frame bytes, fitting u32.
These are format ceilings, not proposed CLI buffers or measured memory grants.
Admit actual configured limits before allocation. Determine large-profile
default/CLI grants, heavy timeouts and parallelism only after resource pilots.

### Failure, measurement and completion gates

All ordinary destinations and metadata commit on success only. Model nodes,
symbols, tokens, payload and restored raw staging are private scratch and may
change on failure. Validate complete entropy, token/history bounds and frame
finish before draining any raw frame. Earlier fully validated frames may remain
published; a failed frame emits zero committed bytes. Check aliasing and every
size/count arithmetic path before touching ordinary output.

For each window, qualify independent vectors, a reference encoder/decoder,
chunking, boundaries, exact/one-short admission and directed malformed inputs.
Use full manifest-verified corpus members, record all sizes/totals, lifecycle
encode/decode time and fresh-process peaks against the same-window contextual
Adaptive Huffman. Verify actual identities, window/frame settings and restored
bytes; where public controls are absent, test configurable existing APIs or
record a verified inability. Do not infer a missing comparison or improvement.

Fuzz execution is explicitly deferred until the maintainer instructs it;
record that gate as pending rather than substituting directed tests for fuzz.
Keep existing and future fuzz infrastructure from executing unintentionally.
Preserve all starting products and stage candidates; keep the qualified Windows
CLI at its fixed Release path until promotion after verification.

Starting inventory is schema85 with95 archives, verified independently before
implementation. Append seven profiles in window order, retaining the starting
order and exact bodies; later schema/102-archive qualification must include
past schemas, malformed-manifest prelaunch refusal and local self/cross routes.
Make reviewable local commits after each related test/documentation gate.
No intermediate push occurs. Hosted CI and external four-route102 evidence
remain separate, maintainer-controlled completion gates.

### Initial64KiB entropy resource contract

The reference entropy core accepts caller-private node and symbol spans and
uses no dynamic allocation. Its conservative minimum charge is65536 control
bytes plus16 descriptor bytes, node entries times `sizeof(AdaptiveHuffmanNode)`,
alphabet entries times `sizeof(uint16_t)`, and planned payload bytes. Supplied
larger workspace spans are charged at their supplied extents. This is logical
admission accounting, not a measured resident peak or an eventual public grant.
Limits must also satisfy common configuration invariants, including block size
not exceeding internal-buffer allowance. Exact/one-short tests use valid common
configurations so that the tested failure is the workspace boundary itself.

Planning and writing reset the same per-context trees. A write receives exactly
the planned private payload span and checks both final decision count and bit
extent. The decoder checks padding before traversing payload, stages each
symbol and validates exact finish; tokens/raw frames still need their own
later all-or-nothing validation. Bit extents use wide intermediate arithmetic
and reject unrepresentable host offsets before traversal.

The reference token adapter additionally admits131072 conservative control
bytes plus every supplied input, model, private staging and ordinary destination
span and context/limit control extent. Planning also accounts for its exact
planned payload before committing metadata. Checked products precede range
checks. These private accounting rules are conservative operation limits, not
yet public default grants. All token-result counts/sizes are zero on failure,
even when private scratch already contains a validated prefix of tokens.

The64KiB stream/frame metadata milestone TVG-1583 qualifies canonical headers,
prefix bounds and private decoder requirements. The stream is known-size only;
the all-ones unknown-size marker is rejected even under a permissive total
output limit. Prefix preflight validates80 bytes before payload buffering;
full preflight checks the complete extent but does not replace entropy/history
validation or authorize raw publication. All other per-window completion gates
remain open at this metadata-only milestone.
