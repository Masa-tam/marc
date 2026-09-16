# LZSS Sparse HashTree 反復利用ゲート

## 1. 目的

この文書は、privateな`Sparse HashTree Exact`に対して、単発の高コストな
HashChain queryではなく、同じbucketで反復して発生する高コストqueryだけを
promotion候補にする機構実験を定める。pool capacityまたは既存candidate
thresholdの追加調整ではなく、hot-bucket選択を直接変更する独立仮説である。

2026-09-16に完了した4、16、64 MiB Silesia matrixでは、全27候補が
HashChainにaggregate throughputで敗れた。各windowで最良だった4,096-node
pool、candidate threshold 64でも、promotion 1回当たりの後続tree queryは
それぞれ8.69、5.19、4.75回に留まった。一方、4 MiB、262,144-node pool、
threshold 1,024では155.99回まで増えたがthroughput比は0.874557に留まり、
単にtreeを長く保持するだけでも構築・維持費を回収できなかった。

仮説は「最初の高コストqueryで即時promotionするため、将来の再利用を示して
いないbucketへpoolと構築費を先着順に割り当てている」である。反復利用ゲートは
一時的なchain深度の突出を除外し、小容量poolを反復して高コストとなるbucketへ
優先的に使う。stream format、token、decoder、public C ABI、codec profile、
既定strategyおよびExact tie breakは変更しない。

## 2. 状態と遷移

workspaceへbucketごとの飽和`uint8_t` qualifying-query countを追加する。最大
bucket数65,536に対する追加量は最大65,536 bytesであり、checked workspace
calculatorとaggregate limitへ含める。global mutable stateや動的な追加確保は
用いない。reuse threshold 1ではcountを参照しないため領域を確保せず、従来の
workspaceサイズと再現済みbenchmark契約を維持する。

Chain modeで完了した各queryを次のように処理する。

1. candidate countが既存の`promotion_candidate_threshold`以下なら、そのbucketの
   qualifying countを0へ戻す。
2. thresholdを超えたら、そのbucketのcountを`UINT8_MAX`まで飽和加算する。
3. countが`promotion_reuse_threshold`へ達したqueryだけをpending promotionとする。
4. promotionをcommitした時点でcountを0へ戻す。
5. `pool_rejected_chain`は既存どおりterminalであり、countを更新せず再試行しない。

他bucketへのqueryはcountを変えない。このため「連続」は同一bucketについての
観測列を意味し、query全体で隣接している必要はない。reuse thresholdの有効範囲は
1から`UINT8_MAX`とし、1は現在の即時promotionと同じ遷移を再現する。既存
candidate threshold、build、transactional commit、pool rejectionおよびtree
maintenanceの意味は変えない。

## 3. 正しさと失敗原子性

この機構は候補を調べる順序と最長一致の選択を変えない。全設定でHashChainと
次の5 fieldが一致しなければならない。

```text
token_count
literal_count
match_count
matched_bytes
token_fingerprint_sha256
```

query完了前、無効bucket、算術異常またはpromotion transaction失敗時には
qualifying countを部分更新しない。pending/building中の重複呼出し、commit、
frame初期化およびエラー後の状態を明示的に試す。飽和はwraparoundしてはならない。
workspaceを1 byteでも不足させた初期化は、caller storageを公開状態として変更せず
拒否する。

## 4. 段階的な検証

最初の実装段階ではCorpus性能を測らない。次を順に満たす。

1. promotion stateとは分離したcaller-owned bucket count viewとchecked calculatorを
   定義する。
2. reuse threshold 1が既存のquery、trigger、promotionおよびrejection列を再現する
   ことを固定testで証明する。
3. transientな1回の深いqueryではpromotionせず、同一bucketの必要回数目でだけ
   pendingとなることを手計算可能なfixtureで証明する。
4. threshold以下の同一bucket queryによるreset、他bucketによる非reset、飽和、
   terminal rejectionおよび失敗原子性を試す。
5. Exhaustive、HashChainおよび従来SparseとのExact identityをbinary、反復、境界
   inputで証明する。
6. MSVC、ClangCLおよび完全CTestの後に、独立した固定Corpus matrixを設計する。

性能matrixはmechanismと診断が確定した後に別decisionで凍結する。過去の結果を
見ながらreuse threshold、poolまたはcandidate thresholdを逐次変更しない。

## 5. 採否境界

反復利用ゲートはprivate研究機構であり、実装完了だけではpublic strategy、
自動selector、既定profileまたはstream variantへ昇格しない。後続の事前固定
matrixは少なくともHashChainに対するaggregate throughput、member勝数、workspace、
promotion、tree-query再利用、build/maintenance費、pool rejectionを報告する。

Exact不一致、非bounded state、workspace契約違反または失敗原子性違反は性能に
かかわらず棄却する。正しさを満たしても構築・維持費を回収できなければ、負の
結果として機構と証拠を保持し公開しない。

## 6. 実装状況

2026-09-16時点で、Section 4の段階1から4までをprivate実装へ接続した。
`promotion_reuse_threshold`はmatch-finder option、checked workspace calculator、
promotion stateおよびcontrollerで同じ値を共有する。threshold 1は空のcount viewを
要求して従来遷移を維持し、threshold 2以上はbucket数と同じ長さのcaller-owned
viewを要求する。値0、長さ不一致および未初期化stateはquery統計やcountを変更する
前にsticky errorとして拒否する。

同一bucketのqualifying queryだけを飽和加算し、non-qualifying queryはそのbucketだけを
resetする。他bucketは独立し、pending中の再通知は冪等である。promotion commitが
成功した場合だけ対象bucketをresetし、失敗したtransactionはcountを保持する。
Section 4の段階5では、binary、単一値反復およびwindow境界入力を、従来値1、
ゲート有効値2および飽和上限値`UINT8_MAX`でtyped-token列とcanonical byte列まで
Exhaustive、HashChain、既存tree finderおよび従来Sparseと比較し、Exact identityを
確認した。Corpus性能測定はまだ開始しておらず、次段階は両toolchainの完全CTestで
ある。MSVCおよびClangCLで3,490件の完全CTestを実行し、長時間の64 MiB試験と
`marc_interoperability_schema_compatibility`を含め全件成功した。これによりSection 4の
段階1から6までが完了した。次段階では測定値を見る前に、独立した固定Corpus matrix、
診断項目および採否規則を文書化する。
