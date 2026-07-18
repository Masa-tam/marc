# References

These implementation references are indexed from [`README.md`](README.md).

## Build and dependency automation

- GitHub, `actions/runner-images`, "Available Images", consulted 2026-07-12.
  The CI pins `windows-2025-vs2026` for the Visual Studio 2026 baseline and
  `ubuntu-24.04` for the portable Ninja build.
  <https://github.com/actions/runner-images>
- GitHub, `actions/checkout` releases, consulted 2026-07-12. CI uses the current
  major v6 and requests recursive submodules.
  <https://github.com/actions/checkout/releases>
- GitHub Docs, "Dependabot supported ecosystems and repositories" and
  "Keeping your actions up to date with Dependabot", consulted 2026-07-12.
  These document the `gitsubmodule` and `github-actions` ecosystems.
  <https://docs.github.com/en/code-security/reference/supply-chain-security/supported-ecosystems-and-repositories>
  <https://docs.github.com/en/code-security/how-tos/secure-your-supply-chain/secure-your-dependencies/auto-update-actions>

References are recorded before work begins on each codec or stream
representation. Algorithm implementation source code is not used as a design
reference.

Build and language baselines:

- ISO/IEC 14882:2020, Programming Languages — C++.
- CMake documentation for the minimum supported CMake release.

Test infrastructure only:

- GoogleTest v1.17.0, commit `52eb8108c5bdec04579160ae17225d66034bd723`,
  BSD-3-Clause. Used only to organize and report tests; it is not a compression
  algorithm implementation reference.
- CMake `GoogleTest` module documentation for `gtest_discover_tests`.

Blocked Huffman design references:

- David A. Huffman, "A Method for the Construction of Minimum-Redundancy
  Codes," Proceedings of the IRE, volume 40, issue 9, 1952. Used for the
  prefix-code construction principle.
  <https://www.cse.iitd.ac.in/~pkalra/siv864/huffman_1952.pdf>
- Lawrence L. Larmore and Daniel S. Hirschberg, "A Fast Algorithm for Optimal
  Length-Limited Huffman Codes," Journal of the ACM, volume 37, issue 3, 1990.
  Used for the Package-Merge length-limiting method.
  <https://ics.uci.edu/~dhirschb/pubs/LenLimHuff.pdf>
- ITU-T Recommendation T.81, Annex C, 1992. Used as a primary specification
  reference for deterministic canonical Huffman table generation concepts;
  marc does not implement the JPEG representation.
  <https://www.w3.org/Graphics/JPEG/itu-t81.pdf>

No implementation source code was consulted for these primitives.

Adaptive Huffman design references:

- Robert G. Gallager, "Variations on a Theme by Huffman," IEEE Transactions on
  Information Theory, volume 24, issue 6, pages 668-674, 1978,
  DOI `10.1109/TIT.1978.1055959`. Used for the sibling-property
  characterization and adaptive prefix-code foundation.
  <https://doi.org/10.1109/TIT.1978.1055959>
- Donald E. Knuth, "Dynamic Huffman Coding," Journal of Algorithms, volume 6,
  issue 2, pages 163-180, 1985, DOI `10.1016/0196-6774(85)90036-7`.
  Used for the FGK node-order update model.
  <https://doi.org/10.1016/0196-6774(85)90036-7>
- Newton Faller, "An Adaptive System for Data Compression," Record of the 7th
  Asilomar Conference on Circuits, Systems and Computers, pages 593-597, 1973.
  Bibliographic provenance for the independently originated adaptive method;
  full text was not consulted.

No Adaptive Huffman implementation source, source-derived pseudocode, or test
suite was consulted.

Dynamic Range Coder design reference:

- G. Nigel N. Martin, "Range Encoding: An Algorithm for Removing Redundancy
  from a Digitised Message," Video and Data Recording Conference,
  Southampton, 1979. Used for the finite-precision interval subdivision and
  range-normalization foundation.
  <https://aghorui.github.io/stuff/docs/ffmpeg-flif-gsoc-2020/renc.pdf>

The byte normalization, delayed-carry representation, adaptive model, frame
layout, and vectors are independently specified for marc. No range-coder
implementation source, source-derived pseudocode, or test suite was consulted.

LZ77 design reference:

- Jacob Ziv and Abraham Lempel, "A Universal Algorithm for Sequential Data
  Compression," IEEE Transactions on Information Theory, volume 23, issue 3,
  pages 337-343, May 1977, DOI `10.1109/TIT.1977.1055714`. Used for the
  recent-history maximum-length copying foundation.
  <https://www.itsoc.org/publications/papers/a-universal-algorithm-for-sequential-data-compression>

