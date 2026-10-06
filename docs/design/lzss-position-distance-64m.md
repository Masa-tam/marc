# Sixty-four-MiB position-distance definition and resource diagnosis

DD-1501 reserves this representation before production implementation. It does
not expose a new factory, CLI selector or exchange archive. IR-1260 records
first-party provenance; TVG-1368 supplies independent recipes and count proof.
The full development goal remains active through implementation and exchange.

## Exact representation

Use Format 2.0 dictionary 2/14, context 1/15, entropy 3/2. Retain the existing
position-distance field order, explicit little-endian integers, numeric extra
bits from least significant to most significant, and byte-oriented integer
Range carry/normalization/finish. Window/frame maximum F=67108864 raw bytes;
legal smaller parameters remain bounded by F and caller limits. Models reset
per frame. Match lengths are 3..258, with overlap copying and longest/nearest
encoder ties. Encoder eligibility remains strict 9<2L, thus L>=5; valid wire
lengths three/four must decode. Match tokens preserve the last literal byte.

The 112-byte stream header has the following unchanged layout:

| Offset | Width, bytes | Value or meaning |
| ---: | ---: | --- |
| 0 | 4 | MARC |
| 4 / 6 | 2 / 2 | 2 / 0, format version |
| 8 / 10 | 2 / 2 | 64 / 1, base size / known-original-size flag |
| 12 / 14 | 2 / 2 | 2 / 14, dictionary |
| 16 / 18 | 2 / 2 | 3 / 2, entropy |
| 20 | 4 | bounded nonzero frame size |
| 24 | 4 | zero |
| 28 / 32 | 4 / 4 | 16 / 16, dictionary / entropy parameters |
| 36 | 4 | zero |
| 40 | 8 | known original raw size |
| 48 | 4 | 16, context parameters |
| 52 | 12 | zero |
| 64 | 4 | bounded nonzero window size |
| 68 / 72 / 76 | 4 / 4 / 4 | 3 / 258 / 0, wire length limits / dictionary flags |
| 80 / 84 | 4 / 2 | 32768 / 50, model total / contexts |
| 86 / 88 | 2 / 8 | zero / zero |
| 96 / 98 | 2 / 2 | 1 / 15, context identity |
| 100 / 104 | 4 / 8 | zero / zero |

Frame prefix is 80 bytes. Its 64-byte header has MRF2 at offset 0, LE16 size
64 and flags zero at 4/6, LE64 sequence at 8, LE32 raw R/token T/field E/
decision N/payload P/descriptor size 16 at 16/20/24/28/32/36, zero side-data
and checksum sizes at 40/44, and sixteen reserved zero bytes at 48. The
following 16-byte descriptor has LE32 N/P at offsets 0/4, LE16 contexts 50
and flags zero at 8/10, and four reserved zero bytes at 12. Frame N/P must
agree with the descriptor. Payload follows immediately. No native structure,
model table, optional flag or checksum is introduced. Empty input contains
only the header; final short frames remain valid. Validate sequence, raw total,
original-size agreement and strict trailing input under the existing contract.

For L<5, length class 8 carries one equiprobable residual bit L-3. For L>=5,
c=floor(log2(L-4)), L=4+2^c+E with c equiprobable residual bits; class zero
carries none. Reject class-seven residual 127 (length 259). Distance D uses
c=floor(log2(D)), D=2^c+E, c=0..26. Class zero has no extra event. Class 26
admits only residual zero; a nonzero residual exceeds F. That endpoint D=F
cannot fit a nonempty match inside a valid reset frame. Reachable maxima are
F-3 for wire L=3, F-258 for L=258 and F-5 for encoder matches, all class 25.
Reject references beyond configured window or earlier frame-local history.

| Field | Context | Alphabet |
| --- | --- | --- |
| Kind | previous Start=0, Literal=1, Match=2 | 2 |
| Literal without an earlier literal | 3 | 256 |
| Literal after stored B | 4+(B>>5) | 256 |
| Length class | 12+previous kind | 9 |
| Distance class | 15+length class | 27 |
| Distance extra bit p | 24+p, p=0..25 | 2 |

There are 50 contexts and 2632 flattened frequencies. Group offsets are
0, 6, 2310, 2337, 2580, 2632. Frequencies begin at one, increase by one after
each adaptive decision and rescale with ceil-half when total reaches 32768,
retaining all symbols. Equiprobable length residual bits do not update a
context. Range normalization is 16777216 (2^24), not the window bound. Finish
has five canonical bytes. Require canonical payload termination and exact raw
count before committing any frame; no valid prefix of a failed frame may drain.

For 1<=R<=F, require:

```text
1 <= T <= R
2T <= E <= min(2R,5T)
E <= N <= min(10R,36T)
5 <= P <= min(2N+5,20R+5)
```

