# LZSS HashTree Exact一致探索器

## 1. 文書の位置付け

この文書は、LZSSエンコーダー内部で評価する`HashTree Exact`一致探索器の
実装前設計を定める。HashTreeはHashChain Exactを基底とし、実際に深い探索が
発生したhash bucketだけをAVL木へ一度だけ昇格するprivateなhybrid戦略である。

対象は一致探索だけであり、LZSS token、frame、stream variant、algorithm ID、
公開C ABI、CLI profileおよびdecoderを変更しない。Exhaustiveを正解oracle、
HashChain Exactをproduction比較対象、global BinaryTree Exactを構造上の参照
実装として維持する。HashTreeは合成入力とSilesia全体の性能証拠がそろうまで
既定経路または公開選択肢へ接続しない。

## 2. 根拠と解決対象

global BinaryTreeはSilesia全36 pairでExact token一致を保ち、木の最大高を
20--25、最大query node数を54--65に抑えた。しかしaggregate throughputでは
全windowでHashChainを下回った。合成matrixでも15 pairすべてで下回り、zerosと
periodicではparser tokenが約4千個しかないにもかかわらず、全入力位置をglobal
AVLへ挿入して26--52億byteのkey比較を行った。

一方、HashChainが深くなる1 MiBの`mr`ではglobal BinaryTreeが1.72倍であり、
合成hash-collisionではHashChain比73.5%まで接近した。この証拠から、常時木を
維持することではなく、実際にchain探索が高価になったbucketだけを木へ移す
価値がある。

HashTreeは次を解決対象とする。

- zeros、periodicおよび浅いrandom bucketではHashChainの安い更新を保つ。
- equal-prefix、collisionおよび`mr`型の深いbucketだけを有界木へ昇格する。
- 一度判明したqueryとのLCPを同じtree traversal内で再利用する。
- Exact最長一致と同長時の最短距離を全経路で維持する。
- 昇格判断、結果およびworkspaceを決定的かつcaller-boundedにする。

初版の非目標はpublic strategy、Bounded探索、runtime計時による選択、SIMD、
suffix array、trie、永続allocation、並列bucket処理およびstream変更である。

## 3. 共通prefix partition

HashTreeはHashChainと同じ5-byte prefix hashを一つの共通helperとして使用する。

```text
hash = 0
repeat five bytes:
    hash = (hash << 5) xor (hash >> 2) xor byte
hash = hash xor (hash >> 16)
bucket = hash and (bucket_count - 1)
```

32-bit unsigned演算は2の32乗を法とする。bucket数は
`bit_ceil(min(node_capacity, 65,536))`であり、0または2冪になる。

有効なLZSS matchは少なくとも5 byte一致するため、同じ5 byteを持つ全候補は
必ず同じhashおよびmasked bucketへ入る。hash collisionは別prefixを同bucketへ
追加するだけであり、byte比較を省略する根拠にはしない。したがって現在位置の
bucketだけを完全に探索すれば、他bucketに有効候補は存在しない。

hash計算、bucket数、headおよび32-bit predecessor-distance ringはHashChainと
共有する。hash規則を二つの実装へ複製してdriftさせない。

## 4. bucket stateと一度だけの昇格

各bucketはframe内で次の単調な状態を持つ。

```text
Chain -> PromotedTree
```

降格は行わない。`PromotedTree`でactive nodeが一時的に0になっても状態を保持し、
後続位置はtreeへ挿入する。これにより同じbucketの再構築を防ぎ、1 bucketの昇格を
frame当たり最大1回に制限する。

private設定`promotion_candidate_threshold`は非負整数とする。Chain queryが
実際に訪問したactive候補数が閾値を厳密に超えた場合、そのqueryは最後まで
HashChain Exactとして完了し、matchを確定した後にbucketを昇格する。閾値0は
最初の非empty query後に昇格させる差分試験用である。

