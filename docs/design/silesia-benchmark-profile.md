# Silesia外部ベンチマークプロファイル

## 1. 目的

この文書は、LZSS一致探索器、とくに大きなスライドウィンドウにおける
HashChain Exactと将来のBinaryTree Exactを比較するための外部Corpus運用と
測定契約を定める。

Silesia Corpusはテストベクトル、marcの配布物、正しさのオラクルではない。
性能特性を観測するための利用者取得データであり、測定値を安定した合否条件
にはしない。

## 2. 外部データ方針

- Corpus本体、配布archive、展開途中のファイルおよび測定結果をリポジトリへ
  commitしない。
- marcのconfigure、build、CTestおよび通常benchmark smokeはCorpusを要求
  しない。
- build、test、benchmarkおよび補助scriptはCorpusを自動downloadしない。
- 利用者が公式ページから明示的に取得し、
  `benchmarks/data/silesia/corpus/`へ展開する。
- local archive用`downloads/`、展開後データ用`corpus/`、local report用
  `results/`を`.gitignore`で除外し、その親の`README.md`だけを配置説明として
  管理する。
- Corpusの利用条件と各構成ファイルの権利は配布元および元データの条件に
  従う。marcはCorpus全体の再配布可能性を主張しない。

ダウンロードしない検証器は正確な12ファイル名、通常ファイルであること、
公式掲載の展開後サイズおよびMD5を照合し、実験記録用SHA-256を表示する。
サイズ不一致または想定外entryをhash対象にせず、全構成ファイルが成功した
後だけ結果を出力する。MD5は公式掲載ファイルとの同一性識別だけに使用し、
真正性保証には使用しない。

検証器はPython 3.9以上の標準libraryだけを使用する。CMakeが対応interpreter
を発見した場合は小型fixtureだけを使う単体試験をCTestへ登録する。Pythonや
実Corpusの不在を通常のconfigure失敗理由にはしない。

## 3. Corpus単位

公式掲載順の次の12ファイルを、それぞれ独立した入力として扱う。

```text
dickens mozilla mr nci ooffice osdb reymont samba sao webster xml x-ray
```

ファイルを暗黙に連結してはならない。各ファイルの結果と12ファイルの合計を
両方報告する。合計スループットは、各入力byte数の合計を対応する測定時間の
合計で割る。合計圧縮率は、各完全stream sizeの合計を各入力sizeの合計で
割り、ファイルごとの圧縮率の単純平均にはしない。

## 4. HashChain診断指標

HashChainの頭打ち原因を区別するため、少なくとも次を記録する。

```text
queries
candidate_links_visited
hash_false_positive_candidates
prefix_equal_candidates
match_bytes_compared
maximum_candidates_in_one_query
candidate-count distribution or fixed histogram
```

`candidate_links_visited`は期限内候補としてチェインから取り出した位置の数、
`hash_false_positive_candidates`は同じbucketに入ったが実際の5-byte接頭辞が
一致しない候補数、`prefix_equal_candidates`は5-byte接頭辞が一致した候補数
とする。短い入力末尾など5-byte hashへ登録できない位置はこれらの候補数へ
混在させず、別のfallback計数が必要なら明示する。

この分離により、異なる接頭辞のhash衝突と、実データ上同一の接頭辞が大量に
存在するために長くなった正当なチェインを区別する。候補数の平均だけでなく
最大値および分布を残し、少数の病的queryを平均値で隠さない。

既存の次の値も同じ実行で記録する。

- LZSS解析スループット
- 完全stream圧縮および展開スループット
- 完全stream圧縮率
- literal数、match数およびmatch対象byte数
- match-finder workspaceとcodec全体のpeak caller-owned workspace
- 計画および書き込みを含む総時間

診断用カウンターの有効化自体が測定時間へ有意な影響を与える場合、通常の
throughput実行と診断実行を分離し、同じ入力と設定であることを記録する。

## 5. 比較軸

最初のHashChain基準測定は、同じRelease build、入力順、frame規則、最大
match長、反復回数を使い、少なくとも次のwindow sizeを比較する。

```text
65,536 bytes
262,144 bytes
1,048,576 bytes
```

256 KiBが公開stream profileとして存在しない段階ではmatch-finder単体の内部
測定に限定し、公開codecが対応しているように表示しない。1 MiBを超える
windowは、既存の1 MiB基準とBinaryTreeの特性がそろってから別途設計する。

