
# Thirty-two-MiB position-distance tANS

## DD-1607: exact thirty-two-MiB specialization

The native position-distance tANS specialization uses `F = 33,554,432` and
`N = 25`. Its identity is dictionary `2/13`, context `1/26`, and entropy
`5/8`. The alphabet sequence is `[2]*3 + [256]*17 + [9]*3 + [26]*9 + [2]*25`,
with 57 contexts and 4,669 frequencies. The descriptor keeps the 24-byte
prefix, uses an eight-byte active mask, and has a maximum extent of 9,313
bytes. States are 4096 through 8191, table log is 12, and payload bits are
LSB-first after the little-endian final-state delta.

Frames reset dictionary and entropy state. For token count `T`, raw extent `R`,
and decision count `D`, the validator requires `2T <= D <= min(34T, 10R)`.
Descriptor, payload, padding, state, history, and sequence validation precede
publication. Failed entropy, token, or reconstruction work remains in private
scratch and leaves ordinary destinations unchanged.

The independent oracle derives 236 vectors, including the 32 MiB distance
boundaries. Clang core, token, frame-format, frame, stream, vector, and
oracle-differential tests pass locally. The public API and CLI now have
deterministic round-trip and malformed-input coverage; the updated Clang
build also passes the new 16/32 MiB CLI campaigns. Benchmark smoke, fuzz
compile smoke, the 16/32 MiB 500-case CLI mutation campaigns, and schema77
exchange admission are recorded separately.
Corpus comparison, sanitizer runtime, cross-producer, hosted-CI, external
routes, and release similarity remain open.

The 2026-10-16 seeded stream sanitizer campaign completes 4,096 libFuzzer
runs with every decoder dependency under ASan and UBSan (FZ-0092).
Its seed passes the normal stream round-trip and truncation assertions.
This supplies bounded stream runtime evidence, while full-window and
public-factory fuzz qualification and corpus measurement remain open.
IX-0093 separately supplies the reported hosted-CI and external-route evidence.

### Completed same-window corpus and resident-memory measurement

All twelve Silesia members restore exactly. Stored headers confirm native
`2/13 + 1/26 + 5/8` and contextual `2/6 + 1/5 + 5/2`, both with frame and
window 33,554,432 bytes. There is no published contextual 32 MiB profile;
the control configures the existing 64 MiB API to that window and frame,
with a 512 MiB payload limit, 1 GiB internal limit, decision limit nine times
the frame size and known original size. These are admission limits, not RSS.

Cells are position/contextual. Times are single samples including creation
and processing. Peak RSS covers the combined encode/decode process and its
retained raw/archive buffers; separate directional peaks were not measured.
MiB is 1,048,576 bytes.

| Member | Archive bytes P/C | Encode seconds P/C | Decode seconds P/C | Process peak MiB P/C |
| --- | ---: | ---: | ---: | ---: |
| dickens | 3,150,522/3,155,009 | 12.298/12.947 | 0.234/0.290 | 540.86/415.13 |
| mozilla | 18,202,379/18,925,440 | 177.394/186.328 | 1.024/1.483 | 924.46/646.93 |
| mr | 3,201,362/3,316,407 | 21.587/54.670 | 0.235/0.304 | 538.02/414.91 |
| nci | 2,101,629/2,103,968 | 143.033/197.465 | 0.190/0.214 | 848.24/590.06 |
| ooffice | 3,014,590/3,019,670 | 2.802/1.873 | 0.204/0.274 | 485.91/407.14 |
| osdb | 3,178,314/3,197,744 | 8.646/3.130 | 0.213/0.281 | 539.53/414.98 |
| reymont | 1,535,556/1,538,759 | 10.470/12.144 | 0.141/0.158 | 487.15/405.21 |
| samba | 4,709,767/4,713,430 | 18.467/20.037 | 0.283/0.389 | 699.87/439.91 |
| sao | 5,095,414/5,229,206 | 6.877/2.966 | 0.263/0.403 | 512.43/413.50 |
| webster | 9,659,595/9,672,905 | 316.854/279.180 | 0.593/0.819 | 883.50/614.62 |
| xml | 550,117/550,977 | 0.837/0.773 | 0.083/0.078 | 464.99/401.40 |
| x-ray | 5,200,436/5,361,523 | 5.217/2.235 | 0.330/0.451 | 528.41/416.08 |

Totals are **59,599,681/60,785,038 bytes**, a **1,185,357-byte (1.950080%)**
position saving. Position is smaller on all twelve members. Sum of encode
samples is 724.480748/773.747992 seconds and decode is 3.792732/5.143081
seconds. Maximum process peaks are 969,367,552/678,350,848 bytes, both on
mozilla. Encode is faster on six members and slower on ooffice, osdb, sao,
webster, xml and x-ray; decode is faster on eleven, with xml the exception.
Position RSS is higher on every member. Single samples do not establish
universal speed or compression advantages. Public-factory sanitizer runtime
and maximum-frame qualification remain separate unfinished gates.