最大一致へ早期到達して閾値以下の候補しか訪問しないzerosやperiodicは、bucket
populationが大きくても昇格しない。判定はbucket長、wall-clock時間、CPU種別、
診断pointerまたは過去benchmark値を使用しない。

訪問数は統計の有無にかかわらず局所変数で数える。診断counterはその値を観測
できるが、昇格判断を制御しない。したがってcounter有効の検証passとcounterなし
の計時passは同じ位置で昇格し、同じtoken列を生成する。

HashTreeの`find_match`はmatch確定後にbucketとtrigger深度をpending promotion
としてfinderへ保存するが、indexをその場で変更しない。直後の`advance`がpending
promotionを最初に構築してから消費区間を挿入する。これによりtrigger queryは
完全にChain stateだけを観測し、次のparse位置ではTreeを利用できる。

このstateful queryを`mutable` fieldでconst APIの背後へ隠さない。privateな共通
finder conceptを非const `find_match(position)`へ明示的に改める。Exhaustive、
HashChainおよびglobal BinaryTreeの意味は変えず、公開C/C++ API、tokenまたは
streamへこの変更を露出しない。

初版では閾値をpublic defaultとして確定しない。64、256、1,024など複数値を
private benchmark引数で測定し、Exact性とは独立に評価する。

## 5. 昇格時の構築

Chain headから32-bit predecessor distanceを新しい位置から古い位置へ辿り、
現在query位置`p`に対して次を満たす候補だけをAVLへ挿入する。

```text
max(0, p - window_size) <= candidate < p
candidate has at least five remaining input bytes
candidate hashes to the promoted bucket
```

chainは既にnewest-to-oldest順であり、window外へ到達した後にactive候補はない。
この順序をそのまま決定的なAVL挿入順とする。window全体の再走査、一時vector、
再帰、heap allocationまたはpayload copyを使用しない。

昇格を起こしたqueryの結果は構築前のHashChain結果である。直後の`advance`冒頭
でpending bucketを構築し、新しいtreeは次のqueryからだけ使用する。promotion中に
failureが起きた場合は部分treeを公開せず、finderをsticky-invalidにする。黙って
Chainへ戻って成功を返さない。

bucketはframe中に一度だけ昇格し、各入力位置は初回構築または後続advanceの
どちらかで最大一度treeへ挿入される。

## 6. per-bucket AVL表現

PromotedTree bucketはglobal BinaryTreeと同じ有限capped suffix keyを使う。

1. 最大`max_match_length` byteのunsigned辞書順。
2. 全比較byteが等しい場合は短いsuffixを先にする。
3. capped suffixも等しい場合は絶対位置の小さい方を先にする。

位置tie-breakにより全active位置は別nodeになり、全順序が得られる。各bucketは
独立rootを持つが、node slotは全bucket共通のwindow-sized ringである。slotは
`absolute_position % node_capacity`に対応する。active windowには同じslotを
使う二位置が同時に存在しない。

各nodeは次を分離配列で保持する。

```text
left/right/parent uint32 node index
uint8 AVL height
absolute input position
maximum absolute position in subtree
```

`UINT32_MAX`はnode null、`SIZE_MAX`はposition sentinelである。AVL balance、
構造的transplant、subtree maximum更新、非再帰validatorはglobal BinaryTreeの
証明済み規則を再利用する。ただしrootは一つではなくhash bucketごとに存在し、
回転はbucket境界を越えない。

## 7. active windowの更新

parserはquery後に`advance(position, next_position)`を呼び、skipされた区間も
一位置ずつ処理する。`advance`はまずpending promotionをquery直前のChainから
atomicに構築する。その後、各位置`x`で次の順序を守る。

1. `x >= window_size`なら期限切れ位置`x - window_size`のbucketを計算する。
2. そのbucketがPromotedTreeなら期限切れnodeを構造的に削除する。
3. 位置`x`を共通HashChainへ追加する。
4. 位置`x`のbucketがPromotedTreeなら同じ位置をAVLへ追加する。