各戦略は測定前にuntimed round tripを通す。Exact戦略は小入力差分試験で
Exhaustiveと同一token列を要求し、Corpus測定でもHashChain Exactと将来の
BinaryTree Exactのtoken列およびstream bytesを照合する。

## 6. BinaryTree着手ゲート

BinaryTree Exactの実装着手は、Silesia全体のHashChain測定完了だけを条件に
しない。反復列、周期列、hash衝突を意図した合成入力も同じ診断指標で測る。

測定後、少なくとも次を判断できる状態にする。

1. 1 MiBで候補探索コストが実際に支配的か。
2. 頭打ちの主因がhash false positiveか、同一接頭辞候補数か。
3. データカテゴリごとに悪化が局所的か継続的か。
4. BinaryTreeの追加workspaceと更新コストに見合う改善余地があるか。
5. 自動選択閾値を決める証拠が十分か。

単一ファイル、合計値だけ、または小さなREADME入力だけを根拠にBinaryTreeを
既定化したり`WindowAdaptiveV1`の閾値を固定したりしない。

## 7. 再現性記録

公開または比較に使用する測定には次を記録する。

```text
marc revision
Corpus file manifest and verification result
operating system and architecture
compiler and version
CMake generator and build type
CPU model
benchmark command line
window size and frame size
match-finder strategy and search effort
warm-up and measured iteration counts
per-file and aggregate results
```

探索戦略はstreamへ保存しない。再現に必要な戦略と努力量はbenchmark report
または外部provenanceへ保存する。

## 8. 実装順序

1. 外部配置READMEと`.gitignore`規則を追加する。
2. local-only Corpus検証器と実Corpus非依存の単体試験を追加する。
3. match-finder benchmarkを大きなファイルのframe反復へ対応させる。
4. HashChain診断カウンターと分布集計を追加する。
5. 64 KiB、256 KiB、1 MiBの基準測定を取得する。
6. 合成worst-case入力を同じ指標で測る。
7. 結果を基にBinaryTree Exactの不変条件と構造を確定する。
8. 専用branchでBinaryTree Exactを実装し、Exhaustive差分試験を行う。
9. 両戦略のCorpus結果がそろってから既定戦略や自動選択を検討する。

## 9. Corpus-wide runner

`tools/run_silesia_match_finder_benchmark.py`はdownloadを行わず、既存の厳密な
manifest検証が全12 memberで成功した後だけ測定を開始する。benchmark実行file、
Corpus directory、出力JSON、iteration、frame size、window集合および環境labelを
明示的な引数とする。既定window集合は65,536、262,144、1,048,576 byteである。

各member/windowについてHashChain ExactとBinaryTree Exactを独立processで実行
し、report schema、入力byte数、frame数、設定値および戦略固有指標を検証する。
両戦略のtoken数が一致しなければ結果を公開しない。JSONには実行command、
Corpus SHA-256、revision、環境、per-file reportと、入力byte数および時間を合算
したstrategy/window別aggregateを保存する。結果fileはignored `results/`へ置き、
tracked sourceまたは配布物へ含めない。

## 10. HashTree threshold runner

`tools/run_silesia_hash_tree_threshold_benchmark.py`は既存の二戦略比較schema
`marc-silesia-match-finder-v1`を変更せず、private HashTreeだけを評価する独立
schema `marc-silesia-hash-tree-threshold-v1`を生成する。通常runnerと同じ厳密な
12 member manifest検証が完了するまでbenchmark processを起動してはならず、
network access、download、Corpus生成を行ってはならない。

既定値は1 MiB frame、65,536、262,144、1,048,576 byte window、1回の診断pass、
1回のtimed pass、および合成matrixで縮約したthreshold 16、64、256、1024である。
thresholdは有限uint64かつ重複なし、windowは正かつ重複なしとする。callerは
明示的に置換できるが、実行JSONへ完全な設定とcommandを保存する。

各member/windowでHashChain Exact baselineを一度測定し、その後すべてのHashTree
thresholdを独立processで測定する。各reportはmode、size、frame count、window、
iteration、threshold、全HashTree診断値、route合計、promotion合計、histogram合計、
有限な時間値を検証する。すべてのHashTree token数は対応するHashChain baselineと
一致しなければならず、不一致時は部分JSONを生成しない。

JSONはmanifest、environment、revision、configuration、baseline records、
threshold records、baseline/window aggregates、threshold/window aggregatesを
分離する。各recordはmember名、検証済みSHA-256、完全command、生reportを保持する。
aggregateは全12 memberの入力byteと時間を合算し、workspaceとpeak値は最大、
diagnostic workとhistogramは合算する。性能値を合否条件にせず、結果はignored
`results/`へだけ保存する。この実験はpublic selector、stream、ABI、既定戦略、
interoperability artifactを変更しない。

