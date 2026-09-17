# LZSS Sparse HashTree immutable snapshot + chain delta

## 1. 目的

この文書は、privateな`Sparse HashTree Exact`のpromotion後保守費を除去する
独立仮説を定める。promotion時に構築したtreeをimmutable snapshotとし、その後に
到着した同bucketのpositionを既存HashChainの有界な接頭deltaとして検索する。
treeへのpositionごとのinsertとretireを行わず、snapshot全体がwindow外へ
出た時だけpoolへ一括返却する。

BM-0071のreuse-one aggregateでは、4、16、64 MiBのtree queryはそれぞれ
52,062、36,300、34,679回だった。これに対しinsertは321,135、230,274、
222,217回、retireは165,347、19,263、0回、maintenance key-byte comparisonは
420,100,929、414,235,587、422,644,212回である。tree query自体のkey-byte
comparison 14,035,541、10,057,798、9,796,264回より、promotion後の可変保守が
大きい。この仮説はその費用を直接変更するものであり、pool、candidate
thresholdまたはreuse thresholdの追加調整ではない。

stream format、token、decoder、public C ABI、codec profile、既存strategy、圧縮結果と
Exact tie breakは変更しない。初期実装はprivateな参照経路とする。

## 2. snapshotとdeltaの所有権

promotion transactionがcommitした時点で、そのbucketのpool treeはimmutable
snapshotになる。snapshot watermarkはrootの`subtree_maximum_position`から導き、
bucketごとの別watermark配列は追加しない。watermark以下のpositionだけが
snapshotに属する。

既存の`heads`/`links` chainはpromotion後も全positionについて更新する。query時は
headからwatermarkより新しいpositionだけをdeltaとして調べ、watermark以下で
停止する。したがってdelta専用node、link、allocation、再帰構造は必要ない。
chainの走査は現window内とcaller-owned ringの大きさで必ず有界となる。

v1では途中rebuildを行わない。snapshot watermarkがqueryのwindow下限より
小さくなればsnapshotにactive nodeは一つも残らない。そのqueryはdelta、すなわち
現window内のchain全体だけでExact結果を得る。次のadvanceでsnapshotを検証して
一括解放し、bucketをchain modeへ戻す。再promotionは既存のgateに委ねる。

## 3. active-aware snapshot query

既存の`query_lzss_hash_tree_bucket_exact`implは、訪問nodeがwindow外なら
invalid treeとするため変更せず保存する。snapshot専用queryは次の別契約を
持つ。

1. nodeのindex、parent/child、height、position、bucket hashおよびBST orderingは
   従来どおり検証する。
2. window外nodeは構造上の探索経路として許容するが、match候補には
   しない。
3. `subtree_maximum_position < window_begin`の部分木はactive nodeを持たないと
   証明できるため訪問しない。
4. active nodeだけからquery keyのpredecessor/successorを求め、最大LCPとその
   prefix range内の最新positionを求める。stale node由来のLCPを候補長に
   使用しない。
5. traversalはpool capacityを上限とし、cycle、不正index、不正subtree maximumを
   stable errorで拒否する。

delta chainとsnapshot queryの結果は、長いmatchを優先し、同長では小さいdistance、
すなわちより新しいpositionを優先して統合する。これはExhaustiveとHashChainの
Exact tie breakと同じである。

## 4. 更新、期限切れ、失敗原子性

snapshot modeでは新positionをtreeへinsertせず、期限切れpositionをtreeから
detachしない。chain head/linkの更新だけを行う。これによりsnapshot生存中の
tree insertion countとretirement countは常に0でなければならない。

promotionと一括releaseは従来と同じvalidate-before-commitとする。構築失敗時は
chain modeとpool free listを変更しない。release検証失敗時はsnapshot metadata、
root、count、poolを変更しない。queryはread-onlyであり、失敗時にchain、tree、
promotion stateまたは統計のcommit済み値を部分更新しない。

poolがpromotion時に不足する場合は既存どおりterminal
`pool_rejected_chain`とする。snapshot化はこのpolicyを変えない。

## 5. workspaceと診断

