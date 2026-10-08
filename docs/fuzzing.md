# Fuzzing

## Target coverage and fixed bounds

The forty-two public bounded targets cover the six standalone dictionary profiles,
the five standalone entropy profiles, the checksum-raw profile, and the full
six-dictionary by five-entropy composed-profile matrix. Targets
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
The combined LZ77 plus Dynamic Range target uses the same 8 KiB supplied-input,
4 KiB output, 1 KiB raw-frame, 16 KiB canonical-token, and 8 KiB payload
ceilings. It exercises both the complete-frame and incremental decoders with
fixed arrays and a fixed call ceiling; no input-controlled allocation is
permitted.
The combined LZ77 plus rANS target caps supplied input at 8 KiB, total raw
output at 4 KiB, one raw frame at 1 KiB, canonical token staging at 4 KiB,
payload at 8 KiB, and entropy metadata at eight fixed `RansBlockView` records.
The encoded frame, views, token staging, private raw frame, and final output
are fixed arrays included in one aggregate policy. Both complete-frame and
incremental paths use byte-derived chunks and a fixed call ceiling.
The combined LZSS plus rANS target applies the same two-boundary policy with
LZSS's `2F` token ceiling. It truncates supplied input to 8 KiB, caps total raw
output at 4 KiB and one frame at 1 KiB, fixes token staging at 2 KiB, payload
at 8 KiB, and metadata at eight `RansBlockView` records. The private exact-
frame decoder and the public C ABI streaming decoder share fixed local limits;
the latter obtains sizes and alignment from its public query but may only bind
preallocated arrays with compile-time ceilings. Byte-derived chunking and a
fixed call budget make stalls reproducible.
The combined LZSS plus Dynamic Range target applies the same dual-decoder
policy with LZSS's tighter variable-token bound: at most 8 KiB supplied input,
4 KiB total output, one 1 KiB raw frame, 2 KiB canonical-token staging, and
8 KiB range payload. Encoded-frame, token, private-raw, and final-output
storage are fixed arrays counted in one aggregate policy. Byte-derived partial
I/O and a fixed call ceiling make stalls reproducible without accepting
input-controlled workspace sizes.
In addition to the forty-two baseline targets, the experimental Format 2 LZSS
contextual Dynamic Range target bounds supplied input at 8 KiB, published raw
output at 4 KiB, one raw frame at 1 KiB, contextual payload at 16,389 bytes,
and native typed-token views at 1,024 fixed records. It exercises the private
complete-frame decoder only after the 112-byte header is accepted and always
exercises the 64 KiB, 1 MiB, 4 MiB, 16 MiB, and 64 MiB public C decoder
admissions with byte-derived chunks and a finite call budget. The largest
profile permits the 64 MiB identity and distance plus the 4,598-entry model
bound but cannot allocate a 64 MiB frame: fuzz storage and raw history remain
capped at 1 KiB. Ordinary builds compile this target warning-clean.
The experimental contextual-rANS Format 2 target caps supplied input at
32 KiB so the selected 9,089-byte maximum descriptor and a complete bounded
frame are reachable. It caps public output at 4 KiB and one raw frame at 1 KiB,
admits at most 6,144 decisions and 12,296 payload bytes, and fixes the
126,976-entry decode tables plus 1,024 typed tokens before processing. Large
views use one thread-local fixed harness workspace instead of per-call stack
storage. The private complete-frame decoder accepts either valid layout, while
the public streaming path separately selects the strict 64 KiB and 1 MiB
profiles with byte-derived chunking and a finite call budget. The wider
identity never allocates a 1 MiB frame or history buffer. Compile-smoke does
not by itself claim a sanitizer campaign.
The experimental contextual-tANS Format 2 target uses the same dual-boundary
shape with its own fixed layout: at most 32 KiB supplied input, 4 KiB published
raw output, one 1 KiB frame, 6,144 decisions, 9,218 payload bytes, the
9,029-byte descriptor, 131,072 fixed `TansDecodeEntry` transitions, and 1,024
typed tokens. All large storage is one thread-local fixed workspace admitted
under a compile-time aggregate ceiling. The private complete-frame and public
C streaming decoders use byte-derived chunks and a finite call budget. An
ordinary-build compile smoke is evidence only; no sanitizer campaign is
claimed until one is separately executed and recorded.
The experimental Contextual Blocked Huffman Format 2 target caps supplied
input at 32 KiB, published raw output at 4 KiB, one frame and typed-token
staging at 1 KiB, modeled decisions at 7,168, payload at 13,440 bytes, and
decode tables at 35 fixed entries. The selected 2,597-byte descriptor ceiling
backs all four valid layouts. The private complete-frame decoder accepts every
layout, while the public C path separately drives strict 64 KiB, 1 MiB, 4 MiB,
and 16 MiB admissions using byte-derived chunks and a finite call budget. A
16 MiB safety distance limit changes no fixed frame/history allocation.
The experimental Contextual Adaptive Huffman target caps supplied input at
64 KiB, published raw output at 4 KiB, one frame at 1 KiB, token staging at
1,024 entries, and payload at 34,176 bytes. Its maximum
9,227-node/4,598-symbol model bank,
private raw storage, public primary/secondary/aligned views, and final output
form one thread-local fixed workspace. The private complete-frame decoder and
all five strict public 64 KiB, 1 MiB, 4 MiB, 16 MiB, and 64 MiB admissions use
a finite call budget; the public paths use byte-derived chunks. Selecting the
64 MiB identity changes only model and distance validation and does not
allocate a profile-sized frame or history.
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
The combined LZ78 plus Dynamic Range target applies the same fixed LZ78 byte
and phrase limits to the exact-frame private decoder and incremental stream
decoder. Supplied input, total output, one raw frame, canonical tokens, range
payload, and phrase records remain capped at 8 KiB, 4 KiB, 1 KiB, 8 KiB,
8 KiB, and 1,024 entries respectively. Input bytes may select only bounded
chunk sizes under the fixed call ceiling.
The combined LZ78 plus rANS target retains the same supplied-input, output,
raw-frame, token, and phrase ceilings while fixing rANS payload at 16 KiB and
metadata at eight `RansBlockView` records. It submits each case to the private
complete-frame decoder and the public C ABI streaming decoder. The public
requirements query may bind only compile-time-sized primary, secondary, and
aligned opaque-view arrays; byte-derived chunks use the common fixed call
ceiling.
The combined LZW plus Adaptive Huffman target also exercises exact-frame and
incremental decode. It caps supplied input and Adaptive payload at 8 KiB,
total output and packed staging at 4 KiB, and one raw frame at 1 KiB. The local
4,096-entry dictionary policy admits 3,639 phrase records from the minimum
nine-bit code density; all frame, packed, raw, and typed storage is included in
the fixed aggregate limit.
The combined LZW plus Dynamic Range target uses the same fixed LZW storage,
phrase-record, dual-decoder, and call-ceiling policy. Its supplied input and
range payload remain capped at 8 KiB, total output and packed staging at
4 KiB, and one raw frame at 1 KiB; no input controls an allocation.
The combined LZD plus Adaptive Huffman target exercises the same two decoder
paths with at most 8 KiB supplied input and Adaptive payload, 4 KiB total
output and token staging, and one 1 KiB raw frame. The frame limit derives
fixed ceilings of 512 LZD phrase records and 513 iterative expansion
references; encoded-frame, token, raw, phrase, and expansion storage are all
included in the aggregate policy before serialized metadata is read.
The combined LZD plus Dynamic Range target retains those fixed LZD byte,
phrase, expansion, dual-decoder, and call-ceiling policies while selecting the
16-byte range descriptor and an 8 KiB payload cap. No serialized extent can
resize its caller-owned arrays.
The combined LZD plus rANS target caps supplied input at 8 KiB, total raw
output and token staging at 4 KiB, one raw frame at 1 KiB, payload at 16 KiB,
and metadata at eight `RansBlockView` records. The frame limit admits 512 LZD
phrases and 513 iterative expansion references. Both the private complete-frame
decoder and public C ABI streaming decoder use fixed arrays; the public query
may bind only compile-time-bounded primary, secondary, and aligned opaque-view
storage. Byte-derived chunks and the independent 12,320-call ceiling make
stalls reproducible without input-controlled allocation.
The combined LZMW plus rANS target uses the same dual-decoder and scalar-rANS
metadata policy with a 4 KiB canonical-reference cap. Its one-KiB raw frame
admits 1,023 LZMW phrase records and 1,024 iterative expansion references.
All encoded-frame, reference, raw, rANS-view, phrase, expansion, and public-C
workspace storage is fixed before input parsing, and byte-derived chunking is
bounded by the independent 12,320-call ceiling.
The combined LZMW plus Adaptive Huffman target also exercises exact-frame and
incremental decode. It caps supplied input and Adaptive payload at 8 KiB,
total raw output and canonical reference staging at 4 KiB, and one raw frame
at 1 KiB. The four-byte reference bound admits 1,023 phrase records and 1,024
iterative expansion references; all encoded-frame, reference, raw, and typed
storage is fixed and included in the aggregate policy before metadata is read.
The combined LZMW plus Dynamic Range target retains the same 8 KiB input and
payload, 4 KiB output and reference, 1 KiB frame, 1,023 phrase, 1,024
expansion, dual-decoder, and finite-call policies while selecting the 16-byte
range descriptor. No serialized extent controls allocation.
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

`marc_fuzz_lz77_tans_stream` applies the fixed-memory dual-decoder boundary to
the composed profile. It caps serialized input and payload at 8 KiB, total raw
output and canonical token staging at 4 KiB, one raw frame at 1 KiB, and tANS
metadata at eight fixed `TansBlockView` records. Complete-frame and incremental
paths share those caller-owned arrays and the finite process-call ceiling.

`marc_fuzz_lzss_tans_stream` applies the same dual-decoder boundary to the
variable-length LZSS token grammar. It caps serialized input and payload at
8 KiB, total raw output at 4 KiB, one raw frame at 1 KiB, canonical token
staging at 2 KiB, and tANS metadata at eight fixed `TansBlockView` records.
Complete-frame and incremental paths share those arrays and the finite
process-call ceiling.

`marc_fuzz_lz78_tans_stream` combines the fixed tANS view/state boundary with
LZ78 phrase-graph validation. It truncates supplied input to 8 KiB, caps total
raw output at 4 KiB and one frame at 1 KiB, fixes canonical token staging at
8 KiB, payload at 16 KiB, metadata at eight `TansBlockView` records, and
phrases at 1,024 records. The complete-frame private decoder and public C ABI
streaming decoder share fixed arrays and hard limits. Byte-derived chunks and
a finite call budget make stalls reproducible without input-controlled
allocation.

`marc_fuzz_blocked_huffman_stream` covers the standalone dictionary-none
profile that the combined target cannot select. It uses eight fixed block
views, 256-symbol blocks, code length 24, a 512-node decode-table cap, and the
same byte, chunking, and call-count limits as the ANS targets. Both canonical
and raw block paths remain bounded by caller-owned arrays.

