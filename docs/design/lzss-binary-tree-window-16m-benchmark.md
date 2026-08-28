# LZSS BinaryTree Exact 16 MiB 比較実験

## 1. 目的と位置付け

この文書は、global AVLを使う`BinaryTree Exact`と現行の
`HashChain Exact`を、大きなLZSS windowで比較する再現可能な実験契約を
定める。既存のSilesia測定ではwindowが64 KiBから1 MiBへ広がるほど両者の
速度差が縮まり、1 MiBの`mr`ではBinaryTreeが勝った。この傾向が4 MiBと
16 MiBでも続くかを、推測ではなく全Corpusの証拠として取得する。

対象はencoder-localな一致探索だけである。stream format、algorithm ID、
public C ABI、decoder、codec profile、既定戦略、`WindowAdaptiveV1`および
interoperability archiveを変更しない。結果は性能上の合否条件ではなく、
将来の探索戦略設計に使う記述的証拠とする。

この実験の`BinaryTree Exact`は
[既存のglobal AVL設計](lzss-binary-tree-match-finder.md)を意味する。
bucket内だけを木へ昇格する`HashTree Exact`および
`Sparse HashTree Exact`は別実験であり、候補へ含めない。

## 2. 固定測定matrix

完全な測定は、厳密に検証済みのSilesia Corpus全12 memberへ次を適用する。

```text
frame bytes                    16,777,216
window bytes                   1,048,576; 4,194,304; 16,777,216
strategies                     hash-chain-exact; binary-tree-exact
timed iterations               1
maximum internal buffered bytes 536,870,912
planned records                12 * 3 * 2 = 72
```

member順は既存manifestのcanonical順、window順は昇順、strategy順は
`hash-chain-exact`、`binary-tree-exact`とする。各recordは独立processで測る。
runnerはnetwork access、download、Corpus生成を行わず、全12 memberのmanifest
検証が成功するまでbenchmark processを開始しない。

1 MiB windowも16 MiB frameで再測定する。既存BM-0056の1 MiB frame結果を
混ぜるとframe reset数が異なるため、比較系列を同一条件にできないからである。

## 3. 明示的なmemory policy

16 MiB frame/windowで、64-bit `size_t`を使う現行calculatorの必要量は次である。

```text
HashChain workspace  = 524,288 + 4 * 16,777,216
                     = 67,633,152 bytes
HashChain aggregate  = 16,777,216 + 67,633,152
                     = 84,410,368 bytes

BinaryTree workspace = 29 * 16,777,216
                     = 486,539,264 bytes
BinaryTree aggregate = 16,777,216 + 486,539,264
                     = 503,316,480 bytes

explicit policy      = 536,870,912 bytes (512 MiB)
remaining headroom   = 33,554,432 bytes
```

正式な実行可否は定数式ではなくrepositoryのchecked workspace calculatorで
判定する。runnerは512 MiBを明示し、両strategyへ同じlimitを渡す。既定の
128 MiB policyを変更せず、window sizeから上限を自動拡張しない。指定値を
超えるworkspace、inputとのaggregate、算術overflowまたは不正limitは確保前に
拒否する。

## 4. benchmark executable境界

既存`--frames`の引数、既定limitおよびreportを変更しない。新しい明示経路を
次の形で追加する。

```text
marc_lzss_match_finder_benchmark --frames-limited \
  <hash-chain-exact|binary-tree-exact> <input-file> <iterations> \
  <frame-bytes> <window-bytes> <max-internal-buffered-bytes>
```

この経路は二つのglobal Exact戦略だけを受理する。limitは正の有限値として
parseし、`DecoderLimits::max_internal_buffered_bytes`だけを明示値へ置き換える。
他のhard limitは既定値を維持する。reportは少なくとも次を含む。

```text
mode=frames-limited
strategy
input_bytes
frame_count
frame_bytes
window_bytes
iterations
max_internal_buffered_bytes
workspace_bytes
token_count
literal_count
match_count
matched_bytes
token_fingerprint_sha256
measured_seconds
```

診断付きuntimed passと診断なしtimed passを分離する。timed passへsummaryの
hashing costを混ぜず、両passの入力byte数、frame数およびtoken数を一致させる。
reportの完全なsummaryとfingerprintはuntimed passから公開し、二strategy間の
全field一致は専用runnerが検証する。

## 5. runner、schemaおよびExact検証

専用runner `tools/run_silesia_binary_tree_16m_experiment.py`は独立schema
`marc-silesia-binary-tree-16m-experiment-v1`を生成する。既存の
`marc-silesia-match-finder-v1`、HashTreeおよびSparse HashTree schemaを変更
しない。

各member/windowについて両strategyの次の値がすべて一致しなければ、最終JSON
を生成しない。

```text
token_count
literal_count
match_count
matched_bytes
token_fingerprint_sha256
```

さらに各summaryについて
`token_count == literal_count + match_count`および
`input_bytes == literal_count + matched_bytes`を要求する。fingerprintは64文字の
lowercase SHA-256とする。時間は有限かつ非負、workspaceはcalculator/reportと
一致しなければならない。

最終JSONはidentity、manifest、configuration、72 records、window/strategy別
aggregateおよび比較値を保存する。aggregate throughputは各群の総input byteを
総秒数で割り、BinaryTree/HashChain比を各windowで求める。1、4、16 MiBの
token数とmatched-byte coverageも並べるが、frame/windowが変わるため異なる
window間のtoken同一性は要求しない。性能、圧縮機会または比率をrunnerの成功
条件にしない。