query位置`x`では距離`window_size`の候補を含み、そのquery後にだけ退役させる。
Chainは従来どおりdistance検査で期限切れを無視し、treeは明示削除する。frame
resetはhead、link、bucket mode、tree rootおよび到達可能node stateをリセット
する。

## 8. LCPを保持するtree traversal

key比較は単なるorderではなく次を返す。

```text
order: less / equal / greater
lcp: query keyとnode keyの正確な共通prefix byte数
```

tree traversalは現在queryを挟む辞書順lower bracketとupper bracket、および
queryと各bracketの正確なLCPを保持する。現在nodeが両bracketの間にある場合、
queryとnodeが共有すると保証できる長さは両LCPの小さい方である。片側が有限で
ない場合の保証長は0とする。比較はこの保証済みoffsetから開始し、既知prefixを
先頭から再走査しない。

比較後、queryがnodeより大きければlower bracketを、小さければupper bracketを
そのnodeと今回のLCPで更新する。選択したchildは更新後の区間内にあるため、次の
保証長も同じ区間性質から導ける。equalなら位置tie-break前のcapped suffix一致を
記録した上で絶対位置順を適用する。

predecessor/successor探索で得たbracket位置とLCPは最大一致長計算へそのまま渡す。
global BinaryTreeのように最終近傍とのLCPを別loopで再走査しない。prefix-rangeの
二本の境界pathも、保持中のbracket LCPを`min(known_lcp, target_length)`まで再利用
する。

この最適化はhash一致をbyte一致とみなさず、未比較byteを推測しない。保証済み
offset以降は必ずraw inputを比較するためcollisionでもExactである。初版は明快な
scalar比較を使用し、fixed-width loadやSIMDは別の同一結果最適化とする。

## 9. Exact queryとtie-break

Chain bucketでは既存HashChain Exactをnewest-to-oldestに走査する。最大一致長へ
到達した最初の候補は、その長さを持つ最も近い候補なので早期終了できる。

PromotedTree bucketでは仮想query keyのpredecessor/successorを求め、保持済み
LCPの最大を`L`とする。辞書順集合で最大LCP候補が近傍に存在する証明はglobal
BinaryTreeと同じである。

`L >= min_match_length`なら先頭`L` byteがqueryと等しい連続区間を二本のAVL
境界pathで集約し、subtree maximum positionから最も新しい候補を得る。
`distance = query_position - maximum_position`が同長候補中の最短距離になる。

全有効候補はqueryと同bucketにあり、PromotedTreeはそのbucketの全active位置を
含む。したがってChain、昇格trigger queryおよびTreeの三経路はすべてExhaustive
と同じ最長一致・最短距離を返す。promotion閾値はtoken列へ影響しない。

## 10. caller-owned workspace

workspace calculatorは少なくとも次をchecked arithmeticで配置する。

```text
HashChain bucket heads:       bucket_count * sizeof(size_t)
HashChain predecessor links: node_capacity * sizeof(uint32_t)
bucket tree roots:           bucket_count * sizeof(uint32_t)
bucket promotion modes:      bucket_count * sizeof(uint8_t)
tree left/right/parent:      3 * node_capacity * sizeof(uint32_t)
tree heights:                node_capacity * sizeof(uint8_t)
tree positions:              node_capacity * sizeof(size_t)
tree subtree maxima:         node_capacity * sizeof(size_t)
```

各配列開始を要素alignmentへ合わせ、全offsetをrequirementsとして公開する。
64-bit host、1 MiB node capacity、65,536 bucketのpadding前概算は
`33 * node_capacity + 13 * bucket_count = 35,454,976 byte`、約33.81 MiBである。
正式値は実装calculatorだけが決める。