## Build and execution workflow

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
  marc_fuzz_lz77_dynamic_range_stream \
  marc_fuzz_lz77_rans_stream \
  marc_fuzz_lz77_tans_stream \
  marc_fuzz_lzss_tans_stream \
  marc_fuzz_lzss_rans_stream \
  marc_fuzz_lzss_adaptive_huffman_stream \
  marc_fuzz_lzss_dynamic_range_stream \
  marc_fuzz_lz78_adaptive_huffman_stream \
  marc_fuzz_lz78_dynamic_range_stream \
  marc_fuzz_lz78_rans_stream \
  marc_fuzz_lz78_tans_stream \
  marc_fuzz_lzw_adaptive_huffman_stream \
  marc_fuzz_lzw_rans_stream \
  marc_fuzz_lzw_tans_stream \
  marc_fuzz_lzd_rans_stream \
  marc_fuzz_lzd_tans_stream \
  marc_fuzz_lzmw_rans_stream \
  marc_fuzz_lzd_adaptive_huffman_stream \
  marc_fuzz_lzd_dynamic_range_stream \
  marc_fuzz_lzmw_adaptive_huffman_stream \
  marc_fuzz_lzmw_dynamic_range_stream \
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
out/build/fuzz/marc_fuzz_lz77_dynamic_range_stream \
  fuzz/corpus/lz77_dynamic_range_stream -max_len=8192
out/build/fuzz/marc_fuzz_lz77_rans_stream \
  fuzz/corpus/lz77_rans_stream -max_len=8192
out/build/fuzz/marc_fuzz_lz77_tans_stream \
  fuzz/corpus/lz77_tans_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzss_tans_stream \
  fuzz/corpus/lzss_tans_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzss_rans_stream \
  fuzz/corpus/lzss_rans_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzss_adaptive_huffman_stream \
  fuzz/corpus/lzss_adaptive_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzss_dynamic_range_stream \
  fuzz/corpus/lzss_dynamic_range_stream -max_len=8192
out/build/fuzz/marc_fuzz_lz78_adaptive_huffman_stream \
  fuzz/corpus/lz78_adaptive_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lz78_dynamic_range_stream \
  fuzz/corpus/lz78_dynamic_range_stream -max_len=8192
out/build/fuzz/marc_fuzz_lz78_rans_stream \
  fuzz/corpus/lz78_rans_stream -max_len=8192
out/build/fuzz/marc_fuzz_lz78_tans_stream \
  fuzz/corpus/lz78_tans_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzw_adaptive_huffman_stream \
  fuzz/corpus/lzw_adaptive_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzw_rans_stream \
  fuzz/corpus/lzw_rans_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzw_tans_stream \
  fuzz/corpus/lzw_rans_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzd_rans_stream \
  fuzz/corpus/lzd_rans_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzd_tans_stream \
  fuzz/corpus/lzd_tans_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzmw_rans_stream \
  fuzz/corpus/lzmw_rans_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzd_adaptive_huffman_stream \
  fuzz/corpus/lzd_adaptive_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzd_dynamic_range_stream \
  fuzz/corpus/lzd_dynamic_range_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzmw_adaptive_huffman_stream \
  fuzz/corpus/lzmw_adaptive_huffman_stream -max_len=8192
out/build/fuzz/marc_fuzz_lzmw_dynamic_range_stream \
  fuzz/corpus/lzmw_dynamic_range_stream -max_len=8192
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

If Windows linking reports `annotate_string` value 0 in the prebuilt libFuzzer
and value 1 in marc, configure the separate fuzz build with
`-DMARC_FUZZ_DISABLE_STRING_ANNOTATION=ON`. This opt-in compatibility setting
applies `_DISABLE_STRING_ANNOTATION` consistently to the fuzz static library
and its build-tree consumers on Windows. It defaults to OFF and has no effect
on ordinary builds or installed interface definitions. Rebuilding recompiles
affected objects; suppressing linker mismatch checks is not an alternative.

For the corresponding `annotate_vector` mismatch, also configure
`-DMARC_FUZZ_DISABLE_VECTOR_ANNOTATION=ON`. It has the same default-OFF,
Windows fuzz-only and build-interface scope. The local 2026-09-23 toolchain
requires both options: its prebuilt libFuzzer records zero for both contracts.

These options retain libFuzzer, ASan and UBSan, but remove the selected
`std::string`/`std::vector` container-boundary annotations. A campaign must
not claim that additional coverage. General memory instrumentation remains
enabled; optional annotations and linker mismatch checks are not disabled.
Remove the opt-ins and rebuild when a matching runtime is available.

## Recorded bounded campaigns

### FZ-0001: Initial six-target Windows smoke

A bounded Windows smoke campaign on 2026-07-16 ran each of the six targets for
10,000 inputs with `-max_len=8192`, `-timeout=5`, and `-rss_limit_mb=512`.
All 60,000 executions completed without a crash, hang, or sanitizer finding;
each process peaked at 64 MiB RSS. This is execution-path evidence only, not a
claim of coverage completion.

### FZ-0002: Initial checksum-raw smoke

The raw-checksum target received an initial bounded sanitizer smoke on
2026-07-16: 1,000 inputs, 8 KiB maximum input, five-second per-input timeout,
and 512 MiB RSS limit. It completed without a crash, hang, or sanitizer finding
and peaked at 37 MiB RSS. Automatically generated reductions were discarded;
only the reviewed hand-authored seed remains in the repository.

### FZ-0003: Checksum-raw dual-path smoke

After adding the incremental decoder path on 2026-07-16, the same bounded
1,000-input sanitizer smoke again completed without a crash, hang, or sanitizer
finding at 37 MiB peak RSS. Generated reductions were again discarded.

### FZ-0004: Adaptive Huffman dual-decoder smoke

The Adaptive Huffman dual-decoder target received its initial bounded sanitizer
smoke on 2026-07-17: 1,000 inputs, 8 KiB maximum input, five-second per-input
timeout, and 512 MiB RSS limit. It completed without a crash, hang, or sanitizer
finding and peaked at 37 MiB RSS. Mutations remained in the disposable build
corpus; the repository retains only the reviewed five-byte seed.

### FZ-0005: Dynamic Range dual-decoder smoke

The Dynamic Range dual-decoder target received the same bounded 1,000-input
sanitizer smoke on 2026-07-17. With an 8 KiB maximum input, five-second timeout,
and 512 MiB RSS limit, it completed without a crash, hang, or sanitizer finding
and peaked at 37 MiB RSS. Generated mutations remained outside the source
corpus.

### FZ-0006: rANS dual-decoder smoke

The rANS dual-decoder target received the same bounded 1,000-input sanitizer
smoke on 2026-07-17. With the eight-view and 4,096-entry table caps, it completed
without a crash, hang, or sanitizer finding and peaked at 37 MiB RSS. Generated
mutations remained in the disposable build corpus.

### FZ-0007: tANS dual-decoder smoke

The tANS dual-decoder target received the same bounded 1,000-input sanitizer
smoke on 2026-07-17. With eight fixed views and the 4,096-state table cap, it
completed without a crash, hang, or sanitizer finding and peaked at 37 MiB RSS.
Generated mutations remained in the disposable build corpus.

### FZ-0008: Blocked Huffman dual-decoder smoke

The standalone Blocked Huffman dual-decoder target received the same bounded
1,000-input sanitizer smoke on 2026-07-17. With eight fixed views, code length
24, and the 512-node table cap, it completed without a crash, hang, or sanitizer
finding and peaked at 37 MiB RSS. Mutations remained in the disposable build
corpus.

### FZ-0009: LZ77 dual-decoder smoke

The standalone LZ77 dual-decoder target received the same bounded 1,000-input
sanitizer smoke on 2026-07-17. With fixed frame arrays and the documented byte
limits, it completed without a crash, hang, or sanitizer finding and peaked at
37 MiB RSS. Mutations remained in the disposable build corpus.

### FZ-0010: LZSS plus Blocked Huffman smoke

The composed LZSS plus Blocked Huffman dual-decoder target received a bounded
10,000-input sanitizer smoke on 2026-07-18 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 64 MiB RSS. Generated mutations remained in the disposable build
corpus; the repository retains only the reviewed five-byte seed.

### FZ-0011: LZW plus Blocked Huffman smoke

The composed LZW plus Blocked Huffman decoder target received a bounded
1,000-input sanitizer smoke on 2026-07-18 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 37 MiB RSS. Generated mutations remained in the disposable build
corpus; the repository retains only the reviewed five-byte seed.

### FZ-0012: LZD plus Blocked Huffman smoke

The composed LZD plus Blocked Huffman decoder target received a bounded
1,000-input sanitizer smoke on 2026-07-18 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 37 MiB RSS. Generated mutations remain only in the ignored build
workspace; the repository retains the reviewed five-byte truncated-magic seed.

### FZ-0013: LZMW plus Blocked Huffman smoke

The composed LZMW plus Blocked Huffman decoder target received a bounded
1,000-input sanitizer smoke on 2026-07-18 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 37 MiB RSS. Generated mutations remain only in the ignored build
workspace; the repository retains the reviewed five-byte truncated-magic seed.

### FZ-0014: LZ77 plus Adaptive Huffman smoke

The composed LZ77 plus Adaptive Huffman frame/stream decoder target received a
bounded 1,000-input sanitizer smoke on 2026-07-19 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 37 MiB RSS. Generated mutations remain only in the ignored build
workspace; the repository retains the reviewed five-byte truncated-magic seed.

### FZ-0015: LZSS plus Adaptive Huffman smoke

The composed LZSS plus Adaptive Huffman frame/stream decoder target received a
bounded 1,000-input sanitizer smoke on 2026-07-19 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 37 MiB RSS. Generated mutations remain only in the ignored build
workspace; the repository retains the reviewed five-byte truncated-magic seed.

### FZ-0016: LZMW plus Adaptive Huffman smoke

The composed LZMW plus Adaptive Huffman frame/stream decoder target received a
bounded 1,000-input sanitizer smoke on 2026-07-22 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 37 MiB RSS. Generated mutations remain only in the ignored build
workspace; the repository retains the reviewed five-byte truncated-magic seed.

### FZ-0017: LZSS plus rANS smoke

The composed LZSS plus rANS private-frame/public-C decoder target received a
bounded 1,000-input sanitizer smoke on 2026-07-31 with 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. It completed without a
crash, hang, AddressSanitizer finding, or UndefinedBehaviorSanitizer finding
and peaked at 39 MiB RSS. Nine generated corpus changes remain only in the
ignored build workspace; the repository retains the reviewed five-byte
truncated-magic seed.

### FZ-0018: LZW plus rANS smoke

The composed LZW plus rANS target fixes an 8 KiB input ceiling, 4 KiB raw
publication ceiling, 1 KiB frame ceiling, eight rANS views, bounded packed-code
and phrase storage, aggregate workspace, and a finite process-call budget. It
drives both complete-frame decoding and the public C streaming lifecycle for
each input. Ordinary builds compile this translation unit as an object target.
Its initial bounded Windows sanitizer smoke on 2026-08-01 completed 1,000
inputs with an 8 KiB maximum input, five-second per-input timeout, and 512 MiB
RSS limit without a crash, hang, AddressSanitizer finding, or
UndefinedBehaviorSanitizer finding; peak RSS was 38 MiB. The Visual Studio
Clang 22 sanitizer runtime directory was added only to that process's `PATH`;
no machine-specific path was committed. Generated mutations remained only in
memory; the repository retains the reviewed five-byte truncated-magic seed.

