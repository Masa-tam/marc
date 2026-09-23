# 64-KiB LZSS Contextual compression-ratio study

Status: baseline modeled-event and complete-payload diagnostic complete; no
format or public-API change (2026-09-24).

## Question and baseline

The near-term question is whether `lzss-contextual-dynamic-range` can improve
compression on the external Silesia `mozilla` member while retaining its
64-KiB frame/window resource profile. Beating the maintainer's reported gzip
result is a stretch goal, not an assumed consequence of any one change.
Selection must consider the complete Silesia corpus, encode/decode time and
queried peak workspace, not just `mozilla` bytes.

The locally held `mozilla` input has 51,220,480 bytes and SHA-256
`657fc3764b0c75ac9de9623125705831ebbfbe08fed248df73bc2dc66e2a963b`.
With the working tree at revision `be380bb8edfea819bb61286964429a508dd202a3`,
the existing MSVC Release CLI with SHA-256
`f1720f1f9f2c582f84e863b7272761ac3b2bd257c1267477d75c2a72d3629a26`
reproduced 20,085,366 bytes with:

```text
marc encode --codec lzss-contextual-dynamic-range mozilla output.marc
```

The archive SHA-256 is
`95eec4f4450a991c75af5dc805c3bde02cafb20d4f21197a55145f2338cd8317`.
The matching `marc decode --codec lzss-contextual-dynamic-range` output had
the original input SHA-256. These are local observations; generated archives
and corpus files remain ignored and are not release fixtures. The executable
predates the release-publication documentation commit; there was no executable
source change between the release tag and this observation.

The maintainer reported these byte counts for the same named corpus member.
The external commands used `-9v` for gzip, bzip2, and lzma. Exact command
lines, tool versions, and output-container details have not yet been frozen,
so external numbers are provisional comparison targets:

| Compressor/profile | Output bytes | Origin |
| --- | ---: | --- |
| bzip2 | 17,914,392 | maintainer report |
| gzip | 18,994,139 | maintainer report |
| lzma | 13,365,111 | maintainer report |
| marc Contextual Dynamic Range, 64 KiB | 20,085,366 | report and local repeat |
| marc, 1 MiB | 19,068,790 | maintainer report |
| marc, 4 MiB | 18,792,234 | maintainer report |
| marc, 16 MiB | 18,576,393 | maintainer report |
| marc, 64 MiB | 18,473,921 | maintainer report |

The reported gzip gap at 64 KiB is 1,091,227 bytes; strictly beating that
specific output would require reducing marc by at least 1,091,228 bytes.
The 16-to-64-MiB window change saves only 102,472 bytes here, so a still
wider window is not the first experiment. The 64-MiB profile holds this whole
input in one frame. Different external transforms and default presets mean
these figures do not establish that a short-match change alone can close the
gap.

## First hypothesis: parser cost does not match typed entropy cost

The current typed-token parser calls `lzss_match_is_beneficial`, shared with
the serialized-byte LZSS parser. That predicate accepts a match only when
its length exceeds `9 / 2`, the canonical Match/Literal byte-size ratio.
Every existing Contextual profile additionally fixes minimum match length 5
and maximum 258. The context backend instead codes token kind, literal,
length and distance fields separately. Therefore 3- and 4-byte matches are
not examined or emitted even when their actual encoded cost might be below
the cost of their literals. The opportunity count below establishes that
such matches exist, not that their encoding would be profitable.

The maximum length 258 is also allowed by DEFLATE (RFC 1951), so it is not
the first explanation for marc losing to the reported gzip output. A change
to match length or benefit policy must not silently reinterpret existing
dictionary/context variants or alter frozen schema-57 archive bytes.

## Staged experiment

1. Freeze the external comparison: record exact command lines, tool versions,
   output formats, input SHA-256, output sizes and hashes. Preserve the
   current marc archive as the 64-KiB control.
2. Add a private, bounded diagnostic that reports baseline Literal/Match
   counts, match-length distribution, matched bytes, and frame counts. Measure
   exact 3- and 4-byte opportunities at positions visited by the current
   greedy parser separately from all-position opportunities. Check a short
   independent matcher against exhaustive search on small synthetic inputs.