marc's parameters, fixed token serialization, deterministic tie breaking,
terminal-match form, frame resets, limits, and vectors are independently
specified. No LZ77 implementation source, source-derived pseudocode, container
format, or test suite was consulted.

The LZ77 plus Blocked Huffman pipeline composes only the two repository-defined
representations above. It introduces no new algorithmic reference and does not
use an external combined-container implementation or test suite.

LZSS design reference:

- James A. Storer and Thomas G. Szymanski, "Data Compression via Textual
  Substitution," Journal of the ACM, volume 29, issue 4, pages 928-951,
  October 1982, DOI `10.1145/322344.322346`. Used for the principle that a
  dictionary substitution is emitted only when its explicit representation is
  shorter than the text it replaces.
  <https://doi.org/10.1145/322344.322346>

marc's byte-token representation, parameters, exact cost test, deterministic
match selection, overlap rule, frame resets, limits, and vectors are
independently specified. No LZSS implementation source, source-derived
pseudocode, container format, or test suite was consulted.

LZ78 design reference:

- Jacob Ziv and Abraham Lempel, "Compression of Individual Sequences via
  Variable-Rate Coding," IEEE Transactions on Information Theory, volume 24,
  issue 5, pages 530-536, September 1978,
  DOI `10.1109/TIT.1978.1055934`. Used for the finite phrase-dictionary and
  `(phrase index, next symbol)` parsing foundation.
  <https://doi.org/10.1109/TIT.1978.1055934>

marc's fixed-width token representation, explicit final-index form, bounded
dictionary-freeze rule, parameters, frame resets, limits, and vectors are
independently specified. No LZ78 implementation source, source-derived
pseudocode, container format, or test suite was consulted.

LZW design reference:

- Terry A. Welch, "A Technique for High-Performance Data Compression,"
  *Computer*, volume 17, issue 6, pages 8-19, June 1984,
  DOI `10.1109/MC.1984.1659158`. Used for the initial byte-string table,
  longest-known-string emission, and decoder table-reconstruction foundation.
  <https://doi.org/10.1109/MC.1984.1659158>

marc's code space, frame termination, LSB-first packing, exact width-change
schedule, dictionary-freeze rule, parameters, malformed-stream policy, and
vectors are independently specified. No LZW implementation source,
source-derived pseudocode, external container format, or test suite was
consulted.

LZD design references:

- Keisuke Goto, Hideo Bannai, Shunsuke Inenaga, and Masayuki Takeda,
  "LZD Factorization: Simple and Practical Online Grammar Compression with
  Variable-to-Fixed Encoding," Proceedings of CPM 2015, LNCS 9133,
  pages 219-230, 2015, DOI `10.1007/978-3-319-19929-0_19`. Used for the
  Lempel-Ziv Double rule that each new factor concatenates the two longest
  matching previous factors or alphabet symbols.
  <https://doi.org/10.1007/978-3-319-19929-0_19>
- Golnaz Badkobeh, Travis Gagie, Shunsuke Inenaga, Tomasz Kociumaka, Dmitry
  Kosolobov, and Simon J. Puglisi, "On Two LZ78-style Grammars: Compression
  Bounds and Compressed-Space Computation," arXiv:1705.09538, 2017. Used as a
  formal-definition and worked-factorization cross-check; its accompanying
  implementation and supplementary source were not consulted.
  <https://arxiv.org/abs/1705.09538>

marc's byte-reference namespace, absent-right terminal form, fixed token
serialization, bounded dictionary-freeze rule, frame reset, malformed-stream
policy, and vectors are independently specified. No LZD implementation source,
source-derived pseudocode, corpus, container format, or test suite was
consulted.

LZMW design references:

- Victor S. Miller and Mark N. Wegman, "Variations on a Theme by Ziv and
  Lempel," *Combinatorial Algorithms on Words*, NATO ASI Series F, volume 12,
  pages 131-140, Springer, 1985, DOI `10.1007/978-3-642-82456-2_9`. Used for
  the Miller-Wegman adjacent-phrase concatenation foundation.
  <https://doi.org/10.1007/978-3-642-82456-2_9>