### FZ-0019: LZSS plus tANS smoke

The composed LZSS plus tANS dual-decoder target received a bounded 1,000-input
sanitizer smoke on 2026-08-03 with 8 KiB maximum input, a five-second
per-input timeout, and a 512 MiB RSS limit. It completed without a crash, hang,
AddressSanitizer finding, or UndefinedBehaviorSanitizer finding and peaked at
37 MiB RSS. Generated mutations were not written to the source corpus; the
repository retains only the reviewed five-byte seed.

### FZ-0020: LZW plus tANS smoke

The composed LZW plus tANS target retains the same 8 KiB input, 4 KiB raw,
1 KiB frame, eight-view, fixed packed/phrase storage, aggregate, and finite
call ceilings while substituting the local tANS complete-frame and public C
decoders. Ordinary builds compile it warning-clean as an object target. Its
initial Ubuntu 26.04 Clang 21 libFuzzer/AddressSanitizer/
UndefinedBehaviorSanitizer smoke on 2026-08-05 completed 1,000 inputs with an
8 KiB maximum input, five-second per-input timeout, and 512 MiB RSS limit with
no crash, hang, or sanitizer finding; peak RSS was 39 MiB. Generated corpus
changes and artifacts remained under WSL `/tmp`; the repository was unchanged.

### FZ-0021: Contextual tANS smoke

The experimental contextual tANS private-frame/public-C decoder target
received its initial bounded Windows Clang 22 libFuzzer/AddressSanitizer/
UndefinedBehaviorSanitizer smoke on 2026-08-10. It completed 1,000 inputs with
a 32 KiB maximum input, five-second per-input timeout, and 512 MiB RSS limit
without a crash, hang, or sanitizer finding; peak RSS was 42 MiB. The matching
Clang 22 sanitizer runtime directory was prepended only to the campaign
process's `PATH`. No input corpus was supplied, generated mutations remained
in memory, and no artifact was produced. This bounded smoke is evidence for
the exercised inputs, not a claim of exhaustive safety.

### FZ-0022: Contextual Blocked Huffman smoke

The experimental Contextual Blocked Huffman private-frame/public-C decoder
target received its initial bounded Windows Clang 22 libFuzzer/
AddressSanitizer/UndefinedBehaviorSanitizer smoke on 2026-08-11. It completed
1,000 inputs with a 32 KiB maximum input, five-second per-input timeout, and
512 MiB RSS limit without a crash, hang, or sanitizer finding. Peak RSS was
40 MiB; final coverage was 192 counters and 390 features over a seven-entry,
29-byte in-memory corpus. The matching Clang 22 runtime path applied only to
the child process. No input corpus was supplied and no artifact was produced.
This bounded result is evidence for the exercised inputs, not an exhaustive
safety claim.

### FZ-0023: Contextual Adaptive Huffman smoke

The experimental Contextual Adaptive Huffman private-frame/public-C decoder
target received its initial bounded Windows Clang 22 libFuzzer/
AddressSanitizer/UndefinedBehaviorSanitizer smoke on 2026-08-11. It completed
1,000 inputs with a 64 KiB maximum input, five-second per-input timeout, and
512 MiB RSS limit without a crash, hang, or sanitizer finding. Peak RSS was
40 MiB; final coverage was 205 counters and 426 features over a seven-entry,
32-byte in-memory corpus. The matching Clang 22 runtime path applied only to
the child process. No input corpus was supplied and no artifact was produced.
This bounded result is evidence for the exercised inputs, not an exhaustive
safety claim.

### FZ-0024: Dual-profile Contextual Dynamic Range smoke

The Contextual Dynamic Range private-frame/public-C target now drives both
public window-profile admissions for every bounded input while retaining its
8 KiB input, 4 KiB output, 1 KiB frame, fixed arrays, and finite call ceiling.
A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer smoke
with seed 12345 completed exactly 1,000 inputs under a five-second per-input
timeout and 512 MiB RSS limit without a crash, hang, or sanitizer finding.
Peak RSS was 40 MiB; final coverage was 196 counters and 308 features over a
five-entry, 20-byte in-memory corpus. The sanitizer runtime path applied only
to the campaign process. No input corpus was supplied and no artifact was
produced. This bounded smoke is evidence for the exercised inputs, not an
exhaustive safety claim.

### FZ-0025: Dual-profile Contextual rANS smoke

The Contextual rANS private-frame/public-C target now drives both strict public
window-profile admissions for every bounded input while retaining its 32 KiB
input, 4 KiB output, 1 KiB frame/token storage, fixed 126,976-entry table, and
finite call ceiling. Only descriptor backing grows to the selected 9,089-byte
maximum; the wider identity does not allocate a 1 MiB frame.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 349530899 completed exactly 1,000 inputs under a 32 KiB maximum
input, five-second per-input timeout, and 512 MiB RSS limit without a crash,
hang, or sanitizer finding. Peak RSS was 44 MiB; final coverage was 200
counters and 320 features over a seven-entry, 32-byte in-memory corpus. The
matching sanitizer runtime path applied only to the campaign process. No input
corpus was supplied and no artifact was produced. This bounded result is
evidence for the exercised inputs, not an exhaustive safety claim.

### FZ-0026: Dual-profile Contextual tANS smoke

The Contextual tANS private-frame/public-C target now drives both strict
public window-profile admissions for every bounded input while retaining its
32 KiB input, 4 KiB output, 1 KiB frame/token storage, fixed 131,072-entry
transition table, and finite call ceiling. Only descriptor backing grows to
the selected 9,093-byte maximum; the wider identity does not allocate a 1 MiB
frame.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 643194783 completed exactly 1,000 inputs under a 32 KiB maximum
input, five-second per-input timeout, and 512 MiB RSS limit without a crash,
hang, or sanitizer finding. Peak RSS was 43 MiB; final coverage was 218
counters and 382 features over a six-entry, 26-byte in-memory corpus. The
matching sanitizer runtime path applied only to the campaign process. No input
corpus was supplied and no artifact was produced. This bounded result is
evidence for the exercised inputs, not an exhaustive safety claim.

### FZ-0027: Dual-profile Contextual Blocked Huffman smoke

The Contextual Blocked Huffman private-frame/public-C target now drives both
strict public window-profile admissions for every bounded input while
retaining its 32 KiB input, 4 KiB output, 1 KiB frame/token storage, 6,144
decisions, 11,520-byte payload, 35-table, and finite-call ceilings. Only
descriptor backing grows to the selected 2,579-byte maximum; the wider
identity does not allocate a 1 MiB frame or history buffer.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 8292579 completed exactly 1,000 inputs under a 32 KiB maximum input,
five-second per-input timeout, and 512 MiB RSS limit without a crash, hang, or
sanitizer finding. Peak RSS was 41 MiB; final coverage was 218 counters and
382 features over a seven-entry, 30-byte in-memory corpus. The matching
sanitizer runtime path applied only to the campaign process. No input corpus
was supplied and no artifact was produced. This bounded result is evidence
for the exercised inputs, not an exhaustive safety claim.

### FZ-0028: Dual-profile Contextual Adaptive Huffman smoke

The Contextual Adaptive Huffman private-frame/public-C target now drives both
strict public window-profile admissions for every bounded input while
retaining its 64 KiB input, 4 KiB output, 1 KiB frame/token storage,
34,176-byte payload, and finite-call ceilings. Only fixed model backing grows
to the selected maximum of 9,131 nodes and 4,550 symbols; the wider identity
does not allocate a 1 MiB frame or history buffer.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 133978600 completed exactly 1,000 inputs under a 64 KiB maximum
input, five-second per-input timeout, and 512 MiB RSS limit without a crash,
hang, or sanitizer finding. Peak RSS was 42 MiB; final coverage was 239
counters and 414 features over a six-entry, 23-byte in-memory corpus. The
matching sanitizer runtime path applied only to the campaign process. No input
corpus was supplied and no artifact was produced. This bounded result is
evidence for the exercised inputs, not an exhaustive safety claim.

### FZ-0029: Triple-profile Contextual Dynamic Range smoke

The Contextual Dynamic Range private-frame/public-C decoder target now drives
the 64-KiB, one-MiB, and four-MiB strict admissions for every bounded input.
It retains its 8-KiB input, 4-KiB total output, one-KiB frame/token storage,
fixed arrays, and finite call ceiling. The largest admitted limits are
`14*1024 + 5` payload bytes, 4,566 flattened model entries, and a 4,194,304-
byte distance; the four-MiB identity does not allocate a four-MiB fuzz frame.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 20260822 completed exactly 1,000 inputs under an 8-KiB maximum input,
five-second per-input timeout, and 512-MiB RSS limit without a crash, hang, or
sanitizer finding. Peak RSS was 41 MiB; final coverage was 208 counters and
328 features over a six-entry, 24-byte in-memory corpus. The matching runtime
path applied only to the campaign process. No input corpus was supplied, no
generated mutation was retained, and no artifact was produced. This bounded
result is evidence for the exercised inputs, not an exhaustive safety claim.

### FZ-0030: Triple-profile Contextual rANS smoke

The Contextual rANS private-frame/public-C decoder target now drives the
64-KiB, one-MiB, and four-MiB strict admissions for every bounded input. It
retains its 32-KiB input, 4-KiB total output, one-KiB frame/token storage,
fixed 126,976-entry decode-table bank, and finite call ceiling. The largest
admitted limits are 7,168 decisions, 14,344 payload bytes, a 9,121-byte
descriptor, and a 4,194,304-byte LZ distance; the four-MiB identity does not
allocate a four-MiB fuzz frame.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 20260822 completed exactly 1,000 inputs under a 32-KiB maximum input,
five-second per-input timeout, and 512-MiB RSS limit without a crash, hang, or
sanitizer finding. Peak RSS was 45 MiB; final coverage was 216 counters and
336 features over a six-entry, 24-byte in-memory corpus. The matching runtime
path applied only to the campaign process. No input corpus was supplied, no
generated mutation was retained, and no artifact was produced. This bounded
result is evidence for the exercised inputs, not an exhaustive safety claim.

### FZ-0031: Triple-profile Contextual tANS smoke

The Contextual tANS private-frame/public-C decoder target now drives the
64-KiB, one-MiB, and four-MiB strict admissions for every bounded input. It
retains its 32-KiB input, 4-KiB total output, one-KiB frame/token storage,
fixed 131,072-entry decode-table bank, and finite call ceiling. The largest
admitted limits are 7,168 decisions, 10,754 payload bytes, a 9,125-byte
descriptor, and a 4,194,304-byte LZ distance; the four-MiB identity does not
allocate a four-MiB fuzz frame.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 20260822 completed exactly 1,000 inputs under a 32-KiB maximum input,
five-second per-input timeout, and 512-MiB RSS limit without a crash, hang, or
sanitizer finding. Peak RSS was 44 MiB; final coverage was 235 counters and
406 features over a six-entry, 23-byte in-memory corpus. The matching runtime
path applied only to the campaign process. No input corpus was supplied, no
generated mutation was retained, and no artifact was produced. This bounded
result is evidence for the exercised inputs, not an exhaustive safety claim.