## 6. checkpointとbounded batch

`--checkpoint`はlocal JSONを各recordの完全な検証後にだけ原子的に置換する。
一時fileをflushし、可能な環境では同期してから同一directory内でreplaceする。
identityには次を含め、再開時は完全一致を要求する。

```text
schema and full Git revision
benchmark absolute path and SHA-256
runner and dependent source SHA-256 values
Corpus absolute path and complete verified manifest
fixed frame/window/strategy/iteration/limit configuration
platform, compiler, generator, architecture, and build label
```

復元recordも通常測定と同じvalidatorへ戻す。重複、canonical grid外、command
差異、summary矛盾、fingerprint不一致または対応するExact pairの不一致を拒否
する。

`--max-new-points N`はcheckpointを必須とし、最終`--output`と併用しない。復元済み
recordを除くbenchmark process数を数え、N件を保存したrecord境界で正常終了する。
0はidentityと既存recordの検証および進捗表示だけを行う。値は再開ごとに変更
できるためcheckpoint identityへ含めない。72件完了後、制限を外して`--output`
を指定した実行だけがcanonical順の最終v1 JSONを生成する。

## 7. 検証要件

実装は次を満たす。

1. `--frames`の既存引数、既定limitおよびreport bytesを維持する。
2. `--frames-limited`が二strategy以外、不足/余剰引数、0、overflow、不正limitを
   確保前に拒否する。
3. 16 MiBのHashChain/BinaryTree calculator値、512 MiB一致、および必要量より
   1 byte短いlimitの拒否を固定する。
4. 小さなfixtureで両strategyのtoken summary/fingerprint一致を固定する。
5. fake benchmarkを使い、runnerのcanonical順、72点計画、aggregate、Exact不一致
   拒否、checkpoint再開、identity不一致、破損recordおよびbounded batchを試す。
6. 実Corpusに依存しない単体試験をMSVC、ClangCLおよびCIで通す。
7. 実測はignored `benchmarks/results/`以下へだけ保存し、repositoryへCorpusや
   checkpointを追加しない。

## 8. 段階的実装

1. 本設計、参照、decision、test-vectorおよびclean-room記録を確定する。
2. 既存reportを保ったまま明示limit付きbenchmark経路を追加する。
3. 独立schema、厳密validator、checkpointおよびbounded batchを持つrunnerを追加する。
4. executable、runner、文書検証と両toolchainの完全試験を通す。
5. 72点をbounded batchで取得し、結果と判断を別のbenchmark記録として残す。

実測がBinaryTreeの優位を示しても、それだけでpublic selectorまたは既定戦略へ
昇格しない。採用判断にはworkspace、Corpus内の分布、既存HashTree系との関係、
より大きなwindowでの再利用性を別途設計する。

## 9. 2026-08-29 MSVC Release実測

revision `f8e9bc2b163708c0d33288108c1f3dde15f594d1`、MSVC
19.51.36252.0、Visual Studio 18 2026、Windows 11 x64、AMD64 Family 25
Model 97、Python 3.12.13で72点を完了した。全36 Exact pairで5 fieldの
summaryとfingerprintが一致し、canonical最終JSONを生成できた。

12 member、211,938,580 input bytes、19 frameのaggregateは次のとおり。

| window | HashChain MiB/s | BinaryTree MiB/s | BinaryTree / HashChain | BinaryTree wins |
| ---: | ---: | ---: | ---: | ---: |
| 1 MiB | 1.542556 | 1.071961 | 0.694925 | 1 / 12 |
| 4 MiB | 0.491163 | 0.715334 | 1.456408 | 5 / 12 |
| 16 MiB | 0.271560 | 0.915582 | 3.371567 | 7 / 12 |

HashChain candidate数は11,010,112,118、33,179,026,662、61,384,255,817と
windowに従って増加した。BinaryTree key comparison数は5,431,259,004、
5,792,021,261、5,952,256,606に留まり、最大query depthは66、71、75だった。
16 MiB BinaryTreeはframeとwindowが等しいためretirementが0となり、4 MiB
BinaryTreeよりaggregate throughputが1.28倍高かった。

token数は37,561,576、34,116,898、33,137,395で、1から4 MiBで9.171%、
4から16 MiBで2.871%、1から16 MiBで11.778%減少した。matched-byte coverageは
0.888512、0.903132、0.906953だった。

16 MiBでBinaryTreeが勝ったmemberと比率は`mr` 11.999、`nci` 4.814、
`mozilla` 3.953、`reymont` 2.815、`samba` 2.544、`webster` 2.410、
`dickens` 1.209である。HashChainが勝ったのは`sao`、`osdb`、`ooffice`、
`x-ray`、`xml`である。従ってwindow sizeだけをselection ruleにしてはならない。
BinaryTreeは大windowかつ深い/多数のHashChain候補に対する有力な明示戦略だが、
低衝突入力ではHashChainを維持する。public既定値、自動selector、stream、ABI、
profileおよびinteroperabilityは本実測では変更しない。

workspaceはHashChain/BinaryTreeについて1 MiBで4.5/29 MiB、4 MiBで
16.5/116 MiB、16 MiBで64.5/464 MiBである。速度優位だけでこの約7.2倍の
workspaceを暗黙に選択せず、将来のselection設計でもhard limitと明示的な
memory policyを維持する。