- Golnaz Badkobeh, Travis Gagie, Shunsuke Inenaga, Tomasz Kociumaka, Dmitry
  Kosolobov, and Simon J. Puglisi, "On Two LZ78-style Grammars: Compression
  Bounds and Compressed-Space Computation," 2017, arXiv:1705.09538. Used for
  the formal longest-prefix definition and the published
  `abbaababaaba$` factorization cross-check. The linked supplementary source
  and experimental implementation were intentionally not consulted.
  <https://arxiv.org/abs/1705.09538>

marc's fixed references, duplicate-entry numbering, smallest-reference tie
break, bounded freeze rule, frame reset, limits, and byte vectors are
independently specified. No LZMW implementation source, source-derived
pseudocode, supplementary code, container format, or test suite was consulted.

rANS design references:

- Jarek Duda, "Asymmetric Numeral Systems," arXiv:0902.0271, 2009. Used for
  the ANS state-machine and asymmetric numeral-system foundation.
  <https://arxiv.org/abs/0902.0271>
- James Townsend, "A tutorial on the range variant of asymmetric numeral
  systems," arXiv:2001.09186, 2020. Used for a mathematical cross-check of the
  rANS encode/decode inverse equations; accompanying implementation source was
  not consulted.
  <https://arxiv.org/abs/2001.09186>

Frequency normalization, tie breaking, byte renormalization, state layout,
descriptor layout, and vectors are independently specified for marc. No ANS
implementation source, source-derived pseudocode, or test suite was consulted.

tANS design reference:

- Jarek Duda, "Asymmetric numeral systems: entropy coding combining speed of
  Huffman coding with compression rate of arithmetic coding," arXiv:1311.2540,
  2013. Used for the finite-state interval, symbol-spreading, inverse table, and
  bulk bit-transfer foundations of tabled ANS.
  <https://arxiv.org/abs/1311.2540>

marc's normalization, deterministic spreading step, descriptor, bit layout,
terminal-state rule, and vectors are independently specified below. No tANS or
FSE implementation source, source-derived pseudocode, or test suite was
consulted.

Build and interoperability workflow reference:

- GitHub Docs, "Store and share data with workflow artifacts." Used only for
  the workflow artifact publication and retention model; it is not an
  algorithm or stream-format reference.
  <https://docs.github.com/en/actions/tutorials/store-and-share-data>

CRC-32C references:

- RFC 3385, "Internet Protocol Small Computer System Interface (iSCSI)
  Cyclic Redundancy Check (CRC)/Checksum Considerations," September 2002.
  Used for selection and identification of the Castagnoli CRC-32C polynomial.
  <https://www.rfc-editor.org/rfc/rfc3385>
- RFC 3720, "Internet Small Computer Systems Interface (iSCSI)," April 2004,
  Section 12.1 and Appendix B. Used for the reflected CRC-32C parameters and
  independently published check values. The accompanying example source code
  was intentionally not consulted.
  <https://www.rfc-editor.org/rfc/rfc3720>

marc independently specifies its algorithm ID, byte-at-a-time reference update,
little-endian digest serialization, API lifecycle, and tests. No CRC library or
implementation source was consulted.

SHA-256 references:

- National Institute of Standards and Technology, FIPS PUB 180-4, "Secure Hash
  Standard (SHS)," August 2015, Sections 5.1.1, 5.2.1, 6.2, and 8. Used for
  SHA-256 padding, initial values, constants, message schedule, compression
  function, and standard digest representation.
  <https://csrc.nist.gov/pubs/fips/180-4/upd1/final>
- NIST Cryptographic Standards and Guidelines, "Examples with Intermediate
  Values." Used only for independently published SHA-256 check messages and
  digests; downloadable implementation source was not consulted.
  <https://csrc.nist.gov/projects/cryptographic-standards-and-guidelines/example-values>

marc independently specifies hash ID 2, bounded incremental buffering, checked
message-length policy, API lifecycle, and tests. No SHA implementation source,
generated constants, library code, or external test suite was consulted.

The hash descriptor record is a repository-defined serialization layer. Its
algorithm IDs refer to the CRC-32C and SHA-256 definitions above, while its
target/scope IDs, fixed layout, validation rules, and vectors were designed
independently for marc. Its tuple ordering, duplicate policy, and two-pass
publication contract are also repository-native; no external container format
was consulted.

The staged version 1.1 prefix gate reuses marc's independently defined 1.0
prefix and hash region. Its version isolation and combined-limit policy are
internal format-evolution decisions and use no new external reference.