### FZ-0032: Triple-profile Contextual Blocked Huffman smoke

The Contextual Blocked Huffman private-frame/public-C decoder target now
drives the 64-KiB, one-MiB, and four-MiB strict admissions for every bounded
input. It retains a 32-KiB input cap, four-KiB total output, one-KiB raw and
token storage, 35 fixed decode tables, and a finite call ceiling. The largest
admitted local limits are 7,168 decisions, 13,440 payload bytes, a 2,588-byte
descriptor, and a 4,194,304-byte LZ distance. The four-MiB identity does not
allocate a four-MiB fuzz frame or history buffer.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 20260822 completed exactly 1,000 inputs under a 32-KiB maximum
input, five-second per-input timeout, and 512-MiB RSS limit without a crash,
hang, or sanitizer finding. Peak RSS was 42 MiB; final coverage was 231
counters and 402 features over a six-entry, 23-byte in-memory corpus. The
matching sanitizer runtime path applied only to the campaign process. No
input corpus was supplied, no generated mutation was retained, and no
artifact was produced. This bounded result is evidence for the exercised
inputs, not an exhaustive safety claim.

### FZ-0033: Four-profile Contextual Dynamic Range smoke

The Contextual Dynamic Range private-frame/public-C decoder target now drives
the 64-KiB, one-MiB, four-MiB, and 16-MiB strict admissions for every bounded
input. It retains its 8-KiB input, 4-KiB total output, one-KiB frame/token
storage, fixed arrays, and finite call ceiling. The largest admitted limits
are `14*1024 + 5` payload bytes, 4,582 flattened model entries, and a
16,777,216-byte distance; the 16-MiB identity does not allocate a 16-MiB fuzz
frame or history buffer.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 20260823 completed exactly 1,000 inputs under an 8-KiB maximum input,
five-second per-input timeout, and 512-MiB RSS limit without a crash, hang, or
sanitizer finding. Peak RSS was 42 MiB; final coverage was 215 counters and
327 features over a six-entry, 23-byte in-memory corpus. The matching runtime
path applied only to the campaign process. No input corpus was supplied, no
generated mutation was retained, and no artifact was produced. This bounded
result is evidence for the exercised inputs, not an exhaustive safety claim.

### FZ-0034: Four-profile Contextual rANS smoke

The Contextual rANS private-frame/public-C decoder target now drives the
64-KiB, one-MiB, four-MiB, and 16-MiB strict admissions for every bounded
input. It retains its 32-KiB input, 4-KiB total output, one-KiB frame/token
storage, fixed 126,976-entry decode-table bank, and finite call ceiling. The
largest admitted limits are 7,168 decisions, 14,344 payload bytes, a
9,153-byte descriptor, and a 16,777,216-byte LZ distance; the 16-MiB identity
does not allocate a 16-MiB fuzz frame or history buffer.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 20260824 completed exactly 1,000 inputs under a 32-KiB maximum input,
five-second per-input timeout, and 512-MiB RSS limit without a crash, hang, or
sanitizer finding. Peak RSS was 46 MiB; final coverage was 224 counters and
336 features over a five-entry, 21-byte in-memory corpus. The matching runtime
path applied only to the campaign process. No input corpus was supplied, no
generated mutation was retained, and no artifact was produced. This bounded
result is evidence for the exercised inputs, not an exhaustive safety claim.

### FZ-0035: Four-profile Contextual Blocked Huffman smoke

The Contextual Blocked Huffman private-frame/public-C decoder target now
drives the 64-KiB, one-MiB, four-MiB, and 16-MiB strict admissions for every
bounded input. It retains its 32-KiB input, four-KiB total output, one-KiB
frame/token storage, 7,168 decisions, 13,440 payload bytes, 2,597-byte shared
descriptor, 35 fixed decode tables, and finite call ceiling. The largest
admitted distance is 16,777,216 bytes; the 16-MiB identity does not allocate a
16-MiB fuzz frame or history buffer.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 20260826 completed exactly 1,000 inputs under a 32-KiB maximum input,
five-second per-input timeout, and 512-MiB RSS limit without a crash, hang, or
sanitizer finding. Peak RSS was 43 MiB; final coverage was 239 counters and
402 features over a six-entry, 24-byte in-memory corpus. The matching runtime
path applied only to the campaign process. No input corpus was supplied, no
generated mutation was retained, and no artifact was produced. This bounded
result is evidence for the exercised inputs, not an exhaustive safety claim.

### FZ-0036: Four-profile Contextual Adaptive Huffman smoke

The Contextual Adaptive Huffman private-frame/public-C decoder target now
drives the 64-KiB, one-MiB, four-MiB, and 16-MiB strict admissions for every
bounded input. It retains its 64-KiB input, four-KiB total output, one-KiB
frame/token storage, 34,176-byte payload, fixed 9,195-node/4,582-symbol model
bank, and finite call ceiling. The largest admitted distance is 16,777,216
bytes; the 16-MiB identity does not allocate a 16-MiB frame, history buffer,
or full one-GiB profile workspace.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 20260828 completed exactly 1,000 inputs under a 64-KiB maximum input,
five-second per-input timeout, and 512-MiB RSS limit without a crash, hang, or
sanitizer finding. Peak RSS was 43 MiB; final coverage was 261 counters and
436 features over a five-entry, 20-byte in-memory corpus. The matching runtime
path applied only to the campaign process. No input corpus was supplied, no
generated mutation was retained, and no artifact was produced. This bounded
result is evidence for the exercised inputs, not an exhaustive safety claim.

### FZ-0037: Five-profile Contextual Dynamic Range smoke

The Contextual Dynamic Range private-frame/public-C decoder target now drives
the 64-KiB, one-MiB, four-MiB, 16-MiB, and 64-MiB strict admissions for every
bounded input. It retains its eight-KiB input, four-KiB total output, one-KiB
frame/token storage, fixed arrays, and finite call ceiling. The largest
admitted limits are `16*1024 + 5` payload bytes, 4,598 flattened model entries,
and a 67,108,864-byte distance; the 64-MiB identity does not allocate a
64-MiB fuzz frame, history, or profile-sized decoder workspace.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 20260901 completed exactly 1,000 inputs under an eight-KiB maximum
input, five-second per-input timeout, and 512-MiB RSS limit without a crash,
hang, or sanitizer finding. Peak RSS was 43 MiB; final coverage was 226
counters and 346 features over a seven-entry, 30-byte in-memory corpus. The
matching runtime path applied only to the campaign process. No input corpus
was supplied, no generated mutation was retained, and no artifact was
produced. This bounded result is evidence for the exercised inputs, not an
exhaustive safety claim.

### FZ-0038: Five-profile Contextual rANS smoke

The Contextual rANS private-frame/public-C decoder target now drives the
64-KiB, one-MiB, four-MiB, 16-MiB, and 64-MiB strict admissions for every
bounded input. It retains its 32-KiB input, four-KiB total output, one-KiB
frame/token/raw storage, fixed 126,976-entry decode-table bank, and finite
call ceiling. The largest admitted limits are 8,192 decisions, 16,392 payload
bytes, a 9,185-byte descriptor, and a 67,108,864-byte distance; the 64-MiB
identity does not allocate a 64-MiB frame, history, or full-profile workspace.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 20260902 completed exactly 1,000 inputs under a 32-KiB maximum input,
five-second per-input timeout, and 512-MiB RSS limit without a crash, hang, or
sanitizer finding. Peak RSS was 48 MiB; final coverage was 236 counters and
356 features over a six-entry, 23-byte in-memory corpus. The matching runtime
path applied only to the campaign process. No input corpus was supplied, no
generated mutation was retained, and no artifact was produced. This bounded
result is evidence for the exercised inputs, not an exhaustive safety claim.

### FZ-0039: Five-profile Contextual tANS smoke

The Contextual tANS private-frame/public-C decoder target now drives the
64-KiB, one-MiB, four-MiB, 16-MiB, and 64-MiB strict admissions for every
bounded input. It retains its 32-KiB input, four-KiB total output, one-KiB
frame/token/raw storage, fixed 131,072-entry decode-table bank, and finite
call ceiling. The largest admitted limits are 8,192 decisions, 12,290 payload
bytes, a 9,189-byte descriptor, and a 67,108,864-byte distance; the 64-MiB
identity does not allocate a 64-MiB frame, history, or full-profile workspace.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 20260904 completed exactly 1,000 inputs under a 32-KiB maximum input,
five-second per-input timeout, and 512-MiB RSS limit without a crash, hang, or
sanitizer finding. Peak RSS was 46 MiB; final coverage was 255 counters and
427 features over an eight-entry, 39-byte in-memory corpus. The matching
sanitizer runtime path applied only to the campaign process. No input corpus
was supplied, no generated mutation was retained, and no artifact was
produced. This bounded result is evidence for the exercised inputs, not an
exhaustive safety claim.

### FZ-0040: 64-MiB Contextual Blocked Huffman bounded dual-path campaign

The Contextual Blocked Huffman harness now exercises public profiles 64 KiB,
1 MiB, 4 MiB, 16 MiB, and 64 MiB while retaining fixed caller-owned storage.
Input is capped at 32 KiB, published output at 4 KiB, frame/token/raw storage
at 1 KiB, decisions at 8,192, payload at 15,360 bytes, and the process-call
ceiling remains finite. Profile 64M selects the exact v5 descriptor and
67,108,864-byte safety distance without allocating a 64-MiB frame, history,
or full-profile workspace. Permanent regressions cover every truncation,
extreme frame sizes, nonzero descriptor flags, all reciprocal crossings with
the four earlier profiles, sticky errors, and sentinel output atomicity.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 20260905 completed exactly 1,000 inputs under a 32-KiB maximum input,
five-second per-input timeout, and 512-MiB RSS limit without a crash, hang, or
sanitizer finding. Peak RSS was 77 MiB; final coverage was 251 counters and
423 features over an eight-entry, 40-byte in-memory corpus. The matching
sanitizer runtime path applied only to the campaign process. No input corpus
was supplied, no generated mutation was retained, and no artifact was
produced. This bounded result is evidence for the exercised inputs, not an
exhaustive safety claim.

### FZ-0041: Five-profile Contextual Adaptive Huffman bounded campaign

The Contextual Adaptive Huffman private-frame/public-C decoder target now
drives the 64-KiB, one-MiB, four-MiB, 16-MiB, and 64-MiB strict admissions for
every bounded input. It retains its 64-KiB input, four-KiB total output,
one-KiB frame/token/raw storage, 34,176-byte payload, fixed
9,227-node/4,598-symbol model bank, and finite call ceiling. The largest
admitted distance is 67,108,864 bytes; selecting the 64-MiB identity does not
allocate a 64-MiB frame, history, or full-profile workspace. Permanent
regressions cover every truncation, reserved identity bytes, extreme frame
lengths, invalid descriptor fields, nonzero payload padding, all reciprocal
crossings with the four earlier profiles, sticky errors, and sentinel output
atomicity.

