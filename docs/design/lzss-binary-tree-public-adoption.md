# LZSS BinaryTree Exact公開採用設計

## 1. 目的と採用範囲

この文書は、privateな`BinaryTree Exact`一致探索器を、Contextual LZSS
encoderで明示選択できる公開方針へ昇格する契約を定める。対象は次の5 codec
である。

```text
LZSS Contextual Dynamic Range
LZSS Contextual rANS
LZSS Contextual tANS
LZSS Contextual Blocked Huffman
LZSS Contextual Adaptive Huffman
```

これらは共通のtyped-token producerを使い、64 KiB、1 MiB、4 MiB、16 MiBの
辞書/context profileを持つ。BinaryTreeとHashChainのExact同値性、および
16 MiB Silesia比較の実測根拠があるため、最初の公開境界をここに限定する。
byte-oriented LZSS codec、entropy-none LZSS、LZ77、他の辞書codecおよび
decoderの探索処理は対象外とする。

この変更はencoder policyだけである。stream format、algorithm/variant ID、
typed-token表現、decoder、context model、entropy coder、profile identity、
interoperability schemaおよびarchive順を変更しない。

## 2. 実測から導く選択方針

固定Silesia実験では、BinaryTree/HashChain aggregate throughput比が1、4、
16 MiBでそれぞれ0.694925、1.456408、3.371567となった。一方、16 MiBでも
BinaryTreeが勝ったmemberは12件中7件だけであり、workspaceは64.5 MiB対
464 MiBだった。

従って次を採用する。

1. `HashChain Exact`を全profileの既定値として維持する。
2. `BinaryTree Exact`は利用者が明示選択する。
3. window sizeだけからBinaryTreeを自動選択しない。
4. 実行環境、CPU、経過時間または未記録の測定結果から選択しない。
5. 将来のcandidate-density samplingまたは`Auto`は別設計、別試験とする。
6. BinaryTreeの選択をstreamへ記録しない。

BinaryTreeは4 MiBまたは16 MiBの大窓でHashChain candidateが深くなる入力の
有力候補である。64 KiBと1 MiBでもExactな有効戦略として実装可能だが、速度
上の推奨にはしない。

## 3. 公開C ABI

次のencoder-local selectorを追加する。

```c
typedef uint32_t marc_lzss_match_finder_strategy;

#define MARC_LZSS_MATCH_FINDER_HASH_CHAIN_EXACT  UINT32_C(0)
#define MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT UINT32_C(1)
```

対象5 configの`direction`直後にある32-bit `reserved`を、同じoffsetと幅の
`match_finder_strategy`へ改名する。構造体のextent、後続field offset、alignment
および`MARC_ABI_VERSION == 1`を維持する。従来のall-zero configおよび既存
initializerは値0、すなわちHashChain Exactを選ぶため、ABI-1の既定動作と
byte streamは変わらない。

`config_init()`は明示的にHashChain Exactを設定する。各`config_apply_profile()`
はselectorを利用者固有値として維持し、profile由来のframe、window、payload、
modelおよびaggregate policyだけを従来どおり適用する。同じprofileの再適用は
selectorを含めて冪等でなければならない。不明なselectorまたはprofileでは
configを変更しない。

selectorはencodeでだけ探索器を選ぶ。decodeでは既知の二値を受理するが、
workspaceと生成するdecoderは同一であり、値をstream identityとして扱わない。
これにより方向を切り替える共通設定コードでも既知値が予期せず拒否されず、
decoderがencoder policyへ依存しない。不明な値は両方向で
`MARC_STATUS_INVALID_ARGUMENT`とする。

初回採用では`Exhaustive`、`HashTree`、`Sparse HashTree`、`Bounded`および
`Auto`を公開enumへ含めない。privateな検証・比較実装の存在と、安定した公開
契約への採用は分離する。

## 4. workspaceとhard limit

encodeのworkspace queryはselectorに対応するrepository-owned checked
calculatorを使い、typed tokens、context operations、entropy model、serialized
frame、match finder、alignment paddingおよび全region aggregateを実際の選択に
基づいて返す。decode queryはselectorに依存しない。

BinaryTreeの選択だけを理由に`max_internal_buffered_bytes`を暗黙に引き上げては
ならない。利用者は次の順序で明示的に資源を許可する。

1. `config_init()`を呼ぶ。
2. 必要なら`config_apply_profile()`でwindow profileを適用する。
3. `match_finder_strategy`へBinaryTree Exactを設定する。
4. 必要な`max_internal_buffered_bytes`を明示する。
5. workspace queryを呼び、返された三regionを確保する。

