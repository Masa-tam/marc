# Fuzzing

The twenty-one bounded targets cover standalone LZ77, LZSS, LZ78, LZW, LZD,
LZMW, Blocked Huffman, Adaptive Huffman, Dynamic Range, rANS, and tANS, plus
the composed LZ77 plus Blocked Huffman, LZ77 plus Adaptive Huffman, LZSS plus
Blocked Huffman, LZSS plus Adaptive Huffman, LZ78 plus
Blocked Huffman, LZ78 plus Adaptive Huffman, LZW plus Blocked Huffman, LZD plus
Blocked Huffman, LZMW plus
Blocked Huffman, and checksum-raw profiles. Targets
exercise their public frame-streaming decoder with chunk sizes derived from the
input and also use a strict one-shot decoder where that internal helper exists.
They use small fixed
local limits and caller-owned workspaces so arbitrary inputs cannot request
unbounded allocation. A call-count guard turns a stalled state machine into a
reproducible failure. The standalone LZ78 target additionally bounds its phrase
table to 512 records. The LZW target permits at most width 10 and bounds its
phrase table to 768 records. The LZD target bounds its phrase table to 512
records and its iterative expansion stack to 513 entries. The LZMW target
bounds its phrase table to 1024 records and its iterative expansion stack to
1025 entries. The combined LZ77 plus Blocked Huffman target additionally
truncates every supplied
case to 8 KiB, permits at most 4 KiB total output, one 1 KiB frame, 4 KiB of
dictionary bytes, and eight entropy blocks, and includes all four frame-local
workspace extents in one fixed aggregate limit.
The combined LZ77 plus Adaptive Huffman target caps input at 8 KiB, total raw
output at 4 KiB, one raw frame at 1 KiB, canonical token staging at 16 KiB,
and compressed payload at 8 KiB. It always exercises incremental decoding and,
after a valid exact profile prefix, also invokes complete-frame private-staging
decode over the remaining extent. Both paths use fixed arrays and the common
input-derived chunk and call-ceiling policy.
The combined LZSS plus Adaptive Huffman target uses the same dual-decoder and
call-ceiling structure with the exact LZSS `2F` token bound: 8 KiB supplied
input, 4 KiB total output, 1 KiB raw frames, 2 KiB canonical token staging,
and 8 KiB compressed payload. No workspace is derived from input-controlled
metadata.
The combined LZSS plus Blocked Huffman target uses the same fixed byte, block,
view, and call-count bounds while exercising variable-length LZSS token
validation after entropy decoding.
The combined LZ78 plus Blocked Huffman target caps supplied input at 8 KiB,
raw output at 4 KiB, a frame at 1 KiB, token and compressed payloads at 4 KiB,
entropy views at eight records, and LZ78 phrases at 512 records. Its aggregate
limit includes encoded-frame storage, token staging, raw staging, block views,
and phrase records. Byte-derived input and output chunks plus a fixed call
ceiling exercise the public incremental decoder without input-controlled
allocation.
The combined LZ78 plus Adaptive Huffman target uses the dual exact-frame and
incremental-decoder structure. It fixes supplied input at 8 KiB, total raw
output at 4 KiB, one raw frame at 1 KiB, canonical token staging and compressed
payload at 8 KiB each, and the LZ78 phrase table at 1,024 records. The aggregate
limit includes all byte regions and phrase records before input metadata is
accepted.
The combined LZW plus Blocked Huffman target uses the same byte, block, and
call-count bounds. Its 4 KiB packed-code cap permits at most 3,639 phrase
records, while a 4,096-entry local dictionary limit admits serialized widths
through 12 bits. All encoded-frame, staging, decoded-frame, block-view, phrase,
and final-output storage remains fixed before processing.
The combined LZD plus Blocked Huffman target additionally fixes 512 phrase
records and 513 iterative expansion references from the one-KiB raw-frame
limit. Its aggregate cap includes encoded-frame storage, four-KiB token
staging, transactional raw storage, eight entropy views, phrase records, and
expansion references before processing begins. Input is truncated to eight KiB
and byte-derived partial I/O is guarded by the common fixed call ceiling.
The combined LZMW plus Blocked Huffman target caps input at 8 KiB, total output
at 4 KiB, each frame at 1 KiB, canonical references and compressed payload at
4 KiB, and entropy metadata at eight block views. The maximum token extent
admits 1,023 LZMW phrase records and 1,024 iterative expansion references;
these, encoded-frame storage, reference staging, raw staging, and output are
fixed arrays counted in one aggregate policy before serialized input is read.
Byte-derived partial I/O uses the common fixed call ceiling.
The raw-checksum target exercises both its strict two-pass decoder and its
incremental decoder with at most 8 KiB of serialized input, 4 KiB of output,
1 KiB frames, and 4 KiB of internal-buffer allowance. The incremental path uses
one-byte chunks and a fixed iteration ceiling; neither path performs
input-controlled allocation.

