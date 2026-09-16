# LZSS Sparse HashTree 反復利用ゲート固定実験

## 1. 目的と非目標

この文書は、privateな`Sparse HashTree Exact`へ追加した反復利用ゲートを、
結果参照前に固定したSilesia Corpus matrixで評価する契約を定める。仮説は、
単発の高コストqueryではなく同一bucketの反復する高コストqueryだけをpromotion
対象にすれば、4,096-nodeのbounded poolを将来再利用されるbucketへ優先配分でき、
構築・維持費を回収しやすくなる、というものである。

pool capacity、既存candidate threshold、window、frameおよびCorpusは前回結果を
受けて追加調整しない。stream format、token、decoder、public ABI、profile、既定
`HashChain Exact`、public selectorおよびinteroperability archiveも変更しない。
結果が良好でも、この実験だけでprivate strategyを公開または自動選択しない。

## 2. 固定matrix

manifest検証済みSilesia Corpus全12 memberへ次を適用する。

```text
frame bytes                          67,108,864
window bytes                         4,194,304; 16,777,216; 67,108,864
baseline                             hash-chain-exact
sparse pool-node capacity            4,096
sparse promotion candidate threshold 64
sparse promotion reuse thresholds   1; 2; 4; 8; 16
timed iterations                     1
maximum internal buffered bytes      536,870,912
planned records                      12 * 3 * (1 + 5) = 216
```

member順はmanifest順、window順は昇順とする。各member/windowでHashChainを最初に
測り、続いてreuse thresholdを昇順に測る。値1は追加count viewを持たない従来
Sparseの内部対照であり、2、4、8、16だけが新しいgate候補である。各recordは独立
processとし、全memberは64 MiB frame内の一frameとして処理する。

4,096-node poolとcandidate threshold 64は、前回の各windowでaggregate比が最大
だった共通条件である。今回これらを動かさないことで、観測差をhot-bucketの
反復利用条件へ限定する。reuse値はboundedな二倍系列とし、結果を見て中間値、
32以上の値、別poolまたは別candidate thresholdを追加しない。

## 3. memory policy

reuse threshold 1のworkspaceは前回契約をそのまま保持する。2以上ではbucketごとに
1 byte、各対象windowでは65,536 bytesを追加する。64-bit hostのchecked calculator
端点は次である。aggregateは64 MiB frame入力との和である。

| window | route | workspace bytes | aggregate bytes |
| ---: | --- | ---: | ---: |
| 4 MiB | HashChain | 17,301,504 | 84,410,368 |
| 4 MiB | Sparse reuse 1 | 17,715,200 | 84,824,064 |
| 4 MiB | Sparse reuse 2..16 | 17,780,736 | 84,889,600 |
| 16 MiB | HashChain | 67,633,152 | 134,742,016 |
| 16 MiB | Sparse reuse 1 | 68,046,848 | 135,155,712 |
| 16 MiB | Sparse reuse 2..16 | 68,112,384 | 135,221,248 |
| 64 MiB | HashChain | 268,959,744 | 336,068,608 |
| 64 MiB | Sparse reuse 1 | 269,373,440 | 336,482,304 |
| 64 MiB | Sparse reuse 2..16 | 269,438,976 | 336,547,840 |

正式判定はrepositoryのchecked calculatorを用いる。全点へ512 MiB limitを明示し、
workspace単体、inputとのaggregate、算術overflowおよび1 byte不足を確保前に拒否
する。stream値からlimitを自動拡張しない。

## 4. benchmark境界

既存の`--frames-limited sparse-hash-tree-exact`を変更せず、reuse thresholdを明示する
private routeを追加する。

```text
marc_lzss_match_finder_benchmark --frames-limited \
  sparse-hash-tree-reuse-gated-exact <input-file> <iterations> \
  <frame-bytes> <window-bytes> <pool-node-capacity> \
  <promotion-candidate-threshold> <promotion-reuse-threshold> \
  <max-internal-buffered-bytes>
```

reuse threshold 1から`UINT8_MAX`だけを受理する。reportへ
`sparse_hash_tree_promotion_reuse_threshold`を必須出力し、既存のworkspace、token、
fingerprint、promotion、tree-query、pool-rejection、比較回数および有限時間診断を
保持する。診断付きuntimed passと診断なしtimed passは同じinput、frame、token列を
処理しなければならない。

## 5. Exact gateとrunner

専用runnerは独立schema
`marc-silesia-sparse-hash-tree-reuse-gate-v1`を生成し、前回schemaとcheckpointを変更
しない。各Sparse recordは直前のHashChain baselineと次の全fieldが一致しなければ
保存しない。

```text
token_count
literal_count
match_count
matched_bytes
token_fingerprint_sha256
```

同一member/windowの全reuse値も相互に一致させる。token算術、query histogram mass、
trigger accounting、pool上限、workspace、全option、commandおよびcanonical gridを
厳密検証する。aggregate throughputは総input byteを総秒数で割り、window/reuse値
ごとにHashChain比、reuse 1比、12 member中の勝数、workspace比、chain candidate、
promotion、tree-query再利用、build/maintenance費およびpool rejectionを保存する。

## 6. checkpointと実行制御