profile helperはselectorを維持するが、BinaryTree向けの追加メモリを推測して
上限へ加算しない。既定policyに収まらなければqueryは確保前に
`MARC_STATUS_LIMIT_EXCEEDED`を返す。利用者は上限を引き上げて再queryでき、逆に
query後もhard limitを厳しく上書きして再queryできる。

calculatorはwindow、frame、`size_t`幅、codec固有token/model extentを正式な
根拠とする。16 MiB global BinaryTree単体の464 MiBという実測workspaceだけを
各codecの総必要量として流用してはならない。算術overflow、node index範囲、
短いworkspace、misalignment、region overlapおよびaggregate超過はtransformを
公開する前に拒否する。失敗時は出力handleをnullのままにする。

## 5. 内部dispatch

対象5 codecは、共通typed-token profile calculator、partitionerおよびstreaming
encoderへ同じstrategyを渡す。dispatchはframeごとに変化せず、transform生成時
に固定する。

```text
HashChain Exact
  -> existing checked HashChain workspace
  -> existing single-pass typed-token producer

BinaryTree Exact
  -> checked BinaryTree workspace
  -> existing single-pass typed-token producer
```

両経路は同じLZSS parameter、longest-match、nearest-distance tie-break、token-cost
判定およびcontext/entropy後段を共有する。同一入力と設定では完成したtoken列と
stream bytesが一致しなければならない。BinaryTree固有の統計、node layout、
pointer幅または選択値を直列化しない。

## 6. 公開ツールと再現性

初回採用の正本はC ABIとする。既存CLI名、位置引数および既定出力を変更せず、
CLI selectorの追加はC lifecycleが完成した後の独立段階とする。benchmarkの
private strategy名は公開enum値の数値契約として使わない。

Exactな二戦略は同じstreamを生成するため、interoperability archiveを追加せず、
既存schemaの再生成もしない。性能測定を再現したい利用者はstream外の記録へ
encoder revision、strategy、profile、hard limitおよび環境を保存できる。

## 7. 検証要件

実装は少なくとも次を固定する。

1. selector型の幅、定数値、5 configの`sizeof`、alignmentおよび全field offset。
2. initializerのHashChain既定値と既存golden streamの不変性。
3. profile helperが両selectorを維持し、再適用で冪等であること。
4. 不明なselectorをquery/create前に拒否し、configやhandleを部分変更しないこと。
5. decode requirementsが二つの既知selectorで完全一致すること。
6. encode requirementsが選択したfinderのcalculator値を含み、BinaryTreeの
   大きなworkspaceを正確に返すこと。
7. 必要aggregateと一致するlimitを受理し、一byte短いlimitを確保前に拒否する
   こと。
8. 短い、misalignedまたは重複したworkspaceでhandleを公開しないこと。
9. 5 codecそれぞれでHashChain/BinaryTreeのtoken summary、fingerprint、完成
   streamおよびround tripが一致すること。
10. empty、one-byte、全byte値、反復、random-like、境界長およびextended-distance
    fixtureを含むこと。
11. 4/16 MiB profileのcalculator試験は巨大確保なしでchecked extentを検証する
    こと。
12. C/C++ header compilation、Static/Dynamic library、MSVC/ClangCL、既存fuzz、
    documentationおよび全interoperability compatibility試験を維持すること。

## 8. 段階的実装

1. 本設計とprovenance/test契約を確定する。
2. 共通内部strategy型とselector-aware workspace calculatorを追加する。
3. typed-token streaming encoderへBinaryTree dispatchを追加する。
4. 5つのC configでABI-preserving selectorを公開する。
5. codec単位でquery/create/round-trip/Exact同値試験を追加する。
6. full suite、sanitizer/fuzz smokeおよびinteroperability compatibilityを通す。
7. 公開CLI selectorまたは自動samplingを別設計として評価する。

2026-08-29時点でContextual Dynamic Range、Contextual rANS、Contextual tANSの
段階5を完了した。各routeは選択したfinderのchecked workspaceを返し、transform
生成時に戦略を固定し、同一Exact token列から同一stream bytesを生成する。残る
Blocked Huffman、Adaptive HuffmanはBinaryTree encodeを明示的にunsupportedとする。

最初の実装完了条件は「BinaryTreeを選べる」だけではない。選択した資源量が
queryへ正確に反映され、全5 codecでHashChainとbyte-identical、失敗が原子的、
既定値と既存archiveが不変であることを同時に満たす必要がある。
