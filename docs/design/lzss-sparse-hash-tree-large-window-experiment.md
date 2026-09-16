# LZSS Sparse HashTree 大window再評価

## 1. 目的と境界

この文書は、privateな`Sparse HashTree Exact`を4、16、64 MiBのLZSS
windowで再評価する、結果参照前の固定実験契約を定める。2026-08-21から22の
64 KiB、256 KiB、1 MiB Silesia matrixではSparse HashTreeはHashChainに
勝たなかったが、最良throughput比は0.465、0.629、0.821とwindowに従って
改善した。完全HashTreeも4 MiBでHashChainの2.218倍を記録している。
従って「大windowでは、限られたhot bucketだけを木へ昇格する費用対効果が
逆転し得る」という既存仮説を、現在対応済みの最大windowまで同一条件で検証
する価値がある。

対象はencoder-localなExact一致探索だけである。stream format、token、decoder、
public C ABI、codec profile、interoperability archive、public selector、既定の
`HashChain Exact`および`WindowAdaptiveV1`を変更しない。実測が良好でも本実験
だけではSparse HashTreeを公開または自動選択しない。

## 2. 固定Silesia matrix

完全な測定は、manifest検証済みのSilesia Corpus全12 memberへ次を適用する。

```text
frame bytes                     67,108,864
window bytes                    4,194,304; 16,777,216; 67,108,864
baseline                        hash-chain-exact
sparse pool-node capacities     4,096; 65,536; 262,144
sparse promotion thresholds     64; 256; 1,024
timed iterations                1
maximum internal buffered bytes 536,870,912
planned records                 12 * 3 * (1 + 3 * 3) = 360
```

member順は既存manifestのcanonical順、window順は昇順とする。各member/windowで
`hash-chain-exact`を最初に測り、その後をpool昇順、threshold昇順の
`sparse-hash-tree-exact`とする。各recordは独立processで測る。全memberが
64 MiB未満なので、同じ64 MiB frame境界の下では各fileが一frameとなる。

以前の完全matrixで16,384-node poolはaggregate最良にならず、threshold 16も
aggregate最良にならなかった。今回は結果を見て候補を追加せず、この二系列を
除く。一方、大windowでpool枯渇が支配的になる可能性を調べるため262,144-node
poolを事前に追加する。これは過去結果の再選別ではなく、大windowに対する
bounded-pool scalingの検証である。

## 3. memory policy

64-bit hostにおける現行checked calculatorの固定workspaceは次となる。
`P`はpool-node capacity、`W`はwindow bytesである。

```text
HashChain workspace = 524,288 + 4W
Sparse workspace    = 851,968 + 4W + 21P
```

matrix端点の必要量は次である。aggregateは64 MiB frame入力との和である。

| window | strategy / pool | workspace bytes | aggregate bytes |
| ---: | ---: | ---: | ---: |
| 4 MiB | HashChain | 17,301,504 | 84,410,368 |
| 4 MiB | Sparse / 4,096 | 17,715,200 | 84,824,064 |
| 4 MiB | Sparse / 262,144 | 23,134,208 | 90,243,072 |
| 16 MiB | HashChain | 67,633,152 | 134,742,016 |
| 16 MiB | Sparse / 4,096 | 68,046,848 | 135,155,712 |
| 16 MiB | Sparse / 262,144 | 73,465,856 | 140,574,720 |
| 64 MiB | HashChain | 268,959,744 | 336,068,608 |
| 64 MiB | Sparse / 4,096 | 269,373,440 | 336,482,304 |
| 64 MiB | Sparse / 262,144 | 274,792,448 | 341,901,312 |

正式な可否はこの説明式ではなくrepositoryのchecked calculatorで判定する。
全点へ512 MiBを明示し、workspace単体、frameとのaggregate、算術overflow、
不正limitを確保前に拒否する。既定128 MiB policyを引き上げず、windowまたは
streamからlimitを自動拡張しない。

## 4. benchmark executable境界

既存の`--frames`とHashChain/BinaryTree用`--frames-limited`の引数およびreportを
変更しない。明示limit付きSparse経路を次の形で加える。

```text
marc_lzss_match_finder_benchmark --frames-limited \
  sparse-hash-tree-exact <input-file> <iterations> \
  <frame-bytes> <window-bytes> <pool-node-capacity> \
  <promotion-threshold> <max-internal-buffered-bytes>
```

正の有限pool、thresholdおよびlimitだけを受理する。reportは既存の完全な
Sparse診断に加え、`mode=frames-limited`、指定limit、workspace、token summary、
lowercase SHA-256 token fingerprintおよび有限の測定時間を含める。診断付き
untimed passと診断なしtimed passを分け、両passのinput、frameおよびtoken数を
一致させる。

## 5. runnerとExact gate

専用runner `tools/run_silesia_sparse_hash_tree_large_window_experiment.py`は独立schema
`marc-silesia-sparse-hash-tree-large-window-v1`を生成する。既存の
`marc-silesia-sparse-hash-tree-v1`を変更しない。

各Sparse recordは対応するHashChain baselineと次の全fieldが一致しなければ
保存しない。