初期化はHashChain配列、tree root、promotion modeだけを構築する。tree node配列を
全clearしない。rootから到達不能なslotを読まず、挿入時に全node fieldを構築し、
削除時にsentinelへ戻す。これによりtreeを使わない入力が29 MiB相当の不要な
初期clearを支払わない。未初期化workspaceを出力、hashまたは比較へ使用しない。

input/output/workspace overlap、alignment、capacity、node index表現、aggregate
memory、limitsおよび算術を最初の変更前に検証する。steady-state allocationと
再帰を使用しない。

## 11. failureとvalidator

初期化failureはfinderを変更しない。advance protocol違反、promotion構築失敗、
slot世代不一致、bucket/root不一致または構造破損はsticky-invalidとし、以後の
mutationを拒否する。共通finder conceptの`find_match`は空matchを返すが、private
詳細入口は安定したHashTree errorを保持する。

非再帰validatorは次を検査する。

- bucket modeがChainならtree rootがnullである。
- 各rootから到達するnodeがそのbucketへhashされる。
- nodeはactive window内でslotとpositionが一致する。
- parent/child reciprocity、acyclic性、AVL order、heightおよびbalance。
- subtree maximum position。
- 異なるbucket tree間でnode slotを共有しない。
- PromotedTreeのactive chain位置集合とtree位置集合が一致する。
- Chain bucketのtree nodeが存在しない。

validatorは通常query経路で呼ばず、試験と診断passだけで使用する。

## 12. 診断統計

既存HashChain/BinaryTree統計を混同せず、少なくとも次を追加する。

```text
chain queries and candidates before promotion
promotion count and triggering candidate depth
promotion-build nodes, key comparisons, and compared bytes
tree queries, nodes, comparisons, compared bytes, and skipped LCP bytes
tree insertions, retirements, rotations, and maximum height
maximum simultaneously promoted buckets and nodes
query path histogram: chain / trigger / tree
```

counterはuint64飽和とoverflow flagを使う。counter有効passとcounterなしpassで
input bytes、frames、tokens、promotion countおよび各promotion位置を一致させる。
診断pointerの有無で昇格してはならない。

## 13. 検証義務

1. workspace offset、alignment、overflow、limit、overlapおよびlazy初期化。
2. hash helperを既存HashChainと共有し、既存HashChain byte列を変えない。
3. 非const finder queryと、他finderでの意味不変。
4. 閾値直下、等値、直上と、0による強制昇格。
5. trigger queryはChain結果とpendingだけを作り、`advance`後の次queryからTreeを使う。
6. promotion chainのnewest-to-oldest構築とwindow端の包含。
7. 一bucket一回だけの昇格と、empty後のPromotedTree維持。
8. skip区間の全位置挿入と距離windowのquery後退役。
9. hash collision、同一capped suffix、短い末尾、`0xff` prefix。
10. LCP bracketの保証offsetを0から全key長まで手計算fixtureで比較。
11. 各位置でExhaustive、HashChain、global BinaryTree、HashTreeを完全比較。
12. token、canonical serializationおよびprivate pipeline byte一致。
13. malformed workspace state、sticky errorおよび有限差分fuzzing。
14. counter有無のpromotion位置とtoken一致。
15. 五種類の合成matrixとSilesia 12 memberの閾値sweep。
16. MSVC、ClangCLおよびCI platform間の決定性。

performance値をtest pass/fail閾値にしない。Exact不一致、構造破損、overflow、
hangまたはcrashは恒久回帰fixtureにする。

## 14. 段階的導入

1. 共通prefix-hash helperと既存HashChain無変更試験。
2. private finder conceptの非const query化と既存三戦略の無変更試験。
3. combined workspace calculator、lazy初期化、bucket mode。
4. Chain-only経路、pending promotion、advance triggerの決定性。
5. per-bucket AVL挿入、削除、validator、LCP comparison result。
6. active chainからのatomic promotion構築。
7. Tree Exact query、prefix-range tie-break、四戦略差分試験。
8. private typed-token entryとcanonical byte比較。
9. 診断counterとstrategy-explicit benchmark mode。
10. 合成threshold sweep。
11. Silesia threshold sweep。
12. production昇格または廃棄を別decisionで判断する。