v1は既存Sparse workspaceのheads、links、mode、root、node count、pool node配列と
promotion stateだけを使う。snapshot watermarkをrootのsubtree maximumから求るため、
追加workspaceは0 bytesである。workspace query、aggregate limit、no-allocationの
初期化契約は変えない。

private診断は少なくとも次をchecked `uint64_t`で報告する。

- snapshot query countとvisited node count;
- stale subtree prune count;
- delta query count、candidate countとmaximum depth;
- snapshot promotion、expirationとbulk-release count;
- promotion build node/key/rotation count;
- snapshot生存中のtree insertion/retirement count。

最後の2値が0でないreportはrunnerが拒否する。overflowした診断は性能判断に
使用しない。

## 6. 正しさと初期ゲート

実Corpus測定より前に、次を順番に満たす。

1. active-aware tree queryを手計算可能な小treeで試し、root、predecessor、
   successor、prefix-range中の各stale/active組合せを網羅する。
2. deltaなし、snapshotなし、両方あり、watermark境界およびsnapshot全期限切れで
   Exact tie breakを固定する。
3. promotion、query、chain update、expirationとbulk releaseの失敗原子性、二重解放拒否、
   pool不足およびカウンタ境界を試す。
4. binary、反復、random、window前後でExhaustive、HashChain、従来Sparseと
   `token_count`、`literal_count`、`match_count`、`matched_bytes`、
   `token_fingerprint_sha256`を比較する。
5. MSVC、ClangCL、sanitizer/fuzzと完全CTestを通す。
6. implementationと診断の固定後にだけ、少数の合成fixtureによる早期性能ゲートを
   別decisionで凍結する。Silesia matrixはその後でさらに別定義とする。

v1は途中rebuild、delta専用バッファ、複数snapshot、adaptive threshold、自動selector、
public APIとformat変更を行わない。これらを追加して初期ゲートを通すことを
禁止する。

## 7. 採否境界

この変更は既存Sparseの可変treeに対するprivateな別strategyである。正しさ、
有界性、0-byte workspace delta、zero steady-state tree mutationが一つでも崩れれば即時棄却する。
それらを満たしても、事前固定した性能ゲートを通るまでpublic strategyへ
昇格しない。負の結果も実装と診断の証拠として保持する。

## 8. 実装状態

最初の正しさゲートとして、privateな
`query_lzss_hash_tree_snapshot_exact`を実装した。既存の
`query_lzss_hash_tree_bucket_exact`は変更せず、pool-local identityだけを受ける
別契約としている。親pointerを用いた反復走査で再帰と追加workspaceを避け、
edge走査をpool capacityの3倍以内に制限する。

この参照queryはstale nodeをrouting pivotとして検証しつつmatch候補から除外し、
`subtree_maximum_position`で完全に期限切れた部分木をpruneする。active候補では
最大LCP、同長なら最新positionを選ぶ。index、親子関係、height、BST ordering、
bucket hash、subtree maximumまたはidentityが不正ならbounded errorを返す。

手計算可能な小treeで、stale rootの下にあるactive child、snapshot全期限切れ、
同長候補の最新position選択、metadata破損およびring identity拒否を固定した。
MSVCとClangCLで新規4件と既存mutable-query 9件が通過している。現時点では
controller、chain delta、lifecycle、診断には接続しておらず、公開API、format、
workspace queryにも変更はない。

次の正しさゲートとして、privateな
`query_lzss_sparse_hash_tree_snapshot_delta_exact`を追加した。この合成queryは
snapshot rootの`subtree_maximum_position`をwatermarkとして読み、既存chain headから
watermarkより新しいpositionだけをdeltaとして走査する。snapshotとdeltaの結果は
長いmatch、同長なら新しいpositionの順で統合する。watermarkがactiveな間の
探索継続中にchainがwatermark到達前で途切れる、またはwatermarkを飛び越える状態は
metadata破損として拒否する。最大長の最新候補を得た場合は、それより古いlinkを
読まずに確定できる。snapshot全体が期限切れた場合はwindow内のdeltaだけで結果を得る。

watermark直結、同長delta優先、snapshot長優先、delta長優先、snapshot全期限切れ、
不正link、active watermark飛び越し、snapshot error先行を小fixtureで固定した。
MSVCとClangCLで合成query 8件と下位snapshot query 4件が通過している。この段階でも
既存controller、更新lifecycle、診断、workspace、公開APIとformatは変更していない。

