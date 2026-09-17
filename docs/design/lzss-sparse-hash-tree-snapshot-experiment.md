# LZSS Sparse HashTree immutable snapshot 固定実験

## 1. 目的

この実験は、promotion後のtreeを不変snapshotとして保持し、新しい位置を既存HashChainの
deltaとして検索するprivate strategyが、mutable Sparse HashTreeの毎位置tree保守費を
除去した結果として実時間性能を改善するか判定する。

対象はmatch finderだけである。codec、C ABI、stream format、profile、自動selector、
既定strategyは変更しない。実験結果だけを理由に公開採用しない。

## 2. 事前に固定する仮説

immutable candidateはmutable controlと同じpromotion条件およびworkspaceを使用する。
差はpromotion後のlifecycleだけである。候補が有効なら、Exact token列を維持しながら
mutable treeへの挿入・削除を0にし、HashChainおよびmutable controlより高いthroughputを
示す。snapshot queryとchain deltaの合成費が保守費削減を上回る場合は不採用とする。

## 3. 固定条件

- corpus: repositoryで検証済みのSilesia 12 member
- build: MSVC Release
- iteration: 1
- frame: 64 MiB
- window: 4 MiB、16 MiB、64 MiB
- hard internal-buffer limit: 512 MiB
- Sparse pool capacity: 4,096 nodes
- promotion candidate threshold: 64
- promotion reuse threshold: 16
- process isolation: 一測定点につき一child process
- checkpoint: 一測定点ごとにatomic保存

`pool=4096`と`promotion=64`は過去の固定Sparse実験から継承する。`reuse=16`は完了済み
reuse-gateで、すべてのSilesia memberにおいてreuse-oneより高速で、測定したreuse条件中
最も強いmutable controlだったため採用する。immutable実験ではこれらを再探索しない。

## 4. 比較strategyと順序

各memberとwindowについて次の順で実行する。

1. `hash-chain-exact`
2. `sparse-hash-tree-reuse-gated-exact`、reuse 16
3. `sparse-hash-tree-immutable-snapshot-exact`、reuse 16

canonical順序は`member -> window -> baseline -> mutable -> candidate`とし、総数は
`12 * 3 * 3 = 108` recordとする。過去のreuse-gate checkpointまたはresultは再利用せず、
新しいschemaとmanifest digestへ束縛する。

## 5. 必須検証

三strategyは各member/windowで次の5 fieldが完全一致しなければならない。

- `token_count`
- `literal_count`
- `match_count`
- `matched_bytes`
- `token_fingerprint_sha256`

workspaceはmanifestの固定値と一致し、mutableとcandidateは相互にも一致しなければならない。
candidateは`immutable-snapshot` lifecycleを報告し、snapshot診断fieldをすべて持つ。
candidateの`hash_tree_tree_queries`、`hash_tree_insertions`、
`hash_tree_retirements`は0でなければならない。query route、promotion accounting、histogram、
frame数、hard limit、有限で非負の時間値もrunnerが検証する。

## 6. 判定

windowごとに次を独立して記録する。

- candidate / HashChain aggregate throughput ratioが1を超えるか
- candidate / mutable aggregate throughput ratioが1を超えるか
- 12 member中candidateがHashChainより速い件数が6以上か
- 12 member中candidateがmutableより速い件数が6以上か
- candidate workspace / HashChain workspaceが1.10以下か
- candidateのtree insertionおよびretirement総数が0か
- candidateのsnapshot queryおよびpromotion総数が正か

throughput条件の一部だけを満たしても自動採用しない。結果は全window、member別勝敗、
diagnostics、workspaceとともに文書化してから採否を決める。

## 7. 実行境界

manifestは不活性JSONであり、command、実行ファイルpath、corpus path、network locationを
含めない。runnerはmanifestの完全一致、実行ファイルとtool sourceのSHA-256、Git revision、
corpus manifest、環境をcheckpoint identityへ含める。未知field、順序違反record、Exact不一致、
既存未完了checkpointとfinal outputの競合を拒否する。

runnerは`tools/run_silesia_sparse_hash_tree_snapshot_experiment.py`である。例えばMSVC
Release測定は次の一回のtop-level起動で開始または再開する。

```console
py -3.14 tools/run_silesia_sparse_hash_tree_snapshot_experiment.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --experiment benchmarks/experiments/silesia-sparse-hash-tree-immutable-snapshot-v1.json --corpus benchmarks/data/silesia/corpus --checkpoint benchmarks/data/silesia/results/sparse-immutable-snapshot-msvc.checkpoint.json --output benchmarks/data/silesia/results/sparse-immutable-snapshot-msvc.json --compiler "MSVC 19.50" --generator "Visual Studio 18 2026" --architecture x64 --build-label windows-msvc-release
```

`--max-new-points N`を指定すれば新規recordを最大N件に制限でき、0なら既存checkpointだけを
再検証する。未完了checkpointがあるときにfinal outputが存在する場合は上書きせず失敗する。
complete checkpointはchild processを再起動せずfinal aggregateを再生成する。

単体テストはmanifest validation、三者report契約、canonical prefix、4+104件の途中再開、
complete-grid再実行抑止、child failure後のatomic record境界、stale-output拒否をfake reportで
固定した。実コーパス測定は別の明示的な実行段階まで開始しない。
