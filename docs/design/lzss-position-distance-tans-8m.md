# Eight-MiB position-distance tANS

## DD-1602: exact eight-MiB specialization

This document specializes the family definition in
`lzss-position-distance-tans.md` for `F = 8,388,608` and `N = 23`. It
defines a distinct stream identity and leaves all earlier position-distance
tANS representations unchanged. The intended public selector is
`lzss-position-distance-tans-8m`. Its dictionary window and configured frame
maximum are both 8,388,608 bytes. Match lengths remain 3 through 258; an
eventual public encoder uses fixed-five eligibility, exact longest matching,
and nearest-distance tie breaking.

The exact identity is dictionary `2/11`, context `1/24`, entropy `5/6`,
55 contexts, and 4,647 frequencies. Its alphabet sequence is
`[2]*3 + [256]*17 + [9]*3 + [24]*9 + [2]*23`. Contexts 0 through 2 encode
the preceding kind; literal contexts begin at 3, with later literals using
`4 + (previous_literal >> 4)` and matches preserving literal history. Length
contexts are 20 through 22. Distance classes use contexts 23 through 46 and
distance residual bits use contexts 32 through 54. Class 23 has a required
zero residual and cannot be emitted by a nonempty reset frame of at most F
bytes; it remains a grammar boundary for validation.

## Descriptor, state, and frame rules

The descriptor has the family 24-byte little-endian prefix: decision count
u32 at 0, payload size u32 at 4, table log u8 at 8, final valid-bit count u8
at 9, zero flags u16 at 10, context count u16 at 12, zero reserved u16 at 14,
frequency count u32 at 16, and zero reserved u32 at 20. The active mask is
seven bytes at offset 24. Its upper one bit is zero. Active models follow in
context order: singleton mode 0, dense mode 1 with a final implied frequency,
or sparse mode 2 with ascending symbols and a final implied frequency.
Singleton is mandatory for one symbol; otherwise choose dense when it is no
larger than sparse, and sparse otherwise. The descriptor is at most 9,270
bytes.

Present frequencies normalize to 4096 using the family integer rule and its
specified tie breaks. The tANS table uses the family 4096-state automaton,
spread step 2563, ascending symbols and occurrences. States range from 4096
through 8191. Length residuals use the 2048/2048 uniform model; distance
residuals use their adaptive contexts, low bit first. Payload starts with the
little-endian `final_encoder_state - 4096`, then its prepended LSB-first bits.
The empty entropy representation is the zero-mask descriptor and `00 00`
payload. Strict decoding requires terminal state 4096, exact decision and bit
counts, zero high padding, and use of every active model.

The known-size MARC 2.0 header is 112 bytes and MRF2 frame header is 64 bytes.
The header declares the identity above, an 8,388,608-byte window and maximum
frame, table log 12, state count 1, 55 contexts, and 4,647 frequencies.
Reserved, flags, side data, and trailer fields remain zero. Every nonempty
frame resets dictionary and entropy state. Failed candidates remain private;
no raw prefix from a failed frame may be published.

For token count T, raw extent R no greater than F, and decision count D,
`2T <= D <= min(33T, 9R)`. Payload is bounded by
`2 + ceil(12D/8)`, yielding a theoretical maximum of 113,246,210 bytes at a
full frame. This is a wire-format bound, not an allocation or admission
grant. Workspace limits, timeouts, and parallelism require measured evidence.

## Qualification status

The mathematical oracle's reverse encoder was corrected to emit the low bits
of the pre-transition state. Its 232 frozen cases are now checked on every
generation, and a separate C++ differential campaign adds uniform-bank and
highest-context cases. All 234 fixtures agree on frequencies, descriptor,
payload, valid bits and decoded symbols. This corrects verification evidence
without changing the format or codec implementation.

`tests/position_tans_8m_vectors.py` is the independent mathematical oracle
for the identity, normalization, descriptor grammar, inverse transitions,
boundary lengths, and distances through 8,388,608. The specification and
vectors alone do not establish implementation, benchmark, fuzzing, CLI, or
exchange completion; each remains a separate gate with its own evidence.