期限切れsnapshotの一括解放に先立ち、privateな
`validate_lzss_hash_tree_snapshot`と
`release_lzss_sparse_hash_tree_snapshot`を追加した。validatorは現在windowやchainに
依存せず、pool-local treeの全nodeを親pointerで反復走査する。index、親子関係、
height、bucket hash、局所subtree maximum、全体のin-order ordering、reachable node
countをmutation前に検証する。

releaseはvalidator成功後にだけleafからpoolへ返却する。snapshot構築後にchainが
delta更新やring overwriteで変化していても、immutable tree自身が完全なら解放できる。
metadata破損、node count不一致または別poolのarray viewは解放開始前に拒否する。
全期限切れtreeの全node検証、現chain非依存の全解放、破損時のzero release、pool view
不一致をMSVCとClangCLで固定した。既存controllerへの接続とbucket metadata commitは
まだ行っていない。

専用のprivate lifecycle境界として
`LzssSparseHashTreeSnapshotControllerState`、
`query_lzss_sparse_hash_tree_snapshot_controller_exact`および
`advance_lzss_sparse_hash_tree_snapshot_controller`を追加した。既存のmutable controllerは
変更せず、非promotion bucketのqueryは既存chain queryへ委譲する。promotion後のqueryは
immutable snapshotとchain deltaを統合し、advanceは新しいpositionをchainへだけ挿入する。
したがってsnapshot生存中のtree insertion countとretirement countは常に0である。

snapshot全体の期限切れをqueryが検出すると、stateの`pending_release_bucket`へ同じbucketを
記録する。次のadvanceは通常更新より先にsnapshotを完全検証して一括解放し、成功後にだけ
rootをnull、node countを0、modeをchainへcommitする。検証または解放失敗時はこれらの
bucket metadataとpool countを維持してstateをpoisonする。同一advanceではその後の
promotionとchain挿入を行わない。一つのpending bucketだけを許すことで追加workspaceを
一定量に保ち、二つ目の期限切れ通知はprotocol errorとする。

小fixtureでpromotion後のtree metadata不変、deltaだけの更新、期限切れ通知から次advance
でのchain mode復帰、解放失敗時のmetadata非破壊性、順序違反advanceのzero mutationを
MSVCとClangCLで固定した。この段階でも公開API、format、workspace queryは変更していない。
private診断counterの集約、Exact differential、fuzzingおよびmatch finderへの接続は後続の
独立したgateとする。

private controllerの診断集約を実装した。`LzssMatchFinderStatistics`の既存promotion
構築node、key比較、rotation、tree insertionおよびretirement counterを再利用し、
snapshot固有にはquery count、visited node count、stale subtree prune count、delta
query count、candidate count、maximum candidates per query、promotion count、expiration
count、bulk release countを追加した。counter加算は既存診断と同じくcheckedで飽和し、
`overflowed`を設定する。最大値は単調なmaxで集約する。

実フィールドは`hash_tree_snapshot_query_count`、
`hash_tree_snapshot_query_node_count`、
`hash_tree_snapshot_stale_subtree_prune_count`、
`hash_tree_snapshot_delta_query_count`、
`hash_tree_snapshot_delta_candidate_count`、
`hash_tree_snapshot_delta_maximum_candidates_per_query`、
`hash_tree_snapshot_promotion_count`、
`hash_tree_snapshot_expiration_count`、
`hash_tree_snapshot_bulk_release_count`である。

expirationは同じpending bucketの再queryでは重複計上せず、最初にpendingへ遷移した時だけ
記録する。bulk releaseは検証と全node解放およびbucket metadata commitの成功後にだけ
計上する。失敗したqueryであっても実際に行ったbounded探索量は診断へ反映する。tree
insertion countとretirement countは専用controller経路では0のままでなければならない。
通常query、全期限切れprune、delta深さ、promotion、release、counter飽和をMSVCと
ClangCLで固定した。benchmark出力への接続、Exact differential、fuzzingおよびmatch
finderへの接続は引き続き独立した後続gateである。
専用controller経路では`hash_tree_insertion_count`と
`hash_tree_retirement_count`は0である。
