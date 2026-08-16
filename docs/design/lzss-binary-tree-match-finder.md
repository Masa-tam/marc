# LZSS BinaryTree Exact一致探索器

## 1. 文書の位置付け

この文書は、LZSSエンコーダー内部へ追加する`BinaryTree Exact`一致探索器の
実装前設計を定める。対象は一致探索だけであり、LZSSトークン、フレーム、
stream variant、algorithm ID、公開C ABIおよびデコーダーを変更しない。

実装初期段階ではprivateかつ明示選択の実験戦略とする。Exhaustiveを正解
オラクル、HashChain Exactを現行production比較対象として維持する。
Silesia全体と合成worst-caseの比較が完了するまで、既定戦略または
`WindowAdaptiveV1`へ昇格しない。

## 2. 採用理由と非目標

1 MiB frameの`dickens`診断では、1 MiB windowで訪問した候補の91.43%が
hash false positiveではなく、実際に同じ5-byte接頭辞を持つ候補だった。
最大query深度は10,864である。この結果はbucket数の単純拡大より、同一
接頭辞集合を辞書順で分割する探索を評価する理由になる。

この単一入力はBinaryTreeの既定化、自動選択閾値またはCorpus全体での優位を
証明しない。本段階の目的は次に限定する。

- Exhaustiveと同じmatchを返す有界かつ決定的な木構造を実装可能にする。
- HashChainとの速度、探索量およびworkspace比較を可能にする。
- 1 MiBを超えるstream profileやBounded探索を先取りしない。

## 3. Exact一致規則

query位置を`p`、window sizeを`W`とする。候補位置`c`は常に
`max(0, p - W) <= c < p`を満たす。比較長はqueryと候補のframe末尾までの
長さ、および設定された`max_match_length`の最小値で制限する。

選択規則は既存Exhaustiveと同じである。

1. 最大の一致長を選ぶ。
2. 同じ一致長では最大の候補位置、すなわち最短距離を選ぶ。
3. 一致長が`min_match_length`未満ならmatchなしとする。
4. token化段階で、既存の厳密なcost規則により有益なmatchだけを使用する。
5. immutableなraw frameを直接比較し、既存と同じoverlap-copy結果を得る。

最大一致長へ到達しても、より近い同長候補を確定するまで探索を終えない。

## 4. 木のキーと全順序

各active位置のsuffixを、最大`max_match_length` byteまでの有限byte列として
扱う。二つのキーは次の順で比較する。

1. unsigned byte値による辞書順。
2. 比較済みbyteがすべて等しい場合は短いsuffixを先にする。
3. capped suffixが完全に等しい場合は絶対入力位置の小さい方を先にする。

絶対位置は同一frame内で一意なので、この規則は全順序になる。pointer値、
native byte order、locale、hash-table反復順序または未規定のlibrary比較を
使用しない。queryは仮想キー`(query suffix, p)`として扱うため、同一byte
keyを持つactive候補の直後に位置する。

## 5. AVL木とcaller-owned workspace

木は固定配列上のAVL木とする。平衡化しない通常の二分木は、ゼロ列や周期列
で線形深度になりHashChainの問題を置き換えるだけなので採用しない。

各nodeは少なくとも次の情報を持つ。

```text
left node index
right node index
parent node index
AVL height
absolute input position
maximum absolute position in the subtree
```

node参照は32-bit indexとし、`UINT32_MAX`をnull sentinelとする。heightは
`uint8_t`の分離配列で保持し、`height == 0`はinactive、active leafはheight 1
とする。32-bit node indexで表現可能なAVL木の最大高はこの範囲に収まる。
位置と部分木最大位置は`size_t`で保持するが、シリアライズしない。

workspace calculatorはnode数、各配列offset、alignment、総byte数および
aggregate input-plus-workspaceをchecked arithmeticで計算する。配列を分離し、
構造体paddingへ依存しない。概算では64-bit環境の1,048,576 nodeに約29 MiB
を要するが、実装されたcalculatorの値だけを正式値とする。

初期layoutは`left`、`right`、`parent`の各`uint32_t`配列、`height`の
`uint8_t`配列、`position`、`subtree maximum position`の各`size_t`配列の順と
する。各配列開始位置をその要素型へalignし、末尾paddingは加えない。inactive
nodeは三つのlinkを`UINT32_MAX`、heightを0、二つのposition値を`SIZE_MAX`で
初期化する。64-bit環境の1,048,576 nodeに対する正式な初期workspaceは
29 MiBである。