各段階を独立commitにし、性能証拠前にpublic selector、format、ABI、既定戦略、
`WindowAdaptiveV1`またはinteroperability artifactへ接続しない。

`strategy-explicit benchmark mode`では`hash-tree-exact`を指定し、frameまたは
synthetic modeの末尾に有限なpromotion candidate thresholdを必須指定する。
閾値はstreamやpublic APIへ保存せず、benchmark reportだけに記録する。既存の
Silesia JSON runnerへは合成threshold sweepの後にversioned schemaとして接続する。

## 15. 合成threshold sweep契約

private HashTreeをSilesiaへ接続する前に、外部データを必要としない独立runnerで
promotion thresholdの挙動を比較する。runnerは既存の二戦略用
`marc-lzss-match-finder-synthetic-v1`を変更せず、
`marc-lzss-hash-tree-threshold-synthetic-v1`を生成する。

既定matrixは5つの決定的合成case、64 KiB、256 KiB、1 MiB window、および
閾値0、4、16、64、256、1024、4096からなる。この対数的な閾値集合は
Chain、promotion、Treeの仕事量が移行する範囲を観測するための実験設定であり、
production既定値を意味しない。callerは重複しない有限uint64閾値と正のwindowを
明示して置換できる。

各case/windowについてHashChain Exactを一度だけbaselineとして測定し、その後
すべてのHashTree閾値を測定する。各HashTree reportは次を満たさなければならない。

1. mode、case、input、frame、window、iterations、thresholdが要求値と一致する。
2. generic query数がChain routeとTree routeの和に一致する。
3. trigger query数がpromotion数に一致する。
4. ChainおよびTreeの各depth histogramの和が対応route数に一致する。
5. 必須のworkspace、component cost、最大値、時間値がすべて存在する。
6. token数が同じcase/windowのHashChain baselineと完全一致する。

不一致時は部分JSONを公開せず失敗する。時間やthroughputは記録するがpass/fail
閾値にはしない。JSONはbaseline records、threshold records、および
threshold/window別の合成集計を分離し、実行command、revision、environmentと
完全なreportを保持する。このrunnerはnetworkもSilesia Corpusも使用せず、
public API、stream、ABI、encoder選択、既存JSON schemaを変更しない。

最初の完全合成matrixでは全105 HashTree測定がHashChain baselineとExact一致したが、
throughputでbaselineを上回る測定はなかった。閾値0と4は大windowでpromotion過多、
4096は1024とroute構成がほぼ重複した。したがって次のSilesia段階は
16、64、256、1024だけを候補とする。これはproduction既定値の選択ではなく、
異なる移行域を実データで評価するための縮約である。HashTreeのproduction昇格は
引き続き禁止し、Silesia結果または初期化・未promotion経路の改善証拠を別decisionで
要求する。

## 16. 最初のSilesia判定

revision `b704ca5`の完全Silesia matrixでは、全144候補が36 HashChain baselineと
Exact一致した。HashTreeは1 MiB windowの`mozilla`、`mr`、`reymont`で局所的に
baselineを上回り、最大は`reymont` threshold 256の1.24倍だった。しかし
Corpus aggregateでは最良のthreshold 1024でも64 KiB、256 KiB、1 MiBで
HashChainの0.158倍、0.293倍、0.744倍であり、production昇格条件を満たさない。

threshold 1024はChain候補訪問をwindow順に71.9%、80.5%、85.4%削減したため、
Tree queryへの置換そのものには効果がある。一方、maintenance key byte comparisonは
約1016億、1242億、785億回、最大workspaceはHashChainの3.83倍、6.04倍、7.51倍
だった。したがって支配的問題はpromotion thresholdの微調整ではなく、長いordered
keyを使うbuild/insert/retire維持とcombined workspaceである。

