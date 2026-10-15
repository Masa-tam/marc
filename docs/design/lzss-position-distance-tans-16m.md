# Sixteen-MiB position-distance tANS

## DD-1606: exact sixteen-MiB specialization

This specialization uses the native position-distance tANS family with
`F = 16,777,216` and `N = 24`. Its distinct identity is dictionary `2/12`,
context `1/25`, and entropy `5/7`. The configured dictionary window and
maximum frame are both 16,777,216 bytes; grammar lengths remain 3 through
258. The encoder, when admitted, uses fixed-five eligibility, exact longest
matching, and nearest-distance tie breaking.

There are 56 contexts and 4,658 frequencies. The alphabet sequence is
`[2]*3 + [256]*17 + [9]*3 + [25]*9 + [2]*24`. The descriptor follows the
family 24-byte prefix and has a seven-byte active mask with all 56 bits used. Its maximum extent is
9,291 bytes. States are 4096 through 8191, table log is 12, and the payload
starts with the little-endian final-state delta followed by prepended LSB-first
bits. Empty entropy is the zero-mask descriptor and `00 00` payload.

Each nonempty frame resets the dictionary and entropy state. With token count
T and raw extent R no greater than F, decision count D satisfies
`2T <= D <= min(34T, 10R)`. Unknown identities, noncanonical models, invalid
state/counts, truncation, padding, or trailing bytes reject without publishing
a failed frame. No public factory, CLI selector, benchmark, or bundle entry is
admitted by this definition alone.

`tests/position_tans_16m_vectors.py` independently derives 234 descriptor and
payload vectors, including boundary lengths and distances through 16 MiB. Its
successful generation is evidence for this definition only; implementation and
public qualification remain incomplete.

The internal entropy and token implementations pass their dedicated core and
token tests. These check canonical descriptor rejection, terminal state and
padding, uniform-bank separation, overlap and budget rejection, discarded
private token prefixes, and retention of ordinary destinations on failure.
The C++ core also agrees with 236 independent oracle fixtures. The oracle
was corrected to emit bits from the pre-transition state. The inherited
eight-MiB oracle was separately corrected and passed 234 core fixtures. Frame, public API/CLI,
benchmark, fuzz and exchange completion remain unproven.

The bounded stream/frame-header validator is implemented. It verifies the
exact identity, fixed stream layout, known-size partition, sequence, descriptor
canonicality before payload buffering, and decision/payload ceilings. Tests
check the singleton literal header bytes, truncations, malformed fields,
ordinary-output retention, exact private-footprint admission and rejection one
byte below it. The 233,472 decode-table entries include the uniform bank.
Metadata-only ceiling tests supply explicit limits and allocate no full frame;
their limits do not establish public defaults or measured resource admission.
Frame entropy completion, raw reconstruction, streaming quarantine and public
connection remain separate unfinished gates.

The private 16 MiB frame layer is now implemented. It admits a planned token
sequence only after bounded validation, writes descriptor and payload bytes to
private scratch, and publishes the serialized frame only after every stage
succeeds. The decoder validates the complete descriptor before payload
buffering, decodes into private tokens, reconstructs into private raw storage,
and exposes raw bytes only while draining a validated frame. A truncated
frame, invalid history, or entropy failure therefore leaves ordinary frame and
raw destinations unchanged. The first frame and streaming split-boundary
tests pass; public API/CLI, measurements, fuzzing, exchange, and full-frame
corpus qualification remain unfinished.

The public C API now exposes the distinct `2/12 + 1/25 + 5/7` profile and
reports its bounded ownership and private-admission requirements before
allocation. The CLI selector is
`lzss-position-distance-tans-16m`. Fine-grained API processing, a two-frame
failure-publication regression, and a CLI encode/decode smoke round trip pass
with the updated implementation. Benchmark, fuzz, exchange, and corpus
qualification are still open.

The 16 MiB benchmark executable and smoke test are present. Its contextual
control is the published profile 16 configured to a 16 MiB window and frame;
the measurement harness uses an explicit 512 MiB compressed-payload admission
limit because the public workspace query measured 176,169,991 bytes for the
contextual encoded frame. The full twelve-member corpus comparison was
started but remains pending the process-idle audit; no partial corpus result is
treated as a qualification receipt.

