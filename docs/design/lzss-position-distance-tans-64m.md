# Sixty-four-MiB position-distance tANS

## DD-1608: exact sixty-four-MiB specialization

The native position-distance tANS specialization uses `F = 67,108,864` and
`N = 26`. Its emitted identity is dictionary `2/14`, context `1/26`, and entropy
`5/9`. The alphabet sequence is `[2]*3 + [256]*17 + [9]*3 + [27]*9 + [2]*26`,
with 58 adaptive contexts and 4,680 frequencies. The descriptor keeps the
24-byte prefix, uses an eight-byte active mask, and has a maximum extent of
9,334 bytes. States are 4096 through 8191, table log is 12, and payload bits
are LSB-first after the little-endian final-state delta.

The original design reserved context variant 27, but the implemented stream
serializer, parser, fixed header test, and retained schema78 archive all use
26. The dictionary/entropy variants distinguish this complete tuple from the
32 MiB tuple `2/13 + 1/26 + 5/8`. The documentation now records the emitted
tuple; no existing stream bytes or accepted identities are changed.

Frames reset dictionary and entropy state. For token count `T`, raw extent `R`,
and decision count `D`, the validator requires `2T <= D <= min(34T, 10R)`.
Descriptor, payload, padding, state, history, and sequence validation precede
publication. Failed entropy, token, or reconstruction work remains in private
scratch and leaves ordinary destinations unchanged.

The independent oracle derives 237 vectors, including the 64 MiB distance
boundaries and a dense descriptor at the declared 9,334-byte limit. Clang core,
token, frame-format, frame, stream, vector, and oracle-differential tests pass
locally. The public API resource query, fixed-capacity process path, CLI
selector, deterministic small-input round trips, malformed-input rejection,
and unpublished failed-frame check pass locally. The local schema-78 bundle
contains 88 archives, preserves the preceding order, and passed self
verification, 125 manifest/header rejection cases before codec launch, and
the schema-1 through schema-78 compatibility audit. The benchmark smoke also
passed, but its recorded small fixture is 1,125 bytes for position-distance
versus 1,104 bytes for the contextual control, so the smaller-than-contextual
target is not met by that fixture and no corpus-wide claim is made.

The benchmark smoke emits deterministic position/contextual measurements with
encode and decode timing plus queried workspace bytes. The fuzz target and
compile smoke are wired; sanitizer runtime execution is a separate GNU-driver
qualification step and is not claimed by this milestone. Cross-producer,
hosted-CI, external-route, full-corpus measurement, and similarity review
remain open release gates.
The updated Clang 22.1.3 build re-ran the ten 64 MiB CLI, benchmark-smoke,
core, token, frame-format, frame, stream, API, vector, and differential tests;
all ten passed in 54.38 seconds.

The first corpus measurement completed for the retained `dickens` material:
position-distance produced 3,150,534 bytes and contextual tANS produced
3,155,009 bytes, a 4,475-byte position-distance saving. Position encode
samples were 17.113141/23.8587048/23.1631539 seconds and contextual samples
were 13.0996804/13.1251859/12.7370649 seconds. Decode samples were
0.3206042/0.3250113/0.3242852 and 0.2842562/0.2857147/0.2855767 seconds,
respectively. The queried workspace values were 4,108,825,632 versus
311,569,567 bytes for encode and 1,692,594,118 versus 1,423,439,767 bytes
for decode. The remaining eleven materials were not completed because the
next large-material position run produced no result within the local
observation window; no corpus total or peak-resident claim is made.

An additional `mr` comparison completed with the single-sample mode:
position-distance produced 3,201,375 bytes and contextual produced 3,316,407
bytes, a 115,032-byte position-distance saving. Position encode/decode took
25.2601615/0.3302469 seconds and contextual encode/decode took
56.316897/0.2969483 seconds. Queried workspace values were
4,105,241,650/1,692,372,236 versus 301,551,007/1,423,217,885 bytes. These
are single samples and are not mixed with the three-sample `dickens` results.

The completed eight-material size comparison is:

