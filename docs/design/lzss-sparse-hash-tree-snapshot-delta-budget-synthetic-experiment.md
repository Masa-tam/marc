# LZSS Sparse HashTree snapshot delta budget 合成実験

## 1. 目的

この実験は、privateなimmutable snapshotのdelta候補予算について、Silesiaを一切参照せず、
後続の実データ実験へ渡す候補を事前に絞り込む。予算はExact queryを打ち切る上限ではなく、
完了済みqueryの候補数が閾値を超えたbucketを次のadvanceでHashChainへ戻すcircuit breakerで
ある。したがって評価対象は、Exact identity、demotion遷移、反復delta費の抑制、および
HashChainへ収束した後の実時間である。

codec、C ABI、stream format、profile、自動selector、既定strategyは変更しない。この合成
実験だけでprivate strategyまたはbudgetを公開採用しない。

## 2. 観測前に固定する条件

- build: MSVC Release
- iteration: 1（各child process内で別のuntimed validation passを先行する）
- inputおよびframe: 各case 64 MiB、単一frame
- window: 4 MiB、16 MiB、64 MiB
- hard internal-buffer limit: 512 MiB
- Sparse pool capacity: 4,096 nodes
- promotion candidate threshold: 64
- promotion reuse threshold: 16
- delta candidate budgets: 16、64、256、1,024、4,096
- process isolation: 一測定点につき一child process
- checkpoint: 一測定点ごとにatomic保存

pool、promotionおよびreuseは完了済みimmutable snapshot実験から継承し、この実験では
再探索しない。budgetは4倍刻みの有限な正の系列とし、smoke専用の1、無効値0、および
4,096を超える値を含めない。値はSilesia結果または今回のtimingを見る前に固定する。

## 3. 決定的な合成fixture

runnerはnetwork、Corpusおよび乱数deviceを使用せず、profile
`marc-lzss-snapshot-delta-synthetic-v1`から次の6 fixtureを生成する。

1. `zeros`: 全byteが0。
2. `periodic-251`: positionを251で割った剰余をbyte値とする。
3. `shared-prefix-records`: 64-byte recordの先頭56 byteを固定し、末尾8 byteへ
   little-endian record番号を格納する。
4. `prefix-collision-runs`: smokeで用いた、共通prefixと変化する末尾を持つ56-byte unitを
   反復する。
5. `phase-shifted-periodic`: 4,096-byte blockごとに251-byte周期の位相を1進める。
6. `fixed-seed-pseudorandom`: 固定seedの明記された64-bit generatorで生成する。

正確なbyte生成式、seed、各fixtureのSHA-256はrunner実装ゲートでテストベクトルとして固定
する。生成済みfixtureはrepositoryへ追加せず、ignoredなbuild/data領域へ一つずつ生成して
検証する。checkpoint identityはgenerator source、profile、fixture名、sizeおよびdigestを
含み、再開時に再生成byteと一致しなければならない。

## 4. 比較strategyとcanonical順序

各fixtureとwindowについて次を実行する。

1. `hash-chain-exact`
2. `sparse-hash-tree-immutable-snapshot-exact`（budget無効）
3. `sparse-hash-tree-snapshot-delta-budget-exact`をbudget昇順で5件

canonical順序は`fixture -> window -> baseline -> unbudgeted -> budget`で、総数は
`6 * 3 * 7 = 126` recordとする。一回のrunner起動で全gridを処理するが、各recordを独立した
child processで実行し、検証成功後ただちにcheckpointへatomic保存する。再開時は完全に検証
されたcanonical prefixだけを再利用し、既完了recordを起動しない。

## 5. 必須検証

同じfixture/windowの全strategyで次の5 fieldが完全一致しなければならない。

- `token_count`
- `literal_count`
- `match_count`
- `matched_bytes`
- `token_fingerprint_sha256`

budgeted candidateは明示したbudget、immutable lifecycle、全queryのroute accounting、
snapshot queryとbudget queryの一致、breachとsuccessful demotionの一致、expirationとdemotionの
bulk-release accounting、zero insertion/retirementおよび非overflowを満たす。breachが正なら
maximum candidates at breachはbudgetより大きく、0ならmaximumも0でなければならない。

budgetedとunbudgeted snapshotのworkspaceは一致し、manifest固定値とも一致しなければなら
ない。HashChainとのworkspace比は1.10以下とする。時間は有限かつ非負でなければならないが、
個別recordの速度は正しさの判定に使用しない。

## 6. Silesia候補の機械的選択

各windowについて、次をすべて満たすbudgetだけをeligibleとする。

1. Section 5の必須検証をすべて通過する。
2. `fixed-seed-pseudorandom`を除く5 fixtureのうち少なくとも2件でbreachが正である。
3. 6 fixture合計のcandidate throughputがunbudgeted snapshotを上回る。
4. workspace / HashChainが1.10以下である。

eligible budgetをcandidate / unbudgeted aggregate throughput ratioの降順、同値なら小さいbudget
の順で並べ、各windowの先頭2件までをshortlistとする。eligibleが0件のwindowはSilesia実験
から除外し、1件ならその1件だけを採用する。HashChainに対する速度比は報告するが、合成入力
への過適合を避けるためshortlist条件にはしない。

shortlistはrunner resultに機械的に記録する。人手で値を差し替えず、Silesia測定前に別の
versioned manifestへwindowごとの候補を転記し、reviewとcommitを完了する。timingの再実行で
shortlistが変わる場合は測定ノイズとして調査し、都合のよいrunを選択しない。

## 7. 実行境界

experiment manifestは不活性JSONであり、command、実行ファイルpath、checkpoint path、output
pathまたはnetwork locationを含めない。runnerはmanifestの完全一致、manifest bytesとdigest、
実行ファイル、generatorおよびtool sourceのSHA-256、Git revisionと環境をcheckpoint identityへ
含める。未知field、非canonical record、digest不一致、Exact不一致、既存未完了checkpointと
final outputの競合を拒否する。

runnerと単体テストが完成し、manifest validation、fixture identity、途中再開、改竄拒否、
complete-grid再実行抑止およびshortlist規則を確認するまで実測を開始しない。合成resultのreview
とcommitが完了するまでSilesia用manifestを作成しない。