```text
token_count
literal_count
match_count
matched_bytes
token_fingerprint_sha256
```

各summaryで`token_count == literal_count + match_count`、
`input_bytes == literal_count + matched_bytes`、query histogram massと
`query_count`の一致、Sparse route/promotion/pool診断の内部整合も要求する。
pool capacity、threshold、workspace、明示limit、commandおよびcanonical grid
からの逸脱を拒否する。

aggregate throughputは各群の総input byteを総秒数で割る。各window/pool/
thresholdについてHashChain比、12 member中の勝数、workspace比、chain candidate、
tree query、promotion、pool exhaustion/rejectionを保存する。性能値は記述的証拠
であり、runner成功条件にしない。

## 6. checkpointと実行制御

`--checkpoint`は完全検証済みrecordの後だけ同一directoryで原子的に置換する。
identityはschema、full Git revision、benchmark absolute pathとSHA-256、runnerと
依存sourceのSHA-256、Corpus absolute pathと完全manifest、固定matrix、platform、
compiler、generator、architectureおよびbuild labelを含む。復元recordも通常の
validatorへ戻し、canonical prefixでなければ拒否する。

`--max-new-points N`はcheckpointを必須とし、最終`--output`と併用しない。Nは
再開時に変更可能なのでidentityに含めない。0は検証と進捗表示だけを行う。
360点完了後、制限を外し`--output`を指定した実行だけがcanonicalな最終JSONを
生成する。runnerはnetwork access、download、Corpus生成を行わない。

## 7. 解釈を固定する分類

結果を見て基準を動かさないため、各候補/windowを次の独立した記述で分類する。

- `aggregate_gain`: Sparse/HashChain aggregate throughput比が1を超える。
- `broad_gain`: 12 memberのうち6以上でSparse throughputがHashChainを超える。
- `low_workspace_premium`: Sparse/HashChain workspace比が1.10以下である。
- `pool_pressure_observed`: promotion要求がpool不足で拒否された記録がある。

前三分類をすべて満たす候補はpublic採用設計へ進む有力候補となるが、自動昇格
しない。一部だけを満たす結果、全候補の敗北、pool pressureも価値ある確定結果
として記録する。良い点だけを選ぶ再測定や同じCorpusを見た後の追加tuningは、
別の事前設計なしに行わない。

## 8. 段階的実装

1. 本設計、参照、decision、test-vectorおよびclean-room記録を確定する。
2. 既存経路を保ったまま明示limit付きSparse benchmark経路を追加する。
3. executableの引数、calculator端点、Exact identityおよび失敗原子性を試す。
4. 独立schema、厳密validator、checkpointとbounded batchを持つrunnerを追加する。
5. fake benchmarkによる360点grid、resume、破損拒否およびaggregateを試す。
6. MSVC、ClangCLと完全CTestを通した後、実Corpusをbounded batchで測る。
7. 最終結果と採否判断を別commitで記録する。

## 9. 確定結果と採否判断

MSVC Release、commit
`5c3106e68c679b9268a13c75f96033a98f523063`で固定matrixの全360点を
完了した。全candidateは直前のHashChain baselineと`token_count`、
`literal_count`、`match_count`、`matched_bytes`および
`token_fingerprint_sha256`が一致した。checkpointのzero-work再検証は
`360/360`を保ち、canonical最終JSONには3 baseline aggregate、27 Sparse
aggregateおよび27 comparisonが存在する。

各windowでaggregate throughput比が最大だった条件は、いずれも
4,096-node pool、promotion threshold 64であった。

| window | HashChain MiB/s | Sparse MiB/s | Sparse / HashChain | member勝数 | workspace比 | pool rejection |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 MiB | 0.435927 | 0.393705 | 0.903146 | 1 / 12 | 1.023911 | 213,754 |
| 16 MiB | 0.155423 | 0.146837 | 0.944754 | 1 / 12 | 1.006117 | 391,200 |
| 64 MiB | 0.094125 | 0.086622 | 0.920290 | 1 / 12 | 1.001538 | 406,314 |

`aggregate_gain`と`broad_gain`は全27候補でfalseであり、事前に定めた
有力候補条件を満たすものはない。`low_workspace_premium`は24候補でtrue
だったが、`pool_pressure_observed`は27候補すべてでtrueだった。16 MiBまで
見えた相対改善も64 MiBでは継続せず、poolを増やした候補はpromotionの構築・
維持費を探索候補削減で回収できなかった。

従って、測定したSparse HashTree policyをpublic strategy、自動selector、
既定値またはprofileへ採用しない。private実装、明示limit付きbenchmark経路、
診断、strict runnerおよびcheckpoint形式は、Exact oracle、負の結果、ならびに
将来の別仮説を事前設計して検証する基盤として保持する。同じCorpus結果を見た
後にpoolまたはthresholdだけを追加調整しない。再評価には、promotion構築費、
terminal rejectionまたはhot-bucket選択のいずれかを直接変える新しい仮説と、
独立した固定実験が必要である。

管理対象外のcanonical結果JSONのSHA-256は
`cdd526d40ef81406ec2cd87bb91799e3dc30ab400290a9152f5dfa2869ab8e95`
である。
