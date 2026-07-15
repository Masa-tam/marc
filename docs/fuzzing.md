# Fuzzing

`marc_fuzz_lzss_stream`, `marc_fuzz_lz78_stream`, `marc_fuzz_lzw_stream`, and
`marc_fuzz_lzd_stream`
exercise both the strict
one-shot stream decoder and the matching frame-streaming decoder with chunk
sizes derived from the input. They use small fixed local limits and
caller-owned workspaces so arbitrary inputs cannot request unbounded
allocation. A call-count guard turns a stalled state machine into a
reproducible failure. The LZ78 target additionally bounds its phrase table to
512 records. The LZW target permits at most width 10 and bounds its phrase
table to 768 records. The LZD target bounds its phrase table to 512 records and
its iterative expansion stack to 513 entries.

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
cmake --build out/build/fuzz --target marc_fuzz_lzss_stream marc_fuzz_lz78_stream marc_fuzz_lzw_stream marc_fuzz_lzd_stream
out/build/fuzz/marc_fuzz_lzss_stream fuzz/corpus/lzss_stream -max_len=8192
out/build/fuzz/marc_fuzz_lz78_stream fuzz/corpus/lz78_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzw_stream fuzz/corpus/lzw_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzd_stream fuzz/corpus/lzd_stream -max_len=8192
```

MSVC remains the reference normal-build toolchain, but its native driver is not
used for this libFuzzer target. Ordinary test builds compile the harness as an
object target, catching portable C++ errors without requiring a fuzz runtime.

Do not treat a disappearing crash as sufficient. Minimize each finding, add the
smallest input or an equivalent explicit assertion to a permanent GoogleTest
regression, record the stable error/atomicity expectation, and then retain the
minimized file in the corpus. Record externally sourced corpus provenance and
license before adding it; generated and hand-authored inputs are preferred.