The current public contextual tANS API exposes profiles only for 64 KiB, 1,
4, 16, and 64 MiB. `marc_position_tans_8m_contextual_probe` passes the next
profile value to its public profile applicator and verifies rejection with
`MARC_STATUS_INVALID_ARGUMENT` without configuration mutation. Accordingly,
the absence of a dedicated profile alone does not establish comparison
availability. The extended probe sets both window and frame to 8,388,608
bytes under every published profile. Encoder workspace queries reject the
64 KiB, 1 MiB, and 4 MiB profiles but admit the 16 MiB and 64 MiB profiles.
Actual serialized identity and exact restoration remain to be checked before
using either admitted configuration for the comparison.

The native 8 MiB tANS frame parser, private-token/private-raw decoder,
owning workspace adapter, and C API are implemented. Their local tests cover
independent headers, exact re-encoding of a complete frame, arbitrary stream
splits, short buffers, malformed terminal state and padding, invalid history,
and successful-publication-only output. The API exposes the 8 MiB identity and
reports full-window resource requirements before allocation. CLI, benchmark,
and exchange-bundle schema75 admission, cross-route qualification, and
fixed-Release closure are implemented, while contextual comparison measurements
and final bundle closure remain incomplete. A bounded sanitizer-backed stream decoder fuzz
target exists; it does not replace full-frame or public-boundary fuzzing. The public CLI
campaign first restores a known seed, then runs 500 deterministic truncation, bit-flip,
insertion, and overwrite mutations.  It requires bounded completion, no process crash, and
no output or temporary file after every decoder failure.  A successful mutation can describe
another valid stream because this profile has no integrity trailer, so its output is not
compared with the seed.  The contextual tANS
control uses its 16 MiB public profile with an explicit 8 MiB frame and window:
the resulting stream identifies dictionary variant 5, context variant 4, and
tANS algorithm/variant 5/2, and its public decoder restores the source bytes.
This establishes a usable same-window measurement control; it does not yet
establish any compression, speed, or peak-memory result.

### Local corpus comparison

The complete twelve-member Silesia comparison used the admitted contextual
control above, with both its window and frame set to 8 MiB.  Every source was
decoded and verified by the benchmark before a sample was accepted.  The
position-distance profile was smaller for every member: 60,386,573 bytes in
total versus 61,561,630 bytes for contextual tANS, a reduction of 1,175,057
bytes.  Per-member compressed sizes were 3,202,391/3,207,580 (`dickens`),
18,178,475/18,887,433 (`mozilla`), 3,230,777/3,346,496 (`mr`),
2,332,401/2,335,773 (`nci`), 3,014,557/3,019,638 (`ooffice`),
3,218,940/3,238,839 (`osdb`), 1,535,529/1,538,735 (`reymont`),
4,748,386/4,752,873 (`samba`), 5,095,397/5,229,190 (`sao`),
10,069,203/10,083,307 (`webster`), 550,084/550,945 (`xml`), and
5,210,433/5,370,821 (`x-ray`), in position/contextual order.

The three-sample medians showed faster position decoding for every member.
Position encoding was faster on six members and slower on the other six.
Its measured process peak working set was larger for every member.  The
per-member sample arrays and peaks are retained with the measurement evidence;
these observations describe this measurement only and do not establish a
general throughput or memory guarantee.

BM-0249 publishes the audited per-member timing medians and process peaks
from that retained receipt. The maxima are 317,353,984/242,679,808 bytes
(position/contextual), both on mozilla. These are combined encode/decode
process peaks with retained buffers; directional peaks remain unmeasured
in this corpus campaign. TVG-1471 and BM-0248 separately qualify actual
maximum frames, late failures and public F-1/F/F+1 boundaries with fresh
per-direction processes. The two measurement scopes are not interchangeable.