| material | position bytes | contextual bytes | position saving |
| --- | ---: | ---: | ---: |
| dickens | 3,150,534 | 3,155,009 | 4,475 |
| mr | 3,201,375 | 3,316,407 | 115,032 |
| ooffice | 3,014,606 | 3,019,670 | 5,064 |
| osdb | 3,178,327 | 3,197,744 | 19,417 |
| reymont | 1,535,568 | 1,538,759 | 3,191 |
| sao | 5,095,422 | 5,229,206 | 133,784 |
| x-ray | 5,200,442 | 5,361,523 | 161,081 |
| xml | 550,133 | 550,977 | 844 |
| samba | 4,709,783 | 4,713,430 | 3,647 |
| nci | 2,101,645 | 2,103,968 | 2,323 |
| **total (10 materials)** | **31,737,835** | **32,186,693** | **448,858** |

The 64 MiB profile was smaller for every completed material. The remaining
two materials (`mozilla` and `webster`) have no completed pair and are
excluded from the total. The benchmark reports queried
workspace bytes and timings, but not peak resident memory; peak-memory
comparison remains unverified.

The long `webster` run was retried with the MSVC Release benchmark build as
an independent local execution path and again produced no result within the
observation window. It remains unmeasured; no compiler-specific performance
conclusion is inferred.

The benchmark now also reports peak resident memory. On its 256-byte probe,
position-distance reported 812,380,160 bytes and contextual tANS reported
811,724,800 bytes. These are process peaks for the paired single-sample run,
not the corpus peak values; the completed corpus rows above therefore retain
their explicit peak-memory-unverified status.

The current benchmark was also rerun once for the completed `dickens`
material with both profiles. Position-distance reproduced 3,150,534 bytes
and measured 18.9698882 seconds encode, 0.3649101 seconds decode, and
969,093,120 bytes peak resident memory. Contextual tANS reproduced 3,155,009
bytes and measured 15.1092503 seconds encode, 0.3208361 seconds decode, and
837,902,336 bytes peak resident memory. This is representative paired
evidence; it does not promote the peak values to a corpus-wide maximum.

The public 64 MiB decoder mutation pilot ran 100 deterministic truncation,
bit-flip, insertion, and replacement cases. It completed without a crash or
timeout and retained the no-output-on-failure contract. This is a bounded
pilot and does not claim sanitizer runtime coverage.

The existing schema-78 88-archive bundle was also verified through the
updated Clang CLI, independently of the fixed Release verifier. All 88
archives restored and re-encoded successfully; the bundle order and archive
hashes remained accepted. Hosted CI and external four-route verification are
still separate gates.

The final local bundle audit confirms that its first 81 archive entries are
byte-identical to the retained schema-77 source, followed by the seven
position-distance tANS selectors in the required order. The fixed Release
executable and its pre-update backup are both present.

On 2026-10-16 the maintainer reports successful hosted CI and all four
88-archive external routes at `28c980a8507aac9ef75b2b5e07c545f01ec91e7b`
(IX-0093). This supplies the CI and external-route evidence left pending
above. The two unmeasured corpus pairs, corpus peak-memory measurements,
sanitizer runtime fuzzing, and similarity review remain open; these exchange
results do not establish those separate completion criteria.

The subsequent 2026-10-16 seeded stream sanitizer campaign completes 4,096
libFuzzer runs with all decoder dependencies under ASan and UBSan (FZ-0092).
It checks split/whole equivalence, output guards and sticky terminal errors,
starting from a successfully reconstructed literal frame. This closes the
bounded stream-runtime pilot; full-window and public-factory sanitizer
coverage and the remaining measurement/review gates are still unproven.

The 64 MiB `mozilla` corpus comparison was retried with the single-sample
benchmark after the external qualification. The position-distance encode
stage produced no result during a 120-second observation window and was
stopped without retaining a partial measurement. `mozilla` therefore remains
excluded from the corpus total; no speed, size or memory value is inferred.

A further retry was observed for five minutes with the same result: no
position-distance encode result was emitted. It was stopped at that measured
limit without retaining a partial output. This strengthens the measured
resource/time boundary but does not turn `mozilla` into a completed pair.

The public 64 MiB factory was probed with a one-byte input to establish its
resource floor. The benchmark queried 4,095,271,087 bytes for encode and
1,679,039,787 bytes for decode, with a 214-byte encoded result. These are
fixed-window workspace requirements rather than input-sized fuzz buffers.
Consequently a sanitizer public-factory fuzz target under the 1,024 MiB
instrumented RSS limit would not be a valid bounded campaign. Public-factory
fuzz remains unclaimed; the CLI mutation and stream sanitizer campaigns are
the bounded evidence for this profile.