A Windows Clang 22 libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer run
with seed 20260906 completed exactly 1,000 inputs under a 64-KiB maximum input,
five-second per-input timeout, and 512-MiB RSS limit without a crash, hang, or
sanitizer finding. Peak RSS was 44 MiB; final coverage was 274 counters and
457 features over a seven-entry, 31-byte in-memory corpus. The matching
sanitizer runtime path applied only to the campaign process. No input corpus
was supplied, no generated mutation was retained, and no artifact was
produced. This bounded result is evidence for the exercised inputs, not an
exhaustive safety claim.

### FZ-0042: Post-probe-switch contextual decoder smoke with annotation compatibility

On 2026-09-23, rebuild the static library and the five contextual Dynamic
Range, rANS, tANS, Blocked Huffman and Adaptive Huffman decoder harnesses
from the production-probe source at `811d5c4b` plus DD-1172/DD-1173 build
configuration changes. Windows Clang 22.1.3 uses libFuzzer, ASan and UBSan,
with both string/vector annotation compatibility options ON. Their extra
container-boundary checks are therefore not covered. General sanitizer
instrumentation and linker mismatch checking remain enabled.

Each target completed 1,000 runs with seed 20260923, five-second per-input
timeout and 512-MiB RSS ceiling. Maximum input lengths were 8 KiB for Dynamic
Range, 64 KiB for Adaptive Huffman and 32 KiB for the other three. No input
corpus was supplied; mutations stayed in memory and no finding artifact was
produced. All five exited zero with no reported sanitizer finding. The matching
ASan runtime directory was prepended only to the campaign process PATH.

This is bounded decoder smoke, not deep valid-stream exploration, direct
encoder/probe fuzz coverage, or proof of safety. Encoder differential/round-trip
sanitizer coverage and the post-switch measurement/CI gates remain separate.

### FZ-0043: Bounded typed encoder probe differential campaign

`marc_fuzz_lzss_encoder_probe` compares production HashChain with explicit
no-probe and exhaustive typed encoders, validates token fields, independently
reconstructs overlapping matches and checks statistics partitioning. Inputs
larger than 512 bytes are rejected before work. Raw bytes and their four-symbol
projection share the same bounded parameters and fixed arrays. An initial
all-position shared-prefix fixture requires positive pruning and exhaustive
match equality; greedy encoding is not required to query every position.

On 2026-09-23, the Windows Clang 22.1.3 ASan/UBSan build completed 10,000
runs with `-seed=20260923 -max_len=512 -len_control=0 -timeout=5
-rss_limit_mb=512`. The corpus was generated locally from the repository's
hand-authored shared-prefix fixture and retained under ignored `out/build/`.
Both Windows annotation compatibility options were ON; matching ASan runtime
PATH was process-local. The final run exited zero without a sanitizer finding.
An earlier harness-only positive-pruning assumption was corrected as recorded
in CR-1323. Large-distance, serialized, whole-codec and measurement gates remain
separate; this finite campaign is not proof of safety.

### FZ-0044: Private context-9 stream bounded campaign

`marc_fuzz_lzss_position_distance_stream` tests the private 2/8 + 1/9 + 3/2
strict stream decoder. The supplied input is capped at 4,096 bytes, output at
128 bytes, and each raw frame at 64 bytes; token, frame and output workspaces
are fixed arrays. Errors must commit no input or output and preserve the entire
caller output. A second path encodes at most 128 supplied bytes with fixed
eligibility 3, strictly decodes the result, then mutates one serialized byte.
It reaches valid frame and model handling even when random bytes do not form a
valid header. The ordinary-build smoke covers empty input, frame boundaries,
two-frame input and supplied-input rejection beyond the fixed cap.

On 2026-09-26 the Windows Clang 22 ASan/UBSan build completed 10,000 runs with
`-seed=20260926 -max_len=128 -len_control=0 -timeout=5 -rss_limit_mb=512`.
The ASan runtime directory was added to process-local PATH. The run exited
zero with no sanitizer finding. This bounded private campaign does not cover
public admission or incremental chunking.

## Finding retention policy

Do not treat a disappearing crash as sufficient. Minimize each finding, add the
smallest input or an equivalent explicit assertion to a permanent GoogleTest
regression, record the stable error/atomicity expectation, and then retain the
minimized file in the corpus. Record externally sourced corpus provenance and
license before adding it; generated and hand-authored inputs are preferred.
Corpus paths are marked binary in `.gitattributes`; do not enable text or line
ending normalization for individual seeds or minimized reproducers.

### FZ-0045: Position-distance scratch differential coverage

Extend the existing bounded context-9 harness with frame-by-frame comparisons
between the retained transactional helper and the discardable-token-scratch
adapter. Compare token diagnostics, successful tokens and raw bytes; require
unchanged failed raw frames and storage guards. Generated valid archives and
their mutations also traverse the frame-atomic incremental decoder at different
chunk sizes with sticky-error and publication comparisons.

The 2026-09-27 bounded campaign completed the 27 boundary cases and 10,000
fuzz runs without a sanitizer finding or differential mismatch. Harness
input/output/frame bounds remain 4,096/128/64 bytes. This finite campaign is
not exhaustive malformed-stream or public-API coverage; public factory and
hash tests remain separate evidence.

### FZ-0046: Grouped literal search differential coverage

Extend the bounded position-distance stream harness with operation-level
comparison of grouped and linear literal searches while using the same
specialized distance decoder. Compare every result and operation, then final
frequency totals, interval and canonical state, including sticky failures.
The existing frame scratch, raw publication and incremental comparisons remain.

The 2026-09-27 campaign completed 27 boundary cases and 10,000 fuzz runs with
no sanitizer finding or differential mismatch. Harness input/output/frame
bounds remain 4,096/128/64 bytes. Larger model histories and rescaling are
covered separately by TVG-1164; this bounded campaign is not exhaustive.

### FZ-0047: Transactional reference oracle for incremental publication

TVG-1165 strengthens the stream harness: incremental output must equal exactly
the raw prefix accepted by the retained transactional frame decoder, even on
malformed streams. Agreement between chunk schedules alone is insufficient.
The ordinary smoke and sanitizer initializer add 738 generated serialized cases
covering every truncation and per-byte high-bit mutation of a two-frame stream,
plus intact and trailing-byte cases. The prior 27 raw boundary cases remain.

Harness bounds remain 4,096 input bytes, 128 output bytes and 64 bytes per frame.
Random inputs above 128 bytes exercise supplied-stream decoding only; generated
raw round trips remain capped at 128 bytes. Larger histories and model rescaling
retain their separate test coverage. This oracle shares first-party format and
reconstruction primitives and is not external interoperability evidence.

On 2026-09-27 the extended harness completed the 27 raw and 738 serialized
cases, followed by 100,000 ASan/UBSan fuzz runs, with no reported sanitizer
finding or differential mismatch. The ordinary smoke and documentation checks
also passed. No production codec changes were made; this finite campaign does
not establish exhaustive malformed-stream coverage or replace public API tests.

### FZ-0048: Encoder scratch versus transactional tokenization

Add direct indexed scratch/reference comparisons over at most 64 raw bytes,
using full and data-dependent token capacities. Compare every diagnostic and
token field, including guards and failed transactional output. The existing
generated archive comparison now exercises indexed scratch encoding against
the exhaustive reference path, while decoder publication checks remain active.

On 2026-09-27 the extended harness passed its 27 raw and 738 serialized cases
and 10,000 ASan/UBSan fuzz runs with no reported finding or differential mismatch.
The original input/output/frame bounds remain unchanged. This bounded campaign
does not replace full-frame differential measurements or public API tests.

### FZ-0049: Private entropy scratch versus transactional payload writing

Extend the position-distance stream harness with direct entropy comparisons
over bounded literal-operation sequences, data-selected malformed fields,
actual-size memory budgets and short output capacities. Compare every result
counter and descriptor, successful bytes, output guards and unchanged failed
transactional output. Existing generated archive comparisons exercise the new
private frame writer through incremental encoding, including match operations.
The existing decoder publication and canonical checks remain active.

On 2026-09-27 the extended harness passed its 27 raw and 738 serialized boundary
cases and 10,000 ASan/UBSan fuzz runs with no reported finding or mismatch.
Bounds remain unchanged; distance-width/rescaling and late frame rejection
have separate deterministic tests. This finite campaign is not exhaustive.

### FZ-0050: Short-prefix probe order against retained and exhaustive search

Extend the bounded position-distance harness with probe-first, prefix-first
and exhaustive match comparisons at every position of up to 64 input bytes.
Select window and maximum match length from input bytes within valid bounds.
Reuse the existing bounded workspace; generated token/archive, malformed
decode, guard and publication comparisons remain active.

On 2026-09-27 the harness passed 27 raw and 738 serialized initial cases and
10,000 ASan/UBSan fuzz runs without a reported finding or mismatch. Full-frame
boundaries and greedy eligibility variants retain deterministic tests; this
finite campaign does not establish exhaustive coverage.

### FZ-0051: Integrated compact dual-prefix search

Run the existing position-distance harness against the integrated compact
finder. Its per-position comparisons now cover dual-prefix queries, retained
three-byte queries and exhaustive search; incremental encoder/decoder,
malformed input, guard and failed-frame publication checks remain active.

On 2026-09-28, the harness passed 27 raw and 738 serialized initial cases and
10,000 ASan/UBSan runs without a reported finding or mismatch. Full-frame
compact sentinel and exact workspace-limit boundaries have deterministic
tests in TVG-1172. Container annotations remain disabled in the compatible
build; this finite campaign is not exhaustive decoder coverage.

After the final guarded-probe, nearest-prefix entry and insertion-loop
refinements, rebuild and repeat all 765 initial cases and 10,000 runs with a
different seed. The final source again passes without a finding or mismatch;
initial and final campaign artifacts are retained separately.

### FZ-0052: Integrated position-distance validation consolidation

Rebuild the existing position-distance ASan/UBSan harness after integrating
DD-1309's validated-field check consolidation. On 2026-09-28, all 738
serialized boundary cases, 27 raw boundary cases and 10,000 generated runs
pass without a reported finding or mismatch. Reference encoding, incremental
stream decoding, malformed input, guards and failed-frame publication checks
remain active. Compatible container annotations remain disabled. This finite
campaign complements TVG-1176; it is not exhaustive malformed-input coverage.


### FZ-0053: Private four-MiB decoder and finder-scratch stream boundaries

Add optional private stream decoder and encode-stream differential libFuzzer
targets under DD-1381. The decoder compares borrowed/owned paths, arbitrary
malformed bytes, partial/zero output, terminal errors and generated-stream
publication frontiers; a guaranteed failed second frame releases only earlier
committed bytes. The encoder compares finder-scratch ownership to exhaustive
scalar encoding and round-trips through the retained decoder, with guards and
all three greedy eligibility policies.

On 2026-10-01 each target passes forty initial boundary seeds and 2,000
ASan/UBSan/libFuzzer executions with no reported finding or differential mismatch.
Declared seeds are 138101 and 138102. Separately instrumented CMake executables
repeat 2,000 executions each with seeds 138111 and 138112, for 8,000 total runs
across both build routes. Decoder input/frame bounds are 8,192/21
bytes; encoder raw/frame bounds are 128/1..64 bytes. Instrumented CMake targets
build separately; runtime annotation settings remain consistent. Preserve all
initial and evolved corpus cases and logs. Finite small-frame runs complement
existing deterministic four-MiB/long-distance tests and do not establish
exhaustive or full-window decoder coverage. Public factory, full-suite and
revision-specific external qualification remain separate gates.