workspaceは初期化時に一度だけ構築し、steady-stateでallocateしない。短い
workspace、misalignment、inputとのoverlap、算術overflow、node index範囲、
`max_internal_buffered_bytes`超過はfinderを公開する前に拒否する。

## 6. active windowとnode寿命

finderはquery直前に次の不変条件を満たす。

```text
tree contains every indexable position in [max(0, p - W), p)
tree contains no position outside that interval
```

`indexable`はframe末尾まで少なくとも既存の5-byte prefix長が残る位置とする。
query側に5 byte残らない場合は木を探索せずmatchなしを返す。

node slotは絶対位置をnode capacityで割った剰余に対応させる。parserがmatchで
複数byteを進めた場合も、`advance(position, next_position)`は区間内の各位置を
順番に処理する。位置`x`を追加するときは次の順序を守る。

1. slotに位置`x - W`のactive nodeがあれば削除する。
2. 位置`x`がindexableなら同じslotへ新nodeを構築する。
3. AVL metadataを根まで更新する。

これによりquery位置`x`では距離`W`を候補に含め、そのquery後の挿入時にだけ
退役させる。frame/reset境界ではroot、active nodeおよびmetadataを完全に
初期化する。

## 7. 決定的な挿入、削除および平衡化

挿入は第4節の全順序を使用する。balance factorは`left_height -
right_height`とする。`+2`または`-2`になった最初の祖先からAVL回転を行い、
childのbalanceが同方向または0ならsingle rotation、逆方向ならdouble
rotationとする。各回転後にheightと部分木最大位置を下位nodeから更新する。

削除はnode payloadを別slotへcopyまたはswapしない。slotと絶対位置の対応を
維持するため、0または1 childでは構造的transplant、2 childrenではin-order
successor node自体を構造的にtransplantする。successorの旧parentからrootへ
向かってmetadata更新とAVL rebalanceを行う。root、parent、childの全link更新
順序を単体試験で固定する。

再帰は使用しない。parent linkを使った反復処理とし、入力によるstack消費を
発生させない。回転規則とsuccessor選択には任意性を残さない。

## 8. Exact queryアルゴリズム

### 8.1 最大一致長

仮想queryキーの辞書順predecessorとsuccessorをAVL木から求める。それぞれと
queryのLCPを`max_match_length`まで計算し、大きい方を`L`とする。

この近傍探索はquery位置がfinderの記録する次位置と一致するときだけ行う。
query位置が入力終端と一致する場合、または残りが5-byte prefix未満の場合は
正常な候補なしを返し、木を探索しない。近傍結果はpredecessor/successorの
絶対位置と個別LCPを保持する。この段階では`min_match_length`を適用せず、距離
およびtoken costも決定しない。

辞書順集合では、queryとのLCPが最大となる要素はqueryの直前または直後に
存在する。より離れた要素が長いLCPを持つなら、その共通prefixを持つ要素は
辞書順上連続区間を作るため、直前または直後にも同じ以上のLCPを持つ要素が
存在し、仮定に反する。したがってこの段階で最大一致長はExactに決まる。

### 8.2 最短距離tie-break

predecessorまたはsuccessorだけでは、同じ`L`を持つ辞書順上離れた候補のうち
最も近い位置を取り逃がし得る。このため、先頭`L` byteがqueryと同じ全node
の連続区間を求め、区間内の最大絶対位置を取得する。

prefix区間のlower boundはqueryの先頭`L` byteそのものとする。exclusive
upper boundは、そのprefixを末尾から調べ、最初に`0xff`でないbyteを1増やして
後続を切り捨てた有限byte列とする。全byteが`0xff`ならupper boundなしとする。

AVL nodeの`maximum absolute position in the subtree`を使い、lower/upperの
二つの境界pathだけを辿る。区間へ完全に含まれるsibling subtreeはmetadataを
一度参照してまとめる。全候補を列挙せず、区間内最大位置`c_max`を得る。
`p - c_max`が同長候補中の最短距離になる。

