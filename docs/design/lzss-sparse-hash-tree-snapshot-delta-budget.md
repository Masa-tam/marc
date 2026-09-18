# LZSS Sparse HashTree snapshot delta budget

## 1. 目的

完了済みimmutable snapshot実験では、mutable Sparseに対する保守費削減は確認できたが、
HashChainに対する採用条件は全windowで未達だった。特に64 MiB windowではsnapshot後に
到着したchain deltaの候補走査が合計92,199,351,781件に達した。

次のprivate仮説は、1 queryのdelta候補数が明示的な予算を超えたsnapshotを維持せず、
正確なquery完了後にそのbucketを既存HashChainへ戻すcircuit breakerである。目的は
snapshotを公開採用することではなく、悪化が観測されたbucketでsnapshot queryと長い
delta scanを反復する状態を止めることである。

## 2. 非目標

この段階では次を行わない。

- stream format、codec ID、C ABI、CLI、profileまたは既定strategyの変更
- query途中での候補打ち切り
- delta用の第二tree、追加hash table、複数snapshotまたは途中rebuild
- Silesia resultを見ながらの閾値調整
- immutable snapshot strategyのpublic admission

既存のHashChain、mutable Sparseおよび予算なしimmutable snapshotの動作は変更しない。

## 3. Exact契約

delta candidate budgetは探索結果を近似してはならない。queryは従来どおりsnapshotと、
snapshot watermarkより新しいchain deltaの全候補を評価し、nearest-distance tie breakを
含むExact matchを確定する。予算超過はquery完了後にだけ判定する。

したがって、予算を超えた当該queryにも次が必要である。

- Exhaustive ExactおよびHashChain Exactと同じmatch
- 同じtyped-token列と`token_fingerprint_sha256`
- 実際に訪問した全delta候補の診断計上
- error時にmaintenance stateをcommitしないfailure atomicity

予算を探索上限として使って早期returnすることは禁止する。それはより古い候補の長い
matchまたは同長で近い候補を見落とし得るためである。

## 4. 状態遷移

private controller optionに`delta_candidate_budget`を追加する。0はcircuit breaker無効で
あり、既存immutable snapshotと同じ動作を保つ。正の値`B`では、成功したsnapshot queryの
`delta_candidates_visited > B`をbudget breachとする。等しい場合はbreachではない。

queryはbucketとrelease reasonをpending stateへ記録する。既存のwindow expirationと同時に
成立した場合はbudget breachを優先する。同一bucketのquery再試行はbreach countを重複
計上せず、異なるpending bucketを要求する呼び出しは従来同様`invalid_protocol`とする。

次の正しいadvanceはsnapshotを検証して全nodeをreleaseした後、bucket metadataを次のように
commitする。

```text
root       = null
node_count = 0
mode       = pool_rejected_chain
```

`pool_rejected_chain`は既存の有効なchain-only終端状態であり、当該フレーム中の再昇格を
防ぐ。以後のqueryは既存HashChain Exact経路を使用する。snapshot解放前の検証またはpool
releaseが失敗した場合、modeを変更せずsticky errorとする。

window expirationだけによる従来releaseは引き続き`chain`へ戻す。これによりbudget policyを
無効にした既存lifecycleを変えない。

## 5. メモリと時間の境界

既存のpending bucketにrelease reasonと予算値を保持するだけなので、per-position array、
第二indexおよび動的確保は追加しない。workspace queryの結果は既存immutable snapshotと
同一で、追加workspaceは0 bytesである。

このpolicyはbreachを検出したquery自体の時間を制限しない。またdemotion後のHashChain
queryを一定候補数へ制限するものでもない。保証するのは、同じsnapshotに対する予算超過
delta合成を後続queryで反復しないことだけである。最悪経路は既存HashChainへ収束し、
Snapshot固有の追加tree queryは停止する。

## 6. 診断

既存fieldを変更せず、少なくとも次をprivate statisticsへ追加する。

- budgetを評価したsnapshot query count
- budget breach count
- budget demotion count
- breach時のmaximum delta candidates

counterは既存統計と同じchecked saturationおよび`overflowed`契約を使う。breach countは
pendingへの最初の遷移時、demotion countはsnapshot解放とmetadata commitの成功後にだけ
増加する。budget無効時は全fieldが0でなければならない。

## 7. 段階的検証

1. controller単体で`B-1`、`B`、`B+1`候補を固定し、strict greater-than境界を証明する。
2. budget breach queryがExact matchを返した後にだけpendingとなることを証明する。
3. advance後の`pool_rejected_chain`、全node解放、再昇格抑止を証明する。
4. expiration-onlyは`chain`、同時成立はbudget demotionとなる優先順位を固定する。
5. Exhaustive、HashChain、予算なしsnapshot、budgeted snapshotのbytewiseおよび
   token-boundary differentialを実行する。
6. malformed state、counter overflow、out-of-order query/advanceを限定的なdeterministic
   fuzz-regressionへ追加する。
7. private benchmarkへ明示的strategy名とbudget診断を接続する。
8. 合成入力で閾値候補を事前固定してから、別manifestのSilesia実験を行う。

controller、differential、fuzz、benchmark、固定実験はそれぞれ独立したcommit gateとする。

## 8. 採否

このpolicy単独の成功はimmutable snapshotのpublic admissionを意味しない。後続の固定実験は
HashChain、予算なしimmutable snapshot、budgeted snapshotを同一member/windowで比較する。
全点で五つのExact identity fieldを一致させ、次をwindowごとに判定する。

- budgeted / HashChain aggregate throughputが1を超える
- 12 member中6以上でHashChainより速い
- budgetedが予算なしsnapshotよりaggregateで速い
- breachとdemotionが正かつ同数で、tree insertion/retirementが0
- workspace / HashChainが1.10以下

HashChain採用条件を満たさない場合は引き続きprivateとする。改善がsnapshot比だけに留まる
場合も、circuit breakerの構造的証拠として記録するに留める。