### FZ-0054: Private four-MiB guarded C adapter boundaries

Under DD-1383 add an optional C-boundary target with the retained scalar decoder
as differential oracle. Generate valid literal streams independently of C
encoding, record frame ends and reject truncated/forced failed-second-frame
publication beyond the complete-frame frontier. Also exercise C encode chunk
determinism, round-trip, exact/one-under factory budgets, metadata/handle aliases
and full-capacity workspace tails. Terminal results and output sentinels are
checked across partial/zero buffers and EndInput/Flush.

On 2026-10-01 the separately instrumented CMake target passes fifty initial
boundary seeds and 2000 ASan/UBSan/libFuzzer executions with seed 138301, without
a reported finding, guard/differential/frontier mismatch or artifact. Initial
seed hashes remain unchanged. Preserve initial/evolved cases and all logs.
Malformed input is bounded to 8192 bytes, generated raw to 64, frame capacity
to 21, decoded/encoded results to 128/2048 bytes and calls to 32768. Compatible
container annotation settings remain unchanged. Finite small-frame fuzzing
complements deterministic full-window/distance tests; it does not establish
exhaustive or full-window fuzz coverage or public/external qualification.


### FZ-0055: Public four-MiB guarded C factory boundaries

Transfer FZ-0054 driving to the actual public four-MiB config/query/create and
generic process/destroy, using the selected scalar five-prefix implementation.
Compare retained scalar decoder status/counts/raw/error positions, C encode
chunk determinism, exact/one-under budgets, metadata/handle/full-tail guards and
independently generated complete-frame publication frontiers.

On 2026-10-01 the separately instrumented CMake library/target passes fifty
initial boundary seeds and 2000 ASan/UBSan/libFuzzer executions with seed 138501,
without a reported finding, artifact, guard/differential/frontier mismatch or
initial seed change. Keep all initial/evolved cases and logs. Bounds remain
8192 malformed input bytes, 64 generated raw bytes, frame capacity 21, decoded/
encoded limits 128/2048 and 32768 calls. Compatible annotation settings remain
unchanged. Finite small-frame coverage complements full-window/distance tests
and does not establish exhaustive or external qualification.

### FZ-0056: Private eight-MiB scalar Range operations

Date: 2026-10-03. The independent harness in
`tests/lzss_position_distance_8m_range_fuzz.cpp` instantiates two private
operation decoders on identical borrowed payloads and verifies deterministic
results/fields, unchanged caller operation on failure, sticky errors, consumed
extent and bounded progress. It calls canonical finish only after the declared
decisions are consumed. No operation or raw frame is published by this harness.

Input is a two-byte little-endian decision count followed by up to2048 payload
bytes, count0..4096, at most4096 successful field calls. Initial seeds include
literal41, all literals, all distance classes, invalid length/distance and an
incomplete token; their payloads come from TVG-1285's independent mathematical
writer. These bounds intentionally exclude large rescaling sequences, which
are covered by permanent focused tests instead. Reproducible standalone build:
compile the harness, private eight-MiB Range decoder and core limits together
with C++20, src includes and `-fsanitize=fuzzer,address,undefined`, using a
compatible compiler/runtime. This is not a public codec target.

The final bounded campaign uses seed1418, max_len2050, timeout10 and 10000
executions under ASan/UBSan/libFuzzer, with leak detection disabled. A prior
10000-execution campaign and all initial/evolved corpus entries are retained.
Both complete without reported sanitizer finding, crash, timeout or invariant
failure. Initial standalone fuzz linkage needed the runtime-compatible CRT;
its failed build output is retained and is not counted as a fuzz execution.
Coverage is finite and operation-only: no frame/history/raw reconstruction,
stream chunking, publication-frontier or external qualification is claimed.

### FZ-0057: Private eight-MiB transactional token assembly

Date: 2026-10-03. The standalone harness in
`tests/lzss_position_distance_8m_tokens_fuzz.cpp` supplies declared raw/token/
event/decision counts, independent output/scratch capacities and payload to
two token bridge calls. Compare results, entropy counters and both complete
buffers; require unchanged caller output on failure and untouched tails.
Only after success independently replay tokens into a bounded raw array,
checking distance/history/overlap/length/output extent. That oracle does not
constitute production raw/frame publication or incremental stream coverage.

Input has ten metadata bytes (four little-endian uint16 counts, two byte
capacities) followed by at most2048 payload bytes. Bounds are raw512, tokens64,
events320, decisions2112 and capacities66; decoder frame/block/input limits
and a65536-byte internal budget are explicit. Initial finite mathematical
recipes from TVG-1286 provide complete and zero-output-capacity seeds, including
overlap and invalid history/length/endpoint cases. Large-history boundaries
remain permanent focused tests rather than fuzz claims.

Compile harness, private token bridge, private Range decoder and core limits
together with C++20/src includes and `-fsanitize=fuzzer,address,undefined`, using
a compatible runtime. The bounded campaign runs10000 executions, seed1419,
max_len2058, timeout10 under ASan/UBSan/libFuzzer with leak detection disabled.
It completes without reported crash, timeout, sanitizer finding or invariant
failure. Preserve initial/evolved corpus and logs. No exhaustive coverage,
production raw/frame/stream decoder, performance or external gate is implied.

### FZ-0058: Private eight-MiB finite-frame raw publication

Date: 2026-10-03. The independent harness in
`tests/lzss_position_distance_8m_frame_decoder_fuzz.cpp` supplies four bounded
buffer capacities and a serialized frame to two private frame decoders. Compare
results, full raw/typed workspaces and deterministic output. Every failure must
preserve the entire caller raw buffer and original layout object representation,
report no consumed/produced frame bytes and leave guards outside spans intact.
Only successful frames are checked against an independent bounded token/raw
oracle with explicit literal, history, match and overlap-copy rules.

Input is eight capacity bytes (four little-endian uint16) and at most2128 frame
bytes. Bounds are raw512, tokens64, events320, decisions2112, token capacities66,
raw capacities514; serialized payload policy is2048 and internal budget65536.
Initial mathematical prefixes/payloads from TVG-1287 include complete and zero
raw-output-capacity cases. Header truncation and large-history boundaries are
covered by permanent focused tests, not claimed as unbounded fuzz coverage.

Compile harness, private frame decoder, prefix preflight, token helper, Range
decoder and core limits together with C++20/src includes and
`-fsanitize=fuzzer,address,undefined`, using a compatible runtime. The bounded
campaign runs10000 executions with seed1420, max_len2136 and timeout10 under
ASan/UBSan/libFuzzer, leak detection disabled, and finishes without reported
crash, timeout, sanitizer finding or invariant failure. Preserve initial/evolved
corpus and logs. This finite-frame boundary is not an incremental stream
publication frontier, exhaustive proof, public codec or performance/external gate.

## FZ-0059: Private eight-MiB incremental scheduling and committed frontier

Date: 2026-10-03. The harness in
`tests/lzss_position_distance_8m_stream_decoder_fuzz.cpp` accepts one schedule
selector followed by at most4096 stream bytes. Compare complete-input scheduling
against bounded partial input/output, including zero output capacity followed
by capacity, final suffixes and sticky terminal calls. Verify independent
consumption/production bounds, guarded caller tails, no zero-count Progress,
identical committed raw prefix and stable total consumption/error positions.
A separate finite traversal validates complete frames through the already
qualified helper and compares that committed frontier. This oracle shares the
finite decoder, so it tests coordinator publication/scheduling and does not
claim independent Range or raw codec conformance.

Bounds: frame/raw512, each token capacity512, serialized workspace4096,
payload policy4096, total output65536, internal budget65536, positive output
capacity1..32 after bounded starvation, and fewer than100000 calls. Initial
corpus uses independent mathematical small valid/invalid frames, empty/two
frame streams, truncations, trailing data and late canonical failure; production
encoder/serializer is not invoked. Large history remains focused tests.

Compile the harness and all private stream/frame/prefix/token/Range/core sources
together with C++20 and `-fsanitize=fuzzer,address,undefined`. The finite campaign
runs10000 executions, seed1422, max_len4097 and timeout10 with leak detection
disabled, completing without crash, timeout, sanitizer or invariant finding.
Preserve corpus and logs. This is private bounded fuzz, not exhaustive proof,
public profile/encoder admission, benchmark or external qualification.

## FZ-0060: Private eight-MiB operation encoder transaction/differential

Date: 2026-10-03. The harness in
`tests/lzss_position_distance_8m_range_encoder_fuzz.cpp` reads two little-endian
uint16 capacities followed by ten-byte field records: kind byte, context uint16,
alphabet uint16, value uint32 and width byte. Decode these explicitly; never
cast input bytes to a native operation. Bound input to2564 bytes/256 operations,
both payload capacities0..512, payload/block policy512 and internal budget65536.
Two calls compare result fields, caller/scratch bytes and descriptor values;
failures require committed0, whole caller-output invariance and the actual
original descriptor object bytes unchanged. Success also requires guarded
unused tails and operation equality/full finish through the qualified decoder.

Initial independent mathematical seeds include literals, lengths/classes,
invalid length/endpoint extras and zero-output/scratch capacities. The first
campaign stops during initial seed loading at a padding-unsafe harness invariant;
retain input and logs. A diagnostic executes the fixed input once and does not
constitute fuzz coverage. Compare unrelated descriptors by value, and snapshot
original bytes with memcpy rather than a typed copy. Preserve that zero-scratch
case as a permanent focused regression. Production encoder code is unchanged.

Compile harness/encoder/decoder/limits together with C++20 and
`-fsanitize=fuzzer,address,undefined`. The corrected campaign completes10000
executions, seed1424, max_len2564, timeout10, leak detection disabled, with no
crash, timeout, sanitizer or invariant finding. Keep both campaigns, diagnostic
and corpus. Coverage is finite operation syntax/transaction/differential only;
raw history, frame/stream encoder, public admission, performance and external
qualification remain later gates.

## FZ-0061: Private eight-MiB validated mapper transaction and differential

Use an explicitly decoded16-byte header: output/scratch capacities, declared
T/E/D/F as six little-endian uint16 values, then window as little-endian uint32.
Follow with ten-byte token records: kind, literal, distance uint32, length uint32.
Never cast fuzz bytes to native tokens. Bound input to656 bytes/64 tokens,
operation capacities0..320, raw/frame/block policy512 and internal budget65536.
Independent equation seeds cover literal contexts, short overlap, length classes,
reachable history, before-history references, window errors and zero capacities.

Two mapper calls compare result fields and zero commitment on failure. Require
whole caller-output and actual original metadata bytes unchanged, using memcpy
snapshots; compare unrelated operation/metadata instances by semantic fields.
Success checks the independent field oracle, guarded caller/scratch tails,
qualified Range encoding and token decoding back to the original sequence.