実装では先頭`L` byteだけを比較してprefix区間内のsplit nodeを一つ求める。
splitの左側では同prefix node自身とそのright subtreeを集約してleftへ進み、
prefix未満ならrightへ進む。splitの右側では同prefix node自身とそのleft
subtreeを集約してrightへ進み、prefix超過ならleftへ進む。これら二つの境界
pathにより有限upper-boundをmaterializeせず、`0xff`だけのprefixも同じ処理で
扱う。

`L < min_match_length`なら区間queryを省略してmatchなしを返す。この二段階
手順により、最長一致と最短距離の双方をExhaustiveと一致させる。

private finderの`find_match(position)`は、range結果の候補位置`c_max`から
`distance = position - c_max`を導出し、`LzssMatch{distance, L}`へ変換する。
候補なしまたは内部query errorでは空matchを返す。詳細な内部errorは
`find_candidate`結果に保持し、共通finder conceptへ新しいerror表現を持ち
込まない。

最初のparser統合はprivateなtyped-token single-pass入口とする。callerは入力
byte数分のworst-case token spanとcalculatorどおりのBinaryTree workspaceを
供給する。入口は全buffer境界とaggregate memoryを検証してからfinderを初期化
し、既存parserのcost判定をそのまま使用する。strategy enum、通常のencoder
entry pointおよびframe profileは変更しない。

## 9. 計算量と限界

AVL heightはactive node数`N`に対して`O(log N)`である。挿入、削除、辞書順
近傍探索およびprefix区間集約はそれぞれ`O(log N)` nodeを訪問する。
suffix比較は最大`max_match_length` byteなので、素朴な初版の上界は
`O(max_match_length * log N)` per operationとなる。

初版ではLCP cache、SIMD、複数byte native load、bounded depth、lazy deletion
および非平衡木へ最適化しない。まずExact性と実測を確立する。反復データで
辞書順比較byte数が新たな支配要因になる場合は、同一formatの後続最適化と
して別途設計する。

## 10. 診断統計

統計pointerがnullのproduction経路では診断counterを更新しない。任意統計は
少なくとも次を持つ。

```text
queries
key comparisons
key bytes compared
predecessor/successor LCP bytes compared
prefix-range boundary comparisons
AVL rotations
insertions and retirements
maximum tree height
maximum nodes visited by one query
query node-visit histogram
```

counterはuint64飽和とoverflow flagを使用する。benchmarkはcounter有効の
untimed passとcounter無効のtimed passを分離し、bytes、framesおよびtokensの
一致を要求する。

## 11. テスト要件

実装は次の順序で検証する。

1. workspace offset、alignment、容量、overflowおよびoverlap。
2. 手計算できる挿入、single/double rotation、root削除、2-child削除。
3. 距離`W`をqueryでは含み、その後のadvanceで退役させる境界。
4. matchで飛ばした全位置のindex化。
5. 同一capped suffix群から最新位置を選ぶtie-break。
6. predecessorとsuccessorの両側に最長候補があるfixture。
7. prefixが`0xff`だけのupper-boundなし区間。
8. 空、短入力、全byte値、ゼロ列、周期列、randomおよびhash衝突入力。
9. 各位置の`find_match`をExhaustiveおよびHashChain Exactと差分比較。
10. typed token、serialized tokenおよび全LZSS pipelineのbyte一致。
11. 小入力の有限差分fuzzingと、失敗時の恒久回帰fixture。
12. MSVC、ClangCLおよびCI platform間の決定性。

木の内部shape一致は正しさの条件にしない。ただし同じ実装と入力ではshapeも
決定的であり、match結果とstream bytesは常に一致しなければならない。

## 12. 段階的導入

1. workspace calculatorと空finder初期化。
2. AVL挿入、削除、metadata不変条件validator。
3. predecessor/successor LCPとprefix区間最大位置query。
4. Exhaustiveとの差分match試験。
5. typed-token private entry point。
6. `--frames binary-tree-exact` benchmarkとHashChain比較。
7. 全LZSS encoder経路のprivate比較。
8. Silesia 12 memberと合成worst-case測定。
9. production選択肢への昇格可否を別のdesign decisionで判断。
10. HashChain/BinaryTree双方の証拠がそろった後だけ
    `WindowAdaptiveV1`を検討する。

実装は専用branchで行い、各段階を独立commitにする。BinaryTreeが有効な
streamを生成できても、Exact差分、malformed境界、workspace、安全性および
性能証拠がそろう前に既定経路へ接続しない。