`marc_fuzz_lz77_stream` separately covers the entropy-None LZ77 profile. It
uses at most 8 KiB of input, 4 KiB of output and canonical token payload,
1 KiB frames, fixed frame arrays, byte-derived chunking, and the common checked
call ceiling. This reaches standalone prefix and payload branches absent from
the combined entropy target.

`marc_fuzz_adaptive_huffman_stream` applies the same dual-decoder structure to
the framed FGK profile. It caps input at 8 KiB, output and frame-local buffered
bytes at 4 KiB, and individual frames at 1 KiB. Fixed arrays hold the encoded
frame, decoded frame, and total output; byte-derived chunk sizes and a checked
call ceiling exercise partial I/O without input-controlled allocation.

`marc_fuzz_dynamic_range_stream` covers the corresponding one-shot and
incremental range-decoder paths with the same byte and workspace bounds. It
also fixes the accepted adaptive model total to 32,768 so malformed descriptors
cannot enlarge model policy. Fixed arrays, byte-derived chunks, and the same
checked call ceiling retain bounded execution.

`marc_fuzz_rans_stream` additionally bounds block-controlled metadata. It uses
at most eight fixed `RansBlockView` records, 256-symbol blocks, a 4,096-entry
table cap, 8 KiB of descriptor-plus-payload buffering, and the common input,
output, frame, payload, chunking, and call-count limits. Both decoder paths use
the same fixed views and byte arrays.

`marc_fuzz_tans_stream` applies those same block, view, table, byte, chunking,
and call-count limits to the tabled ANS decoder paths. It thereby exercises
malformed state transitions and additional-bit traversal without deriving
workspace size from serialized metadata.

`marc_fuzz_blocked_huffman_stream` covers the standalone dictionary-none
profile that the combined target cannot select. It uses eight fixed block
views, 256-symbol blocks, code length 24, a 512-node decode-table cap, and the
same byte, chunking, and call-count limits as the ANS targets. Both canonical
and raw block paths remain bounded by caller-owned arrays.

Build fuzzers in a separate Clang build using the GNU-style driver. The fuzz
option instruments the complete static marc library with libFuzzer, AddressSanitizer,
and UndefinedBehaviorSanitizer:

```console
cmake -S . -B out/build/fuzz -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DMARC_BUILD_SHARED=OFF -DMARC_BUILD_STATIC=ON \
  -DMARC_BUILD_TESTS=OFF -DMARC_BUILD_TOOLS=OFF \
  -DMARC_BUILD_EXAMPLES=OFF -DMARC_BUILD_FUZZERS=ON
cmake --build out/build/fuzz --target \
  marc_fuzz_lz77_stream \
  marc_fuzz_lzss_stream \
  marc_fuzz_lz77_blocked_huffman_stream \
  marc_fuzz_lz77_adaptive_huffman_stream \
  marc_fuzz_lzss_adaptive_huffman_stream \
  marc_fuzz_lz78_adaptive_huffman_stream \
  marc_fuzz_lzss_blocked_huffman_stream \
  marc_fuzz_lz78_blocked_huffman_stream \
  marc_fuzz_lzw_blocked_huffman_stream \
  marc_fuzz_lzd_blocked_huffman_stream \
  marc_fuzz_lzmw_blocked_huffman_stream \
  marc_fuzz_checksum_raw_stream \
  marc_fuzz_adaptive_huffman_stream \
  marc_fuzz_dynamic_range_stream \
  marc_fuzz_rans_stream \
  marc_fuzz_tans_stream \
  marc_fuzz_blocked_huffman_stream \
  marc_fuzz_lz78_stream marc_fuzz_lzw_stream \
  marc_fuzz_lzd_stream marc_fuzz_lzmw_stream
out/build/fuzz/marc_fuzz_lz77_stream fuzz/corpus/lz77_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzss_stream fuzz/corpus/lzss_stream -max_len=8192
out/build/fuzz/marc_fuzz_lz77_blocked_huffman_stream \
  fuzz/corpus/lz77_blocked_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lz77_adaptive_huffman_stream \
  fuzz/corpus/lz77_adaptive_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzss_adaptive_huffman_stream \
  fuzz/corpus/lzss_adaptive_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lz78_adaptive_huffman_stream \
  fuzz/corpus/lz78_adaptive_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzss_blocked_huffman_stream \
  fuzz/corpus/lzss_blocked_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lz78_blocked_huffman_stream \
  fuzz/corpus/lz78_blocked_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzw_blocked_huffman_stream \
  fuzz/corpus/lzw_blocked_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzd_blocked_huffman_stream \
  fuzz/corpus/lzd_blocked_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzmw_blocked_huffman_stream \
  fuzz/corpus/lzmw_blocked_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_checksum_raw_stream \
  fuzz/corpus/checksum_raw_stream -max_len=8192
out/build/fuzz/marc_fuzz_adaptive_huffman_stream \
  fuzz/corpus/adaptive_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_dynamic_range_stream \
  fuzz/corpus/dynamic_range_stream -max_len=8192
out/build/fuzz/marc_fuzz_rans_stream \
  fuzz/corpus/rans_stream -max_len=8192
out/build/fuzz/marc_fuzz_tans_stream \
  fuzz/corpus/tans_stream -max_len=8192
out/build/fuzz/marc_fuzz_blocked_huffman_stream \
  fuzz/corpus/blocked_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lz78_stream fuzz/corpus/lz78_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzw_stream fuzz/corpus/lzw_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzd_stream fuzz/corpus/lzd_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzmw_stream fuzz/corpus/lzmw_stream -max_len=8192
```

On Windows, the ASan runtime found by the dynamic loader must come from the
same Clang toolchain that linked the fuzzer. If an environment setup script
places a different compiler generation's runtime earlier in `PATH`, startup
may fail before libFuzzer can print diagnostics; put the selected Clang
runtime directory first for the campaign process.

On Windows, this configuration selects the static C runtime required by the
Clang libFuzzer runtime. Before executing a fuzzer, add Clang's sanitizer
runtime directory to `PATH`; it is the `lib/windows` directory below the path
reported by `clang++ --print-resource-dir`. This makes the dynamic AddressSanitizer
runtime discoverable without recording a machine-specific compiler path in the
repository.

MSVC remains the reference normal-build toolchain, but its native driver is not
used for this libFuzzer target. Ordinary test builds compile the harness as an
object target, catching portable C++ errors without requiring a fuzz runtime.

A bounded Windows smoke campaign on 2026-07-16 ran each of the six targets for
10,000 inputs with `-max_len=8192`, `-timeout=5`, and `-rss_limit_mb=512`.
All 60,000 executions completed without a crash, hang, or sanitizer finding;
each process peaked at 64 MiB RSS. This is execution-path evidence only, not a
claim of coverage completion.

The raw-checksum target received an initial bounded sanitizer smoke on
2026-07-16: 1,000 inputs, 8 KiB maximum input, five-second per-input timeout,
and 512 MiB RSS limit. It completed without a crash, hang, or sanitizer finding
and peaked at 37 MiB RSS. Automatically generated reductions were discarded;
only the reviewed hand-authored seed remains in the repository.