Compile harness, mapper, token helper, Range encoder/decoder and limits together
with C++20 and `-fsanitize=fuzzer,address,undefined`. The campaign completes10000
executions, seed1425, max_len656, timeout10, leak detection disabled, with no
crash, timeout, sanitizer or invariant finding. Preserve corpus and logs.
Coverage is bounded finite mapping/history/transaction/differential; complete
raw/frame/stream encoding, full composed ownership, performance, external
verification and public admission remain separate gates.

## FZ-0062: Private eight-MiB exhaustive raw reference parser differential

Explicitly decode a16-byte header: output/scratch capacities as little-endian
uint16, window uint32, maximum length/retained bytes/already-committed raw as
three uint16, then minimum length and flags bytes. Remaining bytes are finite
raw input, capped at128; both token capacities are0..128. Frame/block policy128,
internal budget32768. Bound campaign input to144 bytes. Independent seeds cover
empty/one-byte input, short literal-only runs, overlap, patterns, nearest ties,
farther longer matches, distinct bytes, windows, maximum3 and invalid parameters.

Two parser calls compare semantic results/tokens and guarded unused tails.
Failures require committed0, whole original caller-output bytes and original
metadata bytes unchanged. Success compares the independent descending-length
oracle and forward reconstruction, then mapper/Range encoding/token decoding
and raw reconstruction. No native serialization cast is used.

Compile harness/parser/mapper/token/Range encoder/decoder/limits together with
C++20 and `-fsanitize=fuzzer,address,undefined`. Preserve initial successful
10000 executions. After correcting the simultaneous-result memory reservation,
a separate final campaign completes10000 executions, seed1426, max_len144,
timeout10, leak detection disabled, with no crash, timeout, sanitizer or
invariant finding. Coverage is finite reference parsing/transaction/differential;
indexed speed, complete frame/stream encoding, composed owner memory, external
verification and public admission remain separate gates.

## FZ-0063: Private eight-MiB indexed parser transaction and differential

Extend FZ-0062's explicitly decoded header with workspace excess-word uint16;
the18-byte header is followed by at most128 raw bytes. Supplied live uint32
workspace has65536+(field modulo129) words. Both token capacities are0..128,
frame/block128 and internal budget1048576. Max input146 bytes. Independent
seeds retain raw/tie/overlap/window/parameter/capacity cases; mutate all header
fields and raw bytes without casting them to native codec objects.

Two indexed calls compare result/token semantics, complete initialized index
contents and guards. Failures commit zero and preserve original caller output
and metadata bytes; private scratch/workspace are discardable. Success also
compares the unchanged exhaustive parser and independent descending-length
oracle, then mapper/Range/token/raw reconstruction. Workspace unused tails
remain guarded and the entire capacity is charged.

Initial harness migration left one call targeting the exhaustive parser, so its
workspace stayed untouched and the equality invariant stopped during seed load
on a one-byte raw seed. Keep input/log and record this as an incomplete campaign,
not a codec defect or completed coverage. Correct that call to the indexed
parser; the same seed remains in the corrected corpus. Production code unchanged.
Compile all helpers/harness with C++20 and `-fsanitize=fuzzer,address,undefined`.
The corrected campaign completes10000 executions, seed1427, max_len146,
timeout10, leak detection disabled, with no crash, timeout, sanitizer or invariant
finding. Indexed speed, full ownership, complete frame/stream encoding,
external verification and public admission remain separate gates.

## FZ-0064: Private eight-MiB stream-header/frame-prefix serializer transaction

Date: 2026-10-03. Independent14 frame-prefix fixtures seed a128-byte input shape
with trailing stream-parameter, capacity, sequence/committed and retained-owner
controls. Full-capacity guard snapshots require zero commitment and unchanged
caller bytes/bytes_written for every error. Two identical calls compare complete
output, metadata and stable errors; query and entry results agree. Success
checks untouched tails and unchanged private stream-parser/prefix-preflight
differential. Negative payload recipes are prefix checks only, never evidence
of valid complete frames. Live alias and overflow boundaries are additionally
covered by TVG-1296's deterministic cases.

Compile harness and all serializer/preflight/limit helpers with C++20 and
`-fsanitize=fuzzer,address,undefined`. Campaign completes10000 executions,
seed1429, max_len128, timeout10, leak detection disabled, with no crash, timeout,
sanitizer or invariant finding. No complete frame publication, public admission,
external interoperability or performance claim follows from this campaign.

## FZ-0065: Private finite eight-MiB complete-frame encoder transaction

Date: 2026-10-03. Bounded1..128 raw bytes and eight control bytes mutate token,
operation, frame/payload/caller capacities, stream minimum, sequence, internal
budget and retained-owner bytes. First-party one-byte, repetitive, pattern and
binary seeds include both success and shortage controls. Two identical calls
compare stable result, full caller bytes and size. ANY failure commits zero,
preserving every caller byte, native layout snapshot and bytes_written. Success
guards unused output tails and round-trips through the unchanged complete
private frame decoder, including raw-size and exact raw-byte equality.
Deterministic TVG-1297 cases supplement live alias, exact ledger and overflow
boundaries and independently generated mathematical frame bytes.

Compile harness and every encoder/decoder dependency with C++20 and
`-fsanitize=fuzzer,address,undefined`. Campaign completes10000 executions,
seed1430,max_len136,timeout10,leak detection disabled, without crash, timeout,
sanitizer or invariant finding. This does not qualify large8MiB capacities,
physical peak, performance, streaming coordinator or public/external admission.


## FZ-0066: Private known-size eight-MiB stream encoder scheduling

Date: 2026-10-03. Mutate1..16-byte frame sizes, bounded0..64 raw bytes, original
size discrepancies, token/operation/private storage capacities, input chunks,
output capacities and final-on-data/empty-final schedules. Compare full and split
encoding for identical terminal categories and published bytes. Verify counts,
no zero-count Progress, guarded output tails and sticky terminal status/position.
Feed each published byte sequence through the unchanged private stream decoder;
its successful raw output must be an exact prefix of the supplied raw input.
Successful complete encoding must decode all raw bytes and reach end-of-stream.
Deterministic TVG-1300 tests additionally cover live alias and exact budget bounds.

Compile the harness and every encoder/decoder dependency with C++20 and
`-fsanitize=fuzzer,address,undefined`. Complete10000 executions,seed1433,
max_len72,timeout10,leak detection disabled, without crash, timeout, sanitizer
or invariant finding. These are small bounded stream schedules; no large8MiB
coordinator fit, physical peak, performance or public/external admission follows.


## FZ-0067: Private typed-token Range query and transactional encoding

Date: 2026-10-03. Derive bounded valid token sequences, including generic
length3/4, uniform length extras and adaptive distance extras. Mutate late token
fields, declared E/D/F, parameters, memory limits and both payload capacities.
For success compare exact bytes against the unchanged mapper/operation encoder,
decode all token fields with the unchanged consumer and repeat deterministically.
For every error require zero committed bytes, unchanged whole caller payload
and descriptor. Success also preserves caller tail bytes beyond counted P.

Compile the harness and all helper/reference dependencies with C++20 and
`-fsanitize=fuzzer,address,undefined`. Complete10000 executions,seed1436,
max_len128,timeout10,leak detection disabled, without crash, timeout, sanitizer
or invariant finding. This finite small-input campaign does not qualify a new
frame/stream coordinator, universal8MiB fit, physical peak or performance.


## FZ-0068: Isolated private token-frame query/encoder publication

Date: 2026-10-03. Mutate bounded1..128 raw bytes, token/index/private payload/
caller output capacities, parameters, sequence, retained-owner and memory limits.
Repeat encoding and compare status, exact counts/aggregate and complete output.
For every failure require zero commit, whole output guards, original layout and
written count. For success compare every complete frame byte against the unchanged
operation/frame reference using its own workspace and explicit sufficient policy;
decode through the unchanged frame consumer with all retained comparison owners
and a separate sufficient policy, and compare every raw byte. Verify
untouched caller tails. Unit tests separately qualify whole aliases, query-only
mutation and exact full owner budgets with all comparison owners retained.

Compile harness and all new/reference/decoder dependencies with C++20 and
`-fsanitize=fuzzer,address,undefined`. Final campaign completes10000 executions,
seed1438,max_len136,timeout10,leak detection disabled, without crash, timeout,
sanitizer or invariant finding. Initial campaign is retained separately. These
bounded finite cases do not qualify streaming schedules, arbitrary8MiB owner
admission, physical peak, performance or external/public integration.


## FZ-0069: bounded sixteen-MiB finite frame transaction

Date: 2026-10-05. tests/lzss_position_distance_16m_frame_fuzz.cpp mutates complete finite frame bytes and four private/public capacity fields. Input is at most 2136 bytes, raw extent at most 512, token count at most 64, events at most 320 and decisions at most 2176. Seeds include valid literals, overlap and short length families, plus canonical invalid history, length and distance recipes. Decode twice and compare deterministic result categories, exact counts, raw buffers and private tokens. Every failure requires zero publication and unchanged whole caller raw output and layout; every success is compared with independent scalar raw reconstruction and untouched tails.

The actual harness and every reachable helper are compiled with address/undefined-behavior instrumentation and libFuzzer. Complete 10,000 runs, seed 1477, maximum length 2136 and per-input timeout 10, without crash, timeout, sanitizer or invariant finding. Leak detection is disabled. CMake includes an optional target under the existing fuzz configuration; this local campaign compiled the reachable helper set directly. Directed native tests separately cover full raw-frame far references. The bounded campaign does not establish maximum-window fuzz coverage, streaming schedules, public memory admission, peak memory, encoder behavior or release readiness.


## FZ-0070: bounded sixteen-MiB incremental scheduling

Date: 2026-10-05. tests/lzss_position_distance_16m_stream_fuzz.cpp mutates at most 4097 bytes including a schedule selector. Borrowed workspaces and decode limits bound raw frames to 512 bytes, tokens to 512 per frame and total raw output to 65,536 bytes. Compare unsplit and split/output-starved schedules for raw bytes, consumed extent, status and stable error category/position. Require committed raw bytes to equal a separate finite-frame traversal, which shares the qualified payload decoder but has no stream/draining state. Verify untouched caller output tails, progress counts and repeated terminal behavior.

Actual harness and all reachable helper sources are compiled with libFuzzer and address/undefined-behavior instrumentation. Complete 10,000 runs, seed 1478, maximum length 4097, per-input timeout 10, without crash, timeout, sanitizer or invariant finding. Leak detection is disabled. Seeds include independent empty, single/two-frame, truncated, trailing and late malformed streams. Local compilation uses the reachable source set directly; an optional CMake target is registered separately. Directed native tests cover maximum raw-frame extent and one-byte output. This bounded scheduling campaign does not prove maximum-window fuzz coverage, incompressible storage, public resource admission, encoder behavior or release readiness.


## FZ-0071: bounded sixteen-MiB exhaustive parser transaction

Date: 2026-10-05. Mutate at most 128 raw bytes and a 16-byte parameter/capacity header. Compare selected tokens with the independent descending-length oracle, repeated results, exact counts and scalar raw reconstruction. Guard caller tokens and metadata on every refusal. Valid results additionally traverse distinct sixteen-MiB mapping, Range coding and token decoding and compare every raw byte. Capacities, window, match bounds, flags and resource/count overflow are bounded or rejected before traversal.