canonical順はmember、window、HashChain、reuse 1、2、4、8、16とする。checkpointは
完全検証済みrecordの後だけ同一directoryで原子的に置換し、canonical prefix以外を
拒否する。固定条件の正本は
`benchmarks/experiments/silesia-sparse-hash-tree-reuse-gate-v1.json`とする。
runnerはJSONの完全なkey集合、型、値、配列順および相互関係をstrictに検証する。
未知key、未知strategy、重複値、非canonical順、異なるworkspaceまたは分類条件を
拒否し、JSONからcommand、path、environment variableまたはnetwork locationを
解釈しない。

identityはschema、full Git revision、experiment manifestのabsolute path、
raw SHA-256およびcanonical parsed value、benchmarkとrunnerのabsolute pathとSHA-256、
依存source、Corpus pathとmanifest、platform、compiler、generator、architecture
およびbuild labelを含む。manifestの一文字でも変わった場合、既存checkpointを
再開してはならない。

通常の実測は次の一回のtop-level runner起動で未完了の全recordを処理する。runner
内部では故障分離と計測契約を保つため一record一child processを維持するが、利用者が
承認・起動するcommandは一つだけである。

```console
py -3.14 tools/run_silesia_sparse_hash_tree_reuse_gate_experiment.py \
  out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe \
  --experiment benchmarks/experiments/silesia-sparse-hash-tree-reuse-gate-v1.json \
  --corpus benchmarks/data/silesia/corpus \
  --checkpoint benchmarks/data/silesia/results/sparse-reuse-gate-msvc.checkpoint.json \
  --output benchmarks/data/silesia/results/sparse-reuse-gate-msvc.json \
  --compiler "MSVC 19.51" --generator "Visual Studio 18 2026" \
  --architecture x64 --build-label windows-msvc-release
```

checkpointとoutputは通常実測で必須とし、別pathでなければならない。各childの終了後、
reportを完全検証し、HashChainおよび同一member/windowの先行reuseとのExact identity
を確認した後だけrecordを追加する。temporary fileをflushし、同一directoryでatomic
replaceしてから次のchildを起動する。child実行中の停止ではその未保存pointだけを
再実行し、保存済みrecordを再測定しない。同じcommandを再起動すればcheckpointを
最初から再検証し、canonical prefixの直後から続行する。完全checkpointではchildを
起動せずfinal aggregateを再生成できる。

`--max-new-points N`とzero-work検証はfake benchmark、runner回帰試験および明示的な
診断用に保持するが、通常実測では指定しない。execution-control値はidentityに含めず、
固定測定条件を変更できない。216点完了後にだけcanonical aggregateを生成する。
runnerはnetwork access、downloadまたはCorpus生成を行わない。

## 7. 事前固定する分類

reuse 2、4、8、16の各候補/windowを次で分類する。

- `aggregate_hash_chain_gain`: aggregate throughput / HashChainが1を超える。
- `aggregate_legacy_gain`: aggregate throughput / reuse 1が1を超える。
- `broad_hash_chain_gain`: 12 member中6以上でHashChainを超える。
- `broad_legacy_gain`: 12 member中6以上でreuse 1を超える。
- `low_workspace_premium`: workspace / HashChainが1.10以下である。
- `pool_pressure_reduced`: pool rejectionがreuse 1より少ない。

最初の5分類をすべて満たす候補だけをpublic採用設計へ進む有力候補とするが、
自動昇格はしない。`pool_pressure_reduced`は機構説明用であり、promotion自体を抑えた
だけでも成立し得るため採否条件には単独で用いない。全候補の敗北、reuse 1だけの
勝利、member偏在または構築費増大も確定結果として保存する。

## 8. 実装順序

1. 本設計、reference、decision、test-vectorおよびclean-room記録を確定する。
2. 既存routeを保ったprivate benchmark routeとreport fieldを追加する。
3. 引数、reuse境界、workspace端点、Exact identityおよび失敗原子性を試す。
4. JSON正本、独立schema、strict validator、point単位checkpointおよび一回起動を
   持つrunnerを追加する。
5. fake benchmarkで216点grid、resume、zero-work、破損拒否およびaggregateを試す。
6. MSVC、ClangCLおよび完全CTest後に、実Corpusをbounded batchで測る。
7. 最終結果と採否判断を別commitで記録する。

## 9. 実装状態

手順2と3は完了した。privateな
`--frames-limited sparse-hash-tree-reuse-gated-exact` routeはreuse thresholdを
明示的に受け取り、reportへ同じ値を出力する。値1は旧
`sparse-hash-tree-exact`とworkspaceおよび五つのExact identity fieldが一致し、
値2以上はbucketごとのbounded count viewをchecked workspaceへ加える。

回帰試験は旧routeに新fieldが現れないこと、値1の旧route identity、値2の
HashChain identityとworkspace差、`UINT8_MAX`受理、zeroと`UINT8_MAX + 1`、
引数不足・過剰および
不十分なhard limitの拒否をMSVCとClangCLで検証する。Corpus測定、runner実装、
実Corpus checkpoint作成および性能上の採否判断はまだ行っていない。手順4と5の
versioned JSON正本、strict runner、一回起動、point単位atomic checkpoint、再開、
完全checkpointからの無測定再生成およびfake 216点gridは実装・検証済みである。