3. Count modeled symbol and bypass-bit categories. Do not present a sum of
   independent per-field bit costs as the actual range-coded payload size:
   arithmetic coding shares state across events. Use complete experimental
   payload sizes for any compression claim.
4. Only if diagnostics show a useful opportunity, prototype minimum lengths
   3 and 4 behind a private new decoder-visible variant. Specify the new
   length-value mapping, limits, model/descriptor validation, deterministic
   parse, and malformed-input handling before implementing its encoder.
   Existing profiles and schema-57 bytes remain untouched.
5. Then compare greedy, limited lookahead, and cost-aware match selection as
   separately named encoder policies. A policy that changes canonical output
   needs its own documented selection and deterministic tests, even if the
   decoder representation is shared. Do not equate the nine-byte diagnostic
   token transcript with entropy cost.

## Stage 2: exact short-prefix opportunity count

The private `marc_lzss_short_match_diagnostic` executable reads one input file
in independent 65,536-byte frames. Its fixed-capacity index records the nearest
prior equal 3-byte and 4-byte prefix at every frame position. It uses the
production exact HashChain typed-token encoder with the current 5..258 match
contract and reports both all-position and parser-visited counts. The index
was checked against exhaustive search on bounded synthetic inputs. It does
not change archive bytes, model state, public APIs, or the match finder.

On the local `mozilla` input identified above:

| Measure | Count |
| --- | ---: |
| Frames | 782 |
| Baseline literal tokens | 14,711,301 |
| Baseline match tokens | 3,065,042 |
| Baseline matched bytes | 36,509,179 |
| All positions with a 3-byte prefix match | 37,298,509 |
| All positions with a 4-byte prefix match | 31,922,000 |
| Parser-visited positions with a 3-byte prefix match | 6,710,314 |
| Parser-visited positions with a 4-byte prefix match | 4,024,782 |
| Baseline literal positions with a 4-byte prefix match | 959,740 |
| Baseline literal positions with a 3-byte but no 4-byte prefix match | 2,685,532 |

The two final rows are disjoint and together cover 3,645,272 baseline literal
positions. They are opportunities under the existing parse, not independent
replacement matches: accepting one would skip later positions and alter
model history. Prefix equality alone does not establish an encoded-bit saving.
In particular, the all-position counts include locations inside existing
matches and must not be read as a candidate token count. See BM-0099 for
distance and existing-match length distributions.

## Stage 3: modeled events and complete baseline payload

The diagnostic now passes those production tokens through the real 64-KiB
field-context mapper and Dynamic Range payload planner. It counts modeled
symbols and bypass decisions separately, then sums *complete* frame payload
sizes. The model resets per frame exactly as it does in the encoder; the
reported payload is not a sum of independently estimated field costs.

For the same `mozilla` input, the baseline comprises 17,776,343 token-kind,
14,711,301 literal, 3,065,042 length-class, and 3,065,042 distance-class
symbols. It also has 5,467,985 length and 25,892,152 distance bypass bits.
The 44,284,147 modeled operations represent 69,977,865 arithmetic decisions.
The complete Range payload is 20,022,694 bytes. Adding the 112-byte stream
header and 782 frame-header-plus-descriptor pairs of 80 bytes yields
20,085,366 bytes, exactly the measured baseline archive. A tracked single-
frame and generated two-frame smoke test each independently compare this
prediction with the CLI archive size.

These counts identify which fields dominate *decision count*, not their
compressed-bit contribution. In particular, 25.9 million distance bypass
decisions are coded with equal binary probabilities, but the count alone is
not a measured 3.2-MiB opportunity: alternative parsing changes token kinds,
lengths, distances, and subsequent model state together. The next comparison
must use a separately specified short-match representation and its complete
encoded payloads, including format overhead.

Admission requires byte-exact round trips, split-buffer determinism, strict
malformed-stream rejection, bounded workspace queries, sanitizer coverage,
and a measured size/speed/memory comparison across all twelve Silesia members.
Report the aggregate and worst-member regressions; a `mozilla` win alone is
insufficient. The reported gzip size is an aspirational reference, not a test
assertion or a promise to match a different compressor architecture.