A match has 3 + length-residual-bit-count + distance-class decisions. Exhaustive
length/class arithmetic proves N<=36T and N<=10R, including the grammatical
class-26 endpoint. Reachable class-25 length-three matches have 29 decisions
per token; the old nine-per-byte token argument is invalid. No tighter
whole-frame bound is assumed. Maxima P=1342177285 and frame=1342177365 fit
uint32, as do R/T/E/N. Caller policy and checked arithmetic precede allocation.

## Private storage and resource observations

Retain a clear typed reference and independent mathematical expected bytes.
The production candidate uses explicit two-byte literal and nine-byte match
records. Encoder storage is bounded by 2R; generic decoder storage by
min(3R,9T), with full requested owner capacities charged. Do not allocate a
decoder using 2R: a literal followed by an overlapping length-three match is
eleven record bytes for four raw bytes. A full repeated-byte frame using such
matches needs 3F-1 record bytes. Serialized, token/publication and private raw
scratch spans must remain disjoint, metadata immutable on every failure, and
borrowed values stable until terminal state. Retain previous successful frame
publication until a fully validated replacement commits.

At diagnostic payload capacity Pcap=134217728 (128 MiB), complete five-backing
capacities are serialized Pcap+80, two token/record buffers and two F raw buffers:

| Candidate | Five backing bytes, excluding controls | Observed process peak resident bytes |
| --- | ---: | ---: |
| Typed stride 12 | 26F+Pcap+80 = 1879048272 | 1884205056 |
| Compact decoder stride bound 3R | 8F+Pcap+80 = 671088720 | 676245504 |

Separate fresh processes allocate all five arrays concurrently, initialize them
and force page contact. These are allocation-only observations; no new codec,
control query, file loop, throughput or compression ratio is measured. Typed
stride is working-memory evidence, not wire layout or an ABI promise. Runtime,
allocator and process overhead are included in the peaks. Environment details
remain private. Neither Pcap nor a future CLI memory policy is selected here.

Even at Pcap=5, compact backings require 8F+85 > 512 MiB before controls. Thus
the old 512-MiB policy cannot cover the full candidate. An unchanged exact
index layout projects raw+index = 5F+4 MiB, before controls. Extending the
previous conservative owning-encoder candidate charge projects
15F+4 MiB+240 = 1010827504 bytes before the complete fixed/control query, at
Pcap=2F. This is a capacity projection, not a future resource-query result or
admission grant. Do not import old control sizes, discard prior publications,
discount a working grant, silently raise global defaults or retry larger limits.
Choose actual memory/payload/time policy only after complete new factory
queries and separate full codec-process measurements with retained generations.

## Remaining implementation and integration gates

Implement validator/field/model references and a compact private-token path;
prove canonical payload and entire token/metadata/output invariance before
frame reconstruction/publication. Then qualify finite frame and borrowed/
owned stream paths, exact dictionary/reference equality, callback lifetimes,
every allocation refusal and exact/one-below full admission. Preserve existing
32-MiB public ABI and introduce distinct configurations, queries and factories.
Carry the literal-extent constexpr fixture correction into all new generators.

Mandatory full raw boundaries are F-1/F/F+1/2F/2F+1, incompressible/repetitive
retained generations, far F-3/F-258/F-5, short wire L3/L4, one-byte input/output,
all split and finish/reset cases, invalid class/residual/history/counts, aliases,
reserved fields, late canonical corruption, sticky terminal states and strict
trailing input. Retain full maximum-window cases when choosing watchdogs.

After actual full encode/decode speed, ratio and memory evidence, connect an
explicit CLI selector with bounded policy and atomic whole-file output. Freeze
all existing 73 archives before schema 64/marc-cli-v64 appends archive 74.
Qualify genuine historical schemas, entire-manifest admission before launch,
negative guards, independent source-bound producer bundles and fixed-path
runtime publication while saving the whole old build. The fixed external
runtime path remains unchanged. The maintainer performs push and supplies
revision-specific hosted CI/four external consumer reports. Current design
alone does not complete any of those gates.

## Independent finite vectors

The first-party `tests/lzss_position_distance_64m_reference_vectors.py` regenerates expected bytes from the equations; it does not invoke a native codec. The M64V0001 fixture container is test data, not a stream-format ID. All recipes validate complete frame-local history/counts, including the two maximum-size raw frames.

| Recipe | R | T | E | N | P |
| --- | ---: | ---: | ---: | ---: | ---: |
| literal | 1 | 1 | 2 | 2 | 6 |
| overlap | 267 | 5 | 17 | 23 | 11 |
| lengths | 33409 | 257 | 1025 | 2303 | 306 |
| rescale | 40000 | 40000 | 80000 | 80000 | 295 |
| far3 | 67108864 | 260114 | 1040455 | 2601151 | 227741 |
| far258 | 67108864 | 260113 | 1040451 | 2601147 | 227740 |

