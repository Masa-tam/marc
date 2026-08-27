# References

These implementation references are indexed from [`README.md`](README.md).

References are recorded before work begins on each codec or stream
representation. Algorithm implementation source code is not used as a design
reference.

## Foundational and project references

### Build and dependency automation

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

### Build and language baselines

- ISO/IEC 14882:2020, Programming Languages — C++.
- CMake documentation for the minimum supported CMake release.

### Test infrastructure only

- GoogleTest v1.17.0, commit `52eb8108c5bdec04579160ae17225d66034bd723`,
  BSD-3-Clause. Used only to organize and report tests; it is not a compression
  algorithm implementation reference.
- CMake `GoogleTest` module documentation for `gtest_discover_tests`.

### Blocked Huffman design references

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
- P. Deutsch, RFC 1951, "DEFLATE Compressed Data Format Specification
  version 1.3," 1996, consulted 2026-08-11. Used only for the independently
  evaluated idea of separating literal/length and distance prefix-code
  alphabets and transmitting canonical code lengths. marc's typed LZSS
  ranges, context schema, descriptor candidates, LSB-first representation,
  and any future Format 2 variant are distinct and are not DEFLATE-compatible.
  <https://www.rfc-editor.org/rfc/rfc1951>

No implementation source code was consulted for these primitives.

### Adaptive Huffman design references

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

### Dynamic Range Coder design reference

- G. Nigel N. Martin, "Range Encoding: An Algorithm for Removing Redundancy
  from a Digitised Message," Video and Data Recording Conference,
  Southampton, 1979. Used for the finite-precision interval subdivision and
  range-normalization foundation.
  <https://aghorui.github.io/stuff/docs/ffmpeg-flif-gsoc-2020/renc.pdf>

The byte normalization, delayed-carry representation, adaptive model, frame
layout, and vectors are independently specified for marc. No range-coder
implementation source, source-derived pseudocode, or test suite was consulted.

### LZ77 design reference

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

### LZSS design reference

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

### LZ78 design reference

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

### LZW design reference

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

### LZD design references

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

### LZMW design references

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

### rANS design references

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

### tANS design reference

- Jarek Duda, "Asymmetric numeral systems: entropy coding combining speed of
  Huffman coding with compression rate of arithmetic coding," arXiv:1311.2540,
  2013. Used for the finite-state interval, symbol-spreading, inverse table, and
  bulk bit-transfer foundations of tabled ANS.
  <https://arxiv.org/abs/1311.2540>

marc's normalization, deterministic spreading step, descriptor, bit layout,
terminal-state rule, and vectors are independently specified below. No tANS or
FSE implementation source, source-derived pseudocode, or test suite was
consulted.

### Build and interoperability workflow reference

- GitHub Docs, "Store and share data with workflow artifacts." Used only for
  the workflow artifact publication and retention model; it is not an
  algorithm or stream-format reference.
  <https://docs.github.com/en/actions/tutorials/store-and-share-data>
- GitHub `actions/runner-images`, supported-image table. Used on 2026-07-18 to
  confirm the `windows-2025-vs2026` hosted-runner label.
  <https://github.com/actions/runner-images>
- GitHub `actions/checkout` release and usage documentation. Used on
  2026-07-18 to confirm major version 6 for repository checkout.
  <https://github.com/actions/checkout>
- GitHub `actions/upload-artifact` release and usage documentation. Used on
  2026-07-18 to select current major version 7 for interoperability artifacts.
  <https://github.com/actions/upload-artifact>
- GitHub Docs, "Adding a workflow status badge." Used for the main-branch CI
  badge URL in the repository README.
  <https://docs.github.com/en/actions/how-tos/monitor-workflows/add-a-status-badge>
- Masa-tam/mffv1, `THIRD_PARTY_NOTICES.md`. Used only as the requested
  presentation precedent for development-only GoogleTest attribution and
  inclusion of its license text; marc retains its own path and usage statement.
  <https://github.com/Masa-tam/mffv1/blob/main/THIRD_PARTY_NOTICES.md>

### CRC-32C references

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

### SHA-256 references

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

## Implementation reference ledger

### IR-0001

The hash descriptor record is a repository-defined serialization layer. Its
algorithm IDs refer to the CRC-32C and SHA-256 definitions above, while its
target/scope IDs, fixed layout, validation rules, and vectors were designed
independently for marc. Its tuple ordering, duplicate policy, and two-pass
publication contract are also repository-native; no external container format
was consulted.

### IR-0002

The staged version 1.1 prefix gate reuses marc's independently defined 1.0
prefix and hash region. Its version isolation and combined-limit policy are
internal format-evolution decisions and use no new external reference.

### IR-0003

The initial per-frame checksum profile reuses the documented CRC-32C algorithm
and repository-native descriptor. Its exact raw-byte inclusion range, reset
boundary, trailer placement, and single-descriptor restriction are independent
marc format decisions; no additional external format was consulted.

### IR-0004

The staged version 1.1 frame-header gate reuses marc's fixed frame header and
initial checksum profile. Its three-way agreement rule and version isolation
are repository-native validation policy and use no new external reference.

### IR-0005

The complete raw-checksum reference profile is a composition of marc's own
prefix, descriptor, frame, and CRC components. Its two-pass publication policy
and hand vectors were independently defined without an external container or
codec reference.

### IR-0006

The raw-checksum public-ABI completion matrix uses only that repository-defined
format, its public process contract, and the general data classes required by
AGENTS.md. No additional algorithm, format, implementation, corpus, or external
test-suite reference was used.

### IR-0007

The Adaptive Huffman stream fuzz boundary uses only marc's independently
specified FGK variant, outer framing, decoder limits, and transform contract.
Its seed and input schedules are repository-authored; no external corpus,
fuzzer harness, or implementation behavior was consulted.

### IR-0008

The Dynamic Range stream fuzz boundary uses only marc's independently defined
integer range-coder variant, framing, model-total rule, decoder limits, and
transform contract. Its seed and schedules are repository-authored; no
external range-coder corpus, harness, or implementation was consulted.

### IR-0009

The rANS stream fuzz boundary uses only marc's independently specified scalar
rANS format, normalized-table limits, block views, outer framing, and transform
contract. Its seed and schedules are repository-authored; no external ANS
corpus, fuzz harness, or implementation behavior was consulted.

### IR-0010

The tANS stream fuzz boundary uses only marc's independently specified tabled
ANS format, fixed table-log rule, block views, outer framing, and transform
contract. Its seed and schedules are repository-authored; no external FSE/ANS
corpus, fuzz harness, or implementation behavior was consulted.

### IR-0011

The standalone Blocked Huffman fuzz boundary uses only marc's bounded canonical
Huffman primitives, raw-block alternative, block views, outer framing, and
transform contract. Its seed and schedules are repository-authored; no external
Huffman corpus, fuzz harness, table, or implementation behavior was consulted.

### IR-0012

The standalone LZ77 stream fuzz boundary uses only marc's independently
specified fixed token representation, outer framing, decoder limits, and
transform contract. Its seed and schedules are repository-authored; no
external LZ corpus, fuzz harness, or implementation behavior was consulted.

### IR-0013

The standalone Blocked Huffman command-line selector composes only marc's
public C ABI, repository-defined format, bounded profile, and common atomic
file adapter. No external command-line tool, archive format, or implementation
behavior was consulted.

### IR-0014

The standalone Blocked Huffman benchmark composes only marc's public C ABI,
repository-defined format, profile sizing, and existing measurement contract.
No external benchmark harness, compression tool, or implementation behavior
was consulted.

### IR-0015

The standalone Blocked Huffman completion matrix uses only the repository's
public C ABI, format, required data classes, deterministic generator, and
existing process-contract assertions. No external vectors, corpus, test suite,
or implementation behavior was consulted.

### IR-0016

The Adaptive Huffman command-line selector composes only marc's independently
specified FGK profile, public C ABI, bounded workspace policy, and common file
adapter. No external compression tool, command-line implementation, archive
format, or test suite was consulted.

### IR-0017

The Adaptive Huffman benchmark composes only marc's independently specified
FGK profile, public C ABI, profile sizing, and repository measurement contract.
No external benchmark harness, published result, implementation, or test suite
was consulted.

### IR-0018

The Adaptive Huffman completion matrix uses only marc's public FGK C ABI,
repository-defined format, required data classes, deterministic generator, and
process-contract assertions. No external vector, corpus, test suite, or
implementation behavior was consulted.

### IR-0019

The Dynamic Range command-line selector composes only marc's independently
specified integer range profile, public C ABI, bounded workspace policy, and
common file adapter. No external range-coder tool, command-line implementation,
archive format, or test suite was consulted.

### IR-0020

The Dynamic Range benchmark composes only marc's independently specified
integer range profile, public C ABI, profile bounds, and repository measurement
contract. No external benchmark harness, published result, range-coder source,
or test suite was consulted.

### IR-0021

The Dynamic Range completion matrix uses only marc's public integer range C
ABI, repository-defined format, required data classes, deterministic generator,
and process-contract assertions. No external vector, corpus, test suite, or
implementation behavior was consulted.

### IR-0022

The rANS command-line selector composes only marc's independently specified
scalar rANS profile, public C ABI, bounded workspace policy, and common file
adapter. No external ANS tool, command-line implementation, archive format, or
test suite was consulted.

### IR-0023

The rANS benchmark composes only marc's independently specified scalar profile,
public C ABI, profile bounds, aligned view policy, and repository measurement
contract. No external benchmark harness, published result, ANS source, or test
suite was consulted.

### IR-0024

The rANS completion matrix uses only marc's public scalar C ABI,
repository-defined format, required data classes, deterministic generator,
aligned view contract, and process assertions. No external vector, corpus, test
suite, or implementation behavior was consulted.

### IR-0025

The tANS command-line selector composes only marc's independently specified
tabled profile, public C ABI, bounded workspace and aligned-view policy, and
common file adapter. No external FSE/ANS tool, command-line implementation,
archive format, or test suite was consulted.

### IR-0026

The tANS benchmark composes only marc's independently specified tabled profile,
public C ABI, 12-bit transition bound, aligned view policy, and repository
measurement contract. No external benchmark harness, published result,
FSE/ANS source, or test suite was consulted.

### IR-0027

The tANS completion matrix uses only marc's public tabled C ABI,
repository-defined format, required data classes, deterministic generator,
aligned view contract, and process assertions. No external vector, corpus, test
suite, or implementation behavior was consulted.

### IR-0028

The standalone LZ77 completion matrix uses only marc's public C ABI,
repository-defined fixed-token stream, required data classes, deterministic
generator, and process-contract assertions. No external LZ vectors, corpus,
test suite, or implementation behavior was consulted.

### IR-0029

The standalone LZSS completion matrix uses only marc's public C ABI,
repository-defined variable-token stream and literal/match cost rule, required
data classes, deterministic generator, and process-contract assertions. No
external LZSS vectors, corpus, test suite, or implementation behavior was
consulted.

### IR-0030

The standalone LZ78 completion matrix uses only marc's public C ABI,
repository-defined phrase-index token stream, required data classes,
deterministic generator, aligned-view contract, and process assertions. No
external LZ78 vectors, corpus, test suite, or implementation behavior was
consulted.

### IR-0031

The supplemental LZW public completion matrix uses only marc's public C ABI,
repository-defined packed-code stream, required data classes, deterministic
generator, aligned-view contract, and process assertions. No external LZW
vectors, corpus, test suite, or implementation behavior was consulted.

### IR-0032

The strengthened LZD completion matrix uses only marc's public C ABI,
repository-defined reference-pair stream, deterministic generator, aligned
workspace contract, and process assertions. No external LZD vectors, corpus,
test suite, or implementation behavior was consulted.

### IR-0033

The strengthened LZMW completion matrix uses only marc's public C ABI,
repository-defined fixed-reference stream, deterministic generator, aligned
workspace contract, and process assertions. No external LZMW vectors, corpus,
test suite, or implementation behavior was consulted.

### IR-0034

The baseline-readiness matrix is derived only from repository-owned format,
test, C ABI, CLI, benchmark, fuzz, CI, and interoperability records. No external
completion checklist, product comparison, or third-party implementation status
was consulted.

### IR-0035

Interoperability schema 3 composes only marc's public CLI profiles, frozen
earlier manifest rules, repository-generated fixture, and SHA-256 metadata. No
external archive format, interoperability suite, or third-party tool behavior
was consulted.

### IR-0036

The LZSS plus Blocked Huffman frame codec composes only marc's
repository-defined transactional LZSS variant 1 codec, Blocked Huffman variant
1 representation, generic frame header, checked arithmetic, and decoder
limits. No external combined format, implementation, vector, or test suite was
consulted.

### IR-0037

The LZSS plus Blocked Huffman complete-stream controller uses only marc's
version 1.0 stream header, LZSS parameter serialization, combined frame codec,
and two-pass atomic decode convention. No external container, stream scanner,
profile, vector, or implementation was consulted.

### IR-0038

The LZSS plus Blocked Huffman incremental encoder uses only marc's
`ProcessResult` contract, complete-stream oracle, exact frame planner/encoder,
and caller-owned workspace policy. No external streaming encoder, buffering
scheme, source, or test schedule was consulted.

### IR-0039

The LZSS plus Blocked Huffman incremental decoder uses only marc's prefix and
frame parsers, transactional combined frame decoder, `ProcessResult` contract,
and caller-owned staging policy. No external streaming decoder, parser state
machine, source, malformed corpus, or test schedule was consulted.

### IR-0040

The LZSS plus Blocked Huffman profile and workspace calculation use only
marc's documented LZSS all-Literal worst case, Blocked Huffman raw-fallback
layout, generic frame header, local decoder limits, checked arithmetic, and
existing internal profile conventions. No external profile API, allocator,
workspace formula, implementation, or test suite was consulted.

### IR-0041

The LZSS plus Blocked Huffman C adapter uses only marc's public opaque-transform
lifecycle, size-tagged configuration convention, DD-215 workspace query, and
the repository's combined streaming transforms. No external compression ABI,
binding, allocator interface, source, or C test suite was consulted.

### IR-0042

The `lzss-blocked-huffman` CLI adapter composes only marc's public combined C
factory, existing bounded file-processing loop, atomic temporary-output policy,
and repository-defined frame/block defaults. No external compression command,
option vocabulary, file workflow, implementation, or CLI test was consulted.

### IR-0043

The `lzss-blocked-huffman` benchmark composes only marc's public combined C
factory, documented profile bounds, existing measurement loop, and queried
workspace accounting. No external benchmark harness, compression comparison,
measurement code, implementation source, or performance result was consulted.

### IR-0044

The combined LZSS fuzz boundary uses only marc's strict and incremental
decoders, local limit model, fixed-workspace policy, `ProcessResult` invariants,
and repository-owned canonical stream. No external fuzzer harness, malformed
corpus, compression implementation, source-derived seed, or regression suite
was consulted.

### IR-0045

The public-profile evidence matrix and combined LZSS completion test use only
the repository's C ABI contract, required test classes in `AGENTS.md`, existing
marc-owned completion-test conventions, and the already specified combined
stream representation. No external API matrix, compression test suite,
implementation, stream corpus, or compatibility claim was consulted.

### IR-0046