Actual harness and all reachable helpers are freshly compiled with libFuzzer and address/undefined-behavior instrumentation. Complete 10,000 runs, seed 1479, maximum input length 144 and per-input timeout 10, without crash, timeout, sanitizer or invariant finding. Leak detection is disabled. This bounded reference/parser differential does not establish indexed lookup, maximum incompressible performance, physical peak, whole-frame/stream encoding, public resource admission or release readiness.


## FZ-0072: bounded sixteen-MiB indexed parser differential

Date: 2026-10-05. Mutate at most 128 raw bytes and an 18-byte parameter/capacity header. Compare exact indexed tokens with the exhaustive parser and independent descending-length oracle. Check repeated results, private workspace guards, exact counts, scalar overlap reconstruction, and complete caller/metadata preservation on every refusal. Valid results also traverse sixteen-MiB mapping, Range coding and token decoding.

The actual harness and reachable helpers are freshly compiled with libFuzzer and address/undefined-behavior instrumentation. Complete 10,000 runs, seed 14800, maximum input length 146 and per-input timeout ten, without crash, timeout, sanitizer or invariant finding. Leak detection is disabled. This bounded differential does not qualify maximum incompressible encoding, physical peak of a complete encoder, prepared ownership, public resource admission or release readiness.

The scaled index uses 1,048,576 heads. Its bounded fuzz fixture admits the real complete workspace under a 16-MiB harness-only budget; this does not change any public resource limit. Preserve the earlier 65,536-head campaign separately.


## FZ-0073: bounded sixteen-MiB token-direct Range differential

Date: 2026-10-05. Construct bounded typed-token recipes from inputs of at most 128 bytes. Mutate late token kind/length/distance, declared event/decision/raw counts, flags, memory and output/scratch capacities. Valid tokens traverse token-direct and materialized-operation Range paths, compare every payload byte, decode and compare every token, and repeat for determinism. Every refusal preserves the complete caller output and descriptor with zero committed bytes.

The actual harness and reachable helpers are freshly compiled with libFuzzer and address/undefined-behavior instrumentation. Complete 10,000 runs, seed 1481, maximum input length 128 and per-input timeout ten, without crash, timeout, sanitizer or invariant finding. Leak detection is disabled. Directed native tests separately cover the high reachable distance classes and full-frame far references. This bounded differential does not qualify complete frame/stream encoding, prepared lifetime, physical peak of a public encoder, public memory policy or release readiness.


## FZ-0074: bounded sixteen-MiB complete-frame differential

Date: 2026-10-05. Use an eight-byte control prefix and at most 128 raw bytes. Vary full private token/frame/payload capacities, caller capacity, sequence, parameters, memory budget and retained owners. Repeat token-direct frame encoding, compare the materialized-operation complete frame and decode every raw byte. Every refusal preserves the complete caller output, layout and byte count with zero committed bytes. Charge actual bounded harness and comparison owners; use a separate sufficient consumer budget rather than hide them behind a mutated encoder budget.

The actual harness and reachable helpers are freshly compiled with libFuzzer and address/undefined-behavior instrumentation. Complete 10,000 runs, seed 1482, maximum input length 136 and per-input timeout ten, without crash, timeout, sanitizer or invariant finding. Leak detection is disabled. Directed native tests and a separate full-size raw experiment supplement this bounded campaign. It does not establish owning/prepared generations, full streaming behavior, public memory policy, CLI admission or release readiness.


## FZ-0075: bounded sixteen-MiB compact parser and byte reader

Date: 2026-10-05. A four-byte control prefix selects bounded window and match lengths, followed by at most 128 raw bytes. Compare private compact output against independently selected longest/nearest tokens, check all unused byte tails, and verify one-below complete ownership refusal before private byte/index mutation. Independently interpret arbitrary serialized tags and little-endian fields, checking cursor and previous-token invariance on malformed or truncated records. Reader syntax success does not authorize history-valid decoding or frame publication.

The actual harness and reachable helpers are freshly built with libFuzzer and address/undefined-behavior instrumentation. Complete 10,000 runs, seed 1483, maximum input length 132 and per-input timeout ten, without crash, timeout, sanitizer or invariant finding. Leak detection is disabled. Directed tests and a separate full-size actual-input differential supplement this bounded campaign. Compact Range/frame transactions, owning/prepared storage and full-stream/public integration remain separate work.


## FZ-0076: compact sixteen-MiB complete-frame and record Range differential

Date: 2026-10-05. Eight control bytes select bounded private capacities, caller capacity, sequence, parameters, memory and retained owners, followed by at most 128 raw bytes. Repeat compact complete-frame encoding, compare the materialized reference and reconstruct every raw byte. Any failure preserves whole caller output, layout and written count, with zero committed bytes. Charge actual bounded backing owners and conservative harness controls; references and consumers use a separate sufficient budget that includes still-live encoder/reference ownership.

Independently parse the same bytes as possible canonical records using explicit tag/length/little-endian equations. Compare syntax-complete compact Range encoding with the unchanged typed-direct encoder, including invalid history/count refusal. Malformed or truncated records cannot publish a payload. Refusals preserve the complete previous descriptor and every caller output byte. Canonical literal and short-match seeds supplement the arbitrary syntax mutations.

The actual harness and reachable helpers are freshly built with libFuzzer and address/undefined-behavior instrumentation. Complete 10,000 runs, seed 1484, maximum input length 136 and per-input timeout ten, without crash, timeout, sanitizer or invariant finding. Leak detection is disabled. Directed mathematical/full-frame/prefix tests and a separate actual full-size input experiment supplement this bounded campaign. Owning/prepared generations, whole-stream chunking and public integration remain pending.


## FZ-0077: sixteen-MiB owning stream allocation and publication

Date: 2026-10-05. Four control bytes select bounded frame size, call chunks, allocation refusal and memory policy, followed by at most 128 raw bytes. Vary Flush, ResetBlock, EndInput suffixes, partial output and terminal calls. Check the core progress/count contract, unchanged unused output tails, sticky errors/end states and actual destruction of all owned blocks.

Independently decode every fully published frame with the unchanged finite decoder. An error may preserve the header and earlier complete frames, but cannot leave a prefix or fragment of a failed frame. Every reconstructed byte agrees with the accepted raw prefix; successful termination reconstructs all input.

The actual harness and reachable helpers are freshly built with libFuzzer and address/undefined-behavior instrumentation. Complete 10,000 runs, seed 1485, maximum input length 132 and per-input timeout ten, without crash, timeout, sanitizer or invariant finding. Leak detection is disabled. Directed allocation/publication tests and separate actual full-size native experiments supplement this bounded campaign. Public memory policy, factory/CLI and exchange integration remain pending.


## FZ-0078: sixteen-MiB public C construction, dispatch and borrowed workspaces

Date: 2026-10-05. Four control bytes followed by at most 64 raw bytes select memory/ABI/reserved configuration refusals, call chunks, output capacities, decoder workspace aliases and late payload corruption. Actual public factory/process/destruction calls exercise a fully instrumented production library. Successful streams reconstruct every byte; rejected construction leaves no handle; any encoder failure has at most the fixed header published. A corrupted single frame produces zero raw bytes. Check committed counts, unused output tails and sticky error state.

Complete 1,000 libFuzzer runs, seed 1486, maximum input length 68 and per-input timeout ten, without crash, timeout, sanitizer or invariant finding. Address/undefined-behavior instrumentation covers the production library and harness; leak detection is disabled. Directed multiple-frame/allocation/alias tests and separate full-size native public experiments supplement this bounded campaign. CLI and exchange integration remain pending.

## FZ-0079: 64KiB native position-distance rANS public lifecycle

The new public harness exercises factory/process/destruction in a fully
instrumented production static library. Compare whole and one-byte encode,
roundtrip arbitrary bytes, mutate model/header/payload state, truncate and
compare whole versus split decode status, position and published bytes.
Guard output tails and require stable terminal states and progress counts.
The descriptor/payload mathematical harnesses each complete 10,000 runs.
The public ASan/UBSan campaign completes 10,000 runs (seed 3661313838), with
an additional 1,000-run campaign (seed 1528) for full-alphabet and
512-byte inputs.
Directed frame/API tests additionally verify late-frame nonpublication and
allocation rollback. These finite campaigns do not establish exhaustive
malformed-input coverage.


## FZ-0080: private 4MiB native position-distance rANS layers

Date: 2026-10-08. ASan/UBSan descriptor and payload harnesses each complete
10,000 unseeded runs and10,000 additional runs seeded from200 independent
valid fixtures. Valid seeds reach canonical model and symbol decoding paths.
Typed-token and streaming harnesses each complete10,000 runs, with bounded
inputs/output and malformed mutations; no sanitizer or invariant finding.
Directed tests additionally cover late-frame quarantine and owner allocation
rollback. This is private-layer evidence, not public factory/CLI fuzzing,
release admission or exhaustive malformed-input coverage.

## FZ-0081: full-literal 4MiB position-distance rANS public lifecycle

Date: 2026-10-08. Descriptor and payload harnesses each complete 10,000
ASan/UBSan runs seeded with 200 independently generated valid fixtures.
Typed-token and stream harnesses each complete 10,000 runs. The public
harness additionally completes 10,000 runs through the actual C factory,
process dispatch and destruction in the fully instrumented production
static library. No sanitizer or invariant finding occurs.

The public harness compares whole and one-byte encoding, roundtrips bounded
inputs, mutates and truncates streams, and compares whole versus split
decode status, error position and committed bytes. It guards unused output
tails, progress counts and sticky terminal states. Directed finite tests
supplement this campaign with late-frame quarantine and allocation rollback.
These campaigns do not establish exhaustive malformed-input coverage or
exchange/release admission.

## FZ-0082: private eight-MiB native rANS layers

Date: 2026-10-08. Descriptor and payload ASan/UBSan harnesses each complete
10,000 runs seeded from200 independent valid fixtures, seed1536. Typed-token
and split-stream harnesses each complete10,000 runs with the same explicit
seed. All reachable private helper sources are instrumented; no sanitizer
or invariant finding occurs. Descriptor limits include the full9F count
ceiling so maximum-count valid seeds reach canonical model validation.

Check unchanged failed descriptor/symbol destinations, canonical model
reserialization, bounded payload parsing, reconstructed tokens and whole
versus split stream status/positions/publication. Directed finite tests
separately cover maximum-frame distance recipes, late-frame quarantine and
allocation rollback. This is private-layer evidence; public C factory/CLI
fuzz qualification and exhaustive malformed coverage are not claimed.

## FZ-0083: eight-MiB public owning C boundary

Date: 2026-10-08. The public owning C factory and all reachable production
sources are built with ASan/UBSan. Complete10,000 runs with seed1536 and no
sanitizer or invariant finding. Compare whole and split output/status/error
positions; check immutable ended/error states, output guards, varied zero
capacities, Flush, arbitrary input splits and final-input handling. Directed
TVG-1401 tests separately check resource-query failure destinations and
allocation rollback. This campaign does not claim exhaustive malformed
coverage, final memory-default admission or exchange qualification.