## 11. Private 4 MiB HashTree experiment runner

`tools/run_silesia_hash_tree_4m_experiment.py`は既存のv1 matrix schemaを
変更せず、`marc-silesia-hash-tree-4m-experiment-v1`を生成する。全12 memberの
manifest検証が成功するまでbenchmarkを起動せず、network accessまたはdownloadを
行わない。各memberを次の固定された3経路で独立processとして測定する。

```text
control-1m:   frame 1,048,576, window 1,048,576, HashChain Exact
oracle-4m:    frame 4,194,304, window 4,194,304, HashChain Exact
candidate-4m: frame 4,194,304, window 4,194,304, HashTree Exact threshold 1,024
```

4 MiB oracleとcandidateはtoken count、literal count、match count、matched bytes、
token fingerprintの全てが一致しなければならず、不一致時はJSONを生成しない。
1 MiB controlと4 MiB oracleはframe resetとwindowが異なるためtoken列の一致を要求
しない。各summaryはtoken kind数からtoken countを、literal数とmatched bytesから
入力extentを再構成し、fingerprintは64桁lowercase SHA-256でなければならない。

JSONはmanifest、environment、revision、固定configuration、36 records、role別の
byte-weighted aggregate、比較値およびgateを保存する。HashTree/HashChain aggregate
throughput比が1を超えることと、4 MiB oracleが1 MiB controlよりaggregate token数を
減らすかmatched-byte coverageを増やすことを別々に判定する。これらが不成立でも
測定は有効な否定的証拠なのでrunnerを失敗させない。両方が成立した場合だけ
`eligible_for_format_design`をtrueとする。この値はpublic formatの承認ではなく、
aggregate workspace設計へ進むためのgateにすぎない。

## 12. Sparse HashTree pool/threshold runner

`tools/run_silesia_sparse_hash_tree_matrix.py`は既存schemaを変更せず、独立schema
`marc-silesia-sparse-hash-tree-v1`を生成する。測定開始前に全12 memberの厳密なmanifestを
必ず検証する。開発時smokeでは`--members`により測定memberだけを限定できるが、Corpus検証を
部分化してはならず、unknownまたは重複member名を拒否する。選択順はmanifestのcanonical順とし、
引数順によってJSONまたは実行順を変えない。

既定gridは1 MiB frame、標準3 window、pool capacity 4,096、16,384、65,536 nodeとthreshold
16、64、256、1,024である。全capacityは選択した全frame/windowの最小extent以下、全thresholdは
有限uint64、各集合は重複なしとする。capacity 0は明示的なchain-only測定として受理するが、
閾値間で同じ処理を繰り返すため既定gridには含めない。各member/windowでHashChain Exact baselineを一度だけ測り、
全pool/threshold候補のtoken countが一致しなければJSONを生成しない。

report validatorは明示設定、共通/sparse workspace一致、有限時間、全診断値、query route、
histogram、`promotions <= triggers`および`maximum promoted nodes <= pool capacity`を検証する。
aggregateはwindow/capacity/thresholdごとに時間とworkを合算し、workspaceとpeakを最大化する。
結果はignored directoryにのみ保存し、性能を合否条件、public selectorまたはformat規則にしない。

### 12.1 Checkpoint contract

`--checkpoint`は明示したlocal JSONを新規作成し、存在時は自動再開する。各baselineまたは候補processが
正常終了し、report検証とExact token検証が完了した後だけ原子的に置換する。identityにはrevision、
benchmark絶対path/SHA-256、runner依存source SHA-256、Corpus絶対path/manifest、全測定設定とplatform/build環境全体を含め、完全一致を要求する。
復元recordも通常実行と同じvalidatorへ戻し、重複、grid外、baseline不在、command差異、token差異を
拒否する。checkpointは性能結果ではなく進捗journalであり、最終schemaはv1のまま変更しない。

### 12.2 Bounded batch contract

`--max-new-points`はcheckpointを必須とし、final outputとは併用しない。復元済みrecordを除くbaselineまたは
候補processの起動数を数え、指定数を保存した直後の境界で成功終了する。0は検証専用とする。この値は測定設定
ではないためcheckpoint identityに含めず、再開ごとに変更できる。表示するprogressは検証済みbaseline数と
候補数の和を、`members * windows * (1 + pools * thresholds)`で割った値とする。