The initial per-frame checksum profile reuses the documented CRC-32C algorithm
and repository-native descriptor. Its exact raw-byte inclusion range, reset
boundary, trailer placement, and single-descriptor restriction are independent
marc format decisions; no additional external format was consulted.

The staged version 1.1 frame-header gate reuses marc's fixed frame header and
initial checksum profile. Its three-way agreement rule and version isolation
are repository-native validation policy and use no new external reference.

The complete raw-checksum reference profile is a composition of marc's own
prefix, descriptor, frame, and CRC components. Its two-pass publication policy
and hand vectors were independently defined without an external container or
codec reference.

The raw-checksum public-ABI completion matrix uses only that repository-defined
format, its public process contract, and the general data classes required by
AGENTS.md. No additional algorithm, format, implementation, corpus, or external
test-suite reference was used.

The Adaptive Huffman stream fuzz boundary uses only marc's independently
specified FGK variant, outer framing, decoder limits, and transform contract.
Its seed and input schedules are repository-authored; no external corpus,
fuzzer harness, or implementation behavior was consulted.

The Adaptive Huffman command-line selector composes only marc's independently
specified FGK profile, public C ABI, bounded workspace policy, and common file
adapter. No external compression tool, command-line implementation, archive
format, or test suite was consulted.

The Adaptive Huffman benchmark composes only marc's independently specified
FGK profile, public C ABI, profile sizing, and repository measurement contract.
No external benchmark harness, published result, implementation, or test suite
was consulted.

The Adaptive Huffman completion matrix uses only marc's public FGK C ABI,
repository-defined format, required data classes, deterministic generator, and
process-contract assertions. No external vector, corpus, test suite, or
implementation behavior was consulted.

The Dynamic Range stream fuzz boundary uses only marc's independently defined
integer range-coder variant, framing, model-total rule, decoder limits, and
transform contract. Its seed and schedules are repository-authored; no
external range-coder corpus, harness, or implementation was consulted.

The Dynamic Range command-line selector composes only marc's independently
specified integer range profile, public C ABI, bounded workspace policy, and
common file adapter. No external range-coder tool, command-line implementation,
archive format, or test suite was consulted.

The Dynamic Range benchmark composes only marc's independently specified
integer range profile, public C ABI, profile bounds, and repository measurement
contract. No external benchmark harness, published result, range-coder source,
or test suite was consulted.

The Dynamic Range completion matrix uses only marc's public integer range C
ABI, repository-defined format, required data classes, deterministic generator,
and process-contract assertions. No external vector, corpus, test suite, or
implementation behavior was consulted.

The rANS stream fuzz boundary uses only marc's independently specified scalar
rANS format, normalized-table limits, block views, outer framing, and transform
contract. Its seed and schedules are repository-authored; no external ANS
corpus, fuzz harness, or implementation behavior was consulted.

The rANS command-line selector composes only marc's independently specified
scalar rANS profile, public C ABI, bounded workspace policy, and common file
adapter. No external ANS tool, command-line implementation, archive format, or
test suite was consulted.

The rANS benchmark composes only marc's independently specified scalar profile,
public C ABI, profile bounds, aligned view policy, and repository measurement
contract. No external benchmark harness, published result, ANS source, or test
suite was consulted.

The rANS completion matrix uses only marc's public scalar C ABI,
repository-defined format, required data classes, deterministic generator,
aligned view contract, and process assertions. No external vector, corpus, test
suite, or implementation behavior was consulted.

The standalone Blocked Huffman fuzz boundary uses only marc's bounded canonical
Huffman primitives, raw-block alternative, block views, outer framing, and
transform contract. Its seed and schedules are repository-authored; no external
Huffman corpus, fuzz harness, table, or implementation behavior was consulted.

The tANS stream fuzz boundary uses only marc's independently specified tabled
ANS format, fixed table-log rule, block views, outer framing, and transform
contract. Its seed and schedules are repository-authored; no external FSE/ANS
corpus, fuzz harness, or implementation behavior was consulted.

The tANS command-line selector composes only marc's independently specified
tabled profile, public C ABI, bounded workspace and aligned-view policy, and
common file adapter. No external FSE/ANS tool, command-line implementation,
archive format, or test suite was consulted.

The tANS benchmark composes only marc's independently specified tabled profile,
public C ABI, 12-bit transition bound, aligned view policy, and repository
measurement contract. No external benchmark harness, published result,
FSE/ANS source, or test suite was consulted.