The stream sanitizer harness is also present and was built with the updated
Clang toolchain. A 2,048-input bounded run completed without a crash, hang, or
sanitizer finding; it checks split versus whole input, terminal-state
stability, output-tail retention, and the failed-frame publication boundary.
This is a fuzz pilot receipt, not the full public fuzz campaign.

The exchange scripts now admit schema76 as the schema75 prefix plus
`lzss-position-distance-tans-16m`. The 86-entry local producer and verifier
agree on the exact identity `2/12 + 1/25 + 5/7`; the appended 3,234-byte
fixture archive has SHA-256
`fc2e42e18e953942e742b6b5ea1ab78f9ccc67a051c5cebe5373f27390ac0a44`.
The schema76 negative campaign rejects 125 malformed manifests or header
mutations before codec launch and before output publication. The compatibility
audit restores and re-encodes schemas 1 through 75 after deriving schema76.
Independent second-producer, fixed-Release, hosted-CI, external-route,
full-corpus measurement, and release similarity gates remain open.

### Completed same-window corpus and resident-memory measurement

The resumed sequential campaign completed all twelve Silesia members with
exact raw restoration. The stored headers confirm native tuple
`2/12 + 1/25 + 5/7` and contextual tuple `2/5 + 1/4 + 5/2`. Both use a
16,777,216-byte frame and window. The control applies the public 16 MiB
profile with a 512 MiB compressed-payload admission limit, 1 GiB internal
limit, 9 times the frame size as its decision limit and known original size.
Those limits are not physical memory measurements.

Each cell is position/contextual. Timings are single samples, including
creation and processing. Process peak RSS covers the whole encode/decode
process and retained input/archive buffers; it is not a direction-specific
peak or an API query. MiB is 1,048,576 bytes.

| Member | Archive bytes P/C | Encode seconds P/C | Decode seconds P/C | Process peak MiB P/C |
| --- | ---: | ---: | ---: | ---: |
| dickens | 3,150,509/3,154,985 | 12.478/12.902 | 0.203/0.276 | 348.81/223.07 |
| mozilla | 18,140,320/18,860,594 | 89.964/107.477 | 0.935/1.581 | 508.84/369.67 |
| mr | 3,201,347/3,316,381 | 21.596/54.746 | 0.208/0.283 | 345.96/222.93 |
| nci | 2,211,420/2,214,379 | 40.505/77.048 | 0.164/0.204 | 445.45/317.16 |
| ooffice | 3,014,573/3,019,638 | 3.141/1.911 | 0.202/0.264 | 293.91/215.13 |
| osdb | 3,178,299/3,197,718 | 10.605/3.147 | 0.187/0.269 | 347.52/224.99 |
| reymont | 1,535,543/1,538,735 | 10.521/12.038 | 0.112/0.142 | 294.48/213.24 |
| samba | 4,774,836/4,778,760 | 12.523/13.192 | 0.261/0.392 | 445.18/312.24 |
| sao | 5,095,405/5,229,190 | 6.989/3.067 | 0.238/0.395 | 319.82/223.40 |
| webster | 9,856,122/9,870,235 | 103.593/87.131 | 0.581/0.975 | 472.70/339.03 |
| xml | 550,100/550,945 | 0.792/0.762 | 0.056/0.063 | 272.33/209.33 |
| x-ray | 5,200,429/5,361,511 | 5.236/2.204 | 0.300/0.434 | 337.02/224.03 |

Totals are **59,908,903/61,093,071 bytes**, a **1,184,168-byte (1.938302%)**
position saving, with position smaller on every corpus member. Sum of sample
encode times is 317.942859/375.625776 seconds and decode times
3.447699/5.276111 seconds. Maximum process peaks are
533,553,152/387,629,056 bytes, both on mozilla. Position decode is faster on
all twelve members in this run. Encode is faster on six members and slower
on ooffice, osdb, sao, webster, xml and x-ray. Position resident memory is
higher for every member. These corpus-specific single samples do not
establish universal speed or size advantages. Public-factory/full-window
sanitizer qualification and source-expression review remain separate gates;
IX-0093 records the reported CI and external exchange evidence.