現在のHashTreeはprivateのまま不採用とする。1 MiBを超えるwindow拡張より先に、
maintenance比較とworkspaceを構造的に削減する後継設計を別decisionで定義する。
後継はExact token、bounded memory、決定性、全既存試験を維持し、同一syntheticと
Silesia matrixを再実行しなければならない。既存実装を単一thresholdへtuningして
production化してはならない。

## 17. Maintenance v2: direct retirement and structural hot-path checks

Silesiaで支配的だったordered-key maintenanceを、workspace形式の変更と
混同せず先に分離して削減する。第一段階ではbucket、node、root、left、
right、parent、height、absolute position、subtree maximumの全配列と
promotion/query契約を変更しない。これによりCPU効果と後続workspace
圧縮の効果を別々に評価できる。

promotion時の完全validatorがordered AVLを公開し、挿入と削除がその不変条件を
保存することを帰納的契約とする。hot pathの各探索stepは子ノードと
親ノードの長いsuffix keyを再比較せず、次の構造とmetadataだけを
有界に確認する。

- node index、child index、parent indexが範囲内かnullである。
- childが指すparentとparentが指すchildが相互に一致する。
- 訪問回数がcapacityを超えず、cycleを無限追跡しない。
- height、balance、subtree maximumが直接の子から得る値と一致する。
- nodeのabsolute positionが対応するring slotとbucketに属する。

挿入では新位置と各訪問ノードを1回だけ比較し、その結果で次の子を
選ぶ。局所構造確認のためのordered-key比較は行わない。

期限切れ位置の削除では、対象nodeを`position % capacity`から直接
得る。保存されたabsolute positionとbucketを確認し、parent chainをrootまで
追跡して到達性と相互linkを有界に確認する。その後はnode handleで直接
構造削除し、successor探索もchild/parent/metadataのみを確認する。削除時の
suffix-key searchはゼロでなければならない。

キー順序の破壊は、promotion公開前validator、明示的active-range validator、および
独立した破壊試験が検出する。hot mutationは内部workspaceに対する完全な
再検査ではなく、正常に公開された不変条件を保存する操作とする。

参照実装とv2に同一のbuild、leaf/one-child/two-child/root削除、ring
wraparound、randomized insert/retireを与え、rootと全配列の同一性を確認する。
integrated finderではHashChainとExhaustiveに対するExact token同一性を維持する。
そのうえでsynthetic matrixを先に再実行し、maintenance key-byte comparisonの減少を
確認する。Exactが一件でも異なるか、比較削減が実質的でなければSilesiaへ
進まない。第二段階のworkspace圧縮と1 MiB超windowは、このCPU変更の
証拠と分離した別decisionとする。

## 18. Maintenance v2のsynthetic判定

revision `c270a76`の旧finderとrevision `212a671`のv2 finderを、どちらも
MSVC 19.51.36252.0で専用benchmark targetとしてbuildし、同一の5 case、3 window、
7 thresholdを完全測定した。全105候補でExact tokenは一致し、token、
route、promotion、insertion、retirement、rotation、workspaceの945比較値も
一致した。

v2はmaintenance key comparisonを64.05%～80.01%、key-byte comparisonを63.79%～
89.92%削減した。build directory間の絶対速度差をv2効果と混同しないため、
それぞれの同一run内HashChain baselineで正規化する。最良HashTree/HashChain比は
64 KiBで0.316から0.437、256 KiBで0.445から0.561、1 MiBで0.756から
0.893へ改善した。最速thresholdは旧の1024/4096/4096からv2では全window
64へ変わった。

比較削減と正規化速度改善はsynthetic gateを通過するに十分である。ただし
105候補のうちHashChainに勝った個別測定はゼロであり、production採用は依然
禁止する。次に同一のSilesia threshold 16、64、256、1024を再測定し、実データの
局所勝利とaggregateを判定する。workspace圧縮と1 MiB超windowはその後の
別decisionに留める。