The tANS completion matrix uses only marc's public tabled C ABI,
repository-defined format, required data classes, deterministic generator,
aligned view contract, and process assertions. No external vector, corpus, test
suite, or implementation behavior was consulted.

The standalone LZ77 stream fuzz boundary uses only marc's independently
specified fixed token representation, outer framing, decoder limits, and
transform contract. Its seed and schedules are repository-authored; no
external LZ corpus, fuzz harness, or implementation behavior was consulted.

The standalone Blocked Huffman command-line selector composes only marc's
public C ABI, repository-defined format, bounded profile, and common atomic
file adapter. No external command-line tool, archive format, or implementation
behavior was consulted.

The standalone Blocked Huffman benchmark composes only marc's public C ABI,
repository-defined format, profile sizing, and existing measurement contract.
No external benchmark harness, compression tool, or implementation behavior
was consulted.

The standalone Blocked Huffman completion matrix uses only the repository's
public C ABI, format, required data classes, deterministic generator, and
existing process-contract assertions. No external vectors, corpus, test suite,
or implementation behavior was consulted.

The standalone LZ77 completion matrix uses only marc's public C ABI,
repository-defined fixed-token stream, required data classes, deterministic
generator, and process-contract assertions. No external LZ vectors, corpus,
test suite, or implementation behavior was consulted.

The standalone LZSS completion matrix uses only marc's public C ABI,
repository-defined variable-token stream and literal/match cost rule, required
data classes, deterministic generator, and process-contract assertions. No
external LZSS vectors, corpus, test suite, or implementation behavior was
consulted.

The standalone LZ78 completion matrix uses only marc's public C ABI,
repository-defined phrase-index token stream, required data classes,
deterministic generator, aligned-view contract, and process assertions. No
external LZ78 vectors, corpus, test suite, or implementation behavior was
consulted.

The supplemental LZW public completion matrix uses only marc's public C ABI,
repository-defined packed-code stream, required data classes, deterministic
generator, aligned-view contract, and process assertions. No external LZW
vectors, corpus, test suite, or implementation behavior was consulted.

The strengthened LZD completion matrix uses only marc's public C ABI,
repository-defined reference-pair stream, deterministic generator, aligned
workspace contract, and process assertions. No external LZD vectors, corpus,
test suite, or implementation behavior was consulted.

The strengthened LZMW completion matrix uses only marc's public C ABI,
repository-defined fixed-reference stream, deterministic generator, aligned
workspace contract, and process assertions. No external LZMW vectors, corpus,
test suite, or implementation behavior was consulted.

The baseline-readiness matrix is derived only from repository-owned format,
test, C ABI, CLI, benchmark, fuzz, CI, and interoperability records. No external
completion checklist, product comparison, or third-party implementation status
was consulted.

Interoperability schema 3 composes only marc's public CLI profiles, frozen
earlier manifest rules, repository-generated fixture, and SHA-256 metadata. No
external archive format, interoperability suite, or third-party tool behavior
was consulted.

Interoperability schema 4 extends only marc's frozen schema-3 profile order
with the repository-defined LZSS and LZ78 Blocked Huffman CLI profiles. No
external combined-codec archive, compatibility suite, manifest, or test vector
was consulted.

The LZSS plus Blocked Huffman frame codec composes only marc's
repository-defined transactional LZSS variant 1 codec, Blocked Huffman variant
1 representation, generic frame header, checked arithmetic, and decoder
limits. No external combined format, implementation, vector, or test suite was
consulted.

The LZSS plus Blocked Huffman complete-stream controller uses only marc's
version 1.0 stream header, LZSS parameter serialization, combined frame codec,
and two-pass atomic decode convention. No external container, stream scanner,
profile, vector, or implementation was consulted.

The LZSS plus Blocked Huffman incremental encoder uses only marc's
`ProcessResult` contract, complete-stream oracle, exact frame planner/encoder,
and caller-owned workspace policy. No external streaming encoder, buffering
scheme, source, or test schedule was consulted.

The LZSS plus Blocked Huffman incremental decoder uses only marc's prefix and
frame parsers, transactional combined frame decoder, `ProcessResult` contract,
and caller-owned staging policy. No external streaming decoder, parser state
machine, source, malformed corpus, or test schedule was consulted.

The LZSS plus Blocked Huffman profile and workspace calculation use only
marc's documented LZSS all-Literal worst case, Blocked Huffman raw-fallback
layout, generic frame header, local decoder limits, checked arithmetic, and
existing internal profile conventions. No external profile API, allocator,
workspace formula, implementation, or test suite was consulted.