For literal A (65), the kind interval uses floor(0xffffffff/2)=0x7fffffff. The literal interval unit is floor(0x7fffffff/256)=0x007fffff, and low is 65 times that unit, 0x207fffbf. One normalization followed by canonical finish gives `00207fffbf00`. The overlap recipe (literal A, D1/L3, D1/L4, literal B, D2/L258) gives `0020f9041443c157b205c7`. These expected payloads still require future native differential qualification.


## Finite native core qualification (DD-1502)

The six independent recipes above now agree with private native field mapping,
reference Range encoding, typed/compact single-pass decoding and scalar raw
reconstruction on four compiler/instrumentation routes. Canonical failures do
not publish caller token output. This supersedes the earlier pending native
recipe qualification only. Whole-frame reconstruction, streaming, public
factories, complete resource policy, CLI and archive-74 exchange remain pending.


## Finite complete-frame qualification (DD-1503)

Private typed and compact complete-frame helpers now validate exact frame
extent, decode canonical tokens, reconstruct in private raw scratch and commit
raw output/layout together only after full success. Eleven independent valid
and malformed recipes pass four native routes, including maximum-size far
references and full five-buffer capacity accounting. Generic defaults remain
unchanged. This does not yet qualify streaming, public ownership, CLI policy or
archive-74 exchange.


## Borrowed stream qualification (DD-1504)

A private no-allocation streaming decoder now collects bounded frames into
borrowed disjoint workspaces, validates and reconstructs before draining, and
keeps prior validated output when a later frame fails. Complete workspace
queries include actual capacities, owner, controls, helpers and separately
retained bytes. Sixteen mathematical streams pass arbitrary split, starvation,
delayed finish, sticky terminal and full two-frame one-byte output schedules on
four native routes. Owning/public factories, dictionary encoders, CLI resource
policy and archive-74 exchange remain pending.


## Finite dictionary qualification (DD-1505)

Exhaustive, exact indexed and private compact encoders now share the specified
greedy longest/nearest parsing, strict length-five eligibility and overlap.
Four native routes verify independent small cases and full-frame repetition
and F-5/F-258 references, exact capacities, failure invariance and honest
retained storage accounting. Private compact records are canonical byte
serialization and bounded by 2R. This remains an internal finite helper;
direct and compact entropy encoders, frame/stream ownership, public factories,
CLI resource policy and archive-74 exchange remain pending.


## Finite entropy and frame qualification (DD-1506)

Typed and compact inputs now feed direct Range encoding, retaining the separate
operation-array reference. Three finite frame paths validate and finish every
private intermediate before committing caller bytes/layout/written count.
All twenty-nine payload vectors and independent full-window repetition and
F-5/F-258 frame bytes agree on four native routes, with full-capacity admission
and failure invariance. Encoder compact-prefix accounting does not lower the
decoder's record floor. The large constexpr fixtures retain explicit literal
extents and pass default and stricter syntax compilation.
Owning stream, distinct public factories, measured CLI policy and archive-74
exchange remain requirements of the active goal; this finite gate is internal.


## Owning encoder qualification (DD-1507)

The compact publication owner and known-size owning stream now use exact
bounded allocations and charge old/current/candidate capacities until actual
destruction. Every allocation failure preserves existing publication and no
failed frame drains. Independent full-window and two-frame boundary streams
agree on four native routes, including far F-5/F-258 and late corrupt-frame
nonpublication; small chunk schedules cover one-byte input and output.
Distinct public factories, measured public/CLI resource policy and archive-74
exchange remain required. Diagnostic full-owner grants leave defaults unchanged.


## Public lifecycle qualification (DD-1508)

Six distinct public entry points connect the qualified owning encoder and
borrowed compact byte decoder. Metadata is copied, complete backing remains
borrowed/charged through destruction, and every candidate generation is admitted
before allocation. Initial queries guarantee initial storage only; decoder
queries guarantee capacities only. Exact/one-below grants, every allocation
refusal, unaligned generic short-wire records, sticky states and failed-frame
nonpublication pass native and C boundary checks. Fifty-six prior initializers
remain unchanged and two new ones are appended. Four routes, previous profile
regressions and a shared C client qualify the lifecycle. Full seeded public
resource experiments reproduce the private bytes without establishing CLI policy.
Explicit CLI selection, measured CLI grants and archive-74 exchange remain required.


Explicit CLI qualification (DD-1509) adds a separate selector, full 64 MiB boundaries and two full incompressible frames. Candidate admission uses initial query + 2F + 4Pcap + 240, including every complete control/call/external owner. A 1 GiB grant and Pcap=128 MiB leave room for the conservative bound; every actual query is checked at runtime. The decoder admits all five complete byte capacities before allocation. Full-file atomic publication uses the established temporary-output transaction. Archive-74 exchange qualification remains a separate subsequent step.