The pre-publication CI audit consulted the official
[GitHub Actions runner-image table](https://github.com/actions/runner-images#available-images),
[`actions/checkout` usage and releases](https://github.com/actions/checkout),
[workflow-artifact documentation](https://docs.github.com/en/actions/tutorials/store-and-share-data),
and the official
[GoogleTest 1.17.0 release record](https://github.com/google/googletest/releases/tag/v1.17.0).
These were used only to verify hosted infrastructure and the pinned test
dependency. No compression implementation source was consulted.

### IR-0047

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

### IR-0048

Interoperability schema 4 extends only marc's frozen schema-3 profile order
with the repository-defined LZSS and LZ78 Blocked Huffman CLI profiles. No
external combined-codec archive, compatibility suite, manifest, or test vector
was consulted.

### IR-0049

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
The `lzw-blocked-huffman` benchmark adapter uses only the same public C
factory, fixed CLI profile, conservative encoded-capacity rules, and marc's
existing measurement contract. No external benchmark harness, result, corpus,
implementation, or tuning data was consulted.

### IR-0050

Interoperability schema 5 extends only marc's frozen schema-4 profile order
with the repository-defined LZW Blocked Huffman CLI profile. No external
combined-codec archive, compatibility suite, manifest, corpus, or test vector
was consulted.

### IR-0051

The LZD plus Blocked Huffman composition specification uses only marc's
already documented LZD variant 1 reference-pair grammar, Blocked Huffman
variant 1 block format, generic frame format, checked workspace policy, and the
LZD references recorded above. No external combined codec, format, source,
profile, stream, test vector, or workspace layout was consulted.
The complete-frame validator and decoder reuse marc's local Blocked Huffman
controller/decoder, LZD validator/decoder, generic frame parser, and checked
arithmetic helpers. No external combined decoder, validation order, workspace
formula, source, test, or malformed-stream corpus was consulted.
The complete-frame planner and encoder reuse marc's local deterministic LZD
encoder, Blocked Huffman planner/encoder, generic frame serializer, and checked
arithmetic helpers. No external combined encoder, planning order, workspace
formula, source, vector, or compression heuristic was consulted.
The profile sizing and opaque typed partition derive only from marc's LZD bounds,
Blocked Huffman descriptor rules, checked arithmetic helpers, and established
three-region ABI convention. No external allocator, combined profile, object
layout, source, or workspace implementation was consulted.
The incremental transforms reuse marc's existing composed-frame state machine,
the LZD complete-frame codec, and the DD-248 three-view partition contract. No
external streaming adapter, buffering policy, combined codec, error behavior,
source, or test suite was consulted.
The public C factory uses only marc's established configuration, requirements,
opaque workspace, transform-handle, and pure-C test conventions together with
the local LZD composition profile. No external compression ABI, factory,
allocator convention, source, or test was consulted.
The public completion matrix uses only marc's C ABI, AGENTS.md data classes and
chunking requirements, deterministic local generators, and the repository's
transactional-frame contract. No external corpus, combined-codec test suite,
malformed stream, source, or compatibility tool was consulted.
The combined LZD fuzz boundary derives only from the local streaming decoder,
DD-248 workspace layout, fixed LZD pair grammar, core process invariants, and
marc's existing bounded fuzz conventions. No external fuzzer harness, corpus,
combined decoder, allocation policy, or source was consulted.
The `lzd-blocked-huffman` CLI adapter uses only marc's public C ABI, fixed local
profile bounds, existing atomic file protocol, and repository-owned CLI test
harness. No external command-line codec, wrapper, allocation policy, source,
or test was consulted.
The `lzd-blocked-huffman` benchmark adapter uses only the same public C factory,
fixed CLI profile, conservative complete-stream capacity rules, and marc's
existing measurement contract. No external benchmark harness, corpus, result,
implementation, or tuning data was consulted.

### IR-0052

Interoperability schema 6 extends only marc's frozen schema-5 profile order
with the repository-defined LZD Blocked Huffman CLI profile. No external
combined-codec archive, compatibility suite, manifest, corpus, or test vector
was consulted.

### IR-0053

The LZMW plus Blocked Huffman composition specification uses only marc's
already documented LZMW variant 1 fixed-reference grammar, Blocked Huffman
variant 1 block format, generic frame format, checked workspace policy, and the
LZMW references recorded above. No external combined codec, format, source,
profile, stream, test vector, or workspace layout was consulted.
The complete-frame validator and decoder reuse marc's local Blocked Huffman
controller/decoder, LZMW validator/decoder, generic frame parser, and checked
arithmetic helpers. No external combined decoder, validation order, workspace
formula, source, test, or malformed-stream corpus was consulted.
The complete-frame planner and encoder reuse marc's local deterministic LZMW
encoder, Blocked Huffman planner/encoder, generic frame serializer, and checked
arithmetic helpers. No external combined encoder, planning order, workspace
formula, source, vector, or compression heuristic was consulted.
The profile sizing and opaque typed partition derive only from marc's LZMW
fixed-reference bounds, Blocked Huffman descriptor rules, checked arithmetic,
and established three-region ABI convention. No external allocator, combined
profile, object layout, source, or workspace implementation was consulted.
The incremental transforms reuse marc's established composed-frame state
machine, the local LZMW complete-frame codec, and the DD-259 three-view
partition contract. No external streaming adapter, buffering policy, combined
codec, error behavior, source, or test suite was consulted.
The public factory reuses marc's size-tagged C ABI lifecycle, the local LZMW
combined profile query, and its checked opaque partitions. No external C ABI,
allocator convention, object layout, wrapper source, or test was consulted.
The completion matrix uses only DD-262, marc's public combined C ABI, and
deterministic repository-local inputs. No external vector, completion suite,
malformed corpus, or codec implementation was consulted.
The combined fuzz boundary uses only DD-263, marc's LZMW validator and streaming
decoder contracts, fixed local workspace types, and repository fuzz invariants.
No external fuzzer harness, corpus, malformed sample, or implementation source
was consulted.
The `lzmw-blocked-huffman` CLI adapter uses only DD-264, marc's public combined
C ABI, common transactional file loop, and repository integration script. No
external CLI, archive tool, adapter source, or test fixture was consulted.
The `lzmw-blocked-huffman` benchmark adapter uses only DD-265, the public C
factory, CLI profile constants, checked output-capacity formulas, and marc's
existing measurement contract. No external benchmark implementation, result,
corpus, or tuning guidance was consulted.

### IR-0054

Interoperability schema 7 extends only marc's frozen schema-6 profile order
with the repository-defined LZMW Blocked Huffman CLI profile. No external
combined-codec archive, compatibility suite, manifest, corpus, or test vector
was consulted.

### IR-0055

The first public pushed-revision evidence was recorded from GitHub Actions
[run 29647453799](https://github.com/Masa-tam/marc/actions/runs/29647453799)
and its official Actions API metadata on 2026-07-18. This reference establishes
job conclusions, source revision, artifact names, and retention dates only; it
is not an external codec implementation or interoperability result.

### IR-0056

The repository owner supplied the first external interoperability report and
its generated Ubuntu 26.04 schema-7 bundle on 2026-07-18. The report records
Ubuntu 26.04 under WSL2 x86-64, Clang 21.1.8, CMake 4.2.3, PowerShell 7.6.3,
revision `c4f831917a43f75ca5c698d19d3674f12803f40b`, and successful verification
of both public CI bundles. The copied bundle was used only as test data for
marc's repository-owned verifier and byte comparison; no external codec source
or implementation was consulted.

### IR-0057

The LZ77 plus Adaptive Huffman composition specification uses only marc's
already documented LZ77 variant 1 token grammar, Adaptive Huffman FGK variant 1
bitstream, generic frame format, checked decoder limits, and existing
repository-owned composition rules. No external combined codec, format,
implementation, profile, stream, test vector, or workspace layout was
consulted.

### IR-0058

The LZ77 plus Adaptive Huffman C ABI uses only DD-283, marc's size-tagged
factory lifecycle, the completed combined profile/workspace calculators, and
the two local streaming transforms. No external ABI, allocator convention,
workspace layout, wrapper source, or combined factory test was consulted.

### IR-0059

The LZ77 plus Adaptive Huffman completion matrix uses only DD-284, marc's
public combined C ABI, AGENTS.md completion data classes, and deterministic
repository-local inputs. No external completion suite, corpus, malformed
sample, codec implementation, or chunk schedule was consulted.

### IR-0060

The LZ77 plus Adaptive Huffman fuzz boundary uses only DD-285, marc's completed
frame and streaming decoders, fixed local workspace bounds, core process-result
invariants, and the repository fuzz policy. No external harness, corpus,
malformed archive, combined decoder, or implementation source was consulted.

### IR-0061

The `lz77-adaptive-huffman` CLI adapter uses only DD-286, marc's public
combined C ABI, the local transactional file loop, and the established CLI
integration script. No external command-line tool, adapter source, archive
workflow, or test fixture was consulted.

### IR-0062

The `lz77-adaptive-huffman` benchmark adapter uses only DD-287, the same public
C factory and CLI profile constants, checked complete-stream capacity
arithmetic, and marc's repository-owned measurement contract. No external
benchmark harness, implementation, result, corpus, capacity formula, or tuning
guidance was consulted.

### IR-0063

Interoperability schema 8 uses only DD-288, marc's frozen schema-7 manifest
order, the completed public CLI selector, and the repository-owned generator,
verifier, and compatibility regression. No external archive format,
interoperability suite, manifest, combined-codec archive, corpus, or test vector
was consulted.

### IR-0064

The schema-8 external validation record uses the user-supplied execution report
for revision `a4e3d1a5acb7bfc393aca4f2195188cfe0421817` and marc's own manifest
verifier output. No external archive tool, decoder implementation, test suite,
or third-party compatibility claim was used.

### IR-0065

The LZSS plus Adaptive Huffman composition specification uses only marc's
documented LZSS variant 1 token grammar, Adaptive Huffman FGK variant 1
bitstream, generic frame representation, checked decoder limits, and existing
repository-owned composition rules. No external combined codec, format,
implementation, profile, stream, test vector, or workspace layout was
consulted.

### IR-0066

The first LZSS plus Adaptive Huffman vector uses only DD-290, the published
LZSS Literal grammar, the independently specified FGK NYT traversal, LSB-first
packing, and explicit generic serializers. No external vector, encoder output,
combined implementation, or test suite was consulted.

### IR-0067

The LZSS plus Adaptive Huffman complete-frame validator uses only DD-291,
marc's generic frame parser, Adaptive Huffman descriptor and decoder, LZSS
token validator, and checked limit helpers. No external combined decoder,
format validator, workspace policy, malformed corpus, source, or test suite was
consulted.

### IR-0068

The LZSS plus Adaptive Huffman raw-frame decoder uses only DD-292, the strict
DD-291 validator, and marc's standalone transactional LZSS decoder. No external
combined decoder, buffering strategy, overlap-copy implementation, source,
vector, or test suite was consulted.

### IR-0069

The LZSS plus Adaptive Huffman exact frame planner and encoder use only DD-293,
marc's deterministic LZSS encoder, Adaptive Huffman planner and encoder,
generic serializers, and DD-290 hand vector. No external combined encoder,
planning strategy, output layout, source, vector, or test suite was consulted.

### IR-0070

The LZSS plus Adaptive Huffman streaming decoder uses only DD-294, marc's
generic stream/frame parsers, DD-292 private frame decoder, and existing core
process contract. No external streaming decoder, state machine, buffering
policy, source, corpus, or test suite was consulted.

### IR-0071

The LZSS plus Adaptive Huffman streaming encoder uses only DD-295, the DD-293
exact frame encoder, marc's explicit serializers, checked limits, and core
process contract. No external streaming encoder, state machine, finish policy,
source, corpus, or test suite was consulted.

### IR-0072

The LZSS plus Adaptive Huffman profile and workspace calculation use only
DD-296, the already specified `2F` LZSS token bound, the 264-bit Adaptive
worst-case bound, generic header and descriptor extents, checked arithmetic,
and marc's existing local decoder-limit contract. No external combined
profile, allocation policy, factory, source, or test suite was consulted.

### IR-0073

The LZSS plus Adaptive Huffman C ABI uses only DD-297, DD-296's checked
workspace contract, marc's existing size-tagged C configurations, opaque
transform lifecycle, nonthrowing handle construction, and stable status
mapping. No external API, combined factory, allocator design, binding, source,
or test suite was consulted.

### IR-0074

The LZSS plus Adaptive Huffman completion matrix uses only DD-298, marc's
public C lifecycle, deterministic generator convention, generic frame extents,
and established transactional final-frame admission criteria. No external
combined implementation, conformance suite, corpus, vector, or malformed test
set was consulted.

### IR-0075

The LZSS plus Adaptive Huffman fuzz boundary uses only DD-299, marc's exact and
incremental decoders, fixed local limits, core process-result invariants, and
repository-owned canonical stream generation. No external fuzz harness,
corpus, dictionary, malformed stream, combined decoder, source, or test suite
was consulted.

### IR-0076

The LZSS plus Adaptive Huffman CLI selector uses only DD-300, the DD-297 public
C factory, DD-296 bounds, and marc's existing transactional file-processing
adapter and integration script. No external CLI, archive tool, workspace
policy, file-commit strategy, source, corpus, or test suite was consulted.

### IR-0077

The LZSS plus Adaptive Huffman benchmark adapter uses only DD-301, the DD-297
public C factory and workspace query, DD-296 bounds, checked capacity planning,
and marc's repository-owned measurement contract. No external benchmark
harness, implementation, corpus, result, capacity formula, or tuning guidance
was consulted.

### IR-0078

Interoperability schema 9 uses only DD-302, the frozen schema-8 manifest order,
the published `lzss-adaptive-huffman` CLI selector, and marc's repository-owned
bundle generator, verifier, fixture, and compatibility regression. No external
interoperability harness, archive set, manifest, corpus, combined-codec
implementation, or test suite was consulted.

### IR-0079

The schema-9 external validation record uses the user-supplied four verifier
results for revision `8a854eaf9c7c6c36cc2d444cc8e1a135935887b2`, the previously recorded
Ubuntu 26.04/Clang 21.1.8 environment boundary, and marc's own verifier
contract. No external archive tool, decoder implementation, compatibility
suite, or third-party result claim was used.

### IR-0080

The LZ78 plus Adaptive Huffman composition specification uses only DD-303,
marc's already documented LZ78 variant 1 token grammar and phrase bounds,
Adaptive Huffman FGK variant 1 tree and descriptor rules, generic framing, and
the repository-owned composition policy. No external combined codec, format,
implementation, vector, workspace layout, corpus, or test suite was consulted.

### IR-0081

The first LZ78 plus Adaptive Huffman validator uses only DD-304, the specified
combined frame, marc's strict Adaptive frame decoder, LZ78 token validator,
checked arithmetic, generic frame validation, and caller-owned aligned phrase
records. No external combined decoder, parser, validation order, malformed
corpus, implementation, or test suite was consulted.

### IR-0082

The LZ78 plus Adaptive Huffman transactional frame decoder uses only DD-305,
the DD-304 validator, marc's iterative standalone LZ78 decoder, private raw
staging, checked aggregate limits, and exact post-success copy. No external
combined decoder, phrase-expansion structure, transactional adapter, source,
or test suite was consulted.

### IR-0083

The LZ78 plus Adaptive Huffman exact-frame planner and encoder use only DD-306,
marc's standalone deterministic LZ78 encoder, Adaptive Huffman frame encoder,
generic frame serializers, and the independently frozen single-`A` vector. No
external implementation was consulted.

### IR-0084

The LZ78 plus Adaptive Huffman streaming frame encoder uses only DD-307, the
DD-306 exact-frame API, marc's generic bounded transform contract, stream and
LZ78 parameter serializers, and existing first-party known-size state-machine
rules. No external streaming implementation was consulted.

### IR-0085

The LZ78 plus Adaptive Huffman streaming frame decoder uses only DD-308, the
DD-305 transactional exact-frame decoder, generic stream and frame parsers,
checked bounds, and marc's first-party transform state rules. No external
streaming decoder, buffering order, malformed corpus, or test suite was
consulted.

### IR-0086

The LZ78 plus Adaptive Huffman profile and typed workspace partition use only
DD-309, marc's LZ78 entry types and sizing rules, Adaptive worst-case bound,
checked arithmetic, and existing first-party profile conventions. No external
profile or workspace layout was consulted.

### IR-0087

The LZ78 plus Adaptive Huffman public C ABI uses only DD-310, the DD-309
profile and typed partition helpers, marc's existing size-tagged ABI contract,
and the repository-owned streaming transforms. No external combined-codec API,
workspace convention, allocator design, implementation, or test suite was
consulted.

### IR-0088

The LZ78 plus Adaptive Huffman public completion audit uses only DD-311,
AGENTS.md completion data classes, marc's fixed C ABI, deterministic generator,
generic frame extents, and existing first-party terminal-state contract. No
external corpus, compatibility suite, combined implementation, or tests were
consulted.

### IR-0089

The LZ78 plus Adaptive Huffman fuzz boundary uses only DD-312, marc's exact
frame and incremental decoders, fixed local limits, typed LZ78 phrase records,
and the repository's first-party call-ceiling policy. No external fuzz harness,
corpus, malformed vector, combined implementation, or test suite was
consulted.

### IR-0090

The LZ78 plus Adaptive Huffman CLI selector uses only DD-313, the published C
factory and requirements query, fixed profile bounds, and marc's existing
transactional file adapter. No external command-line tool, archive workflow,
workspace convention, source, or test suite was consulted.

### IR-0091

The LZ78 plus Adaptive Huffman benchmark adapter uses only DD-314, the same
public C factory and bounded policy, checked whole-stream capacity arithmetic,
and marc's repository-owned measurement contract. No external benchmark,
result, tuning guidance, implementation, or corpus was consulted.

### IR-0092

Interoperability schema 10 uses only DD-315, the frozen schema-9 manifest order,
the published `lz78-adaptive-huffman` CLI selector, and marc's repository-owned
bundle generator, verifier, fixture, and compatibility regression. No external
interoperability harness, archive set, manifest, corpus, combined-codec source,
or test suite was consulted.

### IR-0093

The schema-10 external validation record uses the user-supplied four verifier
results for revision `bc8faba3043db78a953f18876f153abc847f814d`, the previously
documented Ubuntu 26.04/Clang 21.1.8 environment boundary, and marc's own
verifier contract. No external archive tool, decoder implementation,
compatibility suite, or third-party result claim was used.

### IR-0094

The LZW plus Adaptive Huffman composition specification uses only DD-316,
marc's already documented LZW variant 1 packed-code grammar, Adaptive Huffman
FGK variant 1 rules, generic frame format, checked workspace policy, and the
original Welch reference recorded above. No external combined codec, format,
implementation, vector, workspace layout, corpus, or test suite was consulted.

### IR-0095

The LZW plus Adaptive Huffman complete-frame validator uses only DD-317, the
DD-316 representation and bounds, marc's existing generic frame parser,
Adaptive Huffman decoder, LZW validator, and checked caller-owned workspace
policy. No external combined decoder, validation order, malformed vector,
workspace layout, source code, or test suite was consulted.

### IR-0096

The LZW plus Adaptive Huffman private-staging decoder uses only DD-318, the
DD-317 validator, marc's existing bounded LZW decoder, typed phrase records,
and checked aggregate-workspace policy. No external combined decoder,
transactional publication design, source code, malformed corpus, workspace
layout, or test suite was consulted.

### IR-0097

The LZW plus Adaptive Huffman transactional frame decoder uses only DD-319, the
DD-318 private reconstruction boundary, checked destination capacity, and
marc's existing all-or-nothing frame publication convention. No external
combined decoder, output transaction, source code, malformed corpus, API, or
test suite was consulted.

### IR-0098

The LZW plus Adaptive Huffman exact-frame planner and encoder use only DD-320,
the DD-316 representation, marc's existing LZW and Adaptive Huffman planners
and encoders, generic serializers, checked arithmetic, and caller-owned
workspace policy. No external combined encoder, parser, source code, output
transaction, vector, workspace design, or test suite was consulted.

### IR-0099

The LZW plus Adaptive Huffman streaming encoder uses only DD-321, the DD-320
exact frame encoder, marc's core transform contract, generic stream serializers,
checked packed-code bounds, and caller-owned workspace policy. No external
combined streaming encoder, buffering strategy, source code, API, chunk
schedule, or test suite was consulted.

### IR-0100

The LZW plus Adaptive Huffman streaming decoder uses only DD-322, the DD-318
private-staging decoder, marc's generic prefix and frame parsers, checked LZW
packed bounds, core transform contract, and caller-owned workspace policy. No
external combined streaming decoder, buffering strategy, source code,
malformed corpus, chunk schedule, or test suite was consulted.

### IR-0101

The LZW plus Adaptive Huffman bounded profile uses only DD-323, the DD-316
representation, marc's existing LZW code-width and dictionary-capacity rules,
Adaptive Huffman's documented payload ceiling, checked arithmetic, and the
already implemented streaming constructor shapes. No external combined
profile, allocator, ABI layout, workspace calculator, source code, or test
suite was consulted.

### IR-0102

The LZW plus Adaptive Huffman public C ABI uses only DD-324, the DD-323
workspace profile, marc's common three-region transform ABI, and the existing
combined streaming encoder and decoder. No external combined API, allocator,
factory, ABI layout, source code, or C test suite was consulted.

### IR-0103

The LZW plus Adaptive Huffman public completion matrix uses only DD-325, the
published C ABI, marc's required data-class inventory, generic frame fields,
and deterministic first-party byte generation. No external combined codec,
conformance corpus, malformed archive, chunk schedule, source code, or test
suite was consulted.

### IR-0104

The LZW plus Adaptive Huffman bounded fuzz boundary uses only DD-326, the
existing exact-frame private decoder, streaming decoder, local limit contract,
and repository-authored canonical stream generator. No external fuzz harness,
corpus, malformed archive, combined decoder, source code, or regression suite
was consulted.

### IR-0105

The LZW plus Adaptive Huffman CLI selector uses only DD-327, the published C
factory and requirements query, and marc's existing transactional file driver.
No external LZW, Adaptive Huffman, compression-tool, archive-manager, source
code, CLI layout, or test suite was consulted.

### IR-0106

The LZW plus Adaptive Huffman benchmark adapter uses only DD-328, the published
C factory and requirements query, DD-327's fixed CLI policy, and marc's
dependency-free measurement driver. No external benchmark harness, LZW or
Adaptive Huffman implementation, performance-tuning source, or result corpus
was consulted.

### IR-0107

Interoperability schema 11 uses only DD-329, the frozen schema-10 manifest
order, the published `lzw-adaptive-huffman` CLI selector, and marc's
repository-owned bundle generator, verifier, fixture, and compatibility
regression. No external interoperability harness, archive set, manifest,
corpus, combined-codec source, or test suite was consulted.

### IR-0108

The schema-11 external validation record uses the user-supplied four verifier
results for revision `163948c61dd8b90359882bee122f16ab3794787c` and the
environment already documented for Ubuntu 26.04/Clang 21.1.8. No external
codec source, archive format, interoperability harness, or third-party claim
was consulted.

### IR-0109

The LZD plus Adaptive Huffman composition specification uses only DD-330,
marc's already documented Lempel-Ziv Double variant 1 reference-pair grammar,
Adaptive Huffman FGK variant 1 rules, generic frame format, checked workspace
policy, and the LZD references already recorded above. No external combined
codec, format, implementation, vector, workspace layout, corpus, or test suite
was consulted.

### IR-0110

The LZD plus Adaptive Huffman complete-frame validator uses only DD-331, the
DD-330 representation and bounds, marc's existing generic frame parser,
Adaptive Huffman decoder, LZD validator, and checked caller-owned workspace
policy. No external combined decoder, validation order, malformed vector,
workspace layout, source code, or test suite was consulted.

### IR-0111

The LZD plus Adaptive Huffman private-staging decoder uses only DD-332, the
DD-331 validator, marc's existing bounded iterative LZD decoder, typed phrase
records, explicit expansion stack, and checked aggregate-workspace policy. No
external combined decoder, transactional publication design, source code,
malformed corpus, workspace layout, or test suite was consulted.

### IR-0112

The LZD plus Adaptive Huffman transactional frame decoder uses only DD-333,
the DD-332 private reconstruction boundary, checked destination capacity, and
marc's existing all-or-nothing frame publication convention. No external
combined decoder, output transaction, source code, malformed corpus, API, or
test suite was consulted.

### IR-0113

The LZD plus Adaptive Huffman exact-frame encoder uses only DD-334, DD-330's
frozen representation and independent vector, marc's existing deterministic
LZD planner/encoder, Adaptive Huffman planner/encoder, generic frame
serializer, and checked workspace policy. No external combined encoder,
source code, control flow, vector, corpus, API, or test suite was consulted.

### IR-0114

The LZD plus Adaptive Huffman streaming encoder uses only DD-335, the DD-334
exact-frame transaction, marc's core transform contract, generic stream and
LZD parameter serializers, checked token bounds, and caller-owned workspace
policy. No external combined streaming encoder, buffering strategy, source
code, API, chunk schedule, corpus, or test suite was consulted.

### IR-0115

The LZD plus Adaptive Huffman streaming decoder uses only DD-336, the DD-332
private-staging transaction, marc's generic prefix and frame parsers, checked
LZD token/phrase/expansion bounds, and core transform contract. No external
combined streaming decoder, buffering strategy, source code, API, malformed
corpus, chunk schedule, or test suite was consulted.

### IR-0116

The LZD plus Adaptive Huffman bounded profile uses only DD-337, DD-330's
checked token and payload ceilings, marc's existing LZD parameter validation,
stream-header validation, typed record definitions, checked arithmetic, and
caller-owned workspace policy. No external profile calculator, ABI layout,
allocator, source code, API, corpus, or test suite was consulted.

### IR-0117

The LZD plus Adaptive Huffman public C ABI uses only DD-338, the DD-337 bounded
profile and partition helpers, marc's existing transform lifecycle, checked
workspace query, opaque aligned-view convention, and first-party C11 assertion
harness. No external combined API, allocator interface, ABI layout, factory
source, corpus, or test suite was consulted.

### IR-0118

The LZD plus Adaptive Huffman public completion matrix uses only DD-339, the
published C ABI, marc's required data-class inventory, generic frame fields,
and deterministic first-party byte generation. No external combined codec,
conformance corpus, malformed archive, chunk schedule, source code, or test
suite was consulted.

### IR-0119

The LZD plus Adaptive Huffman bounded fuzz boundary uses only DD-340, the
existing exact-frame private decoder, streaming decoder, local limit contract,
LZD token/phrase/expansion ceilings, and repository-authored canonical stream
generator. No external fuzz harness, corpus, malformed archive, combined
decoder, source code, or regression suite was consulted.

### IR-0120

The `lzd-adaptive-huffman` CLI adapter uses only DD-341, the published marc C
requirements query and factory, the fixed local reference-profile bounds, and
the repository's existing transactional file adapter and round-trip script. No
external compression CLI, dispatch table, allocation wrapper, source code, or
test suite was consulted.

### IR-0121

The `lzd-adaptive-huffman` benchmark adapter uses only DD-342, the published
marc C requirements query and factory, the CLI's fixed limits, and the existing
repository measurement and verification contract. No external benchmark,
combined-codec tool, record layout, source code, corpus, or test suite was
consulted.

### IR-0122

Interoperability schema 12 uses only DD-343, the frozen schema-11 manifest
order, the public `lzd-adaptive-huffman` CLI selector, and marc's existing local
generator, verifier, SHA-256, exact re-encoding, and one-generation compatibility
contracts. No external archive protocol, codec registry, manifest schema, test
fixture, source code, or verification suite was consulted.

### IR-0123

The schema-12 external validation record uses the user-supplied four verifier
results for revision `7078d0ab20f6e0a1aeaa3c43e480ca866bf8a2fa` and the
previously documented Ubuntu 26.04/Clang 21.1.8 environment. No external codec
source, archive format, interoperability harness, or third-party claim was
consulted.

### IR-0124

The LZMW plus Adaptive Huffman specification uses only marc's independently
specified LZMW variant 1 reference stream, Adaptive Huffman FGK variant 1,
generic frame serialization, and their already recorded primary references.
The combined byte boundary, checked bounds, validation order, reserved name,
and raw-`A` vector were derived locally. No external combined implementation,
format, source code, vector, corpus, or test suite was consulted.

### IR-0125

The LZMW plus Adaptive Huffman complete-frame validator uses only DD-345,
DD-344, marc's generic frame parser, Adaptive Huffman decoder, LZMW token
validator, checked arithmetic, and caller-owned staging policy. No external
combined decoder, validation order, malformed vector, workspace layout, source
code, or test suite was consulted.

### IR-0126

The LZMW plus Adaptive Huffman private-staging decoder uses only DD-346, the
DD-345 validator, marc's existing bounded iterative LZMW decoder, typed phrase
records, explicit expansion stack, and checked aggregate-workspace policy. No
external combined decoder, transactional publication design, source code,
malformed corpus, workspace layout, or test suite was consulted.

### IR-0127

The LZMW plus Adaptive Huffman transactional frame decoder uses only DD-347,
the DD-346 private reconstruction boundary, checked destination capacity, and
marc's existing all-or-nothing frame publication convention. No external
combined decoder, output transaction, source code, malformed corpus, API, or
test suite was consulted.

### IR-0128

The LZMW plus Adaptive Huffman exact-frame encoder uses only DD-348, DD-344's
frozen representation and independent vector, marc's existing deterministic
LZMW planner/encoder, Adaptive Huffman planner/encoder, generic frame
serializer, and checked workspace policy. No external combined encoder, source
code, control flow, vector, corpus, API, or test suite was consulted.

### IR-0129

The LZMW plus Adaptive Huffman streaming encoder uses only DD-349, the DD-348
exact-frame transaction, marc's core transform contract, generic stream and
LZMW parameter serializers, checked reference bounds, and caller-owned
workspace policy. No external combined streaming encoder, buffering strategy,
source code, API, chunk schedule, corpus, or test suite was consulted.

### IR-0130

The LZMW plus Adaptive Huffman streaming decoder uses only DD-350, DD-346's
private reconstruction boundary, DD-344 bounds, marc's generic stream/frame
parsers, core transform contract, and checked caller-owned workspace policy. No
external combined streaming decoder, buffering design, source code, API,
malformed corpus, chunk schedule, or test suite was consulted.

### IR-0131

The LZMW plus Adaptive Huffman workspace profile uses only DD-351, DD-349 and
DD-350 streaming requirements, the local LZMW entry/phrase layouts, checked
alignment arithmetic, and marc's opaque byte-view convention. No external ABI,
combined profile, record layout, allocator, source code, API, or test suite was
consulted.

### IR-0132

The LZMW plus Adaptive Huffman C ABI uses only DD-352, DD-351's bounded profile,
the established marc transform lifecycle and three-region ABI, checked opaque
view partitioning, and the first-party C11 assertion harness. No external
combined API, factory, allocator interface, ABI layout, source code, or test
suite was consulted.

### IR-0133

The LZMW plus Adaptive Huffman completion matrix uses only DD-353, the published
marc C configuration/query/factory/process/destroy lifecycle, DD-344 bounds, and
repository-authored deterministic fixtures and malformed mutations. No external
completion suite, corpus, combined codec API, source code, or test vectors were
consulted.

### IR-0134

The LZMW plus Adaptive Huffman bounded fuzz boundary uses only DD-354, the
existing exact-frame private decoder, streaming decoder, local limit contract,
LZMW reference/phrase/expansion ceilings, and repository-authored canonical
stream generator. No external fuzz harness, corpus, malformed archive,
combined decoder, source code, API, or regression suite was consulted.

### IR-0135

The `lzmw-adaptive-huffman` CLI adapter uses only DD-355, the published marc C
requirements query and factory, the fixed local reference-profile bounds, and
the repository's existing transactional file adapter and round-trip script. No
external compression CLI, dispatch table, allocation wrapper, source code, or
test suite was consulted.

### IR-0136

The `lzmw-adaptive-huffman` benchmark adapter uses only DD-356, the published
marc C requirements query and factory, the CLI's fixed limits, and the existing
repository measurement and verification contract. No external benchmark,
combined-codec tool, record layout, source code, corpus, or test suite was
consulted.

### IR-0137

Interoperability schema 13 uses only DD-357, the frozen schema-12 manifest
order, the public `lzmw-adaptive-huffman` CLI selector, and marc's existing
local generator, verifier, SHA-256, exact re-encoding, and one-generation
compatibility contracts. No external archive protocol, codec registry,
manifest schema, test fixture, source code, or verification suite was
consulted.

### IR-0138

The LZ77 plus Dynamic Range composition specification and hand vector use only
marc's documented LZ77 variant 1 token grammar, Dynamic Range Coder variant 1
integer model and delayed-carry rules, generic frame format, checked decoder
limits, and repository-owned composition policy. No external combined codec,
range-coder implementation, format, profile, stream, test vector, or workspace
layout was consulted.

### IR-0139

The LZ77 plus Dynamic Range complete-frame validator uses only DD-360, the
reserved combined format, marc's generic frame parser, Dynamic Range descriptor
and strict decoder, LZ77 token validator, checked arithmetic, and decoder-limit
contracts. No external LZ/range pipeline, combined validator, source code,
malformed corpus, test suite, or error taxonomy was consulted.

### IR-0140

The LZ77 plus Dynamic Range private raw decoder uses only DD-361, the completed
combined validator, marc's validated LZ77 overlap-copy decoder, checked raw and
aggregate workspace policy, and local complete-frame contracts. No external
combined decoder, decompression pipeline, transactional buffer design, source
code, malformed corpus, or test suite was consulted.

### IR-0141

The LZ77 plus Dynamic Range transactional publication boundary uses only
DD-362, the completed private raw decoder, caller-supplied spans, and marc's
existing complete-frame commit policy. No external decompression API,
publication strategy, buffer design, source code, malformed corpus, or test
suite was consulted.

### IR-0142

The LZ77 plus Dynamic Range exact planner and encoder use only DD-363, marc's
existing deterministic LZ77 encoder, Dynamic Range planner and encoder,
generic frame serializer, checked arithmetic, and local caller-owned staging
contract. No external combined encoder, planning algorithm, source code,
format, vector generator, or test suite was consulted.

### IR-0143

The LZ77 plus Dynamic Range bounded streaming encoder uses only DD-364, the
exact-frame planner and encoder, marc's transform status contract, stream
header and parameter serializers, checked aggregate accounting, and existing
frame-boundary semantics. No external streaming codec, buffering design,
state machine, source code, test vector, or test suite was consulted.

### IR-0144

The LZ77 plus Dynamic Range bounded streaming decoder uses only DD-365, the
complete-frame private decoder, marc's generic stream and frame parsers,
transform status contract, checked workspace accounting, and transactional
frame-publication rule. No external streaming decoder, buffering state machine,
source code, malformed corpus, error taxonomy, or test suite was consulted.

### IR-0145

The LZ77 plus Dynamic Range bounded workspace profile uses only DD-366, the
documented `16F` token and `2S + 5` payload bounds, generic header and descriptor
sizes, local decoder limits, checked arithmetic, and existing streaming-region
ownership. No external workspace calculator, allocator interface, combined
profile, source code, ABI layout, or test suite was consulted.

### IR-0146

The LZ77 plus Dynamic Range public C requirements query and factory use only
DD-367, DD-366's byte-only bounded profile, the completed streaming encoder and
decoder, ABI version 1's existing two-region transform lifecycle, checked
offsets, and `nothrow` handle publication. No external C API, allocation model,
combined factory, source code, ABI layout, or test suite was consulted.

### IR-0147

The LZ77 plus Dynamic Range public-ABI completion matrix uses only DD-368, the
published marc C lifecycle, DD-359 bounds, repository-authored deterministic
generators, and local frame-extent parsing. No external conformance suite,
corpus, combined codec API, malformed archive, source code, or test vectors
were consulted.

### IR-0148

The LZ77 plus Dynamic Range bounded decoder fuzz boundary uses only DD-369,
marc's private complete-frame validator, bounded streaming decoder, local
decoder limits, caller-owned fixed arrays, and the repository-authored
truncated-magic seed. No external fuzzer harness, corpus, combined decoder,
malformed archive, source code, or test suite was consulted.

### IR-0149

The `lz77-dynamic-range` CLI adapter uses only DD-370, marc's public combined C
ABI, fixed profile bounds, local transactional file loop, and established
repository CLI integration script. No external command-line tool, adapter,
archive workflow, source code, or test fixture was consulted.

### IR-0150

The `lz77-dynamic-range` benchmark adapter uses only DD-371, the same public C
factory, independently derived profile and complete-stream capacity bounds,
and marc's repository-owned measurement contract. No external benchmark
harness, implementation, result, corpus, capacity formula, or tuning guidance
was consulted.

### IR-0151

Interoperability schema 14 uses only DD-372, the frozen schema-13 manifest
order, the public `lz77-dynamic-range` CLI selector, and marc's existing
repository-owned generator, verifier, deterministic fixture, and compatibility
chain. No external archive protocol, schema, manifest, corpus, source code,
test vector, or verification suite was consulted.

### IR-0152

The LZSS plus Dynamic Range composition specification and hand vector use only
DD-373, marc's documented LZSS variant 1 token grammar, Dynamic Range Coder
variant 1 integer model and delayed-carry rules, generic frame format, checked
decoder limits, and repository-owned composition policy. No external combined
codec, range-coder implementation, format, profile, stream, test vector,
workspace layout, source code, or test suite was consulted.

### IR-0153

The LZSS plus Dynamic Range complete-frame validator uses only DD-374, the
reserved combined format, marc's generic frame parser, Dynamic Range
descriptor and strict two-pass decoder, LZSS token validator, checked
arithmetic, and local decoder-limit contracts. No external combined
LZ/range validator, decompression pipeline, source code, malformed corpus,
error taxonomy, or test suite was consulted.

### IR-0154

The LZSS plus Dynamic Range private raw decoder uses only DD-375, the completed
DD-374 validator, marc's existing bounded LZSS decoder and overlap-copy
semantics, checked raw and aggregate workspace policy, and local exact-frame
contracts. No external combined decoder, decompression pipeline,
transactional-publication design, source code, malformed corpus, workspace
layout, or test suite was consulted.

### IR-0155

The LZSS plus Dynamic Range transactional publication boundary uses only
DD-376, the completed private raw decoder, caller-supplied byte spans, and
marc's existing exact-frame commit convention. No external decompression API,
transactional-output strategy, buffer design, source code, malformed corpus,
or test suite was consulted.

### IR-0156

The LZSS plus Dynamic Range exact planner and encoder use only DD-377, marc's
existing deterministic LZSS encoder, Dynamic Range planner and encoder,
generic frame and descriptor serializers, checked arithmetic, and local
caller-owned staging contract. No external combined encoder, planning
algorithm, source code, format, vector generator, workspace design, or test
suite was consulted.

### IR-0157

The LZSS plus Dynamic Range bounded streaming encoder uses only DD-378, the
completed exact-frame planner and encoder, marc's transform status contract,
stream header and LZSS parameter serializers, checked aggregate accounting,
and existing frame-boundary semantics. No external streaming codec, buffering
design, state machine, source code, test vector, or test suite was consulted.

### IR-0158

The LZSS plus Dynamic Range bounded streaming decoder uses only DD-379, the
private complete-frame decoder, marc's generic stream and frame parsers,
transform status contract, checked extent and workspace arithmetic, and
transactional frame-publication rule. No external streaming decoder, buffering
state machine, source code, malformed corpus, error taxonomy, or test suite was
consulted.

### IR-0159

The LZSS plus Dynamic Range bounded workspace profile uses only DD-380, the
documented `2F` token and `2S + 5` payload bounds, generic header and descriptor
sizes, local decoder limits, checked arithmetic, and existing streaming-region
ownership. No external workspace calculator, allocator interface, combined
profile, source code, ABI layout, or test suite was consulted.

### IR-0160

The LZSS plus Dynamic Range public C requirements query and factory use only
DD-381, DD-380's byte-only profile, the completed streaming encoder and
decoder, ABI version 1's existing two-region transform lifecycle, checked
offsets, and `nothrow` handle publication. No external C API, allocation model,
combined factory, source code, ABI layout, or test suite was consulted.

### IR-0161

The LZSS plus Dynamic Range public-ABI completion matrix uses only DD-382, the
published marc C lifecycle, DD-380 bounds, repository-authored deterministic
generators, and local frame-extent parsing. No external conformance suite,
corpus, combined codec API, malformed archive, source code, or test vectors
were consulted.

### IR-0162

The LZSS plus Dynamic Range bounded decoder fuzz boundary uses only DD-383,
marc's completed exact-frame and incremental decoders, local workspace
formulas, transform invariants, and repository-authored canonical frame
generation. The five-byte seed and all permanent malformed cases were written
from marc's own magic, header, descriptor, and transactional-publication
rules. No external fuzz harness, corpus, crash sample, combined codec,
implementation source, or test suite was consulted.

### IR-0163

The LZSS plus Dynamic Range CLI adapter uses only DD-384, the published marc C
requirements query and factory, the local fixed 64-KiB reference profile, and
the repository's existing transactional file loop and CLI round-trip script.
No external command-line interface, archive tool, combined codec adapter,
workspace policy, source code, or test suite was consulted.

### IR-0164

The LZSS plus Dynamic Range benchmark adapter uses only DD-385, the published
marc C lifecycle, DD-384's fixed reference profile, checked local arithmetic,
and the repository's existing dependency-free measurement runner. No external
benchmark framework, combined codec adapter, capacity formula, performance
result, source code, or test suite was consulted.

### IR-0165

Interoperability schema 15 uses only DD-386, the frozen schema-14 manifest
order, the public `lzss-dynamic-range` CLI selector, and marc's existing
repository-owned generator, verifier, deterministic fixture, SHA-256 metadata,
and one-generation compatibility chain. No external archive protocol, schema,
manifest, corpus, source code, test vector, or verification suite was
consulted.

### IR-0166

The LZ78 plus Dynamic Range composition specification and hand vector use only
DD-387, marc's documented LZ78 variant 1 fixed token grammar and phrase
bounds, Dynamic Range Coder variant 1 integer model and delayed-carry rules,
generic frame format, checked decoder limits, and repository-owned composition
policy. No external combined codec, range-coder implementation, format,
profile, stream, test vector, workspace layout, source code, or test suite was
consulted.

### IR-0167

The LZ78 plus Dynamic Range complete-frame validator uses only DD-388, the
reserved combined format, marc's generic frame parser, Dynamic Range
descriptor and strict two-pass decoder, LZ78 token and phrase-graph validator,
checked arithmetic, and local decoder-limit contracts. No external combined
LZ/range validator, decompression pipeline, source code, malformed corpus,
workspace policy, error taxonomy, or test suite was consulted.

### IR-0168

The LZ78 plus Dynamic Range private raw decoder uses only DD-389, DD-388's
complete phrase-graph boundary, marc's existing bounded non-recursive LZ78
decoder, caller-owned spans, checked aggregate arithmetic, and local decoder
limits. No external combined decoder, phrase expansion implementation,
buffering layout, source code, malformed corpus, or test suite was consulted.

### IR-0169

The LZ78 plus Dynamic Range transactional publication boundary uses only
DD-390, the completed private raw decoder, caller-supplied spans, and marc's
existing exact-frame commit convention. No external decompression API,
transactional-output strategy, buffer design, source code, malformed corpus,
or test suite was consulted.

### IR-0170

The LZ78 plus Dynamic Range exact-frame planner uses only DD-391, marc's
standalone deterministic LZ78 encoder and Dynamic Range planner, the reserved
composition bounds, generic frame validation, checked arithmetic, and
caller-owned workspaces. No external combined encoder, parse strategy,
workspace layout, source code, encoded corpus, or test suite was consulted.

### IR-0171

The LZ78 plus Dynamic Range deterministic exact-frame encoder uses only DD-392,
DD-391's completed plan, marc's generic header and Dynamic Range serializers,
the local range encoder, and caller-owned spans. No external combined encoder,
archive serializer, transactional-output design, source code, encoded corpus,
or test suite was consulted.

### IR-0172

The LZ78 plus Dynamic Range bounded streaming encoder uses only DD-393, the
completed exact-frame encoder, marc's transform status contract, checked
caller-owned storage, known-size stream header rules, and existing prefix/frame
drain convention. No external streaming encoder, buffering state machine,
source code, chunk schedule, error taxonomy, or test suite was consulted.

### IR-0173

The LZ78 plus Dynamic Range bounded streaming decoder uses only DD-394, the
completed private complete-frame decoder, marc's transform status contract,
generic prefix and frame parsers, checked caller-owned storage, and existing
transactional frame-publication convention. No external streaming decoder,
buffering state machine, malformed corpus, source code, chunk schedule, error
taxonomy, or test suite was consulted.

### IR-0174

The LZ78 plus Dynamic Range bounded profile uses only DD-395, the local LZ78
and Dynamic Range bounds, existing exact-frame encoder and streaming decoder
workspace contracts, checked arithmetic, and marc's opaque aligned-record
partition convention. No external profile API, allocator design, workspace
formula, record layout, source code, or test suite was consulted.

### IR-0175

The LZ78 plus Dynamic Range public C factory uses only DD-396, the completed
DD-395 profile, local streaming transforms, marc ABI version 1's size-tagged
config and transform lifecycle, checked workspace splitting, and opaque
record partition helpers. No external C API, factory lifecycle, allocation
contract, ABI layout, source code, or test suite was consulted.

### IR-0176

The LZ78 plus Dynamic Range public completion matrix uses only DD-397, the
published marc C ABI, the local 64-byte audit convention, deterministic
first-party byte generation, generic frame length fields, and transactional
frame-publication contract. No external completion suite, corpus, malformed
vector, source code, or test framework expression was consulted.

### IR-0177

The LZ78 plus Dynamic Range bounded decoder fuzz boundary uses only DD-398,
the local exact-frame and streaming decoders, fixed compile-time workspaces,
the core process-result validator, and repository-owned canonical streams.
No external fuzz harness, corpus, malformed vector, allocation strategy,
source code, or regression suite was consulted.

### IR-0178

The `lz78-dynamic-range` CLI adapter uses only DD-399, the published marc C
config, requirements query and factory, the existing local transactional
temporary-file loop, and the repository-owned CLI regression script. No
external archive tool, command syntax, combined-codec adapter, workspace
policy, source code, or test suite was consulted.

### IR-0179

The `lz78-dynamic-range` benchmark adapter uses only DD-400, the same published
marc C profile as the CLI, the local checked encoded-capacity helper, and the
repository's dependency-free measurement runner. No external benchmark
framework, combined-codec adapter, capacity formula, performance result,
source code, or test suite was consulted.

### IR-0180

Interoperability schema 16 uses only DD-401, the frozen schema-15 manifest
order, the public `lz78-dynamic-range` CLI selector, and marc's existing
deterministic fixture generator, strict verifier, SHA-256 metadata, and
one-generation compatibility chain. No external archive protocol, manifest
schema, interoperability harness, combined-codec archive, corpus, source code,
or test vector was consulted.

### IR-0181

The LZW plus Dynamic Range reserved representation uses only DD-402, marc's
already documented LZW variant 1 packed-code grammar, Dynamic Range variant 1,
generic frame format, checked arithmetic rules, and repository-authored
standalone encoders. No external LZW/range composition, archive format,
combined-codec implementation, source code, encoded corpus, or test suite was
consulted.

### IR-0182

The first LZW plus Dynamic Range complete-frame validator uses only DD-403,
DD-402's fixed packed-byte boundary and bounds, marc's generic frame parser,
Dynamic Range descriptor and decoder, existing LZW validator, caller-owned
spans, and checked aggregate arithmetic. No external combined decoder,
validation order, workspace layout, malformed corpus, source code, or test
suite was consulted.

### IR-0183

The LZW plus Dynamic Range private raw decoder uses only DD-404, the completed
DD-403 validator, marc's existing iterative LZW decoder, caller-owned packed,
phrase, and raw spans, and checked aggregate arithmetic. No external combined
decoder, phrase-expansion implementation, buffering layout, source code,
malformed corpus, or test suite was consulted.

### IR-0184

The LZW plus Dynamic Range transactional frame decoder uses only DD-405,
DD-404's private reconstruction boundary, caller-owned destination capacity,
and marc's established copy-after-success convention. No external combined
decoder, publication protocol, buffering layout, source code, malformed
corpus, or test suite was consulted.

### IR-0185

The LZW plus Dynamic Range exact-frame planner uses only DD-406, marc's local
LZW variant-1 planner and encoder, Dynamic Range variant-1 planner, generic
frame validator, caller-owned workspaces, and checked arithmetic. No external
combined encoder, planning algorithm, buffering layout, source code, encoded
corpus, or test suite was consulted.

### IR-0186

The LZW plus Dynamic Range deterministic complete-frame encoder uses only
DD-407, DD-406's exact plan, marc's explicit generic-header and Dynamic Range
descriptor serializers, and local Dynamic Range encoder. No external combined
encoder, frame writer, buffering layout, source code, encoded corpus, or test
suite was consulted.

### IR-0187

The LZW plus Dynamic Range bounded streaming encoder uses only DD-408,
DD-407's deterministic complete-frame encoder, marc's core process contract,
explicit stream/LZW parameter serializers, checked arithmetic, and established
caller-owned frame-draining state conventions. No external streaming encoder,
state machine, buffering layout, source code, corpus, or test suite was
consulted.

### IR-0188

The LZW plus Dynamic Range bounded streaming decoder uses only DD-409,
DD-405's transactional complete-frame decoder, marc's core process contract,
explicit prefix and frame parsers, checked arithmetic, and established
caller-owned validated-frame draining conventions. No external streaming
decoder, state machine, buffering layout, source code, malformed corpus, or
test suite was consulted.

### IR-0189

The LZW plus Dynamic Range direction-specific profile uses only DD-410,
DD-408/409's established caller-owned regions, marc's local LZW workspace
formulas and record types, Dynamic Range bounds, checked arithmetic, and the
already published profile conventions. No external allocator, workspace
layout, combined codec, source code, corpus, or test suite was consulted.

### IR-0190

The LZW plus Dynamic Range public C ABI uses only DD-411, DD-410's checked
requirements and typed partitions, the local streaming transforms, and marc's
established three-workspace C lifecycle. No external ABI adapter, ownership
scheme, source code, corpus, or test suite was consulted.

### IR-0191

The LZW plus Dynamic Range public completion matrix uses only DD-412, the
published local C ABI, independently generated byte classes, deterministic
local schedules, and explicit generic-frame extent parsing. No external
completion suite, malformed corpus, source code, or encoded vector was used.

### IR-0192

The LZW plus Dynamic Range bounded fuzz boundary uses only DD-413, marc's local
complete-frame and streaming decoders, fixed-array limits, process-result
invariants, and the first-party LZW fuzz harness. No external fuzz target,
malformed corpus, source code, or test suite was consulted.

### IR-0193

The `lzw-dynamic-range` CLI adapter uses only DD-414, the published marc C
config, requirements query and factory, the existing local transactional
temporary-file loop, and the repository-owned CLI regression script. No
external archive tool, command syntax, combined-codec adapter, workspace
policy, source code, or test suite was consulted.

### IR-0194

The `lzw-dynamic-range` benchmark adapter uses only DD-415, the same published
marc C profile as the CLI, the local checked encoded-capacity helper, and the
repository's dependency-free measurement runner. No external benchmark
framework, combined-codec adapter, capacity formula, performance result,
source code, or test suite was consulted.

### IR-0195

Interoperability schema 17 uses only DD-416, the frozen schema-16 manifest
order, the public `lzw-dynamic-range` CLI selector, and marc's existing
deterministic fixture generator, strict verifier, SHA-256 metadata, and
one-generation compatibility chain. No external archive protocol, manifest
schema, interoperability harness, combined-codec archive, corpus, source code,
or test vector was consulted.

### IR-0196

The LZD plus Dynamic Range reserved representation uses only DD-417, marc's
already documented LZD variant 1 reference-pair grammar, Dynamic Range variant
1, generic frame format, checked arithmetic rules, and repository-authored
standalone encoders. No external LZD/range composition, archive format,
combined-codec implementation, source code, encoded corpus, or test suite was
consulted.

### IR-0197

The first LZD plus Dynamic Range complete-frame validator uses only DD-418,
DD-417's fixed token-byte boundary and bounds, marc's generic frame parser,
Dynamic Range descriptor and decoder, existing LZD validator, caller-owned
spans, and checked aggregate arithmetic. No external combined decoder,
validation order, workspace layout, malformed corpus, source code, or test
suite was consulted.

### IR-0198

The LZD plus Dynamic Range private raw decoder uses only DD-419, the completed
DD-418 validator, marc's existing iterative LZD decoder, caller-owned token,
phrase, expansion-stack, and raw spans, and checked aggregate arithmetic. No
external combined decoder, phrase-expansion implementation, buffering layout,
source code, malformed corpus, or test suite was consulted.

### IR-0199

The LZD plus Dynamic Range transactional frame decoder uses only DD-420,
DD-419's private reconstruction boundary, caller-owned destination capacity,
and marc's established copy-after-success convention. No external combined
decoder, publication protocol, buffering layout, source code, malformed
corpus, or test suite was consulted.

### IR-0200

The LZD plus Dynamic Range exact-frame planner uses only DD-421, marc's local
LZD variant-1 planner and encoder, Dynamic Range variant-1 planner, generic
frame validator, caller-owned workspaces, and checked arithmetic. No external
combined encoder, planning algorithm, buffering layout, source code, encoded
corpus, or test suite was consulted.

### IR-0201

The LZD plus Dynamic Range deterministic complete-frame encoder uses only
DD-422, DD-421's exact plan, marc's explicit generic-header and Dynamic Range
descriptor serializers, and local Dynamic Range encoder. No external combined
encoder, frame writer, buffering layout, source code, encoded corpus, or test
suite was consulted.

### IR-0202

The LZD plus Dynamic Range bounded streaming encoder uses only DD-423,
DD-422's deterministic complete-frame encoder, marc's core process contract,
explicit stream/LZD parameter serializers, checked arithmetic, and established
caller-owned frame-draining state conventions. No external streaming encoder,
state machine, buffering layout, source code, corpus, or test suite was
consulted.

### IR-0203

The LZD plus Dynamic Range bounded streaming decoder uses only DD-424,
DD-420's transactional complete-frame decoder, marc's core process contract,
explicit prefix and frame parsers, checked arithmetic, and established caller-
owned validated-frame draining conventions. No external streaming decoder,
state machine, buffering layout, source code, malformed corpus, or test suite
was consulted.

### IR-0204

The LZD plus Dynamic Range direction-specific profile uses only DD-425,
DD-423/424's established caller-owned regions, marc's local LZD workspace
formulas and record types, Dynamic Range bounds, checked arithmetic, and the
already published profile conventions. No external allocator, workspace
layout, combined codec, source code, corpus, or test suite was consulted.

### IR-0205

The LZD plus Dynamic Range C ABI uses only DD-426, DD-425's requirements and
partition helpers, DD-423/424's streaming transforms, marc's existing
fixed-width C lifecycle, and the local status bridge. No external ABI,
allocator interface, wrapper, source code, corpus, or test suite was consulted.

### IR-0206

The LZD plus Dynamic Range public completion matrix uses only DD-427, the
published local C ABI, independently generated byte classes, deterministic
local schedules, and explicit generic-frame extent parsing. No external
completion suite, malformed corpus, source code, or encoded vector was used.

### IR-0207

The LZD plus Dynamic Range bounded fuzz boundary uses only DD-428, marc's local
complete-frame and streaming decoders, fixed-array limits, process-result
invariants, and the first-party LZD fuzz harness. No external fuzz target,
malformed corpus, source code, or test suite was consulted.

### IR-0208

The `lzd-dynamic-range` CLI adapter uses only DD-429, the published marc C
config, requirements query and factory, the existing local transactional
temporary-file loop, and the repository-owned CLI regression script. No
external archive tool, command syntax, combined-codec adapter, workspace
policy, source code, or test suite was consulted.

### IR-0209

The `lzd-dynamic-range` benchmark adapter uses only DD-430, the same published
marc C profile as the CLI, the local checked encoded-capacity helper, and the
repository's dependency-free measurement runner. No external benchmark
framework, combined-codec adapter, capacity formula, performance result,
source code, or test suite was consulted.

### IR-0210

Interoperability schema 18 uses only DD-431, the frozen schema-17 manifest
order, the public `lzd-dynamic-range` CLI selector, and marc's existing
deterministic fixture generator, strict verifier, SHA-256 metadata, and
one-generation compatibility chain. No external archive protocol, manifest
schema, interoperability harness, combined-codec archive, corpus, source code,
or test vector was consulted.

### IR-0211

The LZMW plus Dynamic Range reserved representation uses only DD-432, marc's
already documented LZMW variant 1 reference grammar, Dynamic Range variant 1,
generic frame format, checked arithmetic rules, and repository-authored
standalone encoders. No external LZMW/range composition, archive format,
combined-codec implementation, source code, encoded corpus, or test suite was
consulted.

### IR-0212

The first LZMW plus Dynamic Range complete-frame validator uses only DD-433,
DD-432's fixed reference-byte boundary and bounds, marc's generic frame parser,
Dynamic Range descriptor and decoder, existing LZMW validator, caller-owned
spans, and checked aggregate arithmetic. No external combined decoder,
validation order, workspace layout, malformed corpus, source code, or test
suite was consulted.

### IR-0213

The LZMW plus Dynamic Range private raw decoder uses only DD-434, the completed
DD-433 validator, marc's existing iterative LZMW decoder, caller-owned
reference, phrase, expansion-stack, and raw spans, and checked aggregate
arithmetic. No external combined decoder, phrase-expansion implementation,
buffering layout, source code, malformed corpus, or test suite was consulted.

### IR-0214

The LZMW plus Dynamic Range transactional frame decoder uses only DD-435,
DD-434's private reconstruction boundary, caller-owned destination capacity,
and marc's established copy-after-success convention. No external combined
decoder, publication protocol, buffering layout, source code, malformed
corpus, or test suite was consulted.

### IR-0215

The LZMW plus Dynamic Range exact-frame planner uses only DD-436, marc's local
LZMW variant-1 planner and encoder, Dynamic Range variant-1 planner, generic
frame validator, caller-owned workspaces, and checked arithmetic. No external
combined encoder, planning algorithm, buffering layout, source code, encoded
corpus, or test suite was consulted.

### IR-0216

The LZMW plus Dynamic Range deterministic complete-frame encoder uses only
DD-437, DD-436's exact plan, marc's explicit generic-header and Dynamic Range
descriptor serializers, and local Dynamic Range encoder. No external combined
encoder, frame writer, buffering layout, source code, encoded corpus, or test
suite was consulted.

### IR-0217

The LZMW plus Dynamic Range bounded streaming encoder uses only DD-438,
DD-437's deterministic complete-frame encoder, marc's core process contract,
explicit stream/LZMW parameter serializers, checked arithmetic, and
established caller-owned frame-draining state conventions. No external
streaming encoder, state machine, buffering layout, source code, corpus, or
test suite was consulted.

### IR-0218

The LZMW plus Dynamic Range bounded streaming decoder uses only DD-439,
DD-434's private complete-frame decoder, marc's core process contract,
explicit prefix and frame parsers, checked arithmetic, and established caller-
owned validated-frame draining conventions. No external streaming decoder,
state machine, buffering layout, source code, malformed corpus, or test suite
was consulted.

### IR-0219

The LZMW plus Dynamic Range direction-specific profile uses only DD-440,
DD-438/439's established caller-owned regions, marc's local LZMW workspace
formulas and record types, Dynamic Range bounds, checked arithmetic, and the
already published profile conventions. No external allocator, workspace
layout, combined codec, source code, corpus, or test suite was consulted.

### IR-0220

The LZMW plus Dynamic Range C ABI uses only DD-441, DD-440's bounded profile,
the established marc transform lifecycle and three-region ABI, checked opaque
view partitioning, and the first-party C11 assertion harness. No external
combined API, factory, allocator interface, ABI layout, source code, or test
suite was consulted.

### IR-0221

The LZMW plus Dynamic Range public completion matrix uses only DD-442, the
published C configuration/query/factory/process/destroy lifecycle, DD-432
bounds, and repository-authored deterministic fixtures and malformed
mutations. No external completion suite, corpus, combined-codec API, source
code, or test vector was consulted.

### IR-0222

The LZMW plus Dynamic Range bounded fuzz boundary uses only DD-443, the local
exact-frame private decoder, incremental decoder, fixed limit contract, LZMW
reference/phrase/expansion ceilings, and repository-authored canonical stream
generator. No external fuzz harness, corpus, malformed archive, source code,
or test suite was consulted.

### IR-0223

The `lzmw-dynamic-range` CLI adapter uses only DD-444, the published marc C
configuration, requirements query and factory, the existing local
transactional temporary-file loop, and the repository-owned generic CLI
regression script. No external archive tool, command syntax, combined-codec
adapter, workspace policy, source code, or test suite was consulted.

### IR-0224

The `lzmw-dynamic-range` benchmark adapter uses only DD-445, the same published
marc C profile as the CLI, the local checked encoded-capacity helper, and the
repository's dependency-free measurement runner. No external benchmark
framework, combined-codec adapter, capacity formula, performance result,
source code, or test suite was consulted.

### IR-0225

Interoperability schema 19 uses only DD-446, the frozen schema-18 manifest
order, the public `lzmw-dynamic-range` CLI selector, and marc's existing
deterministic fixture generator, strict verifier, SHA-256 metadata, and
one-generation compatibility chain. No external archive protocol, manifest
schema, interoperability harness, combined-codec archive, corpus, source code,
or test vector was consulted.

### IR-0226

The LZ77 plus rANS reserved representation uses only DD-447, marc's already
documented LZ77 variant 1 token grammar, scalar rANS variant 1, generic frame
format, checked arithmetic rules, and repository-authored standalone
encoders. No external LZ77/rANS composition, archive format, combined-codec
implementation, source code, encoded corpus, or test suite was consulted.

### IR-0227

The first LZ77 plus rANS complete-frame validator uses only DD-448, DD-447's
fixed byte-stream boundary and bounds, marc's generic frame parser, rANS
descriptor controller and strict decoder, existing LZ77 validator,
caller-owned spans, and checked aggregate arithmetic. No external combined
decoder, validation order, workspace layout, malformed corpus, source code, or
test suite was consulted.

### IR-0228

The LZ77 plus rANS private raw decoder uses only DD-449, DD-448's complete
validator, marc's existing allocation-free LZ77 decoder and overlap-copy
semantics, separate caller-owned staging, and checked aggregate arithmetic.
No external combined decoder, reconstruction strategy, buffer layout,
malformed corpus, source code, or test suite was consulted.

### IR-0229

The LZ77 plus rANS transactional publication boundary uses only DD-450,
DD-449's private raw decoder, caller-owned spans, exact preflight capacity, and
marc's established bounded copy policy. No external publication protocol,
combined decoder, buffer layout, source code, malformed corpus, or test suite
was consulted.

### IR-0230

The LZ77 plus rANS exact-frame planner uses only DD-451, DD-447's frozen token
boundary, marc's deterministic LZ77 planner and encoder, scalar rANS block
planner, generic frame validation, and checked aggregate arithmetic. No
external combined encoder, planning strategy, buffer layout, encoded corpus,
source code, or test suite was consulted.

### IR-0231

The LZ77 plus rANS complete-frame encoder uses only DD-452, DD-451's exact
planner, marc's generic frame and rANS descriptor serializers, deterministic
scalar rANS encoder, and checked spans. No external combined encoder, frame
writer, output transaction, buffer layout, encoded corpus, source code, or
test suite was consulted.

### IR-0232

The LZ77 plus rANS bounded streaming encoder uses only DD-453, DD-452's local
complete-frame encoder, marc's established immutable frame-drain state
contract, generic process statuses, checked workspace arithmetic, and
caller-owned spans. No external streaming encoder, buffering strategy, state
machine, source code, encoded corpus, or test suite was consulted.

### IR-0233

The LZ77 plus rANS bounded streaming decoder uses only DD-454, DD-450's local
private frame decoder, DD-453's prefix and frame sequence, marc's generic
header parser, rANS view type, checked workspace arithmetic, and immutable raw
drain convention. No external streaming decoder, frame parser, buffering
strategy, state machine, malformed corpus, source code, or test suite was
consulted.

### IR-0234

The LZ77 plus rANS internal profile calculator uses only DD-455, DD-447's
checked `16F`, `528K`, and `S + 8K` bounds, DD-453/DD-454's caller-owned
streaming workspaces, marc's local limits, checked arithmetic, and established
direction-specific profile conventions. No external profile API, allocation
policy, opaque workspace layout, source code, or test suite was consulted.

### IR-0235

The LZ77 plus rANS public C requirements query and factory use only DD-456,
DD-455's direction-specific requirements, the completed local streaming pair,
ABI version 1's existing three-region lifecycle, checked offsets and
alignment, and `nothrow` handle publication. No external C API, allocation
model, combined factory, ABI layout, source code, or test suite was consulted.

### IR-0236

The LZ77 plus rANS public-ABI completion matrix uses only DD-457, the published
marc C lifecycle, DD-447's fixed representation, repository-authored
deterministic generators, and local generic frame-extent parsing. No external
conformance suite, corpus, combined codec API, malformed archive, source code,
or test vector was consulted.

### IR-0237

The LZ77 plus rANS bounded decoder fuzz boundary uses only DD-458, the local
private complete-frame staging decoder, bounded streaming decoder, fixed
caller-owned arrays and view records, checked process invariants, and the
repository-authored truncated-magic seed. No external fuzz harness, corpus,
combined decoder, malformed archive, source code, or test suite was consulted.

### IR-0238

The LZ77 plus rANS CLI adapter uses only DD-459, the published
`marc_lz77_rans_*` lifecycle, DD-455's local bounded profile arithmetic, and
marc's existing transactional file adapter and repository-authored integration
fixture. No external CLI, combined codec wrapper, allocation layout, archive,
source code, or test suite was consulted.

### IR-0239

The LZ77 plus rANS benchmark adapter uses only DD-460, DD-459's fixed public
profile, the published `marc_lz77_rans_*` lifecycle, checked local capacity
arithmetic, and marc's existing dependency-free measurement harness. No
external benchmark wrapper, performance result, capacity formula, source code,
or test suite was consulted.

### IR-0240

Interoperability schema 20 uses only DD-461, the frozen schema-19 manifest
order, marc's deterministic 8,193-byte fixture, the published `lz77-rans` CLI
profile, and the repository-owned generator, verifier, and compatibility
conversion. No external interoperability schema, manifest, archive corpus,
source code, or test suite was consulted.

### IR-0241

The LZSS plus rANS reserved representation uses only DD-462, marc's already
documented LZSS variant-1 token grammar, scalar rANS variant 1, generic frame
serialization, and the existing independent standalone encoders. No external
LZSS/rANS composition, archive format, combined-codec implementation, encoded
corpus, source code, or test suite was consulted.

### IR-0242

The first LZSS plus rANS complete-frame validator uses only DD-463 and DD-462,
marc's generic frame parser, strict two-pass scalar rANS controller and
decoder, checked arithmetic, bounded spans, and existing LZSS validator. No
external combined decoder, validation order, workspace layout, malformed
corpus, source code, or test suite was consulted.

### IR-0243

The LZSS plus rANS private raw decoder uses only DD-464, DD-463, marc's
existing allocation-free LZSS decoder, documented overlap-copy semantics,
checked aggregate arithmetic, and caller-owned bounded spans. No external
combined decoder, reconstruction strategy, buffer layout, source code,
malformed corpus, or test suite was consulted.

### IR-0244

The LZSS plus rANS transactional frame decoder uses only DD-465, DD-464's
private raw decoder, caller-owned spans, exact preflight capacity, and bounded
byte copying. No external publication protocol, output mutation schedule,
combined decoder, source code, malformed corpus, or test suite was consulted.

### IR-0245

The LZSS plus rANS exact-frame planner uses only DD-466, the local LZSS
planner and encoder, scalar rANS block planner, generic frame-header validator,
checked arithmetic, and bounded caller-owned staging. No external combined
encoder, planning algorithm, allocation layout, source code, encoded corpus,
or test suite was consulted.

### IR-0246

The LZSS plus rANS deterministic frame encoder uses only DD-467, DD-466's
exact plan, marc's generic header serializer, scalar rANS descriptor
serializer and encoder, checked subspans, and caller-owned output. No external
combined encoder, frame writer, buffering layout, source code, encoded corpus,
or test suite was consulted.

### IR-0247

The LZSS plus rANS bounded streaming encoder uses only DD-468, DD-467's local
planner and writer, marc's stream-header and LZSS-parameter serializers,
immutable-direction `ProcessResult` contract, checked aggregate arithmetic,
and caller-owned spans. No external streaming encoder, state machine,
buffering strategy, source code, encoded corpus, or test suite was consulted.

### IR-0248

The LZSS plus rANS bounded streaming decoder uses only DD-469, DD-465's local
transactional frame decoder, marc's prefix and generic-header parsers,
immutable-direction transform contract, checked workspace arithmetic, and
caller-owned spans. No external streaming decoder, state machine, buffering
strategy, malformed corpus, source code, or test suite was consulted.

### IR-0249

The LZSS plus rANS internal profile calculator uses only DD-470, the specified
`S <= 2F`, `528K`, and `S + 8K` bounds, DD-468/DD-469's caller-owned
streaming workspaces, marc's local limits, checked arithmetic, and established
direction-specific profile conventions. No external profile API, allocation
policy, opaque workspace layout, source code, or test suite was consulted.

### IR-0250

The LZSS plus rANS public C requirements query and factory use only DD-471,
DD-470's direction-specific requirements, the completed local streaming pair,
ABI version 1's existing three-region lifecycle, checked offsets and
alignment, and `nothrow` handle publication. No external C API, allocation
model, combined factory, ABI layout, source code, or test suite was consulted.

### IR-0251

The LZSS plus rANS public-ABI completion matrix uses only DD-472, the
published `marc_lzss_rans_*` lifecycle, DD-462's fixed representation,
repository-authored deterministic generators, and local generic frame-extent
parsing. No external conformance suite, corpus, combined codec API, malformed
archive, source code, or test vector was consulted.

### IR-0252

The LZSS plus rANS dual-boundary fuzzer and permanent regressions use only
DD-473, the local private frame decoder, published `marc_lzss_rans_*`
lifecycle, repository-authored canonical stream, fixed caller-owned arrays,
and byte-derived chunk schedules. No external fuzz harness, malformed corpus,
seed corpus, source code, sanitizer finding, or test suite was consulted.

### IR-0253

The LZSS plus rANS CLI selector uses only DD-474, the published
`marc_lzss_rans_*` lifecycle, the independently derived fixed-profile bounds,
and marc's existing transactional file adapter and regression script. No
external compression CLI, combined-codec adapter, private workspace layout,
source code, command syntax, or test suite was consulted.

### IR-0254

The LZSS plus rANS benchmark adapter uses only DD-475, DD-474's fixed public
profile, the published `marc_lzss_rans_*` lifecycle, checked complete-stream
capacity arithmetic, and marc's existing verification-first measurement
runner. No external benchmark framework, combined-codec adapter, capacity
formula, performance result, source code, or test suite was consulted.

### IR-0255

Interoperability schema 21 uses only DD-476, the frozen schema-20 manifest
order, marc's deterministic 8,193-byte fixture, the published `lzss-rans` CLI
profile, and the repository-owned generator, verifier, and compatibility
conversion. No external interoperability schema, manifest, archive corpus,
source code, or test suite was consulted.

### IR-0256

The LZ78 plus rANS reserved representation uses only DD-477, marc's documented
LZ78 variant-1 fixed token grammar, scalar rANS variant 1, generic frame
serialization, and the existing independent standalone encoders. No external
LZ78/rANS composition, archive format, combined-codec implementation, encoded
corpus, source code, or test suite was consulted.

### IR-0257

The first LZ78 plus rANS complete-frame validator uses only DD-478, the
repository's generic frame parser, scalar rANS descriptor controller and
decoder validator, LZ78 phrase-graph validator, caller-owned spans, checked
arithmetic, and the frozen independent 592-byte vector. No external combined
decoder, validation order, workspace layout, malformed corpus, source code,
or test suite was consulted.

### IR-0258

The LZ78 plus rANS private raw decoder uses only DD-479, DD-478's complete
validator, marc's existing iterative LZ78 decoder, exact caller-owned raw
staging, and checked aggregate limits. No external combined decoder,
phrase-expansion structure, allocation layout, malformed corpus, source code,
or test suite was consulted.

### IR-0259

The LZ78 plus rANS transactional publication boundary uses only DD-480, the
completed DD-479 private decoder, caller-owned spans, and marc's existing
exact-frame commit convention. No external decompression API, transactional
output strategy, buffer design, malformed corpus, source code, or test suite
was consulted.

### IR-0260

The LZ78 plus rANS exact-frame planner and encoder use only DD-481, marc's
existing deterministic LZ78 encoder, scalar rANS planner and encoder, generic
frame serializer, checked arithmetic, caller-owned spans, and the frozen
592-byte vector. No external combined encoder, block-planning strategy,
workspace layout, encoded corpus, source code, or test suite was consulted.

### IR-0261

The LZ78 plus rANS streaming encoder uses only DD-482, DD-481's exact-frame
planner and encoder, marc's established bounded frame-draining state machine,
explicit stream and parameter serializers, checked aggregate arithmetic, and
caller-owned spans. No external streaming codec, buffering design, state
machine, source code, encoded corpus, or test suite was consulted.

### IR-0262

The LZ78 plus rANS streaming decoder uses only DD-483, DD-479's local private
staging decoder, marc's prefix and generic-header parsers, established
immutable-direction transform contract, checked aggregate arithmetic, and
caller-owned spans. No external streaming decoder, buffering design, state
machine, malformed corpus, source code, encoded corpus, or test suite was
consulted.

### IR-0263

The LZ78 plus rANS internal profile calculator uses only DD-484, the specified
`S <= 8F`, `528K`, and `S + 8K` bounds, DD-482/DD-483's caller-owned streaming
regions, marc's local hard limits, checked alignment arithmetic, and existing
directional profile conventions. No external profile API, allocation policy,
opaque workspace layout, source code, or test suite was consulted.

### IR-0264

The LZ78 plus rANS public C requirements query and factory use only DD-485,
DD-484's direction-specific requirements, the completed local streaming pair,
ABI version 1's existing three-region lifecycle, checked opaque partitioning,
and `nothrow` handle publication. No external C API, allocation model,
combined factory, ABI layout, source code, or test suite was consulted.

### IR-0265

The LZ78 plus rANS public-ABI completion matrix uses only DD-486, the
published `marc_lz78_rans_*` lifecycle, DD-477's fixed representation,
repository-authored deterministic generators, and local generic-frame extent
parsing. No external conformance suite, corpus, combined codec API, malformed
archive, source code, or test vector was consulted.

### IR-0266

The LZ78 plus rANS dual-boundary fuzzer and permanent regressions use only
DD-487, the local private frame decoder, published `marc_lz78_rans_*`
lifecycle, repository-authored canonical stream, fixed caller-owned arrays,
and byte-derived chunk schedules. No external fuzz harness, malformed corpus,
seed corpus, source code, sanitizer finding, or test suite was consulted.

### IR-0267

The LZ78 plus rANS CLI selector uses only DD-488, the published
`marc_lz78_rans_*` lifecycle, independently derived fixed-profile bounds, and
marc's existing transactional file adapter and regression script. No external
compression CLI, combined-codec adapter, private workspace layout, source
code, command syntax, or test suite was consulted.

### IR-0268

The LZ78 plus rANS benchmark adapter uses only DD-489, DD-488's fixed public
profile, the published `marc_lz78_rans_*` lifecycle, checked complete-stream
capacity arithmetic, and marc's existing verification-first measurement
runner. No external benchmark framework, combined-codec adapter, capacity
formula, performance result, source code, or test suite was consulted.

### IR-0269

Interoperability schema 22 uses only DD-490, the frozen schema-21 manifest
order, marc's deterministic 8,193-byte fixture, the published `lz78-rans` CLI
profile, and the repository-owned generator, verifier, and compatibility
conversion. No external interoperability schema, manifest, archive corpus,
source code, or test suite was consulted.

### IR-0270

The LZW plus rANS reserved representation uses only DD-491, marc's documented
LZW variant-1 packed-code grammar, scalar rANS variant 1, generic frame
serialization, and the existing independent standalone encoders. No external
LZW/rANS composition, archive format, combined-codec implementation, encoded
corpus, source code, or test suite was consulted.

### IR-0271

The first LZW plus rANS complete-frame validator uses only DD-492, the
specified DD-491 bounds and validation order, marc's local rANS controller and
decoder, LZW validator, checked arithmetic, and caller-owned spans. No external
combined decoder, allocation layout, error taxonomy, malformed corpus, source
code, or test suite was consulted.

### IR-0272

The LZW plus rANS private raw decoder uses only DD-493, DD-492's complete
validation boundary, marc's local iterative LZW decoder, checked workspace
accounting, and caller-owned spans. No external combined decoder, phrase
expansion implementation, allocation layout, malformed corpus, source code,
or test suite was consulted.

### IR-0273

The LZW plus rANS transactional publication boundary uses only DD-494, the
local private decoder, checked caller capacity, and bounded span copying. No
external publication protocol, combined decoder, buffer layout, malformed
corpus, source code, or test suite was consulted.

### IR-0274

The LZW plus rANS exact-frame planner uses only DD-495, marc's deterministic
LZW planner and encoder, scalar rANS block planner, generic frame validation,
checked arithmetic, and caller-owned staging. No external LZW/rANS encoder,
planning algorithm, capacity formula, allocation layout, source code, encoded
corpus, or test suite was consulted.

### IR-0275

The LZW plus rANS deterministic frame encoder uses only DD-496, DD-495's exact
plan, marc's generic header and rANS descriptor serializers, scalar rANS
encoder, checked spans, and the independent local vector. No external
LZW/rANS frame encoder, serialization schedule, archive format, source code,
encoded corpus, or test suite was consulted.

### IR-0276

The LZW plus rANS bounded streaming encoder uses only DD-497, the local exact
frame planner and encoder, marc's common process contract, explicit stream and
parameter serializers, checked arithmetic, and caller-owned spans. No external
streaming LZW/rANS implementation, buffering schedule, allocation layout,
source code, encoded corpus, or test suite was consulted.

### IR-0277

The LZW plus rANS bounded streaming decoder uses only DD-498, the local private
complete-frame decoder, marc's common process contract and explicit parsers,
checked arithmetic, and caller-owned spans. No external streaming LZW/rANS
decoder, buffering schedule, allocation layout, malformed corpus, source code,
or test suite was consulted.

### IR-0278

The LZW plus rANS internal profile calculator uses only DD-499, the local
streaming constructor contracts, documented LZW width and record bounds, rANS
block bounds, checked arithmetic, and ordinary C++ alignment rules. No external
workspace calculator, ABI layout, allocation scheme, source code, or test suite
was consulted.

### IR-0279

The LZW plus rANS public C adapter uses only DD-500, DD-499's local profile and
partition helpers, marc's existing transform handle lifecycle, fixed-width C
types, and standard C allocation in its test. No external codec ABI, wrapper,
workspace convention, source code, or test suite was consulted.

### IR-0280

The LZW plus rANS public-ABI completion matrix uses only DD-501, the local
DD-500 C lifecycle, marc's existing independently authored LZW completion
schedules, and the documented scalar-rANS block ceiling. No external
conformance suite, encoded corpus, wrapper, source code, or test expression was
consulted.

### IR-0281

The LZW plus rANS dual-boundary fuzzer and permanent regressions use only
DD-502, marc's local complete-frame decoder, DD-500 public C lifecycle,
fixed-array limit arithmetic, local process-result invariants, and a
repository-generated canonical stream. No external fuzz harness, corpus,
malformed archive, source code, or test suite was consulted.

### IR-0282

The `lzw-rans` CLI adapter uses only DD-503, the published marc C config,
requirements query, factory, generic transform lifecycle, documented local
frame and rANS bounds, and the repository's transactional file protocol. No
external CLI, wrapper, archive tool, source code, or test suite was consulted.

### IR-0283

The `lzw-rans` benchmark adapter uses only DD-504, DD-503's public profile,
marc's existing dependency-free measurement runner, checked integer capacity
arithmetic, and the public transform lifecycle. No external benchmark harness,
LZW/rANS tool, encoded corpus, source code, or performance baseline was used.

### IR-0284

Interoperability schema 23 uses only DD-505, the frozen schema-22 manifest
order, marc's deterministic 8,193-byte fixture, the published `lzw-rans` CLI
profile, and the repository-owned generator, verifier, and compatibility
conversion. No external interoperability schema, manifest, archive corpus,
source code, or test suite was consulted.

### IR-0285

The LZD plus rANS reserved representation uses only DD-506, marc's documented
LZD variant-1 reference-pair grammar, scalar rANS variant 1, generic frame
serialization, and the existing independent standalone encoders. No external
LZD/rANS composition, archive format, combined-codec implementation, encoded
corpus, source code, or test suite was consulted.

### IR-0286

The first LZD plus rANS complete-frame validator uses only DD-507, DD-506's
specified bounds and validation order, marc's local rANS controller and
decoder, LZD validator, checked arithmetic, and caller-owned spans. No external
combined decoder, allocation layout, error taxonomy, malformed corpus, source
code, or test suite was consulted.

### IR-0287

The LZD plus rANS private raw decoder uses only DD-508, DD-507's complete
validation boundary, marc's local iterative LZD decoder, checked workspace
accounting, and caller-owned spans. No external combined decoder, phrase
expansion implementation, allocation layout, malformed corpus, source code,
or test suite was consulted.

### IR-0288

The LZD plus rANS transactional publication boundary uses only DD-509, DD-508's
local private decoder, checked caller capacity, and bounded span copying. No
external publication protocol, combined decoder, buffer layout, malformed
corpus, source code, or test suite was consulted.

### IR-0289

The LZD plus rANS exact-frame planner uses only DD-510, marc's local LZD
planner and encoder, scalar rANS block planner, generic frame validator,
checked arithmetic, and caller-owned staging. No external combined encoder,
planning control flow, capacity formula, encoded corpus, source code, or test
suite was consulted.

### IR-0290

The LZD plus rANS deterministic frame encoder uses only DD-511, DD-510's local
exact plan, explicit generic and rANS serializers, the scalar rANS encoder,
checked offsets, and caller-owned output. No external frame writer, combined
encoder, serialization schedule, encoded corpus, source code, or test suite
was consulted.

### IR-0291

The LZD plus rANS bounded streaming encoder uses only DD-512, the repository's
immutable-direction process contract, DD-510/511 complete-frame boundaries,
local stream-prefix serializers, checked aggregate arithmetic, and caller-owned
storage. No external streaming implementation, state machine, buffering
layout, encoded corpus, source code, or test suite was consulted.

### IR-0292

The LZD plus rANS bounded streaming decoder uses only DD-513, the local stream
and frame parsers, DD-508 private decoder, rANS view controller contract, LZD
phrase and expansion bounds, checked aggregate arithmetic, and caller-owned
storage. No external streaming decoder, state machine, buffering layout,
malformed corpus, source code, or test suite was consulted.

### IR-0293

The LZD plus rANS internal profile calculator uses only DD-514, the local
streaming constructor contracts, documented LZD token and record bounds, rANS
block bounds, checked arithmetic, and ordinary C++ alignment rules. No external
workspace calculator, ABI layout, allocation scheme, source code, or test suite
was consulted.

### IR-0294

The LZD plus rANS public C adapter uses only DD-515, DD-514's local profile and
partition helpers, marc's established opaque transform lifecycle, fixed-width C
types, and standard C allocation in its test. No external codec ABI, wrapper,
workspace convention, source code, or test suite was consulted.

### IR-0295

The LZD plus rANS public-ABI completion matrix uses only DD-516, DD-515's local
C lifecycle, marc's existing independently authored LZD completion schedules,
and the documented scalar-rANS block ceiling. No external conformance suite,
encoded corpus, wrapper, source code, or test expression was consulted.

### IR-0296

The LZD plus rANS dual-boundary fuzzer and permanent regressions use only
DD-517, marc's local complete-frame decoder, DD-515 public C lifecycle,
fixed-array limit arithmetic, local process-result invariants, and a
repository-generated canonical stream. No external fuzz harness, corpus,
malformed archive, source code, or test suite was consulted.

### IR-0297

The `lzd-rans` CLI adapter uses only DD-518, the published marc C config,
requirements, factory, process, and destroy functions, and the repository's
existing transactional file adapter and deterministic fixture. No external
LZD/rANS command-line tool, wrapper, archive, source code, or test suite was
consulted.

### IR-0298

The LZD plus rANS benchmark adapter uses only DD-519, DD-518's public profile,
marc's dependency-free benchmark runner, checked half-pair and frame arithmetic,
and the published C lifecycle. No external LZD/rANS benchmark, wrapper,
measurement result, source code, or capacity formula was consulted.

### IR-0299

Interoperability schema 24 uses only DD-520, the frozen schema-23 profile order,
marc's deterministic 8,193-byte fixture, the published `lzd-rans` selector,
PowerShell file/hash facilities, and the repository's existing bundle scripts.
No external archive, codec implementation, conformance suite, manifest design,
source code, or test corpus was consulted.

### IR-0300

The LZMW plus rANS reserved representation uses only DD-521, marc's documented
LZMW variant-1 phrase-reference grammar, scalar rANS variant 1, generic frame
serialization, and the existing independent standalone encoders. No external
LZMW/rANS composition, archive format, combined-codec implementation, encoded
corpus, source code, or test suite was consulted.

### IR-0301

The first LZMW plus rANS complete-frame validator uses only DD-522, DD-521's
exact reserved representation, marc's scalar rANS controller and decoder, the
ordinary LZMW token validator, checked arithmetic, and caller-owned spans. No
external combined validator, allocation layout, malformed corpus, source code,
or test suite was consulted.

### IR-0302

The LZMW plus rANS private raw decoder uses only DD-523, DD-522's complete
validation boundary, marc's local iterative LZMW decoder, checked workspace
accounting, and caller-owned spans. No external combined decoder, phrase
expansion implementation, allocation layout, malformed corpus, source code,
or test suite was consulted.

### IR-0303

The LZMW plus rANS transactional publication boundary uses only DD-524,
DD-523's local private decoder, checked caller capacity, and bounded span
copying. No external publication protocol, combined decoder, buffer layout,
malformed corpus, source code, or test suite was consulted.

### IR-0304

The LZMW plus rANS exact-frame planner uses only DD-525, marc's local LZMW
planner and encoder, scalar rANS block planner, generic frame validator,
checked arithmetic, and caller-owned staging. No external combined encoder,
planning control flow, capacity formula, encoded corpus, source code, or test
suite was consulted.

### IR-0305

The LZMW plus rANS deterministic frame encoder uses only DD-526, DD-525's exact
plan, the independent 592-byte vector, local generic-frame and scalar-rANS
serializers, checked offsets, and bounded spans. No external combined encoder,
serialization schedule, buffer layout, encoded corpus, source code, or test
suite was consulted.

### IR-0306

The LZMW plus rANS bounded streaming encoder uses only DD-527, the local exact-
frame planner and encoder, marc's transform status contract, stream-prefix
serializers, checked aggregate accounting, and bounded caller-owned spans. No
external streaming codec, buffering design, state machine, source code,
encoded corpus, or test suite was consulted.

### IR-0307

The LZMW plus rANS bounded streaming decoder uses only DD-528, the local
complete-frame validator and private decoder, generic header parsers, checked
workspace accounting, and marc's transform status contract. No external
streaming decoder, parser state machine, buffering layout, malformed corpus,
source code, or test suite was consulted.

### IR-0308

The LZMW plus rANS profile calculator uses only DD-529, the local `4F`
reference ceiling, scalar-rANS descriptor and payload bounds, LZMW record
limits, checked arithmetic, standard alignment, and the existing streaming
constructors. No external requirements API, opaque-layout convention, source
code, or test suite was consulted.

### IR-0309

The LZMW plus rANS public C boundary uses only DD-530, the local profile
calculator and partition helpers, existing C transform lifecycle, fixed-width
ABI conventions, and caller-owned buffers. No external C factory, requirements
API, ownership protocol, source code, or test suite was consulted.

### IR-0310

The LZMW plus rANS public-ABI completion matrix uses only DD-531, DD-530's
local public C lifecycle, marc's independently authored completion schedule,
and the documented scalar-rANS and `4F` ceilings. No external conformance
suite, encoded corpus, wrapper, source code, or test expression was consulted.

### IR-0311

The LZMW plus rANS decoder fuzz boundary uses only DD-532, the local private
complete-frame decoder, DD-530's public C lifecycle, fixed local arrays, and
marc's transform invariants. No external fuzz harness, malformed corpus,
combined decoder, source code, or regression suite was consulted.

### IR-0312

The `lzmw-rans` CLI adapter uses only DD-533, the published marc C config,
requirements, factory, process, and destroy functions, and the repository's
existing transactional file adapter and deterministic fixture. No external
LZMW/rANS command-line tool, wrapper, archive, source code, or test suite was
consulted.

### IR-0313

The LZMW plus rANS benchmark adapter uses only DD-534, DD-533's public profile,
the published `marc_lzmw_rans_*` lifecycle, checked integer arithmetic, and
marc's local benchmark timing/reporting helpers. No external LZMW/rANS
benchmark, wrapper, capacity expression, performance result, source code, or
test suite was consulted.

### IR-0314

Interoperability schema 25 uses only DD-535, the repository-owned schemas 1
through 24, the public `lzmw-rans` CLI selector, PowerShell file/hash APIs, and
the existing deterministic 8,193-byte fixture. No external bundle generator,
manifest schema, archive corpus, verification script, source code, or test
suite was consulted.

### IR-0315

The LZ77 plus tANS reserved representation uses only DD-537, marc's already
documented LZ77 variant 1 token grammar, tabled tANS variant 1, generic frame
format, checked arithmetic rules, and repository-authored standalone encoders.
No external LZ77/tANS composition, FSE format, archive format, combined-codec
implementation, source code, encoded corpus, or test suite was consulted.

### IR-0316

The first LZ77 plus tANS complete-frame validator uses only DD-538, DD-537's
fixed byte-stream boundary and bounds, marc's generic frame parser, tANS
descriptor controller and strict decoder, existing LZ77 validator,
caller-owned spans, and checked aggregate arithmetic. No external combined
decoder, validation order, workspace layout, malformed corpus, source code, or
test suite was consulted.

### IR-0317

The LZ77 plus tANS private raw decoder uses only DD-539, DD-538's complete
validator, marc's existing allocation-free LZ77 decoder and overlap-copy
semantics, separate caller-owned staging, and checked aggregate arithmetic.
No external combined decoder, reconstruction strategy, buffer layout,
malformed corpus, source code, or test suite was consulted.

### IR-0318

The LZ77 plus tANS transactional publication boundary uses only DD-540,
DD-539's private raw decoder, caller-owned spans, exact preflight capacity, and
marc's established bounded copy policy. No external publication protocol,
combined decoder, buffer layout, source code, malformed corpus, or test suite
was consulted.

### IR-0319

The LZ77 plus tANS exact-frame planner uses only DD-541, the local LZ77 token
planner and encoder, tANS block planner, generic frame validator, checked
arithmetic, and caller-owned staging. No external combined encoder, planning
strategy, allocation layout, source code, encoded corpus, or test suite was
consulted.

### IR-0320

The LZ77 plus tANS complete-frame writer uses only DD-542, DD-541's exact
planner, marc's explicit generic-header and tANS descriptor serializers, local
tANS encoder, checked spans, and frozen token staging. No external combined
encoder, archive writer, serialization layout, source code, encoded corpus, or
test suite was consulted.

### IR-0321

The LZ77 plus tANS known-size streaming encoder uses only DD-543, DD-542's
complete-frame writer, marc's core transform contract, explicit stream and
parameter serializers, caller-owned spans, and checked aggregate arithmetic.
No external streaming encoder, buffering state machine, source code, chunking
suite, or test expression was consulted.

### IR-0322

The LZ77 plus tANS known-size streaming decoder uses only DD-544, the local
prefix and frame parsers, DD-539 private frame decoder, core transform contract,
caller-owned storage, and checked aggregate arithmetic. No external streaming
decoder, buffering state machine, malformed corpus, source code, or test suite
was consulted.

### IR-0323

The LZ77 plus tANS internal profile calculator uses only DD-545, the specified
`16F`, `528K`, and per-block `2 + ceil(12n/8)` bounds, DD-543/DD-544's
caller-owned streaming regions, marc's local hard limits, checked arithmetic,
and existing directional profile conventions. No external profile API,
allocation policy, source code, corpus, or test suite was consulted.

### IR-0324

The LZ77 plus tANS public C ABI uses only DD-546, DD-545's directional
requirements, marc's existing fixed-width C transform lifecycle, checked
workspace partitioning, and internal tANS view alignment. No external ABI,
factory design, allocator contract, source code, or C test suite was consulted.

### IR-0325

The LZ77 plus tANS public-ABI completion matrix uses only DD-547, the published
`marc_lz77_tans_*` lifecycle, DD-537's fixed representation, repository-authored
deterministic generators, and local generic-frame extent parsing. No external
conformance suite, corpus, combined codec API, malformed archive, source code,
or test vector was consulted.

### IR-0326

The LZ77 plus tANS bounded decoder fuzz boundary uses only DD-548, the local
complete-frame and incremental decoders, `TansBlockView`, fixed caller-owned
arrays, the core process invariants, and a repository-authored truncated-magic
seed. No external fuzz harness, corpus, mutation schedule, combined decoder,
source code, or test suite was consulted.

### IR-0327

The LZ77 plus tANS CLI adapter uses only DD-549, the published
`marc_lz77_tans_*` lifecycle, DD-545's checked profile arithmetic, and marc's
existing transactional file adapter and repository-authored integration
fixture. No external CLI, combined codec wrapper, allocation layout, archive,
source code, or test suite was consulted.

### IR-0328

The LZ77 plus tANS benchmark adapter uses only DD-550, DD-549's fixed public
profile, the published `marc_lz77_tans_*` lifecycle, checked local capacity
arithmetic, and marc's dependency-free measurement harness. No external
benchmark wrapper, performance result, capacity formula, source code, or test
suite was consulted.

### IR-0329

Interoperability schema 26 uses only DD-551, the frozen schema-25 manifest
order, marc's deterministic 8,193-byte fixture, the published `lz77-tans` CLI
profile, and repository-owned generator, verifier, and compatibility
conversion. No external schema, manifest, archive corpus, source code, or test
suite was consulted.

### IR-0330

The schema-26 external admission record uses only DD-552, the pushed CI result
for exact revision `5b2aa31ba3333c311ad4086b3438915a6c3ce36d`, and the four
verifier result lines reported from the established Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang exchange. No external codec implementation,
archive corpus, source code, or conformance suite was consulted.

### IR-0331

The LZSS plus tANS reserved representation uses only DD-553, marc's already
documented LZSS variant-1 variable-length token grammar, tabled tANS variant
1, generic frame format, checked arithmetic rules, and repository-authored
standalone encoders. No external LZSS/tANS composition, FSE format, archive
format, combined-codec implementation, source code, encoded corpus, or test
suite was consulted.

### IR-0332

The first LZSS plus tANS complete-frame validator uses only DD-554, DD-553's
fixed byte boundary and bounds, marc's generic frame parser, tANS descriptor
controller and strict decoder, existing LZSS validator, caller-owned spans,
and checked aggregate arithmetic. No external combined decoder, validation
order, workspace layout, malformed corpus, source code, or test suite was
consulted.

### IR-0333

The LZSS plus tANS private raw decoder uses only DD-555, DD-554's complete
validator, marc's existing allocation-free LZSS decoder and overlap-copy
semantics, separate caller-owned staging, and checked aggregate arithmetic.
No external combined decoder, reconstruction strategy, buffer layout,
malformed corpus, source code, or test suite was consulted.

### IR-0334

The LZSS plus tANS transactional publication boundary uses only DD-556,
DD-555's private raw decoder, caller-owned spans, exact preflight capacity, and
marc's established bounded copy policy. No external publication protocol,
combined decoder, buffer layout, source code, malformed corpus, or test suite
was consulted.

### IR-0335

The LZSS plus tANS exact-frame planner uses only DD-557, the local LZSS token
planner and encoder, tANS block planner, generic frame validator, checked
arithmetic, and caller-owned staging. No external combined encoder, planning
strategy, allocation layout, source code, encoded corpus, or test suite was
consulted.

### IR-0336

The LZSS plus tANS complete-frame writer uses only DD-558, DD-557's exact
planner, marc's explicit generic-header and tANS descriptor serializers, local
tANS encoder, checked spans, and frozen token staging. No external combined
encoder, archive writer, serialization layout, source code, encoded corpus, or
test suite was consulted.

### IR-0337

The LZSS plus tANS known-size streaming encoder uses only DD-559, DD-558's
complete-frame writer, marc's core transform contract, explicit stream and
parameter serializers, caller-owned spans, and checked aggregate arithmetic.
No external streaming encoder, buffering state machine, source code, chunking
suite, or test expression was consulted.

### IR-0338

The LZSS plus tANS known-size streaming decoder uses only DD-560, DD-556's
private complete-frame decoder, marc's generic stream and frame parsers, core
transform contract, caller-owned spans, and checked aggregate arithmetic. No
external streaming decoder, buffering state machine, malformed corpus, source
code, chunking suite, or test expression was consulted.

### IR-0339

The LZSS plus tANS internal profile calculator uses only DD-561, DD-559 and
DD-560's caller-owned regions, marc's hard limits, checked arithmetic, and the
documented tANS descriptor and blockwise payload ceilings. No external profile
API, allocation policy, capacity formula, codec source, encoded corpus, or
test suite was consulted.

### IR-0340

The LZSS plus tANS public C ABI uses only DD-562, DD-561's directional
requirements, marc's existing fixed-width C transform lifecycle, checked
workspace partitioning, and internal tANS view alignment. No external ABI,
factory design, allocator contract, source code, or C test suite was consulted.

### IR-0341

The LZSS plus tANS public-ABI completion matrix uses only DD-563, the published
`marc_lzss_tans_*` lifecycle, DD-553's fixed representation,
repository-authored deterministic generators, and local generic-frame extent
parsing. No external conformance suite, corpus, combined codec API, malformed
archive, source code, or test vector was consulted.

### IR-0342

The LZSS plus tANS bounded fuzz boundary uses only DD-564, the repository's
existing LZ77/tANS and LZSS decoder-harness conventions, the local composed
decoders, fixed caller-owned arrays, and canonical streams generated by the
local encoder. No external fuzzer harness, corpus, malformed archive, codec
source, mutation schedule, or test suite was consulted.

### IR-0343

The LZSS plus tANS CLI adapter uses only DD-565, the published
`marc_lzss_tans_*` lifecycle, the locally derived fixed profile bounds, and
marc's existing transactional file adapter and fixture. No external CLI,
wrapper, private workspace layout, archive, source code, or integration test
was consulted.

### IR-0344

The LZSS plus tANS benchmark adapter uses only DD-566, DD-565's fixed public
profile, the `marc_lzss_tans_*` lifecycle, checked local capacity arithmetic,
and marc's existing verified measurement loop. No external benchmark harness,
workspace layout, formula, fixture, source code, or result was consulted.

### IR-0345

Interoperability schema 27 uses only DD-567, the frozen local schema-26 order,
marc's deterministic 8,193-byte fixture, the published `lzss-tans` CLI
profile, and repository-owned bundle scripts. No external archive, manifest,
implementation, compatibility suite, or result was consulted.

### IR-0346

The LZ78 plus tANS representation reservation uses only DD-568, marc's
canonical fixed eight-byte LZ78 token format and hand vectors, the local tANS
normalization, spread, reverse-state recurrence, descriptor format, and
generic frame serializer. No external combined codec, encoded corpus, source
code, format, test vector, or implementation result was consulted.

### IR-0347

The first LZ78 plus tANS complete-frame validator uses only DD-569, DD-568's
fixed representation and bounds, marc's generic frame parser, local tANS
descriptor controller and strict decoder, existing LZ78 phrase validator,
caller-owned spans, and checked aggregate arithmetic. No external combined
decoder, validation order, workspace layout, malformed corpus, source code, or
test suite was consulted.

### IR-0348

The LZ78 plus tANS private raw decoder uses only DD-570, DD-569's complete
validator, marc's allocation-free LZ78 decoder and iterative phrase expansion,
separate caller-owned staging, and checked aggregate arithmetic. No external
combined decoder, reconstruction strategy, buffer layout, malformed corpus,
source code, or test suite was consulted.

### IR-0349

The LZ78 plus tANS transactional publication boundary uses only DD-571,
DD-570's private decoder, caller-owned spans, exact output-capacity preflight,
and marc's established bounded copy policy. No external publication protocol,
combined decoder, buffer layout, malformed corpus, source code, or test suite
was consulted.

### IR-0350

The LZ78 plus tANS encoder-side planner uses only DD-572, marc's local LZ78
encoder and workspace rules, DD-568's bounds, local tANS block planner,
generic frame validator, caller-owned spans, and checked arithmetic. No
external combined encoder, planning algorithm, storage layout, encoded corpus,
source code, or test suite was consulted.

### IR-0351

The LZ78 plus tANS complete-frame writer uses only DD-573, DD-572's exact
planner and frozen token staging, marc's generic frame serializer, local tANS
descriptor serializer and block encoder, checked offsets, and caller-owned
output. No external combined writer, serialization schedule, buffering layout,
encoded corpus, source code, or test suite was consulted.

### IR-0352

The LZ78 plus tANS known-size streaming encoder uses only DD-574, DD-573's
exact complete-frame writer, local stream and LZ78 parameter serializers,
checked arithmetic, caller-owned workspaces, and marc's process-result
contract. No external streaming encoder, state machine, buffering layout,
chunk schedule, source code, or test suite was consulted.

### IR-0353

The LZ78 plus tANS known-size streaming decoder uses only DD-575, DD-569
through DD-574, the local generic and tANS header bounds, the private LZ78+tANS
decoder, checked arithmetic, caller-owned spans, and marc's process-result
contract. No external streaming decoder, collection state machine, workspace
layout, malformed corpus, source code, or test suite was consulted.

### IR-0354

The LZ78 plus tANS internal profile calculator uses only DD-576, DD-574 and
DD-575 constructor requirements, DD-568's `8F` bound, local tANS payload
ceilings, LZ78 record and phrase rules, checked arithmetic, and C++ alignment
requirements. No external profile formula, allocator layout, source code,
workspace API, or test suite was consulted.

### IR-0355

The LZ78 plus tANS public C requirements and factory use only DD-577, DD-576's
internal requirements and partitions, marc's existing size-tagged C ABI,
non-throwing transform allocation, stable status mapping, and caller-owned
three-region convention. No external C wrapper, ABI layout, allocation policy,
source code, or test suite was consulted.

### IR-0356

The LZ78 plus tANS CLI selector uses only DD-578, the published
`marc_lz78_tans_*` lifecycle, fixed local format bounds, and marc's existing
transactional CLI adapter and regression script. No external compression CLI,
workspace layout, command syntax, archive, source code, or test suite was
consulted.

### IR-0357

The LZ78 plus tANS benchmark uses only DD-579, DD-578's fixed public profile,
the published C lifecycle, checked complete-stream capacity arithmetic, and
marc's dependency-free timing and workspace-reporting harness. No external
benchmark adapter, tool, corpus result, capacity formula, source code, or test
suite was consulted.

### IR-0358

The LZ78 plus tANS bounded decoder fuzzer and permanent regressions use only
DD-580, marc's private complete-frame decoder, published public C streaming
lifecycle, local tANS descriptor and state rules, LZ78 phrase validation,
fixed arrays, and the core progress contract. No external fuzz harness,
mutation strategy, corpus, crash, malformed fixture, source code, or test suite
was consulted.

### IR-0359

The LZ78 plus tANS public-ABI completion matrix uses only DD-581, the published
size-tagged C lifecycle, DD-577's workspace contract, deterministic local
fixture generation, and marc's existing completion categories. No external
completion suite, encoded vector, chunk schedule, malformed corpus, source
code, or test expression was consulted.

### IR-0360

Interoperability schema 28 uses only DD-582, the frozen local schema-27 order,
marc's deterministic 8,193-byte fixture, the published `lz78-tans` CLI
profile, and repository-owned bundle scripts. No external archive, manifest,
implementation, compatibility suite, or result was consulted.

### IR-0361

The LZW plus tANS representation reservation uses only DD-583, marc's
canonical packed LZW code format and hand vectors, the local tANS
normalization, spread, reverse-state recurrence, descriptor format, and
generic frame serializer. No external combined codec, encoded corpus, source
code, format, test vector, or implementation result was consulted.

### IR-0362

The first LZW plus tANS complete-frame validator uses only DD-584, DD-583's
fixed representation and bounds, marc's generic frame parser, local tANS
descriptor controller and strict decoder, existing LZW code-stream validator,
caller-owned spans, and checked aggregate arithmetic. No external combined
decoder, validation order, workspace layout, malformed corpus, source code, or
test suite was consulted.

### IR-0363

The LZW plus tANS private raw decoder uses only DD-585, DD-584's complete
validation boundary, marc's local iterative LZW decoder, checked workspace
accounting, and caller-owned spans. No external combined decoder, phrase
expansion implementation, allocation layout, malformed corpus, source code,
or test suite was consulted.

### IR-0364

The LZW plus tANS transactional publication boundary uses only DD-586,
DD-585's private raw decoder, caller-owned spans, exact destination preflight,
and one bounded final copy. No external combined decoder, commit protocol,
buffer ownership model, malformed corpus, source code, or test suite was
consulted.

### IR-0365

The LZW plus tANS exact-frame planner uses only DD-587, marc's deterministic
LZW planner and encoder, local tANS block planner, generic frame validation,
checked arithmetic, and caller-owned staging. No external LZW/tANS encoder,
planning algorithm, capacity formula, allocation layout, source code, encoded
corpus, or test suite was consulted.

### IR-0366

The LZW plus tANS deterministic frame encoder uses only DD-588, DD-587's exact
plan, marc's generic header and tANS descriptor serializers, local tANS
encoder, checked spans, and the independent local vector. No external LZW/tANS
frame encoder, serialization schedule, archive format, source code, encoded
corpus, or test suite was consulted.

### IR-0367

The LZW plus tANS bounded streaming encoder uses only DD-589, the local exact
frame planner and encoder, marc's common process contract, explicit stream and
parameter serializers, checked arithmetic, and caller-owned spans. No external
streaming LZW/tANS implementation, buffering schedule, allocation layout,
source code, encoded corpus, or test suite was consulted.

### IR-0368

The LZW plus tANS bounded streaming decoder uses only DD-590, the local private
complete-frame decoder, marc's common process contract and explicit parsers,
checked arithmetic, and caller-owned spans. No external streaming LZW/tANS
decoder, buffering schedule, allocation layout, malformed corpus, source code,
or test suite was consulted.

### IR-0369

The LZW plus tANS workspace profile uses only DD-591, DD-589 and DD-590's local
streaming constructors, the repository's LZW width and record bounds, tANS
block constants and payload ceiling, checked arithmetic, and C++ object
alignment. No external combined workspace calculator, ABI layout, allocation
scheme, source code, or test suite was consulted.

### IR-0370

The LZW plus tANS C ABI uses only DD-592, DD-591's local workspace profile,
marc's existing three-region transform lifecycle, stable status mapping, and
the completed local streaming pair. No external combined C API, ABI layout,
factory ownership model, source code, or test suite was consulted.

### IR-0371

The LZW plus tANS public completion matrix uses only DD-593, DD-592's public C
lifecycle, the local generic frame extent, deterministic generator, and
process-result contract. No external LZW/tANS corpus, completion suite,
chunking schedule, malformed vector, source code, or test expression was
consulted.

### IR-0372

The LZW plus tANS bounded fuzz boundary uses only DD-594, DD-592's public
decoder, the local complete-frame decoder, fixed caller-owned arrays, stable
process invariants, and deterministic chunk derivation. No external fuzz
harness, seed corpus, malformed suite, LZW/tANS implementation, source code,
or test expression was consulted.

### IR-0373

The LZW plus tANS CLI selector uses only DD-595, the published
`marc_lzw_tans_*` lifecycle, independently derived fixed-profile bounds, and
marc's existing transactional file adapter and regression script. No external
compression CLI, combined-codec adapter, private workspace layout, source
code, command syntax, or test suite was consulted.

### IR-0374

The LZW plus tANS benchmark adapter uses only DD-596, DD-595's fixed public
profile, the published `marc_lzw_tans_*` lifecycle, checked complete-stream
capacity arithmetic, and marc's existing verification-first measurement
runner. No external benchmark framework, combined adapter, capacity formula,
performance result, source code, or test suite was consulted.

### IR-0375

Interoperability schema 29 uses only DD-597, the frozen local schema-28 order,
marc's deterministic 8,193-byte fixture, the published `lzw-tans` CLI profile,
and the repository-owned generator, verifier, and compatibility conversion.
No external interoperability schema, manifest, archive corpus, source code, or
test suite was consulted.

### IR-0376

The LZD plus tANS reserved representation uses only DD-598, marc's documented
LZD variant-1 reference grammar and encoder, the local tabled tANS planner,
encoder, descriptor serializer, generic frame serializer, and checked bounds.
No external combined implementation, FSE format, source code, encoded corpus,
or test suite was consulted.

### IR-0377

The first LZD plus tANS complete-frame validator uses only DD-599, DD-598's
fixed representation and bounds, marc's generic frame parser, local tANS
descriptor controller and strict decoder, existing LZD validator,
caller-owned spans, and checked aggregate arithmetic. No external combined
decoder, validation order, workspace layout, malformed corpus, source code,
or test suite was consulted.

### IR-0378

The LZD plus tANS private raw decoder uses only DD-600, DD-599's complete
validator, marc's existing allocation-free non-recursive LZD decoder,
caller-owned raw and expansion spans, and checked aggregate arithmetic. No
external combined decoder, reconstruction strategy, recursion scheme, buffer
layout, malformed corpus, source code, or test suite was consulted.

### IR-0379

The LZD plus tANS transactional frame decoder uses only DD-601, DD-600's
private raw decoder, caller-owned spans, exact output preflight, and bounded
byte copying. No external publication protocol, combined decoder, mutation
schedule, buffer layout, malformed corpus, source code, or test suite was
consulted.

### IR-0380

The LZD plus tANS exact-frame planner uses only DD-602, the local deterministic
LZD planner and encoder, tabled tANS block planner, generic frame validator,
checked arithmetic, and caller-owned staging. No external combined encoder,
planning algorithm, allocation layout, source code, encoded corpus, or test
suite was consulted.

### IR-0381

The LZD plus tANS deterministic frame encoder uses only DD-603, DD-602's exact
plan, marc's generic frame serializer, local tANS descriptor serializer and
encoder, checked subspans, and caller-owned output. No external combined
encoder, frame writer, buffering layout, source code, encoded corpus, or test
suite was consulted.

### IR-0382

The LZD plus tANS bounded streaming encoder uses only DD-604, DD-602/DD-603's
local planner and writer, marc's stream-header and LZD-parameter serializers,
immutable-direction `ProcessResult` contract, checked aggregate arithmetic,
and caller-owned spans. No external streaming encoder, state machine,
buffering strategy, source code, encoded corpus, or test suite was consulted.

### IR-0383

The LZD plus tANS bounded streaming decoder uses only DD-605, DD-600/DD-601's
local validator and private decoder, marc's stream and frame parsers, checked
tANS payload ceilings, LZD workspace calculators, immutable-direction
`ProcessResult` contract, and caller-owned spans. No external streaming
decoder, state machine, buffering strategy, malformed corpus, source code, or
test suite was consulted.

### IR-0384

The LZD plus tANS profile calculator uses only DD-606, the local LZD token and
workspace formulas, local tANS descriptor and payload ceilings, checked
arithmetic helpers, decoder limits, and the DD-604/DD-605 transform
constructors. No external profile API, allocator layout, workspace formula,
source code, generated corpus, or test suite was consulted.

### IR-0385

The LZD plus tANS public C adapter uses only DD-607, DD-606's local profile and
partitioners, DD-604/DD-605 transforms, marc's fixed-width ABI conventions,
checked byte-span partitioning, and common transform lifecycle. No external C
API, factory design, allocation scheme, source code, generated corpus, or test
suite was consulted.

### IR-0386

The LZD plus tANS public-ABI completion matrix uses only DD-608, DD-607's local
C functions, marc's existing LZD public admission schedules, deterministic
local byte generation, and the documented tANS block ceiling. No external
vector, corpus, completion suite, implementation behavior, or test expression
was consulted.

### IR-0387

The LZD plus tANS bounded fuzz boundary uses only DD-609, DD-607's public
decoder, the local complete-frame decoder, fixed caller-owned arrays, checked
workspace formulas, and marc's process invariants. No external fuzz harness,
seed corpus, mutation dictionary, malformed suite, implementation source, or
test expression was consulted.

### IR-0388

The LZD plus tANS CLI selector uses only DD-610, the published
`marc_lzd_tans_*` lifecycle, independently derived fixed-profile bounds, and
marc's existing transactional file adapter and regression script. No external
compression CLI, combined-codec adapter, private workspace layout, source code,
command syntax, or test suite was consulted.

### IR-0389

The LZD plus tANS benchmark adapter uses only DD-611, DD-610's fixed public
profile, the published `marc_lzd_tans_*` lifecycle, checked complete-stream
capacity arithmetic, and marc's existing verification-first measurement
runner. No external benchmark framework, combined adapter, capacity formula,
performance result, source code, or test suite was consulted.

### IR-0390

Interoperability schema 30 uses only DD-612, the frozen local schema-29 order,
marc's deterministic 8,193-byte fixture, the published `lzd-tans` CLI profile,
and the repository-owned generator, verifier, and compatibility conversion.
No external interoperability schema, manifest, archive corpus, source code, or
test suite was consulted.

### IR-0391

The LZMW plus tANS reserved representation uses only DD-613, marc's documented
LZMW variant-1 reference grammar and encoder, the local tabled tANS planner,
encoder, descriptor serializer, generic frame serializer, and checked bounds.
No external combined implementation, FSE format, source code, encoded corpus,
or test suite was consulted.

### IR-0392

The first LZMW plus tANS complete-frame validator uses only DD-614, DD-613's
fixed representation and bounds, marc's generic frame parser, local tANS
descriptor controller and strict decoder, existing LZMW validator,
caller-owned spans, and checked aggregate arithmetic. No external combined
decoder, validation order, workspace layout, malformed corpus, source code, or
test suite was consulted.

### IR-0393

The LZMW plus tANS private reconstruction and transactional publication use
only DD-615, DD-614's complete validator, marc's existing allocation-free LZMW
decoder and iterative expansion rules, separate caller-owned staging, and
checked aggregate arithmetic. No external decoder, publication protocol,
workspace layout, source code, malformed corpus, or test suite was consulted.

### IR-0394

The LZMW plus tANS exact-frame planner uses only DD-616, marc's local LZMW
planner and canonical reference encoder, the local tANS block planner,
generic frame validation, and checked aggregate arithmetic. No external
combined encoder, planning order, workspace layout, source code, encoded
corpus, or test suite was consulted.

### IR-0395

The LZMW plus tANS deterministic frame encoder uses only DD-617 and DD-616's
fixed plan, marc's explicit generic-header and tANS descriptor serializers,
the local tANS encoder, and checked span arithmetic. No external combined
encoder, serialization order, transactional write protocol, source code,
encoded corpus, or test suite was consulted.

### IR-0396

The LZMW plus tANS bounded streaming encoder uses only DD-618, the local exact
planner and deterministic frame encoder, marc's core process/status contract,
explicit stream serializers, caller-owned spans, and checked aggregate
arithmetic. No external streaming implementation, buffering policy, state
machine, source code, encoded corpus, or test suite was consulted.

### IR-0397

The LZMW plus tANS bounded streaming decoder uses only DD-619, DD-615's local
transactional frame decoder, marc's prefix and frame parsers, core
process/status contract, caller-owned spans, and checked aggregate arithmetic.
No external streaming decoder, buffering policy, state machine, source code,
malformed corpus, or test suite was consulted.

### IR-0398

The LZMW plus tANS internal profile calculator uses only DD-620, DD-618,
DD-619, marc's local LZMW token and phrase bounds, the local tANS descriptor
and payload limits, checked arithmetic, and existing workspace-partition
patterns. No external profile calculator, combined-codec allocation layout,
ABI definition, source code, encoded corpus, or test suite was consulted.

### IR-0399

The LZMW plus tANS public C adapter uses only DD-621, DD-620, marc's existing
opaque transform lifecycle, fixed-width ABI conventions, and the local bounded
streaming pair. No external C wrapper, configuration layout, allocation
protocol, source code, encoded corpus, or test suite was consulted.

### IR-0400

The LZMW plus tANS public completion matrix uses only DD-622, DD-621, the
repository-owned LZD completion harness, and the proven equality of the two
256-byte reference ceilings at the fixed 64-byte frame. No external corpus,
combined-codec test matrix, malformed data, source code, or test suite was
consulted.

### IR-0401

The LZMW plus tANS bounded fuzz boundary uses only DD-623, DD-615, DD-619,
DD-621, marc's local fixed-memory LZMW/rANS harness, and the local tANS adapter
pattern. No external fuzz harness, corpus, mutation dictionary, malformed
stream, source code, or test suite was consulted.

### IR-0402

The `lzmw-tans` CLI adapter uses only DD-624, the public
`marc_lzmw_tans_*` lifecycle, the independently fixed profile bounds, and
marc's existing transactional file adapter and regression script. No external
compression CLI, combined-codec adapter, private workspace layout, command
syntax, source code, or test suite was consulted.

### IR-0403

The `lzmw-tans` benchmark adapter uses only DD-625, DD-624's public profile,
the `marc_lzmw_tans_*` lifecycle, locally derived checked capacity arithmetic,
and marc's verification-first timing/reporting runner. No external benchmark
framework, combined-codec adapter, capacity expression, performance result,
source code, or test suite was consulted.

### IR-0404

Interoperability schema 31 uses only DD-626, the frozen local schema-30 order,
marc's deterministic 8,193-byte fixture, the published `lzmw-tans` CLI
profile, and the repository-owned generator, verifier, and compatibility
conversion. No external interoperability schema, manifest, archive corpus,
source code, or test suite was consulted.

### IR-0405

The LZSS typed-token protocol uses the user-proposed typed dictionary boundary,
AGENTS.md section 11.2, DD-627, and marc's independently specified LZSS
variant-1 parse and validation rules. No external typed-token library, LZSS
implementation, object layout, serialization protocol, source code, or test
suite was consulted.

### IR-0406

The `LzssFieldContext` model uses the user-proposed context layer, DD-628, the
local typed-token vocabulary, and independently chosen previous-token,
previous-Literal-nibble, length-class, and distance-class states. No external
context mixer, compressor model, context numbering, field split, source code,
encoded corpus, or test suite was consulted.

### IR-0407

The entropy-backend contract and contextual Dynamic Range variant 2 use
DD-629, marc's local bounded block contracts, and the already specified
variant-1 integer range arithmetic. The per-context model ownership and fixed
bypass-bit rule were designed locally. No external backend abstraction,
contextual range implementation, descriptor layout, source code, corpus, or
test suite was consulted.

### IR-0408

Experimental format 2.0 uses DD-630, the three local interface specifications,
marc's explicit little-endian serializers, and the version-1 frame-atomic
validation policy. The `A` payload was independently calculated and checked by
reproducing the published variant-1 `A` vector before substituting the two
typed-context decisions. No external stream format, typed compression profile,
payload vector, source code, corpus, or test suite was consulted.

### IR-0409

The Format 2 header preflight uses only DD-631, the local Format 2 reservation,
marc's checked arithmetic, explicit little-endian helpers, decoder limits, and
transactional Format 1 header-validation policy. No external parser, format
validator, compression container, source code, malformed corpus, or test suite
was consulted.

### IR-0410

The typed LZSS value validator uses only DD-632, the local typed-token protocol,
variant-1 LZSS parameter/reference semantics, checked arithmetic, and decoder
limits. No external typed-token API, LZSS implementation, validator, object
layout, source code, malformed corpus, or test suite was consulted.

### IR-0411

The typed LZSS reconstructor uses only DD-633, the local typed-token validator,
marc's documented bytewise overlap semantics, checked arithmetic, and
caller-owned span policy. No external LZSS decoder, reconstruction loop,
aliasing helper, source code, corpus, or test suite was consulted.

### IR-0412

The `LzssFieldContext` inverse uses only DD-634, the local context-model
contract, typed LZSS validator, checked arithmetic, decoder limits, and
caller-owned span policy. No external context-model implementation, field
coder, token materializer, alias helper, source code, corpus, or test suite was
consulted.

### IR-0413

The `LzssFieldContext` forward planner and materializer use only DD-635, the
local context-model contract, typed LZSS frame validator, C++20 integer bit
width, checked arithmetic, decoder limits, and caller-owned span policy. No
external context-model encoder, field coder, token transform, source code,
corpus, or test suite was consulted.

### IR-0414

The contextual Dynamic Range decoder uses only DD-636, the local entropy-
backend contract, the repository's independently specified variant-1 integer
range arithmetic, the fixed `LzssFieldContext` schema, checked local limits,
and caller-owned value policy. No external range coder, contextual model,
decoder state machine, source code, corpus, or test suite was consulted.

### IR-0415

The direct contextual range-to-LZSS bridge uses only DD-637, the local
context-model and entropy-backend contracts, the private request-driven
decoder, typed LZSS validator, shared field-context state, checked arithmetic,
decoder limits, and caller-owned span policy. No external compression
pipeline, token decoder, context adapter, source code, corpus, encoded stream,
or test suite was consulted.

### IR-0416

The complete private Format 2 frame decoder uses only DD-638, marc's local
Format 2 preflight, direct contextual range-to-LZSS bridge, typed reconstructor,
checked arithmetic, decoder limits, and caller-owned staging contracts. No
external archive decoder, compression pipeline, workspace allocator, source
code, corpus, encoded stream, or test suite was consulted.

### IR-0417

The private Format 2 streaming decoder uses only DD-639, marc's core process
contract, local Format 2 header parsers and complete-frame decoder, checked
arithmetic, decoder limits, and existing frame-atomic streaming policy. No
external streaming decompressor, archive reader, buffer coordinator, source
code, corpus, encoded stream, or test suite was consulted.

### IR-0418

The contextual Dynamic Range operation encoder uses only DD-640, the local
entropy-backend contract, marc's independently specified variant-1 integer
range arithmetic, the fixed `LzssFieldContext` schema, checked arithmetic,
decoder limits, and caller-owned span policy. No external range encoder,
contextual model, compression pipeline, source code, corpus, encoded stream,
or test suite was consulted.

### IR-0419

The typed LZSS producer uses only DD-641, the local typed-token protocol,
variant-1 LZSS parse rules already specified by marc, the typed-token validator
and reconstructor, checked arithmetic, decoder limits, and caller-owned span
policy. No external LZSS parser, match finder, typed-token API, source code,
corpus, encoded stream, or test suite was consulted.

### IR-0420

The complete private Format 2 frame encoder uses only DD-642, marc's local
typed LZSS producer, forward context model, contextual Dynamic Range encoder,
Format 2 frame contract, explicit endian helpers, checked arithmetic, decoder
limits, and caller-owned workspace policy. No external compression pipeline,
frame encoder, serializer, source code, corpus, encoded stream, or test suite
was consulted.

### IR-0421

The private Format 2 streaming encoder uses only DD-643, marc's local stream
header and complete-frame encoder contracts, the established Transform status
model, explicit serialization helpers, checked arithmetic, decoder limits,
and caller-owned disjoint workspace policy. No external streaming compressor,
frame controller, source code, corpus, encoded stream, or test suite was
consulted.

### IR-0422

The private Format 2 profile calculator uses only DD-644, marc's local typed
stream and frame count bounds, range-normalization invariant, native token and
operation definitions, checked arithmetic, decoder limits, and established
workspace partition policy. No external profile calculator, allocator layout,
compression bound, source code, corpus, encoded stream, or test suite was
consulted.

### IR-0423

The experimental Format 2 C lifecycle uses only DD-645, marc's existing
size-tagged C ABI conventions, private Format 2 profile and partitioners,
bounded streaming pair, stable status mapping, checked pointer arithmetic,
and caller-owned workspace policy. No external compression ABI, wrapper,
allocator layout, source code, corpus, encoded stream, or test suite was
consulted.

### IR-0424

The experimental Format 2 public-completion audit uses only DD-646, the local
Format 2 specification, the already published C lifecycle, established marc
completion-test data classes and status invariants, and independently seeded
binary generators. No external compression implementation, completion suite,
corpus, encoded stream, source code, or malformed-input catalog was consulted.

### IR-0425

The experimental Format 2 fuzz boundary uses only DD-647, the repository's
private complete-frame decoder, public C lifecycle, fixed profile calculator
bounds, core process invariants, and established finite-call fuzz policy. No
external fuzzer harness, compression decoder, malformed corpus, source code,
crash catalog, encoded stream, or test suite was consulted.

### IR-0426

The experimental Format 2 CLI adapter uses only DD-648, the repository's local
Format 2 specification, public C lifecycle, established transactional CLI,
generic CLI regression, and bounded file-adapter policy. No external command-
line compressor, wrapper, workspace layout, corpus, encoded stream, source
code, or test suite was consulted.

### IR-0427

The experimental Format 2 benchmark adapter uses only DD-649, the repository's
local Format 2 capacity specification, public C lifecycle, existing benchmark
measurement contract, and bounded workspace reporting policy. No external
benchmark harness, compressor, capacity formula, corpus, source code, encoded
stream, or result table was consulted.

### IR-0428

Interoperability schema 32 uses only DD-650, the frozen local schema-31 order,
the experimental public CLI profile, marc's existing manifest contract,
PowerShell bundle scripts, and SHA-256/file-equality helpers. No external
bundle format, interoperability suite, compressor, corpus, archive, source
code, or manifest was consulted.

### IR-0429

The paired contextual LZSS baseline uses only DD-651, marc's dependency-free
benchmark executable, the repository's current `README.md`, and the already
implemented public Format 1 and experimental Format 2 lifecycles. No external
benchmark harness, compressor, corpus, result table, source code, or encoded
stream was consulted.

### IR-0430

The contextual rANS variant-2 reservation uses only DD-652, marc's
independently specified scalar rANS variant 1 arithmetic and normalization,
the local entropy-backend contract, and the fixed `LzssFieldContext` schema.
No external contextual ANS implementation, table format, source code, corpus,
encoded stream, or test vector was consulted.

### IR-0431

The contextual rANS descriptor boundary uses only DD-653, the locally reserved
variant-2 descriptor, marc's explicit little-endian helpers, checked arithmetic,
decoder limits, and the repository-owned `LzssFieldContext` schema. No external
ANS descriptor parser, table validator, serializer, source code, malformed
corpus, encoded stream, or test suite was consulted.

### IR-0432

The contextual rANS decode-table builder uses only DD-654, the repository's
accepted contextual descriptor, its existing scalar rANS decode-entry shape,
caller-owned workspace conventions, and fixed `LzssFieldContext` schema. No
external ANS table builder, contextual implementation, source code, lookup
layout, malformed corpus, encoded stream, or test suite was consulted.

### IR-0433

The contextual rANS scalar decoder uses only DD-655, marc's independently
specified variant-1 inverse state arithmetic, the local contextual descriptor
and fixed tables, checked arithmetic/endian helpers, and the existing
caller-driven contextual Dynamic Range lifecycle shape. No external ANS
decoder, contextual coder, source code, state machine, malformed corpus,
encoded stream, or test suite was consulted.

### IR-0434

The contextual rANS typed-token bridge uses only DD-656, marc's local
`LzssFieldContextState`, typed LZSS validator, contextual rANS lifecycle,
checked arithmetic, and the existing Dynamic Range bridge's repository-owned
two-pass publication contract. No external contextual compressor, ANS/LZSS
composition, source code, token grammar, malformed corpus, encoded stream, or
test suite was consulted.

### IR-0435

The contextual rANS complete-frame decoder uses only DD-657, the repository's
reserved Format 2 bytes, local descriptor and direct token decoder, typed LZSS
reconstructor, checked arithmetic, decoder limits, and existing frame-atomic
workspace conventions. No external contextual compressor, ANS frame format,
source code, parser structure, malformed corpus, encoded stream, or test suite
was consulted.

### IR-0436

The contextual rANS operation encoder uses only DD-658, marc's independently
specified scalar rANS variant-1 forward state arithmetic and normalization,
the repository-owned `ModeledOperation` schema, contextual descriptor, checked
arithmetic, and local transactional encoder conventions. No external ANS
encoder, contextual model, source code, normalization routine, encoded stream,
corpus, or test suite was consulted.

### IR-0437

The direct typed-token contextual rANS encoder uses only DD-659, marc's local
typed LZSS validation and field-context rules, the independently written
contextual rANS operation encoder, checked arithmetic, and repository-owned
transactional workspace conventions. No external ANS/LZSS composition, source
code, reverse-context algorithm, encoded stream, corpus, or test suite was
consulted.

### IR-0438

The contextual rANS complete-frame encoder uses only DD-660, marc's local raw-
to-typed LZSS producer, direct contextual rANS token encoder, reserved frame
format, checked arithmetic, decoder limits, and repository-owned frame-atomic
workspace conventions. No external compressor, ANS frame encoder, source code,
workspace layout, encoded stream, corpus, or test suite was consulted.

### IR-0439

The contextual rANS streaming encoder uses only DD-661, the repository's
immutable complete-frame encoder, core transform contract, dedicated stream
serializer, checked arithmetic, and independently established Format 2
streaming lifecycle. No external streaming compressor, ANS integration, source
code, buffer state machine, encoded stream, corpus, or test suite was
consulted.

### IR-0440

The contextual rANS streaming decoder uses only DD-662, marc's complete-frame
decoder, dedicated stream/frame parsers, fixed table layout, checked arithmetic,
core transform contract, and local frame-atomic streaming conventions. No
external streaming decompressor, ANS integration, source code, parser state
machine, malformed corpus, encoded stream, or test suite was consulted.

### IR-0441

The contextual rANS profile calculator and partitioners use only DD-663,
repository-defined typed-token and fixed decode-entry layouts, documented rANS
payload bounds, checked arithmetic, decoder limits, and local opaque-view
partition conventions. No external allocator layout, workspace calculator,
ABI binding, source code, benchmark, corpus, or test suite was consulted.

### IR-0442

The contextual rANS C lifecycle uses only DD-664, marc's existing ABI-1 handle
contract, workspace requirement record, buffer-prefix validation helpers, and
the independently implemented profile and streaming transforms. No external C
binding, ABI wrapper, allocator API, source code, encoded stream, corpus, or
test suite was consulted.

### IR-0443

The contextual-rANS public-completion audit uses only DD-665, the local Format
2 specification, the ABI-1 lifecycle from DD-664, marc's established
completion data classes and process invariants, and independently seeded binary
generators. No external compression implementation, completion suite, corpus,
encoded stream, source code, or malformed-input catalog was consulted.

### IR-0444

The contextual-rANS fuzz boundary uses only DD-666, marc's private
complete-frame decoder, public C lifecycle, fixed profile calculator bounds,
core process invariants, and established finite-call fuzz policy. No external
fuzzer harness, compression decoder, malformed corpus, crash catalog, source
code, encoded stream, or test suite was consulted.

### IR-0445

The contextual-rANS CLI adapter uses only DD-667, the repository's Format 2
specification, public C lifecycle, established transactional CLI, generic CLI
regression, and bounded file-adapter policy. No external command-line
compressor, wrapper, workspace layout, corpus, encoded stream, source code, or
test suite was consulted.

### IR-0446

The contextual-rANS benchmark adapter uses only DD-668, marc's local Format 2
capacity specification, public C lifecycle, existing dependency-free
measurement contract, and bounded workspace reporting policy. No external
benchmark harness, compressor, capacity formula, corpus, source code, encoded
stream, or result table was consulted.

### IR-0447

The contextual-rANS encoder planning audit uses only DD-669, marc's local
complete-frame and typed-token transactional contracts, the dependency-free
benchmark output, and direct call-graph inspection. No external match finder,
compression optimizer, benchmark harness, source code, corpus, archive, or
performance result was consulted.

### IR-0448

The contextual-rANS compact descriptor reservation uses only DD-670, variant
2's repository-owned normalized model, the fixed LZSS field-context alphabets,
checked little-endian serialization, and local descriptor measurements from
BM-0015. No external ANS descriptor, table compressor, source code, archive,
corpus, or bitstream specification was consulted.

### IR-0449

The contextual Dynamic Range planning audit uses only DD-671, marc's local
streaming, complete-frame, typed-token, context-materialization, and entropy
planning contracts plus direct call-graph inspection. No external range coder,
match finder, optimizer, source code, corpus, archive, or benchmark result was
consulted.

### IR-0450

The compact contextual-rANS descriptor parser and serializer use only DD-670,
DD-672, TVG-0549, marc's fixed field-context schema, variant-2 descriptor
record, checked little-endian helpers, and decoder limits. No external ANS
format, frequency serializer, source code, archive, corpus, or test suite was
consulted.

### IR-0451

The compact contextual-rANS decoder entry uses only DD-653 through DD-655,
DD-670, DD-672, DD-673, TVG-0549, TVG-0551, and marc's existing private
descriptor, fixed-table, and scalar-state contracts. No external ANS decoder,
adapter, table layout, source code, archive, corpus, or test suite was
consulted.

### IR-0452

The compact contextual-rANS typed-token decoder bridge uses only DD-653
through DD-655, DD-670, DD-672 through DD-674, TVG-0549 through TVG-0553,
marc's existing LZSS field-context state machine, typed-token validator, and
two-pass transactional decoder. No external token codec, ANS adapter, source
code, archive, corpus, malformed sample, or test suite was consulted.

### IR-0453

The compact contextual-rANS complete-frame decoder uses only DD-653 through
DD-655, DD-670, DD-672 through DD-675, TVG-0549 through TVG-0554, marc's
existing fixed contextual-rANS frame admission and typed-LZSS reconstruction
contracts, and the private compact token bridge. No external frame decoder,
ANS implementation, source code, archive, corpus, malformed sample, or test
suite was consulted.

### IR-0454

The compact contextual-rANS stream-header boundary uses only DD-670, DD-672
through DD-676, TVG-0549 through TVG-0555, the normative Format 2 header
layout, and marc's existing contextual-rANS header value validation and
transactional serialization. No external stream parser, container format,
source code, archive, corpus, or test suite was consulted.

### IR-0455

The compact contextual-rANS streaming decoder uses only DD-675 through DD-677,
TVG-0554 through TVG-0556, marc's variant-3 stream-header parser, compact
complete-frame decoder, existing variant-2 streaming lifecycle, core transform
contract, checked arithmetic, and caller-workspace policy. No external stream
decoder, ANS implementation, source code, archive, corpus, malformed sample,
or test suite was consulted.

### IR-0456

The compact contextual-rANS complete-frame encoder uses only DD-670, DD-672,
DD-675, DD-676, DD-678, TVG-0549, TVG-0554, TVG-0555, TVG-0557, marc's typed
LZSS encoder, direct contextual-rANS token encoder, compact descriptor
serializer, and transactional frame-output policy. No external encoder, ANS
implementation, source code, archive, corpus, or test suite was consulted.

### IR-0457

The compact contextual-rANS streaming encoder uses only DD-675, DD-677,
DD-678, DD-679, TVG-0554, TVG-0556, TVG-0557, TVG-0558, marc's fixed
contextual-rANS streaming lifecycle, compact stream-header serializer, and
compact complete-frame encoder. No external streaming encoder, ANS
implementation, source code, archive, corpus, or test suite was consulted.

### IR-0458

The compact contextual-rANS private profile uses only DD-663, DD-675,
DD-678 through DD-680, TVG-0554, TVG-0557 through TVG-0559, marc's fixed
profile arithmetic, compact descriptor bounds, and compact streaming transform
types. No external profile, workspace calculator, ANS implementation, source
code, archive, corpus, or test suite was consulted.

### IR-0459

The compact contextual-rANS C lifecycle uses only DD-664, DD-675, DD-678
through DD-681, TVG-0557 through TVG-0560, marc's ABI-1 conventions, compact
private profile, compact streaming transform types, and existing public buffer
validation. No external C API, factory, ANS implementation, source code,
archive, corpus, or test suite was consulted.

### IR-0460

The compact contextual-rANS public completion matrix uses only DD-681 and
DD-682, TVG-0560 and TVG-0561, marc's existing fixed contextual-rANS
completion matrix, the compact ABI-1 lifecycle, and the core process contract.
No external completion suite, C API, ANS implementation, source code, archive,
corpus, malformed sample, or test suite was consulted.

### IR-0461

The compact contextual-rANS fuzz boundary uses only DD-675 through DD-683,
TVG-0556, TVG-0560 through TVG-0562, marc's existing fixed-memory
contextual-rANS dual-path harness, compact complete-frame decoder, compact
ABI-1 lifecycle, and repository-owned canonical malformed regressions. No
external fuzzer harness, ANS implementation, source code, archive, corpus,
malformed sample, or test suite was consulted.

### IR-0462

The compact contextual-rANS CLI selector uses only DD-681 through DD-684,
TVG-0560 through TVG-0563, marc's existing transactional CLI adapter, compact
ABI-1 lifecycle, and generic CLI round-trip script. No external command-line
interface, C API, ANS implementation, source code, archive, corpus, malformed
sample, or test suite was consulted.

### IR-0463

The compact contextual-rANS benchmark uses only DD-681 through DD-685,
TVG-0560 through TVG-0564, marc's dependency-free public-C benchmark adapter,
compact workspace/profile bounds, and checked complete-stream arithmetic. No
external benchmark harness, C API, ANS implementation, source code, archive,
corpus, performance result, or test suite was consulted.

### IR-0464

Interoperability schema 33 uses only DD-650, DD-675 through DD-686, TVG-0524,
TVG-0560 through TVG-0565, the frozen local schema-32 order, compact CLI
selector, and repository-owned bundle generator, verifier, and compatibility
scripts. No external archive, manifest, corpus, encoder, decoder, source code,
test suite, or interoperability fixture was consulted.

### IR-0465

The contextual tANS variant-2 reservation uses only DD-687, marc's
independently specified tANS variant 1 table construction and state rules,
contextual rANS variant 3's local canonical model records, the entropy-backend
contract, and the fixed `LzssFieldContext` schema. No external contextual ANS
implementation, table format, source code, corpus, encoded stream, or test
vector was consulted.

### IR-0466

The contextual tANS descriptor implementation uses only DD-687 and DD-688,
TVG-0566 and TVG-0567, marc's repository-owned contextual rANS variant-3
record implementation, fixed field-context schema, checked arithmetic,
little-endian helpers, decoder limits, and tANS variant-1 constants. No
external descriptor, contextual ANS source code, archive, corpus, malformed
sample, test suite, or optimization description was consulted.

### IR-0467

The contextual tANS decode-table builder uses only DD-687 through DD-689,
TVG-0566 through TVG-0568, marc's independently implemented tANS variant-1
table builder, contextual tANS compact descriptor, fixed field-context schema,
and decoder limits. No external contextual ANS implementation, transition
table, source code, archive, corpus, malformed sample, test suite, or
optimization description was consulted.

### IR-0468

The contextual tANS state decoder uses only DD-687 through DD-690, TVG-0566
through TVG-0569, marc's independently implemented tANS variant-1 decoder,
contextual tANS descriptor and fixed decode tables, field-context schema, and
decoder limits. No external contextual ANS decoder, bitstream, source code,
archive, corpus, malformed sample, test suite, or optimization description was
consulted.

### IR-0469

The LZSS contextual-tANS token bridge uses only DD-687 through DD-691,
TVG-0566 through TVG-0570, marc's contextual tANS state decoder,
`LzssFieldContextState`, typed-token validator, checked arithmetic, and decoder
limits. No external LZ/ANS composition, source code, archive, corpus, malformed
sample, test suite, or optimization description was consulted.

### IR-0470

The contextual-tANS Format 2 frame preflight and complete-frame decoder use
only DD-687 through DD-692, TVG-0566 through TVG-0571, marc's Format 2 header
layout, contextual tANS descriptor/token bridge, typed reconstructor, checked
arithmetic, and decoder limits. No external frame format, LZ/ANS composition,
source code, archive, corpus, malformed sample, test suite, or optimization
description was consulted.

### IR-0471

The contextual-tANS streaming frame decoder uses only DD-687 through DD-693,
TVG-0566 through TVG-0572, marc's core transform contract, contextual-tANS
Format 2 complete-frame decoder, checked arithmetic, decoder limits, and the
repository-owned contextual-rANS streaming lifecycle as an architectural
precedent. No external streaming decoder, LZ/ANS composition, source code,
archive, corpus, malformed sample, test suite, or optimization description was
consulted.

### IR-0472

The contextual-tANS operation encoder uses only DD-687 through DD-694,
TVG-0566 through TVG-0573, marc's typed modeled-operation schema, contextual
tANS descriptor and decoder, canonical standalone tANS tables, checked
arithmetic, endian helpers, and decoder limits. No external ANS encoder,
source code, archive, corpus, test vector, test suite, or optimization
description was consulted.

### IR-0473

The direct LZSS contextual-tANS token encoder uses only DD-687 through DD-695,
TVG-0566 through TVG-0574, marc's typed-LZSS validator and field-context state,
contextual-tANS model builder/inverse tables/reverse writer, checked arithmetic,
and decoder limits. No external LZ/ANS composition, encoder, source code,
archive, corpus, test vector, test suite, or optimization description was
consulted.

### IR-0474

The contextual-tANS complete-frame encoder uses only DD-687 through DD-696,
TVG-0566 through TVG-0575, marc's raw-to-typed LZSS encoder, direct contextual-
tANS token encoder, existing `5/2` frame/descriptor serializers and decoder,
checked arithmetic, and decoder limits. No external frame encoder, LZ/ANS
composition, source code, archive, corpus, test vector, test suite, or
optimization description was consulted.

### IR-0475

The contextual-tANS streaming frame encoder uses only DD-687 through DD-697,
TVG-0566 through TVG-0576, marc's core transform/status contract, complete
contextual-tANS frame encoder, stream-header serializer, checked arithmetic,
decoder limits, and repository-owned contextual-rANS streaming lifecycle as an
architectural precedent. No external streaming encoder, LZ/ANS composition,
source code, archive, corpus, test vector, test suite, or optimization
description was consulted.

### IR-0476

The contextual-tANS private profile uses only DD-687 through DD-698,
TVG-0566 through TVG-0577, marc's typed-LZSS bounds, contextual-tANS format and
table constants, streaming encoder/decoder constructors, checked arithmetic,
alignment rules, and decoder limits. No external workspace calculator, ANS
implementation, source code, archive, corpus, test vector, test suite, or
optimization description was consulted.

### IR-0477

The contextual-tANS public C lifecycle uses only DD-687 through DD-699,
TVG-0566 through TVG-0578, marc's ABI-1 size-tagged configuration convention,
common transform handle, private contextual-tANS profile/partitioners and
streaming constructors, checked overlap helpers, stable error mapping, and
decoder limits. No external C API, ANS library, source code, archive, corpus,
test vector, test suite, or optimization description was consulted.

### IR-0478

The contextual-tANS public completion audit uses only DD-700, the local Format
2 `5/2` specification, ABI-1 lifecycle from DD-699, marc's established
completion data classes and process invariants, frame-extent fields, and
independently seeded binary generators. No external compression implementation,
completion suite, corpus, test vector, source code, archive, or optimization
description was consulted.

### IR-0479

The contextual-tANS permanent fuzz regressions and bounded harness use only
DD-701, TVG-0580, marc's local `5/2` stream/frame format, private complete-frame
decoder, public ABI-1 lifecycle, fixed table/token constants, decoder limits,
and repository-owned fuzz contract checks. No external harness, corpus,
malformed sample, source code, archive, test suite, or optimization description
was consulted.

### IR-0480

The contextual-tANS initial sanitizer smoke uses only DD-702, TVG-0581, the
repository-built bounded harness from DD-701, its recorded Clang 22 resource
directory, and libFuzzer's documented command-line limits already used by
marc's local campaigns. No external corpus, malformed input, source code,
archive, test suite, or optimization description was consulted.

### IR-0481

The contextual-tANS benchmark admission uses only DD-703, TVG-0582, the local
Format 2 `5/2` specification, the public ABI-1 lifecycle, existing marc
benchmark measurement contract, and checked profile limits. The comparative
input is the repository's own `README.md`. No external benchmark, corpus,
compression implementation, source code, archive, test vector, test suite, or
optimization description was consulted.

### IR-0482

The contextual-tANS CLI admission uses only DD-704, TVG-0583, the local Format
2 `5/2` specification, public ABI-1 lifecycle, existing marc CLI ownership and
temporary-file commit rules, and repository-owned round-trip script. No
external command-line tool, compression implementation, source code, archive,
corpus, test vector, test suite, or optimization description was consulted.

### IR-0483

Interoperability schema 34 uses only DD-705, TVG-0584, the completed local
`lzss-contextual-tans` CLI selector, the frozen schema-33 manifest order, and
marc's repository-owned generator, verifier, and schema-derivation scripts.
No external archive, corpus, implementation, source code, test vector, test
suite, manifest, or optimization description was consulted.

### IR-0484

The schema-34 external admission record uses only DD-706, TVG-0585, the pushed
revision's two CI-generated bundles, the independently generated Ubuntu 26.04
bundle, and the repository-owned schema-34 verifier. No external compression
implementation, source code, archive format, corpus, test suite, or
optimization description was consulted.

### IR-0485

The contextual Blocked Huffman design probe uses only RFC 1951's published
literal/length-versus-distance separation principle, Huffman's published
prefix-code construction, DD-627 through DD-628, DD-707, TVG-0586, marc's
typed LZSS field-context operations, and marc's independently implemented
length-limited canonical Huffman builder. The provisional descriptor and all
three cost strategies were designed specifically for marc and reserve no
stream representation. No DEFLATE implementation, external Huffman source,
LZ/Huffman composition, archive, corpus, test vector, test suite, or
optimization description was consulted.

### IR-0486

The selective contextual-Huffman probe uses only DD-707 through DD-708,
TVG-0587, marc's own pooled and per-context frequency tables, canonical code
lengths, and checked arithmetic. Its strict profitability rule, fixed pooled
histograms, override-mask interpretation, and deterministic tie policy were
authored for marc. No additional external reference, compression source,
descriptor, archive, corpus, test vector, test suite, or optimization
description was consulted.

### IR-0487

The Contextual Blocked Huffman format reservation and descriptor boundary use
only DD-707 through DD-709, TVG-0588, marc's typed-field schema, LSB-first bit
contract, checked serialization helpers, canonical-Huffman validator, and
bounded decode-table constants. RFC 1951 remains an alphabet-separation idea
reference only; no DEFLATE byte, tree encoding, implementation, source code,
test vector, suite, corpus, archive, or optimization structure was consulted.

### IR-0488

The Contextual Blocked Huffman payload decoder uses DD-709 through DD-710,
TVG-0588 through TVG-0589, marc's canonical Huffman decode-table primitive,
typed-context alphabet schema, checked arithmetic, and existing request-driven
contextual entropy decoder contracts. No external Huffman decoder, DEFLATE
implementation, source code, descriptor, test vector, suite, archive, corpus,
or optimization description was consulted.

### IR-0489

The typed-LZSS Contextual Blocked Huffman decoder uses DD-710 through DD-711,
TVG-0589 through TVG-0590, marc's `LzssFieldContextState`, typed-token
validator, request-driven entropy decoder, checked arithmetic, limit model, and
two-pass contextual decoder contracts. No external LZ/Huffman implementation,
source code, frame, archive, test vector, suite, corpus, or optimization
description was consulted.

### IR-0490

The Contextual Blocked Huffman complete-frame decoder uses DD-709 through
DD-712, TVG-0588 through TVG-0591, marc's Format 2 header contracts, checked
serialization, typed-token decoder, raw reconstructor, overlap checks, and
limit model. No external LZ/Huffman or DEFLATE implementation, source code,
frame, archive, test vector, suite, corpus, or optimization description was
consulted.

### IR-0491

The Contextual Blocked Huffman streaming decoder uses DD-709 through DD-713,
TVG-0588 through TVG-0592, marc's complete-frame decoder, core transform
contract, checked arithmetic, overlap checks, decoder limits, and the local
contextual-tANS streaming lifecycle as an architectural precedent. No external
streaming decoder, LZ/Huffman or DEFLATE implementation, source code, frame,
archive, test vector, suite, corpus, or optimization description was consulted.

### IR-0492

The Contextual Blocked Huffman operation encoder uses DD-707 through DD-714,
TVG-0586 through TVG-0593, marc's selective estimator, reserved descriptor,
length-limited canonical Huffman primitive, typed modeled-operation schema,
checked arithmetic, and LSB-first rules. No external Huffman or DEFLATE
encoder, source code, frame, archive, test vector, suite, corpus, or
optimization description was consulted.

### IR-0493

The typed-LZSS Contextual Blocked Huffman encoder uses DD-710 through DD-715,
TVG-0589 through TVG-0594, marc's typed-token validator,
`LzssFieldContextState`, fixed model builder, forward writer, checked overlap,
and operation-level encoder. No external LZ/Huffman or DEFLATE encoder, source
code, frame, archive, test vector, suite, corpus, or optimization description
was consulted.

### IR-0494

The Contextual Blocked Huffman complete-frame encoder uses DD-709 through
DD-716, TVG-0588 through TVG-0595, marc's typed-LZSS encoder, direct contextual
entropy encoder, Format 2 serializers and validators, checked arithmetic,
overlap checks, and decoder-limit model. No external LZ/Huffman or DEFLATE
encoder, source code, frame, archive, test vector, suite, corpus, or
optimization description was consulted.

### IR-0495

The Contextual Blocked Huffman streaming encoder uses DD-709 through DD-717,
TVG-0588 through TVG-0596, marc's complete-frame encoder, core transform
contract, checked arithmetic, overlap checks, decoder limits, and the local
contextual-tANS streaming lifecycle as an architectural precedent. No external
streaming encoder, LZ/Huffman or DEFLATE implementation, source code, frame,
archive, test vector, suite, corpus, or optimization description was
consulted.

### IR-0496

The Contextual Blocked Huffman private profile uses DD-709 through DD-718,
TVG-0588 through TVG-0597, marc's typed-LZSS bounds, Contextual Blocked Huffman
format maxima, streaming constructors, checked arithmetic, alignment rules,
and decoder limits. No external workspace calculator, Huffman or DEFLATE
implementation, source code, archive, corpus, test vector, test suite, or
optimization description was consulted.

### IR-0497

The Contextual Blocked Huffman public C lifecycle uses DD-709 through DD-719,
TVG-0588 through TVG-0598, marc's private profile, common ABI-1 transform
lifecycle, checked workspace-prefix overlap rules, and neighboring contextual
rANS/tANS public factories as local architectural precedents. No external C
compression API, Huffman or DEFLATE implementation, source code, archive,
corpus, test vector, test suite, or optimization description was consulted.

### IR-0498

The Contextual Blocked Huffman public-completion audit uses DD-709 through
DD-720, TVG-0588 through TVG-0599, marc's ABI-1 lifecycle, fixed completion
data classes and chunk schedules, Format 2 frame layout, and sticky-error
contract. No external Huffman or DEFLATE implementation, source code, archive,
corpus, test vector, test suite, completion checklist, or optimization
description was consulted.

### IR-0499

The Contextual Blocked Huffman malformed regression and fuzz boundary use
DD-709 through DD-721, TVG-0588 through TVG-0600, marc's complete-frame
decoder, public ABI-1 lifecycle, fixed Contextual Huffman maxima, caller-owned
workspaces, checked chunk loop, and sticky-error contract. No external fuzz
harness, corpus, Huffman or DEFLATE implementation, source code, archive, test
vector, test suite, or optimization description was consulted.

### IR-0500

The initial Contextual Blocked Huffman sanitizer execution uses DD-721 and
DD-722, TVG-0600 and TVG-0601, FZ-0022, marc's reviewed bounded harness,
ignored build-artifact policy, and established matching-runtime procedure. No
external corpus, malformed sample, fuzz harness, source code, archive, test
suite, or optimization description was consulted.

### IR-0501

The Contextual Blocked Huffman experimental benchmark admission uses DD-709
through DD-723, TVG-0588 through TVG-0602, marc's public ABI-1 lifecycle,
existing checked benchmark capacity helpers, and neighboring contextual
Dynamic Range, rANS, and tANS benchmark adapters as local architectural
precedents. No external benchmark, corpus, Huffman or DEFLATE implementation,
source code, archive, test vector, test suite, or optimization description was
consulted.

### IR-0502

The Contextual Blocked Huffman experimental CLI admission uses DD-709 through
DD-724, TVG-0588 through TVG-0603, marc's public ABI-1 lifecycle, common CLI
file-commit loop, registered round-trip script, and neighboring contextual
Dynamic Range, rANS, and tANS adapters as local architectural precedents. No
external CLI, Huffman or DEFLATE implementation, source code, archive, test
vector, test suite, or optimization description was consulted.

### IR-0503

Contextual Blocked Huffman interoperability schema 35 uses DD-725, TVG-0604,
marc's schema-34 manifest order, public CLI selector, repository-owned bundle
generator/verifier, compatibility derivation script, and common 8,193-byte
fixture. No external archive, interoperability suite, Huffman or DEFLATE
implementation, source code, test vector, test suite, or optimization
description was consulted.

### IR-0504

The Linux Contextual Blocked Huffman build repair uses DD-726, the C++20
standard-library ownership of `std::in_range` by `<utility>`, the Ubuntu CI
diagnostic supplied by the maintainer, and marc's existing translation-unit
include policy. No external implementation, source code, patch, archive, test
vector, test suite, or optimization description was consulted.

### IR-0505

The schema-35 external admission record uses DD-727, TVG-0606, the pushed
revision `7c276151ab428aa9ba0376f8d9ba9a85a9fbd347`, the Windows/MSVC and
Ubuntu 24.04/Ninja CI bundles, the locally generated Ubuntu 26.04/Clang
bundle, and marc's repository-owned schema-35 verifier. No external
compression implementation, source code, archive contents, corpus, test
suite, or optimization description was consulted.

### IR-0506

The Contextual Adaptive Huffman reservation uses DD-728, TVG-0607, marc's
Adaptive Huffman FGK variant 1, LZSS typed-token protocol, 31-context schema,
Format 2 framing, entropy-backend contract, and neighboring contextual backend
profiles as local design precedents. No external Adaptive Huffman
implementation, source code, archive, test vector, test suite, patent text, or
optimization description was consulted.

### IR-0507

The Contextual Adaptive Huffman format/tree foundation uses DD-728 and DD-729,
TVG-0607 and TVG-0608, marc's Adaptive Huffman variant-1 node semantics,
caller-owned workspace conventions, endian helpers, fixed Format 2 descriptor,
and 31-context alphabet schema. No external Adaptive Huffman implementation,
source code, archive, test vector, test suite, patent text, or optimization
description was consulted.

### IR-0508

The Contextual Adaptive Huffman model bank and operation decoder use DD-728
through DD-730, TVG-0607 through TVG-0609, marc's private bounded FGK tree,
fixed LZSS context schema, checked arithmetic, limit model, and neighboring
contextual decoder lifecycle as local precedents. No external Adaptive Huffman
implementation, source code, archive, test vector, test suite, patent text, or
optimization description was consulted.

### IR-0509

The Contextual Adaptive Huffman LZSS token adapter uses DD-728 through DD-731,
TVG-0607 through TVG-0610, marc's typed-token validator, fixed field-context
state, checked arithmetic, decoder limits, and neighboring private contextual
token adapters as local architectural precedents. No external Adaptive
Huffman or LZSS implementation, source code, archive, test vector, test suite,
patent text, or optimization description was consulted.

### IR-0510

The Contextual Adaptive Huffman stream/frame format boundary uses DD-728
through DD-732, TVG-0607 through TVG-0611, marc's Format 2 common header,
fixed 16-byte entropy descriptor, checked arithmetic, decoder limits, and
neighboring contextual frame-format validators as local architectural
precedents. No external Adaptive Huffman or LZSS implementation, source code,
archive, test vector, test suite, patent text, or optimization description was
consulted.

### IR-0511

The Contextual Adaptive Huffman complete-frame decoder uses DD-728 through
DD-733, TVG-0607 through TVG-0612, marc's private frame preflight, two-pass
typed-token adapter, typed LZSS reconstructor, checked overlap arithmetic, and
neighboring contextual complete-frame decoders as local architectural
precedents. No external Adaptive Huffman or LZSS implementation, source code,
archive, test vector, test suite, patent text, or optimization description was
consulted.

### IR-0512

The Contextual Adaptive Huffman streaming decoder uses DD-728 through DD-734,
TVG-0607 through TVG-0613, marc's complete-frame decoder, core transform
contract, checked arithmetic, caller-owned workspace policy, and neighboring
contextual streaming decoders as local architectural precedents. No external
Adaptive Huffman, LZSS, or streaming implementation, source code, archive,
test vector, test suite, patent text, or optimization description was
consulted.

### IR-0513

The Contextual Adaptive Huffman operation encoder uses DD-728 through DD-735,
TVG-0607 through TVG-0614, marc's caller-owned contextual FGK model bank,
typed modeled-operation boundary, checked arithmetic, and operation encoders
for the other contextual entropy backends as local architectural precedents.
No external Adaptive Huffman, LZSS, or entropy-encoder implementation, source
code, archive, test vector, test suite, patent text, or optimization
description was consulted.

### IR-0514

The Contextual Adaptive Huffman LZSS token encoder uses DD-728 through DD-736,
TVG-0607 through TVG-0615, marc's typed-token validator and field-context
state, the new forward entropy planner/writer, checked arithmetic, and direct
token adapters for neighboring contextual backends as local architectural
precedents. No external Adaptive Huffman, LZSS, or entropy-encoder
implementation, source code, archive, test vector, test suite, patent text,
or optimization description was consulted.

### IR-0515

The Contextual Adaptive Huffman complete-frame encoder uses DD-728 through
DD-737, TVG-0607 through TVG-0616, marc's typed LZSS encoder, direct
contextual token encoder, fixed descriptor serializer, frame validator, checked
arithmetic, and neighboring contextual complete-frame encoders as local
architectural precedents. No external Adaptive Huffman, LZSS, or frame-encoder
implementation, source code, archive, test vector, test suite, patent text, or
optimization description was consulted.

### IR-0516

The Contextual Adaptive Huffman streaming encoder uses DD-728 through DD-738,
TVG-0607 through TVG-0617, marc's complete-frame encoder, core transform
contract, checked arithmetic, caller-owned workspace policy, and neighboring
contextual streaming encoders as local architectural precedents. No external
Adaptive Huffman, LZSS, or streaming implementation, source code, archive,
test vector, test suite, patent text, or optimization description was
consulted.

### IR-0517

The Contextual Adaptive Huffman profile and workspace calculator uses DD-728
through DD-739, TVG-0607 through TVG-0618, marc's fixed model schema, typed
token bounds, stream validator, streaming constructors, checked layout helpers,
and neighboring contextual profiles as local architectural precedents. No
external Adaptive Huffman, LZSS, profile, or allocator implementation, source
code, archive, test vector, test suite, patent text, or optimization
description was consulted.

### IR-0518

The Contextual Adaptive Huffman public C lifecycle uses DD-728 through DD-740,
TVG-0607 through TVG-0619, marc's ABI-1 transform ownership contract, private
workspace profile and partitioners, stable core error mapping, checked overlap
validation, and neighboring contextual C lifecycles as local architectural
precedents. No external Adaptive Huffman, LZSS, C API, allocator, source code,
archive, test vector, test suite, patent text, or optimization description was
consulted.

### IR-0519

The Contextual Adaptive Huffman public completion audit uses DD-728 through
DD-741, TVG-0607 through TVG-0620, only the ABI-1 C header and common transform
lifecycle, the independently specified Format 2 frame layout, and neighboring
public completion audits as local architectural precedents. No external
Adaptive Huffman, LZSS, completion suite, source code, archive, test vector,
test suite, patent text, or optimization description was consulted.

### IR-0520

The Contextual Adaptive Huffman permanent malformed regression uses DD-728
through DD-742, TVG-0607 through TVG-0621, marc's private complete-frame
decoder, public ABI-1 streaming decoder, independently specified Format 2
field offsets, and neighboring contextual dual-boundary regressions as local
architectural precedents. No external Adaptive Huffman, LZSS, malformed corpus,
fuzzer output, source code, archive, test vector, test suite, patent text, or
optimization description was consulted.

### IR-0521

The Contextual Adaptive Huffman bounded fuzz harness uses DD-728 through
DD-743, TVG-0607 through TVG-0622, marc's private complete-frame decoder,
public ABI-1 streaming decoder, fixed decoder limits, status invariants, and
neighboring contextual bounded harnesses as local architectural precedents.
No external Adaptive Huffman, LZSS, fuzz harness, corpus, source code, archive,
test vector, test suite, patent text, or optimization description was
consulted.

### IR-0522

The initial Contextual Adaptive Huffman sanitizer smoke uses DD-728 through
DD-744, TVG-0607 through TVG-0623, FZ-0023, marc's reviewed bounded harness,
established Windows GNU-driver Clang sanitizer tree, matching-runtime process
policy, and ignored failure-artifact policy. No external Adaptive Huffman,
LZSS, fuzz harness, corpus, malformed input, source code, archive, test vector,
test suite, patent text, or optimization description was consulted.

### IR-0523

The Contextual Adaptive Huffman experimental benchmark admission uses DD-728
through DD-745, TVG-0607 through TVG-0624, marc's public ABI-1 lifecycle,
exact 267-bit payload ceiling, checked benchmark capacity helpers, and
neighboring contextual benchmark adapters as local architectural precedents.
No external benchmark, corpus, Adaptive Huffman or LZSS implementation, source
code, archive, test vector, test suite, patent text, or optimization
description was consulted.

### IR-0524

The Contextual Adaptive Huffman experimental CLI admission uses DD-728 through
DD-746, TVG-0607 through TVG-0625, marc's public ABI-1 lifecycle, common CLI
file-commit loop, registered round-trip script, and neighboring contextual CLI
adapters as local architectural precedents. No external CLI, Adaptive Huffman
or LZSS implementation, source code, archive, test vector, test suite, patent
text, or optimization description was consulted.

### IR-0525

Contextual Adaptive Huffman interoperability schema 36 uses DD-747,
TVG-0626, marc's schema-35 manifest order, public CLI selector,
repository-owned bundle generator/verifier, SHA-256 manifest checks, and
compatibility downgrade chain as local architectural precedents. No external
archive, corpus, interoperability suite, Adaptive Huffman or LZSS
implementation, source code, test vector, test suite, patent text, or
optimization description was consulted.

### IR-0526

The schema-36 external admission record uses DD-748, TVG-0627, the pushed
revision `bdcabd439d9cedb9e58f3dd2a3ac4dcb3526e1a2`, the Windows/MSVC and
Ubuntu 24.04/Ninja CI bundles, the locally generated Ubuntu 26.04/Clang
bundle, and marc's repository-owned schema-36 verifier. No external
compression implementation, source code, archive contents, corpus, test
suite, or optimization description was consulted.

### IR-0527

The Contextual rANS canonicalization design uses DD-749 through DD-751,
TVG-0628, marc's variant-2 and variant-3 specifications, public C lifecycles,
CLI and benchmark adapters, fuzz harnesses, schema-36 order, and repository
history as local architectural precedents. No external rANS or compression
implementation, source code, API, archive, corpus, test vector, test suite,
patent text, or optimization description was consulted.

### IR-0528

The canonical Contextual rANS public-boundary change uses DD-749, DD-750, and
DD-752, TVG-0628 and TVG-0629, marc's two existing public lifecycles, variant-3
workspace profile, CLI and benchmark adapters, C11 test, completion matrix,
and malformed regressions as local precedents. No external rANS or compression
implementation, source code, API, archive, corpus, test vector, test suite,
patent text, or optimization description was consulted.

### IR-0529

The Contextual rANS frame canonicalization uses DD-749, DD-750, DD-753,
TVG-0628, TVG-0630, marc's existing variant-3 frame/profile/streaming code,
fixed and compact regression tests, and Format 2 identity validation as local
precedents. No external rANS or compression implementation, source code, API,
archive, corpus, test vector, test suite, patent text, or optimization
description was consulted.

### IR-0530

The canonical Contextual rANS fuzz target uses DD-749, DD-750, DD-754,
TVG-0628, TVG-0631, marc's existing bounded dual-path harness, canonical frame
decoder, public lifecycle, and compile-smoke target as local precedents. No
external fuzz harness, rANS or compression implementation, source code,
corpus, test suite, patent text, or optimization description was consulted.

### IR-0531

The canonical Contextual rANS entropy and typed-token boundary uses DD-749,
DD-750, DD-755, TVG-0628, TVG-0632, marc's existing variable descriptor,
scalar state machine, decode-table builder, direct typed-token encoder and
decoder, and Contextual tANS shared model records as local precedents. No
external rANS or compression implementation, source code, API, archive,
corpus, test vector, test suite, patent text, or optimization description was
consulted.

### IR-0532

Schema-37 migration and contextual header inheritance use DD-751, DD-756,
TVG-0633, marc's schema-36 scripts, canonical Contextual rANS header parser,
three derived contextual format adapters, and repository-generated fixtures as
local precedents. No external compression implementation, source code, API,
archive, corpus, test vector, test suite, patent text, or optimization
description was consulted.

### IR-0533

The canonical Contextual rANS merge admission uses DD-757, TVG-0634, marc's
complete registered MSVC and ClangCL suites, schema-compatibility test,
repository-owned bounded fuzz harness, established matching-runtime sanitizer
procedure, prior compact variant-3 README benchmark record, and local name and
diff audits. No external rANS or compression implementation, source code,
benchmark, corpus, malformed input, archive, test vector, test suite, patent
text, or optimization description was consulted.

### IR-0534

The schema-37 external admission record uses DD-758, TVG-0635, pushed revision
`58b829dafa078e7dadd46e5de9ed7b1af45b5cc2`, the Windows/MSVC and Ubuntu
24.04/Ninja CI bundles, the locally generated Ubuntu 26.04/Clang bundle, and
marc's repository-owned schema-37 verifier. No external compression
implementation, source code, archive contents, corpus, test suite, or
optimization description was consulted.

### IR-0535

The LZSS match-finder acceleration design uses the maintainer-authored
`docs/design/lzss-match-finder-strategy.md`, AGENTS.md's LZSS determinism and
bounded-workspace requirements, marc's exhaustive `lzss_match_finder`, typed
and byte-token encoder paths, existing workspace profiles, and benchmark
contract as first-party references. No external LZSS implementation, source
code, match-finder optimization, hash-chain layout, benchmark result, corpus,
test suite, patent text, or generated table was consulted.

### IR-0536

The Exhaustive match-finder contract uses DD-760, TVG-0636, marc's existing
`find_lzss_match` loop, byte-token and typed-token parser control flow, LZSS
format vectors, GoogleTest infrastructure, and CMake test registration as
first-party references. No external LZSS implementation, match-finder API,
source code, test vector, test suite, corpus, benchmark, patent text, or
optimization description was consulted.

### IR-0537

The first private HashChain Exact boundary uses DD-760 and DD-761, TVG-0636
and TVG-0637, marc's new finder contract and Exhaustive oracle, checked-math
and limit utilities, existing caller-owned opaque workspace conventions, and
GoogleTest differential patterns as first-party references. Its five-byte
hash expression, capped power-of-two buckets, absolute heads, and ring of
32-bit predecessor distances were designed within this repository. No
external LZSS implementation, hash-chain source, hash function, table layout,
test suite, corpus, benchmark result, patent text, or optimization description
was consulted.

### IR-0538

The first HashChain Exact one-shot encoder routes use DD-760 through DD-763,
TVG-0636 through TVG-0639, marc's byte-token and typed-token parsers, private
finder workspace calculator, existing transactional encoder contracts,
checked-overlap utility, limits, and Exhaustive reference output as first-
party references. No external LZSS implementation, source code, match-finder
integration, test vector, test suite, corpus, benchmark result, patent text,
or optimization description was consulted.

### IR-0539

The LZSS Exact measurement boundary uses DD-760 through DD-764, TVG-0636
through TVG-0640, marc's two private finders, explicit one-shot encoder routes,
caller workspace rules, dependency-free benchmark conventions, and repository
README as first-party references. No external LZSS implementation, match-
finder benchmark, instrumentation scheme, source code, corpus, result, test
suite, patent text, or optimization description was consulted.

### IR-0540

The typed-token single-pass route uses DD-760 through DD-765, TVG-0636 through
TVG-0641, marc's HashChain finder, typed parser, conservative contextual token
workspace bounds, atomic buffer checks, and internal benchmark as first-party
references. No external LZSS implementation, parse-reuse design, source code,
benchmark, corpus, result, test suite, patent text, or optimization description
was consulted.

### IR-0541

The first complete HashChain frame route uses DD-760 through DD-766, TVG-0636
through TVG-0642, marc's typed Contextual Dynamic Range frame encoder and
decoder, single-pass typed parser, workspace and alias contracts, and internal
benchmark as first-party references. No external LZSS, range-coder, combined
codec, source code, frame integration, benchmark, corpus, result, test suite,
patent text, or optimization description was consulted.

### IR-0542

The first streaming HashChain promotion uses DD-760 through DD-767, TVG-0636
through TVG-0643, marc's Contextual Dynamic Range profile, C lifecycle,
streaming encoder, exact frame equivalence, opaque workspace partitioning,
stable error mapping, CLI, and public benchmark as first-party references. No
external LZSS, range-coder, combined implementation, source code, workspace
layout, API integration, benchmark, corpus, result, test suite, patent text, or
optimization description was consulted.

### IR-0543

The private Contextual rANS HashChain frame uses DD-760 through DD-768,
TVG-0636 through TVG-0644, marc's canonical variable-descriptor Contextual
rANS frame encoder and decoder, single-pass typed parser, exact finder,
workspace and alias contracts, and internal benchmark as first-party
references. No external LZSS, rANS, combined implementation, source code,
frame integration, workspace layout, benchmark, corpus, result, test suite,
patent text, or optimization description was consulted.

### IR-0544

The Contextual rANS streaming HashChain promotion uses DD-760 through DD-769,
TVG-0636 through TVG-0645, marc's canonical Contextual rANS profile, C
lifecycle, streaming encoder, byte-identical private frame routes, opaque
workspace partitioning, stable error mapping, CLI, and public benchmark as
first-party references. No external LZSS, rANS, combined implementation,
source code, workspace layout, API integration, benchmark, corpus, result,
test suite, patent text, or optimization description was consulted.

### IR-0545

The private Contextual tANS HashChain frame uses DD-760 through DD-770,
TVG-0636 through TVG-0646, marc's Contextual tANS frame encoder and decoder,
single-pass typed parser, fixed encode-table staging, exact finder, workspace
and alias contracts, and internal benchmark as first-party references. No
external LZSS, tANS, FSE, combined implementation, source code, frame
integration, workspace layout, benchmark, corpus, result, test suite, patent
text, or optimization description was consulted.

### IR-0546

The Contextual tANS streaming HashChain promotion uses DD-760 through DD-771,
TVG-0636 through TVG-0647, marc's Contextual tANS profile, C lifecycle,
streaming encoder, byte-identical private frame routes, opaque token/table/
finder workspace partitioning, stable error mapping, CLI, and public benchmark
as first-party references. No external LZSS, tANS, FSE, combined
implementation, source code, workspace layout, API integration, benchmark,
corpus, result, test suite, patent text, or optimization description was
consulted.

### IR-0547

The private Contextual Blocked Huffman HashChain frame uses DD-760 through
DD-772, TVG-0636 through TVG-0648, marc's Contextual Blocked Huffman frame
encoder and decoder, single-pass typed parser, bounded per-context canonical
tables, exact finder, workspace and alias contracts, and internal benchmark as
first-party references. No external LZSS, Huffman, combined implementation,
source code, frame integration, workspace layout, benchmark, corpus, result,
test suite, patent text, or optimization description was consulted.

### IR-0548

The Contextual Blocked Huffman streaming HashChain promotion uses DD-760
through DD-773, TVG-0636 through TVG-0649, marc's Contextual Blocked Huffman
profile, C lifecycle, streaming encoder, byte-identical private frame routes,
opaque workspace partitioning, stable error mapping, CLI, and public benchmark
as first-party references. No external LZSS, Huffman, combined implementation,
source code, workspace layout, API integration, benchmark, corpus, result,
test suite, patent text, or optimization description was consulted.

### IR-0549

The private Contextual Adaptive Huffman HashChain frame uses DD-760 through
DD-774, TVG-0636 through TVG-0650, marc's Contextual Adaptive Huffman frame
encoder and decoder, single-pass typed parser, bounded FGK context models,
exact finder, workspace and alias contracts, and internal benchmark as first-
party references. No external LZSS, Adaptive Huffman, combined implementation,
source code, frame integration, workspace layout, benchmark, corpus, result,
test suite, patent text, or optimization description was consulted.

### IR-0550

The Contextual Adaptive Huffman streaming HashChain promotion uses DD-760
through DD-775, TVG-0636 through TVG-0651, marc's Contextual Adaptive Huffman
profile, C lifecycle, streaming encoder, byte-identical private frame routes,
opaque token/node/symbol/finder workspace partitioning, stable error mapping,
CLI, and public benchmark as first-party references. No external LZSS,
Adaptive Huffman, combined implementation, source code, workspace layout, API
integration, benchmark, corpus, result, test suite, patent text, or
optimization description was consulted.

### IR-0551

The private standalone LZSS HashChain frame uses DD-760 through DD-776,
TVG-0636 through TVG-0652, marc's entropy-none LZSS frame encoder and decoder,
canonical token serializer, exact finder, buffer-overlap and aggregate-memory
contracts, and internal benchmark as first-party references. No external LZSS
implementation, source code, frame integration, workspace layout, benchmark,
corpus, result, test suite, patent text, or optimization description was
consulted.

### IR-0552

The standalone LZSS HashChain streaming promotion uses DD-760 through DD-777,
TVG-0636 through TVG-0653, marc's independently implemented LZSS profile,
streaming encoder, C lifecycle, exact finder, frame codec, overlap helper, and
public benchmark as first-party references. No external LZSS implementation,
source code, streaming integration, workspace layout, ABI design, benchmark,
corpus, result, test suite, patent text, or optimization description was
consulted.

### IR-0553

The byte-oriented LZSS plus Blocked Huffman private HashChain frame uses DD-760
through DD-778, TVG-0636 through TVG-0654, marc's canonical LZSS byte-token
encoder, exact finder, Blocked Huffman frame primitives, generic frame codec,
overlap helper, and internal benchmark as first-party references. No external
LZSS or Huffman implementation, source code, integration, workspace layout,
benchmark, corpus, result, test suite, patent text, or optimization description
was consulted.

### IR-0554

The byte-oriented LZSS plus Blocked Huffman HashChain streaming promotion uses
DD-760 through DD-779, TVG-0636 through TVG-0655, marc's independently written
profile, C lifecycle, streaming encoder, byte-identical private frame routes,
exact finder, overlap helper, CLI, and public benchmark as first-party
references. No external LZSS or Huffman implementation, source code,
integration, workspace layout, ABI design, benchmark, corpus, result, test
suite, patent text, or optimization description was consulted.

### IR-0555

The byte-oriented LZSS plus Adaptive Huffman private HashChain frame uses
DD-760 through DD-780, TVG-0636 through TVG-0656, marc's canonical LZSS
byte-token encoder, exact finder, bounded Adaptive Huffman encoder and decoder,
generic frame codec, overlap helper, and internal benchmark as first-party
references. No external LZSS or Adaptive Huffman implementation, source code,
integration, workspace layout, benchmark, corpus, result, test suite, patent
text, or optimization description was consulted.

### IR-0556

The byte-oriented LZSS plus Adaptive Huffman HashChain streaming promotion
uses DD-760 through DD-781, TVG-0636 through TVG-0657, marc's independently
written profile, C lifecycle, streaming encoder, byte-identical private frame
routes, exact finder, overlap helper, CLI, and public benchmark as first-party
references. No external LZSS or Adaptive Huffman implementation, source code,
integration, workspace layout, ABI design, benchmark, corpus, result, test
suite, patent text, or optimization description was consulted.

### IR-0557

The byte-oriented LZSS plus Dynamic Range private HashChain frame uses DD-760
through DD-782, TVG-0636 through TVG-0658, marc's canonical LZSS byte-token
encoder, exact finder, bounded Dynamic Range encoder and decoder, generic frame
codec, overlap helper, and internal benchmark as first-party references. No
external LZSS or range-coder implementation, source code, integration,
workspace layout, benchmark, corpus, result, test suite, patent text, or
optimization description was consulted.

### IR-0558

The byte-oriented LZSS plus Dynamic Range HashChain streaming promotion uses
DD-760 through DD-783, TVG-0636 through TVG-0659, marc's independently written
profile, C lifecycle, streaming encoder, byte-identical private frame routes,
exact finder, overlap helper, CLI, and public benchmark as first-party
references. No external LZSS or range-coder implementation, source code,
integration, workspace layout, ABI design, benchmark, corpus, result, test
suite, patent text, or optimization description was consulted.

### IR-0559

The byte-oriented LZSS plus rANS private HashChain frame uses DD-760 through
DD-784, TVG-0636 through TVG-0660, marc's canonical LZSS byte-token encoder,
exact finder, bounded block rANS encoder and decoder, generic frame codec,
overlap helper, and internal benchmark as first-party references. No external
LZSS or ANS implementation, source code, integration, workspace layout,
benchmark, corpus, result, test suite, patent text, or optimization description
was consulted.

### IR-0560

The byte-oriented LZSS plus rANS HashChain streaming promotion uses DD-760
through DD-785, TVG-0636 through TVG-0661, marc's independently written
profile, C lifecycle, streaming encoder, byte-identical private frame routes,
exact finder, overlap helper, CLI, and public benchmark as first-party
references. No external LZSS or ANS implementation, source code, integration,
workspace layout, ABI design, benchmark, corpus, result, test suite, patent
text, or optimization description was consulted.

### IR-0561

The byte-oriented LZSS plus tANS private HashChain frame uses DD-760 through
DD-786, TVG-0636 through TVG-0662, marc's canonical LZSS byte-token encoder,
exact finder, bounded block tANS encoder and decoder, generic frame codec,
overlap helper, and internal benchmark as first-party references. No external
LZSS or ANS implementation, source code, integration, workspace layout,
benchmark, corpus, result, test suite, patent text, or optimization description
was consulted.

### IR-0562

The byte-oriented LZSS plus tANS HashChain streaming promotion uses DD-760
through DD-787, TVG-0636 through TVG-0663, marc's independently written
profile, C lifecycle, streaming encoder, byte-identical private frame routes,
exact finder, overlap helper, CLI, and public benchmark as first-party
references. No external LZSS or ANS implementation, source code, integration,
workspace layout, ABI design, benchmark, corpus, result, test suite, patent
text, or optimization description was consulted.

### IR-0563

The LZSS HashChain phase-closure audit uses DD-760 through DD-788,
TVG-0636 through TVG-0664, marc's full Exhaustive differential suite, all
public HashChain profile tests, the complete MSVC and ClangCL suites, and the
repository's eleven LZSS sanitizer fuzz targets as first-party evidence. No
external LZSS implementation, source code, integration, benchmark, corpus,
result, test suite, patent text, or optimization description was consulted.

### IR-0564

The 1 MiB typed-token LZSS contextual reservation uses DD-628, DD-788,
DD-790, TVG-0665, marc's frozen dictionary variant 2 and context variant 1,
the local typed-token and context-model contracts, Format 2 framing, the exact
HashChain implementation, and all five local contextual entropy contracts as
first-party references. No external LZSS, context-model, Huffman, range-coder,
or ANS source code, extended-window format, workspace layout, test vector,
benchmark, corpus, result, test suite, patent text, or optimization
description was consulted.

### IR-0565

The shared 1 MiB contextual LZSS layout implementation uses DD-790 and
DD-791, TVG-0665 and TVG-0666, marc's typed-token validator, field-context
model, frozen 4,518-entry arrays, existing unit tests, and local Format 2
design as first-party references. No external LZSS implementation,
context-model implementation, extended-window format, lookup layout, source
code, tests, benchmark, corpus, result, patent text, or optimization
description was consulted.

### IR-0566

The first 1 MiB Contextual Dynamic Range decoder slice uses DD-790 through
DD-792, TVG-0665 through TVG-0667, marc's typed Contextual Dynamic Range stream
and frame parsers, bounded range decoder, typed-token inverse and
reconstructor, transactional complete-frame decoder, and one-byte streaming
decoder as first-party references. No external LZSS, context-model, range-
coder, extended-window format, source code, tests, benchmark, corpus, result,
patent text, or optimization description was consulted.

### IR-0567

The first 1 MiB Contextual Dynamic Range encoder slice uses DD-790 through
DD-793, TVG-0665 through TVG-0668, marc's typed LZSS Exhaustive and exact
HashChain parsers, field-context modeler, bounded Dynamic Range operation
encoder, complete-frame encoder, streaming lifecycle, and decoder slice as
first-party references. No external LZSS, context-model, range-coder,
extended-window format, source code, tests, benchmark, corpus, result, patent
text, or optimization description was consulted.

### IR-0568

The 1 MiB Contextual Dynamic Range internal profile uses DD-790 through
DD-794, TVG-0665 through TVG-0669, marc's existing 64 KiB typed-context
profile, checked workspace arithmetic, exact HashChain requirements, selected
context layouts, and completed internal frame lifecycle as first-party
references. No external compression profile, workspace formula, ABI design,
source code, tests, benchmark, corpus, result, patent text, or optimization
description was consulted.

### IR-0569

The public 1 MiB Contextual Dynamic Range C lifecycle uses DD-790 through
DD-795, TVG-0665 through TVG-0670, marc's size-tagged ABI-1 configuration,
internal selected profile/workspace calculator, exact HashChain streaming
encoder, typed-context streaming decoder, and existing C lifecycle tests as
first-party references. No external compression API, ABI extension pattern,
extended-window implementation, source code, tests, benchmark, corpus,
result, patent text, or optimization description was consulted.

### IR-0570

The 1 MiB Contextual Dynamic Range CLI adapter uses DD-795 and DD-796,
TVG-0670 and TVG-0671, marc's transactional CLI file adapter, public selected
C lifecycle, existing 64 KiB selector, and shared CLI round-trip script as
first-party references. No external compression CLI, extended-window profile,
source code, tests, benchmark, corpus, result, patent text, or optimization
description was consulted.

### IR-0571

The 1 MiB Contextual Dynamic Range benchmark uses DD-795 through DD-797,
TVG-0670 through TVG-0672, marc's dependency-free benchmark framework, public
selected C lifecycle, checked Format 2 capacity formula, and 64 KiB benchmark
adapter as first-party references. No external benchmark implementation,
compression result, corpus, harness, source code, test, patent text, or
optimization description was consulted.

### IR-0572

The dual-profile Contextual Dynamic Range fuzz boundary uses DD-795 through
DD-798, TVG-0670 through TVG-0673, marc's existing fixed-memory dual-decoder
harness, selected public C decoder, private complete-frame decoder, and local
malformed regression helpers as first-party references. No external fuzz
harness, corpus, finding, compression implementation, source code, test,
patent text, or optimization description was consulted.

### IR-0573

Interoperability schema 38 uses DD-799, the frozen local schema-37 inventory,
the explicit 1 MiB Contextual Dynamic Range CLI profile, marc's existing
manifest contract, PowerShell bundle scripts, compatibility conversion, and
SHA-256/file-equality helpers as first-party references. No external bundle
format, interoperability suite, compressor, corpus, archive, source code,
manifest, conformance vector, patent text, or optimization description was
consulted.

### IR-0574

The schema-38 external admission record uses DD-800, TVG-0675, pushed revision
`363a385168fcfab27adfc8eea3e302129cf01b15`, the Windows/MSVC and Ubuntu
24.04/Ninja CI bundles, the locally generated Ubuntu 26.04/Clang bundle, and
marc's repository-owned schema-38 verifier. No external compression
implementation, source code, archive contents, corpus, test suite, patent
text, or optimization description was consulted.

### IR-0575

The selected-layout Contextual rANS descriptor design uses DD-790, DD-791,
DD-801, TVG-0665, TVG-0666, and TVG-0676; marc's canonical compact-model
grammar, frozen variant-1 descriptor vectors, selected LZSS field-context
layouts, checked arithmetic, and transactional parser as first-party
references. No external rANS implementation, compact-model format, source
code, test, corpus, archive, patent text, or optimization description was
consulted.

### IR-0576

The selected-layout Contextual rANS coding-core design uses DD-790 through
DD-802, TVG-0665, TVG-0666, TVG-0676, and TVG-0677; marc's variant-1 model
builder, reverse writer, decode-table builder, event decoder, selected LZSS
field-context layout, compact descriptor, checked arithmetic, and atomic test
helpers as first-party references. No external rANS implementation, coding
core, table builder, source code, test, corpus, archive, patent text, or
optimization description was consulted.

### IR-0577

The selected-layout LZSS Contextual rANS token-composition design uses DD-790
through DD-803, TVG-0665, TVG-0666, TVG-0676 through TVG-0678; marc's direct
token encoder and decoder, materialized field-operation reference, selected
typed-token validator, selected Contextual rANS core, and atomic workspace
checks as first-party references. No external LZSS or rANS implementation,
composition format, source code, test, corpus, archive, patent text, or
optimization description was consulted.

### IR-0578

The selected-layout Contextual rANS complete-frame design uses DD-790 through
DD-804, TVG-0665, TVG-0666, and TVG-0676 through TVG-0679; marc's frozen
64 KiB rANS stream and frame format, reserved 1 MiB identity, selected direct
token composition, exact HashChain encoder, and already admitted selected
Contextual Dynamic Range frame path as first-party references. No external
LZSS or rANS implementation, frame format, source code, test, corpus, archive,
patent text, or optimization description was consulted.

### IR-0579

The selected-layout Contextual rANS profile and streaming-lifecycle design
uses DD-790 through DD-805, TVG-0665, TVG-0666, and TVG-0676 through
TVG-0680; marc's complete-frame admission, existing rANS workspace
partitioners, selected Contextual Dynamic Range profile/lifecycle, and core
stream-state contract as first-party references. No external LZSS or rANS
implementation, lifecycle, workspace policy, source code, test, corpus,
archive, patent text, or optimization description was consulted.

### IR-0580

The Contextual rANS public C admission uses DD-790 through DD-806, TVG-0665,
TVG-0666, and TVG-0676 through TVG-0681; marc's size-tagged rANS C family,
selected internal lifecycle, existing shared window-profile enum, and
Contextual Dynamic Range public admission as first-party references. No
external LZSS or rANS implementation, C ABI, factory, workspace policy,
source code, test, corpus, archive, patent text, or optimization description
was consulted.

### IR-0581

The Contextual rANS 1 MiB CLI admission uses DD-790 through DD-807,
TVG-0665, TVG-0666, and TVG-0676 through TVG-0682; marc's public selected
rANS C lifecycle, existing 64 KiB rANS CLI adapter, and Contextual Dynamic
Range dual-profile CLI as first-party references. No external LZSS or rANS
implementation, command-line adapter, workspace policy, source code, test,
corpus, archive, patent text, or optimization description was consulted.

### IR-0582

The Contextual rANS 1 MiB benchmark admission uses DD-790 through DD-808,
TVG-0665, TVG-0666, and TVG-0676 through TVG-0683; marc's selected public
rANS lifecycle, 64 KiB rANS benchmark adapter, and 1 MiB Contextual Dynamic
Range benchmark as first-party references. No external LZSS or rANS
implementation, benchmark adapter, capacity formula, source code, test,
corpus, archive, patent text, or optimization description was consulted.

### IR-0583

The dual-profile Contextual rANS fuzz boundary uses DD-790 through DD-809,
TVG-0665, TVG-0666, and TVG-0676 through TVG-0684; marc's existing rANS
dual-boundary harness, selected public decoder policy, and Dynamic Range
dual-profile fuzz route as first-party references. No external LZSS or rANS
implementation, fuzz harness, corpus, source code, test, archive, patent text,
or optimization description was consulted.

### IR-0584

Schema-39 Contextual rANS interoperability admission uses DD-790 through
DD-810, TVG-0665, TVG-0666, and TVG-0676 through TVG-0685; marc's frozen
schema-38 inventory, explicit 1 MiB CLI, bundle generator, verifier, and
downgrade compatibility chain as first-party references. No external LZSS or
rANS implementation, archive, manifest, interoperability suite, source code,
test, corpus, patent text, or optimization description was consulted.

### IR-0585

The selected-layout Contextual tANS descriptor design uses DD-790, DD-791,
DD-811, TVG-0665, TVG-0666, and TVG-0686; marc's frozen Contextual tANS
descriptor, shared selected field-context layouts, compact-model primitive,
and completed Contextual rANS descriptor migration as first-party references.
No external LZSS or tANS implementation, descriptor format, source code, test,
corpus, archive, patent text, or optimization description was consulted.

### IR-0586

The selected-layout Contextual tANS coding-core design uses DD-790 through
DD-812, TVG-0665, TVG-0666, TVG-0686, and TVG-0687; marc's admitted selected
tANS descriptor, frozen tANS transition builder and state machine, shared
field-context layouts, and completed Contextual rANS coding-core migration as
first-party references. No external LZSS or tANS implementation, transition
table, state coder, source code, test, corpus, archive, patent text, or
optimization description was consulted.

### IR-0587

The selected-layout LZSS Contextual tANS token-composition design uses DD-790
through DD-813, TVG-0665, TVG-0666, and TVG-0686 through TVG-0688; marc's
direct tANS typed-token encoder and decoder, materialized field-operation
reference, selected typed-token validator, admitted selected Contextual tANS
core, and completed Contextual rANS token-composition migration as first-party
references. No external LZSS or tANS implementation, composition format,
source code, test, corpus, archive, patent text, or optimization description
was consulted.

### IR-0588

The selected-layout Contextual tANS complete-frame design uses DD-790 through
DD-814, TVG-0665, TVG-0666, and TVG-0686 through TVG-0689; marc's frozen
64 KiB tANS stream and frame representation, reserved 1 MiB identity,
selected direct token composition, exact HashChain encoder, fixed tANS table
workspace, and completed selected Contextual rANS frame migration as first-
party references. No external LZSS or tANS implementation, frame format,
source code, test, corpus, archive, patent text, or optimization description
was consulted.

### IR-0589

The selected Contextual tANS profile and streaming design uses DD-790 through
DD-815, TVG-0665, TVG-0666, and TVG-0686 through TVG-0690; marc's admitted
selected tANS complete frames, frozen 64 KiB profile and partial-buffer state
machines, exact HashChain workspace calculator, fixed transition tables, and
completed dual-profile Contextual rANS lifecycle as first-party references.
No external LZSS or tANS implementation, streaming lifecycle, workspace
policy, source code, test, corpus, archive, patent text, or optimization
description was consulted.

### IR-0590

The Contextual tANS public C admission uses DD-790 through DD-816, TVG-0665,
TVG-0666, and TVG-0686 through TVG-0691; marc's existing size-tagged tANS C
family, selected private lifecycle, shared public window-profile enum, and
completed Contextual rANS public admission as first-party references. No
external LZSS or tANS implementation, C ABI, factory, workspace policy, source
code, test, corpus, archive, patent text, or optimization description was
consulted.

### IR-0591

The Contextual tANS CLI profile design uses DD-790 through DD-817, TVG-0665,
TVG-0666, and TVG-0686 through TVG-0692; marc's existing Contextual Dynamic
Range and Contextual rANS dual-name CLI adapters, completed public tANS profile
admission, generic round-trip harness, and output-preservation checks as
first-party references. No external LZSS or tANS implementation, CLI,
adapter, source code, test, corpus, archive, patent text, or optimization
description was consulted.

### IR-0592

The Contextual tANS selected benchmark design uses DD-790 through DD-818,
TVG-0665, TVG-0666, and TVG-0686 through TVG-0693; marc's completed dual-name
CLI/public C admission, Contextual Dynamic Range and rANS selected benchmark
adapters, checked capacity planner, workspace report, and pre-timing round-trip
contract as first-party references. No external LZSS or tANS implementation,
benchmark, adapter, source code, test, corpus, archive, patent text, or
optimization description was consulted.

### IR-0593

The dual-profile Contextual tANS fuzz boundary uses DD-790 through DD-819,
TVG-0665, TVG-0666, and TVG-0686 through TVG-0694; marc's existing fixed-
memory tANS dual-decoder harness, selected public decoder policy, and completed
Contextual rANS dual-profile fuzz route as first-party references. No external
LZSS or tANS implementation, fuzz harness, corpus, finding, source code, test,
archive, patent text, or optimization description was consulted.

### IR-0594

Schema-40 Contextual tANS interoperability uses DD-790 through DD-820,
TVG-0665, TVG-0666, and TVG-0686 through TVG-0695; marc's frozen schema-39
inventory, explicit 1 MiB tANS CLI, bundle generator, verifier, and downgrade
compatibility chain as first-party references. No external LZSS or tANS
implementation, archive, manifest, interoperability suite, source code, test,
corpus, patent text, or optimization description was consulted.

### IR-0595

The selected-layout Contextual Blocked Huffman descriptor design uses DD-790
through DD-821, TVG-0665, TVG-0666, TVG-0696, and marc's existing canonical
Huffman primitives, 64 KiB Contextual Blocked Huffman descriptor grammar,
field-context variant selectors, and completed 1 MiB Dynamic Range, rANS, and
tANS paths as first-party references. No external Huffman or LZSS
implementation, descriptor grammar, source code, test, corpus, archive,
patent text, or optimization description was consulted.

### IR-0596

The selected-layout Contextual Blocked Huffman coding-core design uses DD-790
through DD-822, TVG-0665, TVG-0666, TVG-0696, TVG-0697, and marc's existing
64 KiB model builder, canonical writer, bounded decoder, selected descriptor,
field-context selector, and completed selected rANS/tANS coding cores as
first-party references. No external Huffman or LZSS implementation, payload
format, API, source code, test, corpus, archive, patent text, or optimization
description was consulted.

### IR-0597

The selected Contextual Blocked Huffman typed-token design uses DD-790 through
DD-823, TVG-0665, TVG-0666, TVG-0696 through TVG-0698, and marc's existing
64 KiB direct adapter, selected entropy coding core, field-context token
modeler/inverter, typed-token validators, and selected rANS/tANS direct
adapters as first-party references. No external Huffman or LZSS
implementation, adapter, format, API, source code, test, corpus, archive,
patent text, or optimization description was consulted.

### IR-0598

The selected Contextual Blocked Huffman complete-frame design uses DD-790
through DD-824, TVG-0665, TVG-0666, TVG-0696 through TVG-0699, and marc's
existing 64 KiB Contextual Blocked Huffman stream/frame format, direct typed-
token adapter, selected descriptor/coding core, and completed selected
Dynamic Range, rANS, and tANS complete-frame paths as first-party references.
No external Huffman or LZSS implementation, frame format, API, source code,
test, corpus, archive, patent text, or optimization description was
consulted.

### IR-0599

The selected Contextual Blocked Huffman profile/streaming design uses DD-790
through DD-825, TVG-0665, TVG-0666, TVG-0696 through TVG-0700, and marc's
existing 64 KiB profile/workspace partitioners, streaming state machines,
selected complete-frame implementation, and completed selected Dynamic Range,
rANS, and tANS lifecycle paths as first-party references. No external Huffman
or LZSS implementation, allocator layout, streaming API, source code, test,
corpus, archive, patent text, or optimization description was consulted.

### IR-0600

The selected Contextual Blocked Huffman public C lifecycle uses DD-790 through
DD-826, TVG-0665, TVG-0666, TVG-0696 through TVG-0701, and marc's existing
ABI-1 Contextual Blocked Huffman factory plus the completed selected Dynamic
Range, rANS, and tANS public lifecycle patterns as first-party references. No
external C API, Huffman or LZSS implementation, ABI layout, source code, test,
corpus, archive, patent text, or optimization description was consulted.

### IR-0601

The selected Contextual Blocked Huffman CLI design uses DD-790 through DD-827,
TVG-0665, TVG-0666, TVG-0696 through TVG-0702, marc's existing 64 KiB CLI
adapter, completed selected public C lifecycle, and selected Dynamic Range,
rANS, and tANS explicit-name patterns as first-party references. No external
CLI, Huffman or LZSS implementation, argument grammar, source code, test,
corpus, archive, patent text, or optimization description was consulted.

### IR-0602

The selected Contextual Blocked Huffman benchmark design uses DD-790 through
DD-828, TVG-0665, TVG-0666, TVG-0696 through TVG-0703, marc's existing 64 KiB
benchmark adapter, completed selected public C lifecycle and CLI naming, and
the selected Dynamic Range, rANS, and tANS benchmark patterns as first-party
references. No external benchmark, Huffman or LZSS implementation, adapter,
source code, test, corpus, archive, patent text, or optimization description
was consulted.

### IR-0603

The dual-profile Contextual Blocked Huffman fuzz boundary uses DD-790 through
DD-829, TVG-0665, TVG-0666, and TVG-0696 through TVG-0704; marc's existing
fixed-memory private/public decoder harness, completed selected public
admission, and dual-profile Contextual rANS and tANS fuzz routes as first-party
references. No external Huffman or LZSS implementation, fuzz harness, corpus,
finding, source code, test, archive, patent text, or optimization description
was consulted.

### IR-0604

Schema-41 Contextual Blocked Huffman interoperability uses DD-790 through
DD-830, TVG-0665, TVG-0666, and TVG-0696 through TVG-0705; marc's frozen
schema-40 inventory, explicit 1 MiB Contextual Blocked Huffman CLI, bundle
generator, verifier, and downgrade compatibility chain as first-party
references. No external Huffman or LZSS implementation, archive, manifest,
interoperability suite, source code, test, corpus, patent text, or optimization
description was consulted.

### IR-0605

The selected Contextual Adaptive Huffman model-bank design uses DD-790 through
DD-831, TVG-0665, TVG-0666, and TVG-0706; marc's validated dual field-context
layouts, existing caller-owned 31-tree FGK bank, frozen 64 KiB Adaptive
Huffman representation, and completed selected Dynamic Range, rANS, tANS, and
Blocked Huffman patterns as first-party references. No external Adaptive
Huffman or LZSS implementation, source code, test, corpus, archive, patent
text, or optimization description was consulted.

### IR-0606

The selected Contextual Adaptive Huffman operation-coding design uses DD-831,
DD-832, TVG-0706, TVG-0707, marc's completed selected FGK model bank, frozen
variant-1 operation encoder and decoder, and the selected Dynamic Range,
rANS, tANS, and Blocked Huffman operation boundaries as first-party
references. No external Adaptive Huffman or LZSS implementation, source code,
test, corpus, archive, patent text, or optimization description was consulted.

### IR-0607

The selected Contextual Adaptive Huffman typed-token bridge design uses
DD-831 through DD-833, TVG-0706 through TVG-0708, marc's completed selected
FGK model bank and operation coder, the frozen 64 KiB LZSS typed-token bridge,
and the completed selected Dynamic Range, rANS, tANS, and Blocked Huffman
token boundaries as first-party references. No external Adaptive Huffman or
LZSS implementation, source code, test, corpus, archive, patent text, or
optimization description was consulted.

### IR-0608

The selected Contextual Adaptive Huffman complete-frame design uses DD-831
through DD-834, TVG-0706 through TVG-0709, marc's completed selected token
bridge, frozen 64 KiB stream/frame format, and completed selected Dynamic
Range, rANS, tANS, and Blocked Huffman frame boundaries as first-party
references. No external Adaptive Huffman or LZSS implementation, frame
format, source code, test, corpus, archive, patent text, or optimization
description was consulted.

### IR-0609

The selected Contextual Adaptive Huffman profile and streaming-lifecycle
design uses DD-831 through DD-834, TVG-0706 through TVG-0709, marc's completed
selected complete-frame transaction, frozen 64 KiB profile/workspace
partitioners and streaming state machines, and the completed selected Dynamic
Range, rANS, tANS, and Blocked Huffman lifecycle patterns as first-party
references. No external Adaptive Huffman or LZSS implementation, allocator
layout, streaming API, source code, test, corpus, archive, patent text, or
optimization description was consulted.

### IR-0610

The selected Contextual Adaptive Huffman public C lifecycle design uses
DD-831 through DD-835, TVG-0706 through TVG-0710, marc's completed selected
private lifecycle, existing ABI-1 Contextual Adaptive Huffman factory, and the
completed selected Dynamic Range, rANS, tANS, and Blocked Huffman public
lifecycle patterns as first-party references. No external C API, Adaptive
Huffman or LZSS implementation, ABI layout, source code, test, corpus,
archive, patent text, or optimization description was consulted.

### IR-0611

The selected Contextual Adaptive Huffman CLI design uses DD-831 through
DD-836, TVG-0706 through TVG-0711, marc's completed selected public C
lifecycle, existing 64 KiB CLI adapter, and the selected Dynamic Range, rANS,
tANS, and Blocked Huffman explicit-name patterns as first-party references.
No external CLI, Adaptive Huffman or LZSS implementation, argument grammar,
source code, test, corpus, archive, patent text, or optimization description
was consulted.

### IR-0612

The selected Contextual Adaptive Huffman benchmark design uses DD-831 through
DD-837, TVG-0706 through TVG-0712, marc's existing 64 KiB benchmark adapter,
completed selected public C lifecycle and CLI naming, and the selected Dynamic
Range, rANS, tANS, and Blocked Huffman benchmark patterns as first-party
references. No external benchmark, Adaptive Huffman or LZSS implementation,
adapter, source code, test, corpus, archive, patent text, or optimization
description was consulted.

### IR-0613

The dual-profile Contextual Adaptive Huffman fuzz design uses DD-831 through
DD-838, TVG-0706 through TVG-0713, marc's existing fixed-memory private/public
decoder harness, completed selected public admission, deterministic malformed
regressions, and dual-profile Contextual rANS, tANS, and Blocked Huffman fuzz
routes as first-party references. No external Adaptive Huffman or LZSS
implementation, fuzz harness, corpus, finding, source code, test, archive,
patent text, or optimization description was consulted.

### IR-0614

Contextual Adaptive Huffman selected-profile interoperability uses DD-831
through DD-839, TVG-0706 through TVG-0714, marc's frozen schema-41 inventory,
explicit selected CLI profile, manifest contract, PowerShell bundle scripts,
compatibility conversion, and SHA-256/file-equality helpers as first-party
references. No external bundle format, interoperability suite, Adaptive
Huffman or LZSS implementation, compressor, corpus, archive, source code,
manifest, conformance vector, patent text, or optimization description was
consulted.

### IR-0615

The external Silesia benchmark-profile design uses the official
[Silesia Corpus page](https://sun.aei.polsl.pl/~sdeor/index.php?page=silesia)
only for the Corpus purpose, twelve member names, uncompressed sizes,
published MD5 identifiers, source descriptions, and manual-download location.
The page's linked compressor implementations, benchmark results, source code,
optimization descriptions, and third-party mirrors were not consulted. The
match-finder diagnostic fields, non-redistribution policy, local-only
verification boundary, aggregation rules, and BinaryTree evidence gate were
designed independently from marc's existing HashChain Exact contract and
benchmark infrastructure.

### IR-0616

The offline Silesia verifier implementation uses DD-841, DD-842, IR-0615,
Python standard-library file and hashing interfaces, CMake's documented
`FindPython3` interpreter discovery, and marc's external-data design as
first-party references. No downloader, remote service, third-party verifier,
Corpus-processing script, compression implementation, benchmark result,
source code, test suite, or optimization description was consulted.

### IR-0617

The large-file HashChain frame benchmark uses DD-841 through DD-843,
TVG-0716 and TVG-0717, marc's existing exact finder contract, checked
arithmetic, caller-owned aligned workspace, statistics, benchmark timing
conventions, and external Silesia profile as first-party references. No
external match-finder benchmark, large-file runner, LZSS implementation,
compressor, Corpus result, source code, test, patent text, or optimization
description was consulted.

### IR-0618

The HashChain classification diagnostic uses DD-843 and DD-844, BM-0053 and
BM-0054, marc's existing five-byte prefix hash, exact match loop, optional
statistics pointer, checked arithmetic, and bounded frame runner as first-party
references. No external HashChain, binary-tree, compressor, profiling tool,
Corpus analysis, benchmark result, source code, test, patent, or optimization
description was consulted.

### IR-0619

The BinaryTree Exact design uses DD-760 through DD-765, DD-841 through DD-845,
BM-0053 and BM-0054, the existing Exhaustive and HashChain contracts, marc's
nearest-distance tie-break, caller-owned workspace and checked-arithmetic
rules, and elementary lexicographic-order and AVL-tree properties as
first-party and mathematical references. No external match-finder, AVL,
suffix-tree, compressor, benchmark, source code, test suite, patent, or
optimization description was consulted.

### IR-0620

The synthetic HashChain admission benchmark uses DD-841 through DD-846,
TVG-0718 through TVG-0720, BM-0054 and BM-0055, marc's documented five-byte
hash, existing fixed-seed LCG constants, checked arithmetic, bounded frame
runner, and optional statistics as first-party references. No external
generator, collision corpus, match-finder benchmark, compressor, source code,
test, patent, or published performance result was consulted.

### IR-0621

The BinaryTree workspace calculator and empty initializer use DD-845 through
DD-847, TVG-0719 through TVG-0721, the repository's HashChain workspace
contract, checked arithmetic, overlap detector, LZSS parameter validator, and
elementary AVL height bounds as first-party and mathematical references. No
external tree, match-finder, compressor, allocator, source code, test suite,
patent, or layout description was consulted.

### IR-0622

The BinaryTree insertion, rotation, metadata, and structural validation stage
uses DD-845, DD-847, DD-848, TVG-0719, TVG-0721, TVG-0722, the repository-owned
finite suffix order, workspace representation, and elementary AVL invariants.
No external AVL implementation, match finder, compressor, source code, test
suite, patent, pseudocode, or optimization description was consulted.

### IR-0623

The BinaryTree structural deletion stage uses DD-845, DD-848, DD-849,
TVG-0719, TVG-0722, TVG-0723, marc's separated slot representation, insertion
rotations, and structural validator as first-party references. Only elementary
binary-search-tree successor and AVL invariants were additionally used. No
external tree, match finder, compressor, source code, test, patent, pseudocode,
or optimization description was consulted.

### IR-0624

The BinaryTree window-advancement stage uses DD-845, DD-848 through DD-850,
TVG-0719, TVG-0722 through TVG-0724, the repository-owned insertion,
structural deletion, slot mapping, and validator as first-party references.
Only elementary half-open sliding-window arithmetic is additionally used. No
external tree, match finder, compressor, source code, test, patent, pseudocode,
or optimization description was consulted.

### IR-0625

The BinaryTree lexicographic-neighbor and LCP stage uses DD-845, DD-848,
DD-850, DD-851, TVG-0719, TVG-0722, TVG-0724, and TVG-0725, together with the
repository-owned finite suffix order, active-window protocol, structural
validator, and Exhaustive reference comparison rules. The mathematical fact
that strings sharing a prefix form a contiguous lexicographic interval is the
only additional basis. No external tree, suffix structure, match finder,
compressor, source code, test, patent, pseudocode, or optimization description
was consulted.

### IR-0626

The BinaryTree equal-prefix range aggregation stage uses DD-845, DD-848,
DD-851, DD-852, TVG-0719, TVG-0722, TVG-0725, and TVG-0726, together with the
repository-owned subtree maximum-position metadata and neighbor-LCP result.
Only elementary binary-search-tree interval decomposition is additionally
used. No external range tree, suffix structure, match finder, compressor,
source code, test, patent, pseudocode, or optimization description was
consulted.

### IR-0627

The private BinaryTree Exact finder contract and three-strategy differential
stage uses DD-760 through DD-765, DD-845, DD-850 through DD-853, TVG-0719,
TVG-0724 through TVG-0727, the repository-owned `LzssMatchFinder` concept,
Exhaustive reference finder, and HashChain Exact finder. No external match
finder, tree, compressor, source code, test, patent, pseudocode, or
optimization description was consulted.

### IR-0628

The private BinaryTree typed-token single-pass entry uses DD-760 through
DD-765, DD-845, DD-853, DD-854, TVG-0719, TVG-0727, and TVG-0728, together
with the repository-owned typed parser, token validation, overlap checks,
checked arithmetic, BinaryTree workspace initializer, and canonical LZSS token
serializer. No external parser, match finder, compressor, source code, test,
patent, pseudocode, or optimization description was consulted.

### IR-0629

The private BinaryTree diagnostic-counter stage uses DD-760, DD-765, DD-845,
DD-855, TVG-0719, TVG-0727 through TVG-0729, the repository-owned common
match-finder statistics contract, HashChain saturation and logarithmic-bin
rules, and the existing private BinaryTree operation boundaries. No external
tree, profiler, match finder, compressor, source code, test, patent,
pseudocode, or optimization description was consulted.

### IR-0630

The private BinaryTree benchmark stage uses DD-731 through DD-734, DD-845,
DD-855, DD-856, TVG-0640, TVG-0641, TVG-0729, and TVG-0730, together with the
repository-owned HashChain benchmark driver, deterministic synthetic inputs,
frame splitting, throughput calculation, and BinaryTree diagnostic contract.
No external benchmark harness, tree, match finder, compressor, source code,
test, patent, pseudocode, or optimization description was consulted.

### IR-0631

The offline Silesia matrix runner uses DD-841 through DD-844, DD-856, DD-857,
TVG-0716 through TVG-0718, TVG-0730, and TVG-0731, together with marc's
repository-owned strict Corpus verifier, strategy-explicit frame benchmark,
JSON reporting conventions, and external-data policy. No external benchmark
or Corpus orchestration implementation, source code, test, result set, patent,
pseudocode, or optimization description was consulted.

### IR-0632

The deterministic synthetic matrix runner uses DD-731 through DD-734, DD-846,
DD-856, DD-858, DD-859, TVG-0641, TVG-0730 through TVG-0732, the
repository-owned five synthetic generators, strategy-explicit benchmark, and
strict Silesia report parsing and aggregation rules. No external benchmark
runner, generator, match finder, compressor, source code, test, result set,
patent, pseudocode, or optimization description was consulted.

### IR-0633

The private HashTree Exact pre-implementation design uses DD-760 through
DD-765, DD-841 through DD-845, DD-850 through DD-860, TVG-0719, TVG-0724
through TVG-0733, BM-0053 through BM-0057, the repository-owned five-byte
HashChain partition and predecessor-distance ring, the private AVL structural
validator and prefix-range maximum-position proof, and the measured Silesia
and synthetic operation counters. The additional basis is elementary AVL
partitioning and the fact that two strings each share at least the smaller of
their known common-prefix lengths with the same third string. No external hash
tree, adaptive index, match finder, compressor, source code, test, benchmark
result, patent, pseudocode, or optimization description was consulted.

### IR-0634

The shared private LZSS five-byte prefix-hash helper uses DD-861, TVG-0733,
and the repository-owned HashChain hash expression that was already specified,
implemented, tested through Exact differential matching, and used to define
the HashTree bucket partition. No external hash function, hash tree, match
finder, compressor, source code, test, patent, pseudocode, or optimization
description was consulted.

### IR-0635

The private HashTree workspace calculator uses DD-861, DD-862, TVG-0733,
TVG-0734, the repository-owned HashChain and BinaryTree checked workspace
calculators, and the exact array inventory in the HashTree pre-implementation
design. No external hash tree, match finder, memory layout, compressor, source
code, test, patent, pseudocode, or optimization description was consulted.

### IR-0636

The private HashTree atomic initializer uses DD-861 through DD-863,
TVG-0733 through TVG-0735, marc's checked workspace requirements, the
repository-owned buffer-overlap classifier, and the atomic-publication pattern
of the HashChain and BinaryTree initializers. No external hash tree, match
finder, lazy arena, compressor, source code, test, patent, pseudocode, or
optimization description was consulted.

### IR-0637

The private HashTree Chain-only query and advance path uses DD-861 through
DD-864, TVG-0733 through TVG-0736, marc's shared five-byte hash, the
repository-owned Exact HashChain traversal, lazy workspace lifetime proof,
sticky BinaryTree protocol state, and Exhaustive differential oracle. No
external hash tree, match finder, compressor, source code, test, patent,
pseudocode, or optimization description was consulted.

### IR-0638

The private HashTree promotion state machine uses DD-861 through DD-866,
TVG-0733 through TVG-0738, the completed Chain-query boundary, and marc's
existing first-error sticky-state conventions. The transition proof is a
repository-owned finite state machine over Idle, Pending, and Building. No
external hash tree, adaptive index, match finder, compressor, source code,
test, patent, pseudocode, or optimization description was consulted.

### IR-0639

The private HashTree bucket builder uses DD-861 through DD-867, TVG-0733
through TVG-0739, marc's repository-owned BinaryTree AVL invariants and
rotations, the checked prefix hash, and HashTree's lazy workspace lifetime
contract. The bounded non-recursive validation and private-root publication
rule were written for this repository. No external hash tree, match finder,
compressor, source code, test, patent, pseudocode, or optimization description
was consulted.

### IR-0640

The private promoted-bucket Exact query uses DD-861 through DD-868,
TVG-0733 through TVG-0740, marc's repository-owned BinaryTree neighbor and
prefix-interval proof, HashTree bucket builder, and Exhaustive/HashChain
oracles. No external tree query, compressor, source code, test, patent,
pseudocode, or optimization description was consulted.

### IR-0641

The private promoted-bucket mutation uses DD-861 through DD-869, TVG-0733
through TVG-0741, marc's repository-owned BinaryTree insertion, structural
deletion, AVL metadata rules, and HashTree ring/bucket contracts. No external
tree mutation, match finder, compressor, source code, test, patent, pseudocode,
or optimization description was consulted.

### IR-0642

The integrated private HashTree finder uses DD-861 through DD-870,
TVG-0733 through TVG-0742, and only the repository-owned promotion planner,
bucket builder, Exact query, mutation, HashChain route, and Exhaustive/global
BinaryTree oracles. No external hybrid match finder, compressor, source code,
test, patent, pseudocode, or optimization description was consulted.

### IR-0643

The first integrated HashTree diagnostic layer uses DD-861 through DD-871,
TVG-0733 through TVG-0743, the repository-owned promotion, builder, query, and
mutation result contracts, and marc's existing saturating match-finder
statistics and logarithmic depth-bin convention. No external profiler, match
finder, adaptive tree, compressor, source code, test, benchmark schema, patent,
pseudocode, or optimization description was consulted.

### IR-0644

HashTree component-cost diagnostics use DD-861 through DD-872, TVG-0733
through TVG-0744, the repository-owned builder, query, mutation, validator,
and match-finder statistics contracts, and the existing saturating-counter
policy. No external tree profiler, match finder, compressor, source code,
test, benchmark schema, patent, pseudocode, or optimization description was
consulted.

### IR-0645

The private HashTree benchmark route uses DD-861 through DD-873, TVG-0733
through TVG-0745, the repository-owned match-finder benchmark contract,
HashTree initializer, diagnostics, and existing HashChain/BinaryTree smoke-test
style. No external benchmark harness, adaptive match finder, compressor,
source code, test, result schema, patent, pseudocode, or optimization
description was consulted.

### IR-0646

The private synthetic HashTree threshold runner uses DD-861 through DD-874,
TVG-0733 through TVG-0746, marc's repository-owned deterministic synthetic
cases, benchmark report, HashChain Exact oracle, histogram convention, and
existing local JSON runner structure. No external benchmark runner, adaptive
match finder, compressor, source code, test, result schema, Corpus harness,
patent, pseudocode, or optimization description was consulted.

### IR-0647

The Silesia HashTree threshold runner uses DD-874 through DD-876, TVG-0746
through TVG-0747, marc's repository-owned strict Silesia verifier, frame-mode
benchmark report, HashChain oracle, threshold aggregator, and existing v1
runner conventions. No external benchmark runner, adaptive match finder,
compressor, source code, test, result schema, Corpus harness, tuning guide,
patent, pseudocode, or optimization description was consulted.

### IR-0648

HashTree maintenance v2 uses DD-861 through DD-878, TVG-0733 through
TVG-0748, the repository-owned ring-slot identity, published AVL invariant,
mutation, active-range validator, HashChain and Exhaustive oracles, and the
measured Silesia maintenance counters. No external hash-tree match finder,
balanced-tree deletion optimization, compressor, source code, test,
benchmark result, patent, pseudocode, or optimization description was
consulted.

### IR-0649

Fixed-width HashTree workspace evidence uses DD-881 through DD-884,
TVG-0749 through TVG-0751, the repository-owned synthetic and Silesia matrix
runners, verified local Corpus manifest, maintenance-v2 JSON evidence, and
HashChain Exact baselines. No external match finder, compressor, source code,
test, benchmark result, packed layout, tuning guide, patent, pseudocode, or
optimization description was consulted.

### IR-0650

The private four-MiB HashTree experiment design uses DD-881 through DD-886,
TVG-0749 through TVG-0753, the repository-owned fixed-width workspace formula,
HashChain and Exhaustive Exact oracles, Silesia verifier and matrix runners,
current 128-MiB aggregate-memory policy, and frozen one-MiB contextual-format
design. No external match finder, sparse tree, compressor, source code, test,
benchmark result, format extension, tuning guide, patent, pseudocode, or
optimization description was consulted.

### IR-0651

The match-finder token fingerprint uses DD-887, TVG-0754, marc's existing
SHA-256 implementation and checked arithmetic, the private benchmark parser,
and its CMake smoke tests. No external benchmark fingerprint format, match
finder, compressor, source code, test vector, schema, or optimization
description was consulted.

### IR-0652

The private four-MiB Silesia runner uses DD-886 through DD-888, TVG-0753
through TVG-0755, the repository-owned Corpus verifier, frame benchmark,
HashChain and HashTree report validators, canonical token fingerprint, and
prior versioned local-runner structure. No external runner, match finder,
compressor, source code, test, benchmark result, schema, tuning guide,
patent, pseudocode, or optimization description was consulted.

### IR-0653

The private four-MiB experiment evidence uses DD-886 through DD-889,
TVG-0753 through TVG-0756, the repository-owned strict Silesia verifier,
versioned experiment runner, canonical token fingerprint, and the locally
generated revision `9de8d29` JSON result. No external match finder,
compressor, source code, test, benchmark result, format design, tuning guide,
patent, pseudocode, or optimization description was consulted.

### IR-0654

The four-MiB aggregate-workspace audit uses DD-886 through DD-890,
TVG-0753 through TVG-0757, the repository-owned contextual profile
calculators, `LzssTypedToken` layout, fixed-width HashTree calculator, default
decoder limits, and revision `9de8d29` experiment evidence. No external match
finder, allocator, compressor, source code, test, benchmark result, sparse-pool
layout, tuning guide, patent, pseudocode, or optimization description was
consulted.

### IR-0655

The bounded sparse HashTree design uses DD-886 through DD-891, TVG-0753
through TVG-0758, the repository-owned complete HashChain, fixed-width
HashTree builder/query/mutation invariants, promotion transaction, checked
workspace layout, and aggregate-memory audit. No external sparse match finder,
node pool, allocator, compressor, source code, test, benchmark result, tuning
guide, patent, pseudocode, or optimization description was consulted.

### IR-0656

The sparse workspace calculator and allocator use DD-890 through DD-892,
TVG-0757 through TVG-0759, the repository-owned checked-math helpers,
fixed-width position sentinels, HashTree workspace conventions, decoder-limit
validation, and aligned raw-workspace initialization style. No external pool,
allocator, sparse match finder, compressor, source code, test, benchmark,
tuning guide, patent, pseudocode, or optimization description was consulted.

### IR-0657

The pool-local Exact query boundary uses DD-891 through DD-893, TVG-0758
through TVG-0760, the repository-owned complete-tree query ordering,
subtree-maximum invariant, bounded traversal checks, and sparse-pool free-node
marker. No external tree query, sparse match finder, compressor, source code,
test, benchmark, tuning guide, patent, pseudocode, or optimization description
was consulted.

### IR-0658

The atomic pool-local bucket builder uses DD-891 through DD-894, TVG-0758
through TVG-0761, the repository-owned complete-chain inspection, AVL ordering
and rotations, sparse allocator contract, pool-local Exact query, and bounded
parent-link validation. No external tree builder, sparse match finder,
compressor, source code, test, benchmark, tuning guide, patent, pseudocode, or
optimization description was consulted.

### IR-0659

The pool-local mutation primitives use DD-891 through DD-895, TVG-0758
through TVG-0762, the repository-owned AVL mutation V2 implementation,
pool allocator reserved state, pool-local query identity, and atomic builder
fixtures. No external tree mutation, sparse match finder, compressor, source
code, test, benchmark, tuning guide, patent, pseudocode, or optimization
description was consulted.

### IR-0660

The pool-exhaustion bucket state transitions use DD-891 through DD-896,
TVG-0758 through TVG-0763, the repository-owned atomic sparse builder,
validated whole-bucket release, pool-local mutation primitive, allocator
accounting, and the three-state design in Section 26. No external sparse state
machine, tree fallback, match finder, compressor, source code, test, benchmark,
tuning guide, patent, pseudocode, or optimization description was consulted.

### IR-0661

The sparse workspace owner and frame reset use DD-890 through DD-897,
TVG-0757 through TVG-0764, the repository-owned checked layout, node-pool
sentinels and accounting, three-state bucket design, and raw-workspace lifetime
conventions. No external workspace owner, allocator reset, sparse match finder,
compressor, source code, test, benchmark, tuning guide, patent, pseudocode, or
optimization description was consulted.

### IR-0662

The promoted-bucket retirement transition uses DD-891 through DD-898,
TVG-0758 through TVG-0765, the repository-owned pool-local detach primitive,
allocator reserved/release contract, complete-chain ring overwrite ordering,
and sparse three-state model. No external tree-retirement implementation,
sparse match finder, compressor, source code, test, benchmark, tuning guide,
patent, pseudocode, or optimization description was consulted.

### IR-0663

The sparse metadata commit and position controller use DD-891 through DD-899,
TVG-0758 through TVG-0766, the repository-owned complete-chain insertion order,
workspace metadata views, state transitions, retirement contract, and checked
fixed-width counts. No external sparse controller, concurrent commit scheme,
match finder, compressor, source code, test, benchmark, tuning guide, patent,
pseudocode, or optimization description was consulted.

### IR-0664

The sparse Exact query dispatcher and promotion trigger use DD-891 through
DD-900, TVG-0758 through TVG-0767, the repository-owned complete-chain Exact
selection rule, pool-local tree query, promotion-state threshold contract,
builder, state transition, and metadata commit controller. No external sparse
query dispatcher, adaptive match finder, compressor, source code, test,
benchmark, tuning guide, patent, pseudocode, or optimization description was
consulted.

### IR-0665

The sparse multi-position advance protocol uses DD-891 through DD-901,
TVG-0758 through TVG-0768, the repository-owned LZSS token loop, exhaustive
reference matcher, complete-chain cursor convention, promotion trigger,
retirement, insertion, and metadata controller. No external matcher advance
loop, sparse tree controller, compressor, source code, test, benchmark, tuning
guide, patent, pseudocode, or optimization description was consulted.

### IR-0666

The sparse diagnostic aggregation uses DD-891 through DD-902, TVG-0758 through
TVG-0769, the repository-owned `LzssMatchFinderStatistics`, component observers,
saturating counter convention, promotion state, sparse workspace metadata, and
pool accounting. No external diagnostic schema, sparse matcher instrumentation,
compressor, source code, test, benchmark, tuning guide, patent, pseudocode, or
optimization description was consulted.

### IR-0667

The private sparse match finder uses DD-891 through DD-903, TVG-0758 through
TVG-0770, the repository-owned match-finder concept, workspace validation,
buffer-overlap check, sparse workspace owner, controller, promotion state,
advance cursor, and error conventions. No external sparse match-finder class,
compressor, source code, test, benchmark, tuning guide, patent, pseudocode, or
optimization description was consulted.

### IR-0668

The explicit private typed-encoder route uses DD-891 through DD-904,
TVG-0758 through TVG-0771, the repository-owned typed-token single-pass
contract, sparse match finder, workspace calculator, overlap validation,
bounded-memory policy, exhaustive oracle, and canonical token serializer. No
external encoder dispatch, adaptive match-finder integration, compressor,
source code, test, benchmark, tuning guide, patent, pseudocode, or optimization
description was consulted.

### IR-0669

The explicit sparse benchmark route uses DD-891 through DD-905,
TVG-0758 through TVG-0772, the repository-owned frame benchmark lifecycle,
synthetic generators, token fingerprint, sparse matcher options, workspace
calculator, diagnostic aggregation, and timing separation. No external sparse
benchmark harness, tuning matrix, compressor, source code, test, benchmark
result, tuning guide, patent, pseudocode, or optimization description was
consulted.

### IR-0670

The offline sparse Silesia matrix uses DD-891 through DD-906, TVG-0758 through
TVG-0773, the repository-owned strict Corpus verifier, HashChain baseline
runner, sparse benchmark report, deterministic manifest order, report parser,
and aggregate conventions. No external sparse matrix runner, checkpoint
scheme, compressor, source code, test, benchmark result, tuning guide, patent,
pseudocode, or optimization description was consulted.

### IR-0671

Sparse matrix checkpointing uses DD-906 through DD-907, TVG-0773 through
TVG-0774, Python's documented JSON, file flush, `os.fsync`, and `os.replace`
operations, and marc's existing strict report and Corpus validators. No external
checkpoint runner, scheduler, compressor, source code, test, benchmark result,
tuning guide, patent, pseudocode, or optimization description was consulted.

### IR-0672

Bounded sparse matrix batching uses DD-907 through DD-908, TVG-0774 through
TVG-0775, the repository-owned checkpoint index, canonical matrix loop, and
validated-record publication boundary. No external batch runner, scheduler,
workflow engine, compressor, source code, test, benchmark result, tuning guide,
patent, pseudocode, or optimization description was consulted.

### IR-0673

The four-MiB contextual LZSS design uses DD-909 through DD-910, the frozen
64-KiB and one-MiB typed-token/context formats, marc's checked profile and
workspace rules, and its repository-owned HashChain, Exhaustive, complete
HashTree, sparse HashTree, and Silesia evidence. No external compressor,
source code, test, format extension, match finder, tuning guide, patent,
pseudocode, or optimization description was consulted.

### IR-0674

The four-MiB shared value-boundary implementation uses IR-0673, DD-910 through
DD-911, TVG-0776 through TVG-0777, the repository-owned typed-token validator,
field-context mapping, checked arithmetic, and frozen old-layout tests. No
external compressor, context mapper, large-window implementation, source code,
test, benchmark, patent, pseudocode, or optimization description was
consulted.

### IR-0675

The four-MiB Contextual Dynamic Range decoder stage uses IR-0673 through
IR-0674, DD-910 through DD-912, TVG-0776 through TVG-0778, the repository-owned
typed-context frame validator, adaptive integer range coder, checked workspace
publication boundary, and frozen older vectors. No external compressor, range
coder implementation, large-window format, source code, test, benchmark,
patent, pseudocode, or optimization description was consulted.

### IR-0676

The four-MiB Contextual Dynamic Range encoder/profile/streaming stage uses
IR-0673 through IR-0675, DD-910 through DD-913, TVG-0776 through TVG-0779,
marc's repository-owned complete-frame encoder, streaming state machines,
HashChain workspace calculator, native typed staging layouts, and checked
aggregate arithmetic. No external compressor, range coder implementation,
large-window format, memory profile, source code, test, benchmark, patent,
pseudocode, or optimization description was consulted.

### IR-0677

The four-MiB Contextual Dynamic Range C boundary uses IR-0673 through IR-0676,
DD-910 through DD-913, TVG-0776 through TVG-0779, the repository-owned ABI-1
window selector, exact typed-context profile mapping, checked workspace query,
and backend-specific configuration validators. No external compressor, C API,
large-window selector, memory policy, source code, test, benchmark, patent,
pseudocode, or optimization description was consulted.

### IR-0678

The four-MiB Contextual Dynamic Range CLI boundary uses IR-0673 through
IR-0677, DD-910 through DD-914, TVG-0776 through TVG-0780, the repository-owned
CLI selector, public C initializer/query/factory lifecycle, transactional file
commit, and cross-profile round-trip harness. No external compressor, CLI,
large-window profile, memory policy, source code, test, benchmark, patent,
pseudocode, or optimization description was consulted.

### IR-0679

The four-MiB Contextual Dynamic Range benchmark boundary uses IR-0673 through
IR-0678, DD-910 through DD-915, TVG-0776 through TVG-0781, the repository-owned
dependency-free benchmark lifecycle, public C workspace query/factory, checked
encoded-capacity calculation, and existing measurement schema. No external
compressor, benchmark harness, large-window measurement, memory policy, source
code, test, benchmark result, patent, pseudocode, or optimization description
was consulted.

### IR-0680

The four-MiB Contextual Dynamic Range fuzz boundary uses IR-0673 through
IR-0679, DD-910 through DD-916, TVG-0776 through TVG-0782, the repository-owned
dual-profile decoder harness, public C bounded-workspace query, complete-frame
decoder, sanitizer build route, and permanent malformed-stream regressions.
No external fuzzer harness, compressor, corpus, large-window implementation,
source code, test, vulnerability report, patent, pseudocode, or optimization
description was consulted.

### IR-0681

The four-MiB Contextual Dynamic Range interoperability boundary uses IR-0673
through IR-0680, DD-910 through DD-917, TVG-0776 through TVG-0783, the
repository-owned schema-42 generator/verifier, deterministic 8,193-byte
fixture, explicit CLI selector, manifest hashing, and one-generation
compatibility chain. No external archive, compressor, interoperability suite,
source code, test vector, manifest schema, patent, pseudocode, or optimization
description was consulted.

### IR-0682

The four-MiB Contextual rANS design uses IR-0673 through IR-0681, DD-910
through DD-918, TVG-0776 through TVG-0784, the repository-owned canonical
contextual rANS format/profile, compact model, typed-token layout, HashChain
workspace calculator, and C hard limits. No external compressor, rANS
implementation, large-window format, source code, test, benchmark, patent,
pseudocode, or optimization description was consulted.

### IR-0683

The four-MiB contextual rANS descriptor/model boundary uses IR-0682, DD-919,
TVG-0785, the repository-owned compact model grammar, contextual rANS format,
selected field-context layout, checked serialization, operation encoder, and
scalar decoder. No external rANS implementation, descriptor format, source
code, test, malformed corpus, patent, pseudocode, or optimization description
was consulted.

### IR-0684

The four-MiB contextual rANS decoder boundary uses IR-0682 through IR-0683,
DD-919 through DD-920, TVG-0785 through TVG-0786, the repository-owned Format
2 rANS stream/frame validator, direct typed-token rANS oracle, complete-frame
decoder, typed reconstructor, and checked workspace contracts. No external
compressor, rANS frame format, source code, test vector, malformed corpus,
patent, pseudocode, or optimization description was consulted.

### IR-0685

The four-MiB contextual rANS complete-frame encoder uses IR-0682 through
IR-0684, DD-919 through DD-921, TVG-0785 through TVG-0787, the repository-owned
direct typed-token encoder, HashChain Exact matcher, selected frame validator,
compact descriptor serializer, and complete decoder. No external compressor,
rANS encoder implementation, match finder, source code, test vector, patent,
pseudocode, or optimization description was consulted.

### IR-0686

The four-MiB contextual rANS profile/streaming boundary uses IR-0682 through
IR-0685, DD-919 through DD-922, TVG-0785 through TVG-0788, the repository-owned
profile calculator, native workspace partitioners, HashChain Exact streaming
encoder, atomic streaming decoder, and exact admission enum. No external
compressor, rANS lifecycle, workspace layout, source code, test, patent,
pseudocode, or optimization description was consulted.

### IR-0687

The four-MiB contextual rANS C boundary uses IR-0682 through IR-0686, DD-919
through DD-923, TVG-0785 through TVG-0789, the repository-owned ABI-1 common
window selector, backend-specific config validator, exact profile mapper,
workspace query, factory, and streaming lifecycle. No external C API,
compressor, rANS binding, workspace policy, source code, test, patent,
pseudocode, or optimization description was consulted.

### IR-0688

The four-MiB contextual rANS CLI boundary uses IR-0682 through IR-0687,
DD-919 through DD-924, TVG-0785 through TVG-0790, the repository-owned public
C initializer, requirements query, factory, streaming lifecycle, common CLI
driver, and round-trip harness. No external compressor, rANS command-line
interface, workspace policy, source code, test, benchmark result, patent,
pseudocode, or optimization description was consulted.

### IR-0689

The four-MiB contextual rANS benchmark boundary uses IR-0682 through IR-0688,
DD-919 through DD-925, TVG-0785 through TVG-0791, the repository-owned public
C lifecycle, checked complete-stream capacity calculator, dependency-free
timer/report driver, and benchmark smoke harness. No external compressor,
rANS benchmark implementation, corpus result, source code, test, patent,
pseudocode, or optimization description was consulted.

### IR-0690

The four-MiB contextual rANS bounded fuzz boundary uses IR-0682 through
IR-0689, DD-919 through DD-926, TVG-0785 through TVG-0792, the repository-
owned private complete-frame decoder, public C streaming decoder, fixed table
and token workspaces, finite call loop, and permanent malformed regressions.
No external compressor, rANS fuzzer, corpus, source code, test, vulnerability
report, patent, pseudocode, or optimization description was consulted.

### IR-0691

The four-MiB contextual rANS interoperability boundary uses IR-0682 through
IR-0690, DD-919 through DD-927, TVG-0785 through TVG-0793, the repository-owned
schema-43 generator/verifier, deterministic 8,193-byte fixture, exact CLI
selector, manifest hashing, and one-generation compatibility chain. No
external archive, compressor, interoperability suite, source code, test
vector, manifest schema, patent, pseudocode, or optimization description was
consulted.

### IR-0692

The four-MiB Contextual tANS design uses IR-0673 through IR-0691, DD-910
through DD-928, TVG-0776 through TVG-0794, the repository-owned contextual
tANS format/profile, compact model, selected typed-token layout, fixed table
staging, HashChain workspace calculator, and C hard limits. No external
compressor, tANS implementation, large-window format, source code, test,
benchmark, patent, pseudocode, or optimization description was consulted.

### IR-0693

The four-MiB Contextual tANS descriptor boundary uses IR-0692, DD-929,
TVG-0795, the repository-owned common compact-model grammar, selected
field-context layout, and frozen Contextual tANS descriptor vectors. No
external compressor, tANS implementation, descriptor format, source code,
test, benchmark, patent, pseudocode, or optimization description was
consulted.

### IR-0694

The four-MiB Contextual tANS coding and direct typed-token boundaries use
IR-0693, DD-930, TVG-0796, the repository-owned selected operation coder,
single-state tANS tables, direct typed-LZSS adapter, and two-pass atomic
decoder. No external compressor, tANS implementation, typed-token format,
source code, test, benchmark, patent, pseudocode, or optimization description
was consulted.

### IR-0695

The four-MiB Contextual tANS complete-frame boundary uses IR-0694, DD-931,
TVG-0797, the repository-owned Format 2 stream/frame parser, selected direct
typed-token coder, atomic frame decoder, exhaustive/HashChain Exact encoder,
and checked bounds. No external compressor, tANS frame format, source code,
test, benchmark, patent, pseudocode, or optimization description was
consulted.

### IR-0696

The four-MiB Contextual tANS private profile and streaming boundary uses
IR-0692 through IR-0695, DD-929 through DD-932, TVG-0795 through TVG-0798,
the repository-owned layout-derived profile calculator, native workspace
partitioners, HashChain Exact streaming encoder, atomic streaming decoder, and
exact admission enum. No external compressor, tANS lifecycle, workspace
layout, source code, test, benchmark, patent, pseudocode, or optimization
description was consulted.

### IR-0697

The four-MiB Contextual tANS public C boundary uses IR-0692 through IR-0696,
DD-929 through DD-933, TVG-0795 through TVG-0799, the repository-owned ABI-1
common window selector, backend-specific validator, exact profile/admission
mappers, workspace query, factory, and streaming lifecycle. No external C API,
compressor, tANS binding, workspace policy, source code, test, patent,
pseudocode, or optimization description was consulted.

### IR-0698

The four-MiB Contextual tANS CLI boundary uses IR-0692 through IR-0697,
DD-929 through DD-934, TVG-0795 through TVG-0800, the repository-owned public
C initializer, requirements query, factory, streaming lifecycle, common CLI
driver, profile inventory, and round-trip harness. No external compressor,
tANS command-line interface, workspace policy, source code, test, benchmark
result, patent, pseudocode, or optimization description was consulted.

### IR-0699

The four-MiB Contextual tANS dependency-free benchmark uses IR-0692 through
IR-0698, DD-929 through DD-935, TVG-0795 through TVG-0801, the repository-owned
public C initializer, requirements query, factory, streaming lifecycle,
bounded output-capacity calculator, and benchmark report harness. No external
compressor, tANS benchmark, workspace policy, source code, test result, patent,
pseudocode, or optimization description was consulted.

### IR-0700

The four-MiB Contextual tANS bounded fuzz boundary uses IR-0692 through
IR-0699, DD-929 through DD-936, TVG-0795 through TVG-0802, the repository-owned
private complete-frame decoder, public C streaming decoder, fixed table/token/
raw workspaces, finite call loop, and permanent malformed regressions. No
external compressor, tANS fuzzer, corpus, source code, test, vulnerability
report, patent, pseudocode, or optimization description was consulted.

### IR-0701

The four-MiB Contextual tANS interoperability admission uses IR-0692 through
IR-0700, DD-929 through DD-937, TVG-0795 through TVG-0803, the repository-owned
schema-44 generator and verifier, deterministic interoperability fixture,
exact CLI selector, and legacy conversion chain. No external compressor, tANS
format, interoperability suite, archive, source code, test vector, patent,
pseudocode, or optimization description was consulted.

### IR-0702

The four-MiB Contextual Blocked Huffman staged design uses the repository-owned
Format 2 identity rules, field-context variant-3 layout, canonical contextual
Blocked Huffman grammar, one-MiB selected implementation, checked HashChain
workspace calculator, profile partitioners, and completed four-MiB backend
designs. No external compressor, DEFLATE implementation, Huffman source code,
table layout, test, benchmark result, patent, pseudocode, or optimization
description was consulted.

### IR-0703

The four-MiB Contextual Blocked Huffman descriptor implementation uses
IR-0702, DD-939, TVG-0805, the repository-owned selected-layout dispatcher,
canonical record analyzer/parser/serializer, variant-3 alphabet table, and
atomic descriptor tests. No external compressor, Huffman implementation,
descriptor grammar, source code, test vector, patent, pseudocode, or
optimization description was consulted.

### IR-0704

The four-MiB Contextual Blocked Huffman entropy-operation and direct
typed-token admission uses IR-0703, DD-940, TVG-0806, the repository-owned
immutable field-context layout, generic model builder/writer/decoder, direct
LZSS typed-token composition, and typed-token validation. No external
compressor, Huffman implementation, source code, test vector, patent,
pseudocode, or optimization description was consulted.

### IR-0705

The private four-MiB Contextual Blocked Huffman frame and lifecycle admission
uses IR-0704, DD-941, TVG-0807, repository-owned selected-layout frame
validation, HashChain workspace calculation, profile partitioners, streaming
state machines, and the completed four-MiB tANS lifecycle as an internal
architectural precedent. No external compressor, Huffman implementation,
source code, test vector, patent, pseudocode, or optimization description was
consulted.

### IR-0706

The four-MiB Contextual Blocked Huffman public C and CLI admission uses
IR-0705, DD-942, TVG-0808, the repository-owned ABI-1 common window selector,
backend-specific profile/admission mappers, checked workspace query, factory,
common CLI driver, and exact-profile round-trip harness. No external C API,
compressor, Huffman binding, command-line interface, source code, test vector,
patent, pseudocode, or optimization description was consulted.

### IR-0707

The four-MiB Contextual Blocked Huffman benchmark and bounded-fuzz admission
uses IR-0706, DD-943, TVG-0809, the repository-owned dependency-free public-C
benchmark adapter, selected-layout frame validator, fixed-memory dual-path
fuzz harness, permanent atomicity regressions, and the completed four-MiB
Contextual tANS tooling boundary as an internal precedent. No external
benchmark, fuzzer harness, compressor, Huffman implementation, source code,
test vector, patent, pseudocode, or optimization description was consulted.

### IR-0708

The four-MiB Contextual Blocked Huffman interoperability admission uses
IR-0707, DD-944, TVG-0810, the repository-owned append-only bundle generator,
strict manifest verifier, schema downgrade chain, and prior schema-45
interoperability design as internal precedents. No external archive format,
compressor, Huffman implementation, interoperability suite, source code, test
vector, patent, pseudocode, or optimization description was consulted.

### IR-0709

The four-MiB Contextual Adaptive Huffman staged design uses IR-0708, the
repository-owned selected field-context layout, FGK tree and model-bank types,
267-bit-per-raw-byte proof, HashChain workspace calculator, one-MiB Adaptive
profile, and completed four-MiB backend profiles. No external compressor,
Adaptive Huffman implementation, source code, test vector, patent,
pseudocode, payload bound, or optimization description was consulted.

### IR-0710

The Contextual Adaptive Huffman profile-application helper design uses
IR-0709, the repository-owned ABI-1 size-tagged configuration, initializer,
workspace-query validation, stable status mapping, and explicit CLI profile
policies. No external C API, compression library, configuration helper,
source code, test vector, patent, pseudocode, or optimization description was
consulted.

### IR-0711

The four-MiB Contextual Adaptive Huffman model-bank and checked-profile
admission uses IR-0709 through IR-0710, DD-946 through DD-947, TVG-0812
through TVG-0813, the repository-owned selected field-context layout, generic
FGK model partitioner, HashChain workspace calculator, and checked directional
profile arithmetic. No external compressor, Adaptive Huffman implementation,
source code, test vector, patent, pseudocode, payload bound, or optimization
description was consulted.

### IR-0712

The four-MiB Contextual Adaptive Huffman operation and direct typed-token
admission uses IR-0711, DD-948, TVG-0814, the repository-owned immutable
selected-layout encoder/decoder, FGK model bank, field-context state machine,
typed-token validator, and one-MiB direct-composition tests. No external
compressor, Adaptive Huffman implementation, source code, test vector, patent,
pseudocode, payload bound, or optimization description was consulted.

### IR-0713

The four-MiB Contextual Adaptive Huffman complete-frame, checked-profile, and
streaming admission uses IR-0712, DD-949, TVG-0815, the repository-owned
selected stream/frame validator, HashChain frame encoder, caller-owned profile
partitioners, streaming state machines, and completed one-MiB Adaptive and
four-MiB Blocked Huffman lifecycles as internal architectural precedents. No
external compressor, Adaptive Huffman implementation, source code, test
vector, patent, pseudocode, payload bound, or optimization description was
consulted.

### IR-0714

The four-MiB Contextual Adaptive Huffman public C, CLI, and benchmark
admission uses IR-0713, DD-947 and DD-950, TVG-0813 and TVG-0816, the
repository-owned ABI-1 configuration validation, checked profile calculator,
exact streaming admission, CLI inventory tests, and benchmark-report
contract. No external compressor, C API, Adaptive Huffman implementation,
source code, test vector, patent, pseudocode, payload bound, or optimization
description was consulted.

### IR-0715

The four-MiB Contextual Adaptive Huffman bounded-fuzz admission uses IR-0714,
DD-951, TVG-0817, the repository-owned selected-layout frame validator,
fixed-memory dual-path fuzz harness, permanent atomicity regressions, and the
completed four-MiB Contextual Blocked Huffman fuzz boundary as an internal
precedent. No external fuzzer harness, compressor, Adaptive Huffman
implementation, source code, test vector, patent, pseudocode, payload bound,
or optimization description was consulted.

### IR-0716

The four-MiB Contextual Adaptive Huffman interoperability admission uses
IR-0715, DD-952, TVG-0818, the repository-owned append-only bundle generator,
strict manifest verifier, schema downgrade chain, and prior schema-46
interoperability design as internal precedents. No external archive format,
compressor, Adaptive Huffman implementation, interoperability suite, source
code, test vector, patent, pseudocode, or optimization description was
consulted.

### IR-0717

The common contextual profile-application contract uses IR-0710, DD-951,
TVG-0817, the repository-owned five contextual C configurations, their
size-tagged ABI validation, checked workspace calculators, public tools, and
tests. No external compression library, C API, configuration-helper source,
test vector, patent, pseudocode, payload bound, or optimization description
was consulted.

### IR-0718

The public contextual profile-name normalization uses IR-0717, DD-954,
TVG-0820, the repository-owned header, C adapter, CLI, benchmark, fuzz
harnesses, C tests, and current user-facing documentation. No external
compression library, C API, source code, tests, vectors, patents, pseudocode,
payload bounds, or optimization descriptions were consulted.

### IR-0719

The Contextual Dynamic Range and rANS profile helpers use IR-0717 through
IR-0718, DD-954, TVG-0820, the repository-owned profile calculators, public
initializers, prior Adaptive Huffman private-copy helper, CLI policy values,
and C API tests. No external compression library, C API, source code, tests,
vectors, patents, pseudocode, payload bounds, or optimization descriptions
were consulted.

### IR-0720

The Contextual tANS and Blocked Huffman profile helpers use IR-0717 through
IR-0719, DD-954 through DD-955, TVG-0820 through TVG-0821, the
repository-owned profile calculators, public initializers, prior atomic
helpers, CLI policy values, and pure-C tests. No external compression library,
C API, source code, tests, vectors, patents, pseudocode, payload bounds, or
optimization descriptions were consulted.

### IR-0721

The contextual CLI and benchmark profile migration uses IR-0717 through
IR-0720, DD-954 through DD-956, TVG-0820 through TVG-0822, the repository-owned
public helpers, tool configuration adapters, workspace queries, CLI
round-trips, profile inventories, and benchmark smokes. No external
compression tool, benchmark, C API, source code, tests, vectors, patents,
pseudocode, payload bounds, or optimization descriptions were consulted.

### IR-0722

The reserved 16-MiB contextual LZSS profile uses the repository-owned one-
MiB and four-MiB window designs, typed-token and context contracts, checked
workspace calculators, HashChain workspace formulas, public profile-helper
policy, and private Silesia match-finder evidence. All numeric limits were
derived directly from marc's current token representation and object extents.
No external compression library, source code, tests, vectors, patents,
pseudocode, payload bounds, memory policy, or optimization description was
consulted.

### IR-0723

The shared 16-MiB dictionary/context primitive implementation uses IR-0722,
DD-960, TVG-0824, the repository-owned typed-token validator, field-context
layout and state machine, checked count validation, prior three variant
boundary vectors, and explicit backend admission gates. No external
compression library, source code, tests, vectors, patents, pseudocode, model
layout, or optimization description was consulted.

### IR-0724

The private 16-MiB Dynamic Range decoder stage uses IR-0722 through IR-0723,
DD-960 through DD-961, TVG-0824 through TVG-0825, marc's existing four-MiB
decoder-only staging pattern, typed-context frame preflight, contextual range
decoder, typed-token reconstruction, and atomic workspace tests. No external
compression library, source code, tests, vectors, patents, pseudocode, format
layout, or optimization description was consulted.

### IR-0725

The private 16-MiB Dynamic Range producing and streaming lifecycle uses
IR-0722 through IR-0724, DD-960 through DD-962, TVG-0824 through TVG-0826,
marc's existing four-MiB lifecycle, selected-layout workspace arithmetic,
transactional stream-header serializer, HashChain encoder, and chunked stream
state machines. No external compression library, source code, tests, vectors,
patents, pseudocode, format layout, workspace formula, or optimization
description was consulted.

### IR-0726

The public 16-MiB Dynamic Range C selector stage uses IR-0722 through
IR-0725, DD-960 through DD-963, TVG-0824 through TVG-0827, marc's existing
profile-helper contract, public configuration loader, and authoritative
workspace queries. The selector value, payload and model ceilings, exact
aggregates, and one-GiB policy are derived from the repository-owned private
lifecycle. No external compression library, C API, source code, tests,
vectors, patents, pseudocode, resource policy, or optimization description
was consulted.

### IR-0727

The 16-MiB Dynamic Range CLI boundary uses IR-0722 through IR-0726, DD-960
through DD-964, TVG-0824 through TVG-0828, the repository-owned explicit
contextual CLI selector family, public profile helper, authoritative workspace
query, transactional file commit, and cross-profile round-trip harness. No
external compressor, CLI, large-window profile, memory policy, source code,
test, benchmark, patent, pseudocode, or optimization description was
consulted.

### IR-0728

The 16-MiB Dynamic Range benchmark boundary uses IR-0722 through IR-0727,
DD-960 through DD-965, TVG-0824 through TVG-0829, the repository-owned
dependency-free benchmark lifecycle, public profile helper and workspace
query, checked complete-stream capacity, and report validator. No external
compressor, benchmark harness, large-window measurement, memory policy,
source code, test, benchmark result, patent, pseudocode, or optimization
description was consulted.

### IR-0729

The 16-MiB Contextual Dynamic Range bounded fuzz boundary uses IR-0722 through
IR-0728, DD-960 through DD-966, TVG-0824 through TVG-0830, the repository-owned
fixed-array dual-decoder harness, public C workspace query, complete-frame
decoder, sanitizer build route, and permanent malformed-stream regressions.
No external fuzzer harness, compressor, corpus, large-window implementation,
source code, test, vulnerability report, patent, pseudocode, or optimization
description was consulted.

### IR-0730

The 16-MiB Contextual Dynamic Range interoperability boundary uses IR-0722
through IR-0729, DD-960 through DD-967, TVG-0824 through TVG-0831, the
repository-owned append-only bundle generator, exact-order verifier,
byte-identical re-encoder, and schema downgrade harness. No external archive,
compressor, manifest, interoperability suite, source code, test, patent,
pseudocode, or optimization description was consulted.

### IR-0731

The 16-MiB canonical Contextual rANS design uses IR-0722 through IR-0730,
DD-960 through DD-968, TVG-0824 through TVG-0832, the repository-owned
16-MiB typed-token/context layout, canonical scalar rANS variant 3, compact
descriptor grammar, checked workspace queries, and completed 4-MiB rANS and
16-MiB Dynamic Range lifecycle evidence. No external compressor, rANS source
code, large-window model, memory policy, test, benchmark, patent, pseudocode,
or optimization description was consulted.

### IR-0732

The 16-MiB canonical Contextual rANS compact descriptor/model implementation
uses IR-0731, DD-969, TVG-0833, the repository-owned compact model grammar,
context-variant layouts, checked descriptor serializer/parser, and atomic
format tests. No external compressor, rANS implementation, source code, test
suite, large-window model, patent, pseudocode, or optimization description was
consulted.

### IR-0733

The 16-MiB canonical Contextual rANS private stream/header and complete-frame
decoder admission uses IR-0731 through IR-0732, DD-969 through DD-970,
TVG-0833 through TVG-0834, the repository-owned exact-pair validator, frame
preflight, direct typed-token rANS encoder used only to construct a decoder
fixture, complete-frame decoder, and raw reconstruction path. No external
compressor, rANS implementation, source code, test suite, large-window model,
patent, pseudocode, optimization description, or encoded vector was
consulted.

### IR-0734

The 16-MiB canonical Contextual rANS complete-frame encoder admission uses
IR-0731 through IR-0733, DD-969 through DD-971, TVG-0833 through TVG-0835,
the repository-owned typed-token exhaustive and HashChain Exact encoders,
canonical scalar rANS encoder, compact descriptor serializer, complete-frame
decoder, and crossed-profile validator. No external compressor, rANS
implementation, match finder, source code, test suite, encoded vector, patent,
pseudocode, or optimization description was consulted.

### IR-0735

The 16-MiB canonical Contextual rANS private profile and streaming lifecycle
uses IR-0731 through IR-0734, DD-969 through DD-972, TVG-0833 through
TVG-0836, the repository-owned checked profile arithmetic, HashChain Exact
workspace query, streaming encoder/decoder, explicit decoder admission enum,
and one-byte chunk helpers. No external compressor, rANS implementation,
streaming framework, workspace policy, source code, test suite, patent,
pseudocode, or optimization description was consulted.

### IR-0736

The 16-MiB Contextual rANS public C boundary uses IR-0731 through IR-0735,
DD-969 through DD-973, TVG-0833 through TVG-0837, the repository-owned
profile helper contract, checked workspace query and partitioning, public C
factory, and Dynamic Range 16-MiB C-boundary precedent. No external
compressor, rANS implementation, C wrapper, source code, test suite, patent,
pseudocode, or optimization description was consulted.

### IR-0737

The 16-MiB Contextual rANS CLI boundary uses IR-0731 through IR-0736,
DD-969 through DD-974, TVG-0833 through TVG-0838, the repository-owned
transactional CLI enum/parser/help/dispatch structure, public C profile helper,
and common CLI round-trip harness. No external compressor, rANS CLI, wrapper,
source code, archive, test suite, patent, pseudocode, or optimization
description was consulted.

### IR-0738

The 16-MiB Contextual rANS dependency-free benchmark boundary uses IR-0731
through IR-0737, DD-969 through DD-975, TVG-0833 through TVG-0839, the
repository-owned benchmark profile selectors, checked capacity arithmetic,
public C profile/workspace/factory lifecycle, and common report validator. No
external compressor, rANS benchmark, framework, source code, corpus result,
test suite, patent, pseudocode, or optimization description was consulted.

### IR-0739

The 16-MiB Contextual rANS bounded decoder-fuzz admission uses IR-0731 through
IR-0738, DD-969 through DD-976, TVG-0833 through TVG-0840, the repository-
owned fixed-array dual-path harness, public decoder lifecycle, canonical small-
frame generator, malformed regressions, and established Clang sanitizer
workflow. No external compressor, rANS fuzzer, corpus, source code, test suite,
patent, pseudocode, or vulnerability description was consulted.

### IR-0740

The 16-MiB Contextual rANS interoperability admission uses IR-0731 through
IR-0739, DD-969 through DD-977, TVG-0833 through TVG-0841, the repository-
owned schema-48 manifest order, exact CLI identity checks, bundle generator,
verifier, and compatibility converter. No external compressor, rANS
implementation, archive, encoded vector, source code, test suite, patent,
pseudocode, or interoperability implementation was consulted.

### IR-0741

The 16-MiB Contextual tANS design uses the repository-owned shared dictionary
variant 5/context variant 4 contract, completed four-MiB contextual tANS
lifecycle, compact-model grammar, tANS table definitions, HashChain workspace
query, and completed 16-MiB Dynamic Range and rANS proofs. No external
compressor, tANS or FSE implementation, source code, archive, encoded vector,
test suite, patent, pseudocode, benchmark result, or optimization description
was consulted.

### IR-0742

The 16-MiB Contextual tANS descriptor/model implementation uses IR-0741,
DD-979, TVG-0843, the repository-owned context-variant-4 layout, compact-model
grammar, and completed four-MiB contextual tANS descriptor. No external
compressor, tANS or FSE implementation, source code, archive, encoded vector,
test suite, patent, pseudocode, benchmark result, or optimization description
was consulted.

### IR-0743

The 16-MiB Contextual tANS operation and typed-token coding proof uses
IR-0741 through IR-0742, DD-979 through DD-980, TVG-0843 through TVG-0844,
the repository-owned selected-layout encoder/decoder paths, and typed-LZSS
context model. No external compressor, tANS or FSE implementation, source
code, archive, encoded vector, test suite, patent, pseudocode, benchmark
result, or optimization description was consulted.

### IR-0744

The 16-MiB Contextual tANS private stream/header and complete-frame decoder
admission uses IR-0741 through IR-0743, DD-979 through DD-981, TVG-0843
through TVG-0845, the repository-owned four-MiB tANS frame format and decoder,
and the preceding 16-MiB Contextual rANS staged admission pattern. No external
compressor, tANS or FSE implementation, source code, archive, encoded vector,
test suite, patent, pseudocode, benchmark result, optimization description, or
malformed-stream corpus was consulted.

### IR-0745

The 16-MiB Contextual tANS complete-frame encoder admission uses IR-0741
through IR-0744, DD-979 through DD-982, TVG-0843 through TVG-0846, the
repository-owned exhaustive and HashChain Exact typed-token encoders,
contextual tANS descriptor/payload encoder, complete-frame decoder, and
crossed-profile validator. No external compressor, tANS or FSE implementation,
match finder, source code, archive, encoded vector, test suite, patent,
pseudocode, benchmark result, or optimization description was consulted.

### IR-0746

The 16-MiB Contextual tANS profile and streaming admission uses IR-0741
through IR-0745, DD-979 through DD-983, TVG-0843 through TVG-0847, the
repository-owned profile arithmetic, HashChain Exact workspace query, and
one-byte streaming lifecycle established by the earlier contextual tANS
profiles. No external compressor, tANS or FSE implementation, match finder,
source code, archive, encoded vector, test suite, patent, pseudocode,
benchmark result, optimization description, or malformed-stream corpus was
consulted.

### IR-0747

The 16-MiB Contextual tANS public C admission uses IR-0741 through IR-0746,
DD-979 through DD-984, TVG-0843 through TVG-0848, the repository-owned common
profile selector and helper contract, checked workspace query, streaming
factory, and earlier contextual rANS public admission pattern. No external
compressor, tANS or FSE implementation, source code, archive, encoded vector,
test suite, patent, pseudocode, benchmark result, optimization description, or
malformed-stream corpus was consulted.

### IR-0748

The `lzss-contextual-tans-16m` CLI and benchmark admission uses IR-0741 through
IR-0747, DD-979 through DD-985, TVG-0843 through TVG-0849, marc's existing
transactional CLI harness, dependency-free benchmark reporter, and the public
Contextual tANS profile/query/factory lifecycle. No external compressor, tANS
or FSE implementation, source code, archive, encoded vector, test suite,
patent, pseudocode, benchmark result, optimization description, or malformed-
stream corpus was consulted.

### IR-0749

The bounded 16-MiB Contextual tANS fuzzing admission uses IR-0741 through
IR-0748, DD-979 through DD-986, TVG-0843 through TVG-0850, marc's existing
dual-path Contextual tANS decoder harness, fixed workspace, and corresponding
Contextual rANS 16-MiB admission pattern. No external compressor, tANS or FSE
implementation, source code, archive, encoded vector, test suite, patent,
pseudocode, benchmark result, optimization description, or malformed-stream
corpus was consulted.

### IR-0750

The 16-MiB Contextual tANS interoperability admission uses IR-0741 through
IR-0749, DD-979 through DD-987, TVG-0843 through TVG-0851, marc's append-only
interoperability manifest, and its schema-reduction compatibility harness. No
external compressor, tANS or FSE implementation, interoperability suite,
archive, source code, encoded vector, test suite, patent, pseudocode,
benchmark result, optimization description, or malformed-stream corpus was
consulted.

### IR-0751

The 16-MiB Contextual Blocked Huffman design uses the repository-owned
four-MiB Contextual Blocked Huffman format, shared dictionary variant 5 and
context variant 4, canonical Huffman primitives, HashChain Exact workspace
query, and checked profile arithmetic. No external compressor, Huffman
implementation, source code, archive, encoded vector, test suite, patent,
pseudocode, benchmark result, optimization description, or malformed-stream
corpus was consulted.

### IR-0752

The 16-MiB Contextual Blocked Huffman descriptor stage uses IR-0751, DD-989,
TVG-0853, marc's existing atomic canonical descriptor parser/serializer, and
the repository-owned context-variant-4 field layout. No external compressor,
Huffman implementation, source code, archive, encoded vector, test suite,
patent, pseudocode, benchmark result, optimization description, or malformed-
stream corpus was consulted.

### IR-0753

The 16-MiB Contextual Blocked Huffman operation/direct-token stage uses
IR-0751 through IR-0752, DD-989 through DD-990, TVG-0853 through TVG-0854,
marc's immutable selected field layout, generic Contextual Blocked Huffman
model/writer/decoder, and direct typed-token adapters. No external compressor,
Huffman implementation, source code, archive, encoded vector, test suite,
patent, pseudocode, benchmark result, optimization description, or malformed-
stream corpus was consulted.

### IR-0754

The decode-only 16-MiB Contextual Blocked Huffman frame stage uses IR-0751
through IR-0753, DD-989 through DD-991, TVG-0853 through TVG-0855, marc's
bounded frame preflight, direct two-pass typed-token decoder, and the earlier
16-MiB contextual rANS/tANS decoder admission pattern. No external compressor,
Huffman implementation, source code, archive, encoded vector, test suite,
patent, pseudocode, benchmark result, optimization description, or malformed-
stream corpus was consulted.

### IR-0755

The bounded-encoder 16-MiB Contextual Blocked Huffman frame stage uses IR-0751
through IR-0754, DD-989 through DD-992, TVG-0853 through TVG-0856, marc's
existing exact frame planner/serializer, authoritative HashChain workspace
query, and earlier four-MiB bounded-encoder tests. No external compressor,
Huffman implementation, source code, archive, encoded vector, test suite,
patent, pseudocode, benchmark result, optimization description, or malformed-
stream corpus was consulted.

### IR-0756

The 16-MiB Contextual Blocked Huffman checked-profile and streaming stage uses
IR-0751 through IR-0755, DD-989 through DD-993, TVG-0853 through TVG-0857,
marc's existing profile arithmetic, workspace partitioners, streaming state
machines, and completed 16-MiB contextual rANS/tANS profile pattern. No
external compressor, Huffman implementation, source code, archive, encoded
vector, test suite, patent, pseudocode, benchmark result, optimization
description, or malformed-stream corpus was consulted.

### IR-0757

The public-C 16-MiB Contextual Blocked Huffman stage uses IR-0751 through
IR-0756, DD-989 through DD-994, TVG-0853 through TVG-0858, the repository's
common contextual profile selector and atomic helper contract, the already-
admitted private profile/workspace calculations, and the explicit streaming
admission. No external compressor, Huffman implementation, source code,
archive, encoded vector, test suite, patent, pseudocode, benchmark result,
optimization description, or malformed-stream corpus was consulted.

### IR-0758

The 16-MiB Contextual Blocked Huffman CLI and dependency-free benchmark stage
uses IR-0751 through IR-0757, DD-989 through DD-995, TVG-0853 through
TVG-0859, marc's public profile helper/workspace lifecycle, common
transactional CLI harness, and common benchmark report harness. No external
compressor, Huffman implementation, source code, archive, encoded vector,
test suite, patent, pseudocode, benchmark result, optimization description,
or malformed-stream corpus was consulted.

### IR-0759

The 16-MiB Contextual Blocked Huffman bounded-fuzzing stage uses IR-0751
through IR-0758, DD-989 through DD-996, TVG-0853 through TVG-0860, marc's
existing fixed-memory dual-path fuzz harness, public profile helper, private
complete-frame decoder, and permanent malformed-stream regression patterns.
No external compressor, Huffman implementation, source code, archive, encoded
vector, test suite, patent, pseudocode, benchmark result, optimization
description, or malformed-stream corpus was consulted.

### IR-0760

The 16-MiB Contextual Blocked Huffman interoperability admission uses IR-0751
through IR-0759, DD-989 through DD-997, TVG-0853 through TVG-0861, marc's
append-only interoperability manifest, and its schema-reduction compatibility
harness. No external compressor, Huffman implementation, interoperability
suite, archive, source code, encoded vector, test suite, patent, pseudocode,
benchmark result, optimization description, or malformed-stream corpus was
consulted.

### IR-0761

The 16-MiB Contextual Adaptive Huffman design uses the repository-owned
four-MiB Contextual Adaptive Huffman representation, shared dictionary variant
5 and context variant 4, FGK model-bank primitives, HashChain Exact workspace
query, and checked profile arithmetic. No external compressor, Adaptive
Huffman implementation, source code, archive, encoded vector, test suite,
patent, pseudocode, benchmark result, optimization description, or malformed-
stream corpus was consulted.

### IR-0762

The 16-MiB Contextual Adaptive Huffman model-bank and descriptor-limit stage
uses IR-0761, DD-999, TVG-0863, marc's existing selected-layout FGK model bank,
fixed 16-byte descriptor validator, and repository-owned context-variant-4
layout. No external compressor, Adaptive Huffman implementation, source code,
archive, encoded vector, test suite, patent, pseudocode, benchmark result,
optimization description, or malformed-stream corpus was consulted.

### IR-0763

The 16-MiB Contextual Adaptive Huffman operation/direct-token stage uses
IR-0761 through IR-0762, DD-999 through DD-1000, TVG-0863 through TVG-0864,
marc's selected-layout FGK operation coder, direct two-pass typed-token
adapter, and repository-owned overlap-built history pattern. No external
compressor, Adaptive Huffman implementation, source code, archive, encoded
vector, test suite, patent, pseudocode, benchmark result, optimization
description, or malformed-stream corpus was consulted.

### IR-0764

The bounded 16-MiB Contextual Adaptive Huffman frame-decoder admission uses
IR-0761 through IR-0763, DD-999 through DD-1001, TVG-0863 through TVG-0865,
marc's existing selected-layout frame validation, direct typed-token coder,
and repository-owned overlap-history construction. No external compressor,
Adaptive Huffman implementation, source code, archive, encoded vector, test
suite, patent, pseudocode, benchmark result, optimization description, or
malformed-stream corpus was consulted.

### IR-0765

The bounded 16-MiB Contextual Adaptive Huffman frame-encoder admission uses
IR-0761 through IR-0764, DD-999 through DD-1002, TVG-0863 through TVG-0866,
marc's generic bounded frame encoder, HashChain Exact match finder, selected
FGK model bank, and repository-owned marker-gap input pattern. No external
compressor, Adaptive Huffman implementation, source code, archive, encoded
vector, test suite, patent, pseudocode, benchmark result, optimization
description, or malformed-stream corpus was consulted.

### IR-0766

The private 16-MiB Contextual Adaptive Huffman profile/streaming stage uses
IR-0761 through IR-0765, DD-999 through DD-1003, TVG-0863 through TVG-0867,
marc's checked profile arithmetic, typed workspace partitioners, bounded frame
codec, and one-byte streaming harness. No external compressor, Adaptive
Huffman implementation, source code, archive, encoded vector, test suite,
patent, pseudocode, benchmark result, optimization description, or malformed-
stream corpus was consulted.

### IR-0767

The public 16-MiB Contextual Adaptive Huffman C lifecycle uses IR-0761 through
IR-0766, DD-999 through DD-1004, TVG-0863 through TVG-0868, marc's existing
common profile helper contract, ABI-1 configuration surface, authoritative
workspace queries, and explicit streaming admission. No external compressor,
Adaptive Huffman implementation, source code, archive, encoded vector, test
suite, patent, pseudocode, benchmark result, optimization description, or
malformed-stream corpus was consulted.

### IR-0768

The 16-MiB Contextual Adaptive Huffman application boundary uses IR-0761
through IR-0767, DD-999 through DD-1005, TVG-0863 through TVG-0869, marc's
existing explicit-profile CLI and dependency-free benchmark adapters, the
public profile helper, authoritative workspace queries, and generic checked
complete-stream capacity logic. No external compressor, Adaptive Huffman
implementation, source code, archive, encoded vector, test suite, patent,
pseudocode, benchmark result, optimization description, or malformed-stream
corpus was consulted.

### IR-0769

The 16-MiB Contextual Adaptive Huffman bounded-fuzz admission uses IR-0761
through IR-0768, DD-999 through DD-1006, TVG-0863 through TVG-0870, marc's
existing fixed-memory dual-path harness, public profile lifecycle, private
complete-frame decoder, and four-profile Blocked Huffman fuzz pattern. No
external fuzzer harness, compressor, Adaptive Huffman implementation, source
code, archive, encoded vector, test suite, patent, pseudocode, benchmark
result, optimization description, or malformed-stream corpus was consulted.