The LZSS plus Blocked Huffman C adapter uses only marc's public opaque-transform
lifecycle, size-tagged configuration convention, DD-215 workspace query, and
the repository's combined streaming transforms. No external compression ABI,
binding, allocator interface, source, or C test suite was consulted.

The `lzss-blocked-huffman` CLI adapter composes only marc's public combined C
factory, existing bounded file-processing loop, atomic temporary-output policy,
and repository-defined frame/block defaults. No external compression command,
option vocabulary, file workflow, implementation, or CLI test was consulted.

The `lzss-blocked-huffman` benchmark composes only marc's public combined C
factory, documented profile bounds, existing measurement loop, and queried
workspace accounting. No external benchmark harness, compression comparison,
measurement code, implementation source, or performance result was consulted.

The combined LZSS fuzz boundary uses only marc's strict and incremental
decoders, local limit model, fixed-workspace policy, `ProcessResult` invariants,
and repository-owned canonical stream. No external fuzzer harness, malformed
corpus, compression implementation, source-derived seed, or regression suite
was consulted.

The public-profile evidence matrix and combined LZSS completion test use only
the repository's C ABI contract, required test classes in `AGENTS.md`, existing
marc-owned completion-test conventions, and the already specified combined
stream representation. No external API matrix, compression test suite,
implementation, stream corpus, or compatibility claim was consulted.

The pre-publication CI audit consulted the official
[GitHub Actions runner-image table](https://github.com/actions/runner-images#available-images),
[`actions/checkout` usage and releases](https://github.com/actions/checkout),
[workflow-artifact documentation](https://docs.github.com/en/actions/tutorials/store-and-share-data),
and the official
[GoogleTest 1.17.0 release record](https://github.com/google/googletest/releases/tag/v1.17.0).
These were used only to verify hosted infrastructure and the pinned test
dependency. No compression implementation source was consulted.

The LZ78 plus Blocked Huffman composition specification uses only marc's
already documented LZ78 variant 1 token grammar, Blocked Huffman variant 1
block format, generic frame format, checked workspace policy, and the original
LZ78 references recorded above. No external combined codec, implementation
source, profile, stream, test vector, or workspace layout was consulted.
The later profile-sizing and typed-partition work likewise derives only from
marc's private record sizes, checked arithmetic helpers, and established
three-region C ABI convention; no external allocator or layout implementation
was used.
The incremental transforms reuse only marc's existing composed-profile state
machine and the LZ78-specific typed partition contract; no external streaming
adapter, combined codec, or error policy was consulted.
The public C binding follows marc's existing caller-owned three-region ABI and
the DD-226 internal partition helpers. No external compression API, allocator,
workspace layout, or language binding was consulted.
The public completion matrix is derived only from AGENTS.md data classes and
marc's existing public-profile evidence contract; no external corpus, test
suite, or combined-codec vector was used.
The bounded fuzz adapter derives only from marc's incremental transform
contract, local decoder limits, private typed record sizes, and existing
sanitizer target conventions. No external fuzz harness, corpus, or combined
codec was consulted.
The `lz78-blocked-huffman` CLI adapter uses only marc's public combined C
factory, existing file-transaction policy, and fixed documented workspace
bounds. No external command-line codec, wrapper, or allocation policy was
consulted.
The `lz78-blocked-huffman` benchmark adapter uses only that same public C
factory, fixed CLI profile, and marc's existing measurement contract. No
external benchmark implementation, result, corpus, or combined-codec tuning
was consulted.

The LZW plus Blocked Huffman composition specification uses only marc's
already documented LZW variant 1 packed-code grammar, Blocked Huffman variant 1
block format, generic frame format, checked workspace policy, and the original
LZW reference recorded above. No external combined codec, format, source,
profile, stream, test vector, or workspace layout was consulted.
The public completion matrix derives only from AGENTS.md data classes, marc's
existing evidence contract, and the profile's independently implemented C ABI.
No external corpus, test suite, vector, or combined-codec behavior was used.
The bounded fuzz adapter derives only from marc's incremental decoder contract,
checked LZW workspace arithmetic, and existing sanitizer target conventions.
No external fuzz harness, corpus, dictionary, or crash collection was used.
The `lzw-blocked-huffman` CLI adapter uses only marc's public combined C
factory, existing transactional file policy, fixed profile bounds, and common
CLI test harness. No external command-line codec, wrapper, allocation policy,
source, or test was consulted.