After adding the incremental decoder path on 2026-07-16, the same bounded
1,000-input sanitizer smoke again completed without a crash, hang, or sanitizer
finding at 37 MiB peak RSS. Generated reductions were again discarded.

The Adaptive Huffman dual-decoder target received its initial bounded sanitizer
smoke on 2026-07-17: 1,000 inputs, 8 KiB maximum input, five-second per-input
timeout, and 512 MiB RSS limit. It completed without a crash, hang, or sanitizer
finding and peaked at 37 MiB RSS. Mutations remained in the disposable build
corpus; the repository retains only the reviewed five-byte seed.

The Dynamic Range dual-decoder target received the same bounded 1,000-input
sanitizer smoke on 2026-07-17. With an 8 KiB maximum input, five-second timeout,
and 512 MiB RSS limit, it completed without a crash, hang, or sanitizer finding
and peaked at 37 MiB RSS. Generated mutations remained outside the source
corpus.

The rANS dual-decoder target received the same bounded 1,000-input sanitizer
smoke on 2026-07-17. With the eight-view and 4,096-entry table caps, it completed
without a crash, hang, or sanitizer finding and peaked at 37 MiB RSS. Generated
mutations remained in the disposable build corpus.

The tANS dual-decoder target received the same bounded 1,000-input sanitizer
smoke on 2026-07-17. With eight fixed views and the 4,096-state table cap, it
completed without a crash, hang, or sanitizer finding and peaked at 37 MiB RSS.
Generated mutations remained in the disposable build corpus.

The standalone Blocked Huffman dual-decoder target received the same bounded
1,000-input sanitizer smoke on 2026-07-17. With eight fixed views, code length
24, and the 512-node table cap, it completed without a crash, hang, or sanitizer
finding and peaked at 37 MiB RSS. Mutations remained in the disposable build
corpus.

The standalone LZ77 dual-decoder target received the same bounded 1,000-input
sanitizer smoke on 2026-07-17. With fixed frame arrays and the documented byte
limits, it completed without a crash, hang, or sanitizer finding and peaked at
37 MiB RSS. Mutations remained in the disposable build corpus.

The composed LZSS plus Blocked Huffman dual-decoder target received a bounded
10,000-input sanitizer smoke on 2026-07-18 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 64 MiB RSS. Generated mutations remained in the disposable build
corpus; the repository retains only the reviewed five-byte seed.

The composed LZW plus Blocked Huffman decoder target received a bounded
1,000-input sanitizer smoke on 2026-07-18 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 37 MiB RSS. Generated mutations remained in the disposable build
corpus; the repository retains only the reviewed five-byte seed.

The composed LZD plus Blocked Huffman decoder target received a bounded
1,000-input sanitizer smoke on 2026-07-18 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 37 MiB RSS. Generated mutations remain only in the ignored build
workspace; the repository retains the reviewed five-byte truncated-magic seed.

The composed LZMW plus Blocked Huffman decoder target received a bounded
1,000-input sanitizer smoke on 2026-07-18 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 37 MiB RSS. Generated mutations remain only in the ignored build
workspace; the repository retains the reviewed five-byte truncated-magic seed.

The composed LZ77 plus Adaptive Huffman frame/stream decoder target received a
bounded 1,000-input sanitizer smoke on 2026-07-19 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 37 MiB RSS. Generated mutations remain only in the ignored build
workspace; the repository retains the reviewed five-byte truncated-magic seed.

The composed LZSS plus Adaptive Huffman frame/stream decoder target received a
bounded 1,000-input sanitizer smoke on 2026-07-19 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 37 MiB RSS. Generated mutations remain only in the ignored build
workspace; the repository retains the reviewed five-byte truncated-magic seed.

Do not treat a disappearing crash as sufficient. Minimize each finding, add the
smallest input or an equivalent explicit assertion to a permanent GoogleTest
regression, record the stable error/atomicity expectation, and then retain the
minimized file in the corpus. Record externally sourced corpus provenance and
license before adding it; generated and hand-authored inputs are preferred.
Corpus paths are marked binary in `.gitattributes`; do not enable text or line
ending normalization for individual seeds or minimized reproducers.
