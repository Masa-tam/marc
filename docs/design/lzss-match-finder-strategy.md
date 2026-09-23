# LZSS一致探索戦略

## 1. 文書の位置付け

この文書は、marcのLZSSエンコーダーに追加する一致探索戦略の設計方針を
定める。探索方法はエンコーダー内部の解析方針であり、LZSSトークンの
デコード規則やストリーム表現ではない。

最初の実装対象は、現在の完全探索を正解オラクルとして維持した上での
`HashChain Exact`である。global `BinaryTree Exact`はExact性を満たし、後続の
16 MiB Silesia測定で大窓かつ深い候補列に対する有効性が確認されたため、
Contextual LZSS encoderだけを対象とする明示的な公開採用段階へ進む。既定値は
HashChainのまま維持し、window sizeだけによる自動選択は行わない。詳細は
[`lzss-binary-tree-public-adoption.md`](lzss-binary-tree-public-adoption.md)で
定める。`WindowAdaptiveV1`および`Bounded`は将来候補として扱う。

## 2. 背景

現在のLZSS一致探索は参照実装として明快な完全探索を使用する。

各入力位置について、距離1からスライドウィンドウ上限まで候補を順番に
調べ、各候補を最大一致長まで1バイトずつ比較する。この実装は決定的で
検証しやすい一方、最悪計算量は概ね次のようになる。

```text
O(input_size * window_size * maximum_match_length)
```

さらに、現在の計画処理と実書き込み処理が同じLZSS解析をそれぞれ行う
経路では、一致探索も二回実行される。

参照実装を正解オラクルとして維持しつつ、ハッシュチェーンおよび二分木
による高速な一致探索を追加する価値がある。

## 3. 形式variantと探索戦略の分離

一致探索方法は、LZSSトークンのデコード規則やトークン表現そのものでは
ない。したがって、辞書コーダーのストリームvariantとは概念的に分離し、
エンコーダーの`match_finder_strategy`として扱う。

```text
LZSS stream variant
    デコーダーから見えるトークン表現と復号規則

LZSS match finder strategy
    エンコーダーが一致候補を発見するための解析方法
```

同じ探索規則、入力、LZSSパラメーターおよび設定からは、プラットフォーム
に依存しない同一トークン列を生成しなければならない。

## 4. 提案する探索戦略

### 4.1 Strategy 1: Exhaustive

現在の完全探索を維持する参照戦略。

- ウィンドウ内の全距離を調べる。
- 各候補を最大一致長まで比較する。
- 圧縮結果と決定規則の正解オラクルとする。
- 高速戦略との比較試験に常時使用できる状態で残す。

### 4.2 Strategy 2: HashChain

先頭5バイトのハッシュ値から候補位置を絞り込み、同じハッシュ値を持つ
過去位置をチェーンとして保持する。

- 単一の最新位置だけを保持する単純なハッシュテーブルにはしない。
- 同一ハッシュの複数候補を検索できるハッシュチェーンとする。
- ウィンドウ外へ出た位置は候補として使用しない。
- 完全モードでは、最長一致の可能性がある全候補を評価する。
- ハッシュ衝突は必ず実データのバイト比較で解決する。

5バイトはvariant 1の最小一致長以下であるため、成立し得る全match候補が
同じハッシュバケットへ入る。最初の内部実装では32-bit unsigned値をゼロ
から開始し、各接頭辞バイトで
`hash = (hash << 5) xor (hash >> 2) xor byte`を5回適用した後、
`hash xor (hash >> 16)`を使用する。unsigned overflowは2の32乗を法とする。

リンク数は`min(input_size, window_size)`、バケット数はリンク数と65,536の
小さい方を次の2冪へ切り上げる。バケット先頭はnative `size_t`絶対位置、
各リングスロットのリンクは直前候補への32-bit距離とし、ゼロを終端値と
する。候補は新しい位置から古い位置へ並ぶ。現在位置からwindowを超えた
候補に到達した時点で、それ以前もすべて期限切れなので探索を終了できる。
これらはcaller-owned workspace内の実装表現であり、シリアライズしない。

### 4.3 実験候補: BinaryTree Exact

ウィンドウ内の候補位置を、後続バイト列の辞書式比較に基づく二分木で管理
する。

- 長いウィンドウで候補を効率的に絞り込むことを目的とする。
- 木の挿入、削除または世代管理は有界メモリで行う。
- 同一接頭辞や重複領域を決定的に処理する。
- 完全モードではExhaustiveと同じ最長一致とtie-breakを返す。

固定配列上のAVL木、期限切れnodeの構造的削除、辞書順近傍による最長一致、
部分木の最新位置を使うprefix区間集約によってExact性を保証する。詳細は
[`lzss-binary-tree-match-finder.md`](lzss-binary-tree-match-finder.md)で定める。
初期のSilesiaと合成matrixではglobal AVLを既定化しなかった。後続の固定
16 MiB比較では4/16 MiB aggregateでHashChainを上回った一方、memberごとの
勝敗と約7.2倍のworkspaceにばらつきが残った。このため既定化やwindow-only
selectionはせず、5つのContextual LZSS encoderに限って明示選択を公開する。
privateな正確性oracleおよび構造比較対象としても保持する。

### 4.4 実験候補: HashTree Exact

HashChainを常時維持し、実際のquery候補数がprivate閾値を超えたbucketだけを
per-bucket AVLへ一度だけ昇格する。浅いbucketはChainのままなので、global
BinaryTreeで観測した全位置の無条件な木更新を避けられる。tree traversalは
既知LCP bracketを保持し、同じquery内で確定済みprefixを再比較しない。

昇格query自体はHashChain Exactで完了し、次queryからtreeを使用する。閾値、
診断counterまたはbucket状態はstreamへ記録しない。いずれの経路もExhaustiveと
同じmatchを返すため、閾値は圧縮結果へ影響しない。詳細は
[`lzss-hash-tree-match-finder.md`](lzss-hash-tree-match-finder.md)で定める。

### 4.5 将来候補: WindowAdaptiveV1

スライドウィンドウサイズだけを入力としてHashChainまたは別のExact索引を
決定的に選択する将来の複合戦略である。

過去の暫定仮説は次であった。

```text
window_size <= 65,536 bytes : HashChain
window_size >  65,536 bytes : BinaryTree
```

BM-0056とBM-0057はこの規則を棄却した。global BinaryTreeは大窓aggregateでも
HashChainより遅く、window sizeだけでは安全に選択できない。現在有効な
`WindowAdaptiveV1`規則またはIDは存在しない。HashTreeはbucket内部で実作業量を
用いて昇格するprivate Exact索引であり、この旧仮説とは別である。

正式な`WindowAdaptiveV1`として採用した後は、同じIDのまま閾値や選択規則
を変更しない。将来異なる規則を採用する場合は`WindowAdaptiveV2`などの
新しい戦略IDを割り当てる。

CPU種別、実行時負荷、計測結果、スレッド数、ポインター幅など、実行環境に
よって変化する条件から探索戦略を選んではならない。

## 5. 共通の一致選択規則

すべての探索戦略は次の規則を共有する。

1. 設定されたウィンドウ内だけを参照する。
2. 設定された最小一致長と最大一致長を守る。
3. 最長一致を選択する。
4. 同じ長さの一致では最短距離、すなわち最も近い参照を選択する。
5. 重複コピー可能な一致を現在のLZSS規則と同じ意味で評価する。
6. 一致がトークン表現上有益な場合だけmatchを出力する。
7. フレームまたは辞書リセット境界で探索索引を完全にリセットする。
8. checked arithmeticを使用し、入力位置、距離、長さおよび索引計算の
   オーバーフローを拒否する。
9. 同じ入力と設定から常に同じトークン列を生成する。

最大一致長へ到達した場合でも、同長のより近い候補が存在し得るときは
tie-breakを確定するために必要な探索を行う。

## 6. ExactとBoundedの分離

探索データ構造と探索努力量は別の設定として扱う。

```text
match_finder_strategy:
    Exhaustive
    HashChain
    BinaryTree
    HashTree
    WindowAdaptiveV1

search_effort:
    Exact
    Bounded(candidate_limit / depth_limit)
```

### 6.1 Exact

- Exhaustiveと同一の最長一致およびtie-breakを返す。
- 高速化は候補索引と比較の省略可能性証明だけで行う。
- 同じ入力に対してExhaustiveと同一のトークン列を要求する。

### 6.2 Bounded

- 候補数、チェーン長または木の探索深度に明示的な上限を持つ。
- 速度と最悪実行時間を優先できる。
- Exactより短い一致を選択する可能性があり、圧縮結果が変化し得る。
- 上限値を含む設定を決定性と再現性の一部として扱う。

初期実装ではExhaustiveとHashChainをExactとして完成させ、その後に
BinaryTreeおよびBoundedを独立した候補として検討する。

## 7. エンコーダー設定とストリームの分離

探索方法は復号そのものには不要であり、どの戦略で作られたLZSSトークンも
同じトークン表現のデコーダーで復号できる。

探索戦略、探索努力量および探索上限はストリームへ記録しない。これらは
エンコーダー設定であり、デコーダーは認識も検証もしない。

同一入力からbyte-identicalなストリームを再生成する必要がある利用者は、
アーカイバー、ビルド記録、ベンチマーク記録または任意の外部provenanceで
次の情報を保持できる。

```text
encoder version
match_finder_strategy
search_effort
candidate_limit または depth_limit（Boundedの場合）
```

同じ入力と完全に同じエンコーダー設定からは、対応プラットフォーム間で
同じストリームを生成する。Exact戦略はExhaustiveと同一出力を要求する。
Boundedは有効な同一形式のストリームを生成するが、Exhaustiveの正規参照
出力との一致を要求しない。

## 8. インターフェース方針

一致探索器は、LZSSパーサーから次のような内部契約を通じて利用できる形が
望ましい。

```cpp
struct LzssMatch {
    std::uint32_t distance;
    std::uint32_t length;
};

LzssMatch find_match(
    std::span<const std::byte> input,
    std::size_t position,
    const LzssParameters& parameters);
```

実際のC++実装では、仮想関数を必須とはしない。variant、テンプレート、
関数オブジェクトまたは明示的なstrategy dispatchを使用できる。ただし、
全戦略で同じ入力・出力契約とエラー規則を共有する。

探索索引に必要なメモリは呼び出し側所有のワークスペースとして計算可能に
し、次を事前に検証する。

- ウィンドウサイズに対する必要エントリ数
- バケット、リンク、木ノードまたは世代情報のバイト数
- アラインメント
- 入力、トークン出力、探索ワークスペース間の重複
- `max_internal_buffered_bytes`などのaggregate limit

## 9. 一回解析と型付きトークン保持

公開 contextual streaming encoder の HashChain Exact 経路は、LZSS解析を
一回だけ行う。frameごとに caller-owned workspace へ入力1 byteあたり最大1個の
型付きトークンを事前予約し、limits、aggregate、alignmentおよびaliasを解析前に
検証する。検証後にmatch finderとparserを一度だけ実行し、実際に生成された
token prefixをcontext modelとentropy encoderの計画・書き込みで再利用する。

```text
raw frame
  -> LZSS match finder + parser（1回）
  -> typed-token workspace
  -> context model
  -> entropy encoder
```

この経路は Contextual Dynamic Range、rANS、tANS、Blocked Huffmanおよび
Adaptive Huffmanで実装済みである。公開streaming lifecycleはframe encodeを
一度だけ呼び出す。呼び出し側がplan APIとencode APIを個別に呼ぶ場合は二つの
独立した要求であり、同じ一回のstreaming encodeとは扱わない。

正確なtoken countだけを先に求めてworkspaceを小さくするprivateな二回解析API
と、Exhaustive参照経路は維持する。一回解析経路は最大token容量分のメモリを
使うため、容量節約と解析速度のtrade-offは明示的であり、自動選択しない。
BM-0025では同一token列とquery countを確認した上で、既存の一回解析経路が
小さいREADME入力で二回解析経路のおよそ2倍のthroughputを示した。この値は
説明的な測定であり、恒久的な性能閾値ではない。

## 10. テスト要件

### 10.1 Exact同値性

HashChain、global BinaryTreeおよびprivate HashTreeのExactモードについて、
Exhaustiveと次を完全一致させる。

- 各入力位置で選択した距離と長さ
- literal/matchの選択
- 完成した型付きトークン列
- 正規化されたシリアライズ済みトークン列
- 最終圧縮ストリーム

### 10.2 境界ケース

- 空入力と1バイト入力
- 全256バイト値
- 最小一致長の直前、同値、直後
- 最大一致長の直前、同値、直後
- ウィンドウサイズの直前、同値、直後
- 距離1の重複一致
- 同長一致が複数存在する入力
- ハッシュ衝突
- 同一接頭辞が多数存在する入力
- 長いゼロ列および周期列
- ランダムデータ
- フレーム境界と辞書リセット
- HashTree promotion閾値の直前、同値、直後

### 10.3 決定性

- 同じ入力と設定を複数回実行して同一結果を要求する。
- MSVC、Clang、GCCなどの対応コンパイラ間で同一結果を要求する。
- 32ビット／64ビット型幅やポインター値を順序決定に使用しない。
- ハッシュテーブルの未規定な反復順序に依存しない。

### 10.4 安全性

- 短い探索ワークスペース
- 計算された必要容量のオーバーフロー
- 入力と探索ワークスペースの別名
- トークン出力と探索ワークスペースの別名
- aggregate limit超過
- 不正または未知のstrategy ID
- Boundedのゼロまたは上限超過パラメーター
- 悪意ある反復入力でも無限ループしないこと

### 10.5 Fuzzing

小さい入力についてExhaustiveをオラクルとし、各Exact戦略が同じmatchと
トークン列を返す差分fuzzingを行う。クラッシュ、ハング、境界外アクセス、
不一致が見つかった場合は恒久的な回帰試験を追加する。

## 11. ベンチマーク要件

各戦略について少なくとも次を独立に測定する。

- LZSS解析スループット
- 完全な圧縮スループット
- 圧縮率
- query数とチェインから訪問した候補数
- 同一bucket内で5-byte接頭辞が異なるhash false positive数
- 実際に5-byte接頭辞が一致した候補数
- match比較byte数
- queryごとの候補数分布、平均および最大
- 最大チェーン長または木探索深度
- 探索ワークスペースのピークメモリ
- 計画と書き込みを合わせた総時間
- トークン再利用によって二重解析を除去した場合の時間

入力カテゴリにはテキスト、UTF-8日本語テキスト、実行ファイル、画像、既圧縮
データ、ランダムデータ、ゼロ列、周期列および大きな反復データを含める。

過去のWindowAdaptiveV1 64 KiB仮説は棄却済みであり、benchmark対象または
pass条件にしない。HashTreeのprivate promotion閾値は複数カテゴリと複数
window sizeでsweepし、単一サンプルから固定しない。

代表的な実データには、利用者が取得してリポジトリ外データとして配置した
Silesia Corpusを使用する。Corpusを自動downloadまたは再配布せず、各構成
ファイルを独立して測定する。配置、検証、集計および再現性の契約は
[`silesia-benchmark-profile.md`](silesia-benchmark-profile.md)で定める。

最初のHashChain診断は64 KiB、256 KiBおよび1 MiB windowを同一入力と設定で
比較する。公開profileが存在しないwindow sizeはmatch-finder単体測定に限定
し、公開codecの対応として扱わない。

## 12. 推奨実装順序

1. 本文書と正式な設計判断で形式非変更方針を固定する。
2. 共通の一致探索契約と設定構造を定義する。
3. 現行実装を`Exhaustive`として分離する。
4. Exhaustiveの既存出力を固定する回帰試験を追加する。
5. caller-owned workspaceの必要容量計算を定義する。
6. 固定5バイト接頭辞を使う`HashChain Exact`を実装する。
7. Exhaustiveとの差分試験と有限fuzzingを行う。
8. Silesiaの外部配置、検証および測定契約を実装する。
9. 64 KiB、256 KiB、1 MiBと合成worst-case入力でHashChainを診断する。
10. 受理済みのExact設計に従い、privateな`BinaryTree Exact`を実装する。
11. global BinaryTreeを合成matrixとSilesiaで測定し、常時AVL仮説を判定する。
12. 深いbucketだけを昇格するprivate `HashTree Exact`を段階実装する。
13. HashTreeの閾値sweepとSilesia証拠後にproduction昇格可否を判断する。
14. 必要性が確認された後にBoundedポリシーを追加する。
15. 再現性情報を公開する必要が生じた場合は、ストリームではなく
    エンコーダー設定または外部provenanceとして設計する。

## 13. 残る未決事項

- HashTreeのprivate promotion閾値とproduction昇格可否
- WindowAdaptiveV1を将来も必要とするか
- 内部strategy設定をどの段階で公開エンコーダー設定へ昇格するか
- 一回解析で予約するworst-case token容量を縮小できるか
- 探索ワークスペースを公開ABIのopaque viewへどう割り当てるか

## 14. 暫定結論

Exhaustiveを参照オラクルとして残し、最初にHashChain Exactを追加する。
固定5バイト接頭辞は、現在のLZSS最小一致長が5以上であるため、成立し得る
全match候補を同じバケットへ導きながら不要候補を絞り込める。

探索戦略はLZSSストリームvariantから完全に分離する。Exact、Boundedの
いずれもストリームへ探索設定を記録せず、必要な再符号化provenanceは
エンコーダー利用側が管理する。

global BinaryTree ExactはAVL近傍探索とprefix区間集約でExhaustiveのtie-breakを
保つことを証明したが、常時維持費によりproduction候補から外れた。HashTree
ExactはHashChainを安い基底として残し、実際に深いbucketだけを同じExact木へ
一度だけ昇格する。既知LCPをtree traversal内で再利用し、診断有無と無関係な
決定的閾値を使う。

`WindowAdaptiveV1`に現在有効な規則はない。HashTreeの合成・Silesia証拠後に
production昇格を別decisionで判断し、BoundedもExact経路と分離して検討する。

公開contextual HashChain経路では、caller-owned typed-token workspaceへの
一回解析と後段での再利用が標準である。二回解析は正確な容量を優先する
private経路としてのみ残し、公開streaming encoderの未解決高速化項目とは
扱わない。

## 15. 0.7.0に向けた探索実験の総括（2026-09-24）

0.6.0以降、同じExactトークン列を保ったまま、Red-Black木、Scapegoat木、
WAVL木、Sparse HashTreeとその再利用・不変スナップショット方式をprivateに
比較した。以下は固定条件での測定結果であり、未測定の窓サイズへの性能保証
ではない。各実験の入力、workspace、比較対象と失敗条件はBM-0061～BM-0075
に残している。

| 候補 | 試験した窓 | 観測結果と判断 |
| --- | --- | --- |
| Red-Black | 64 KiB～1 MiB | AVLと同容量でも総合速度は0.992～0.942倍。公開しない（BM-0061）。 |
| Scapegoat | 64 KiB～1 MiB | AVL比0.607～0.559倍、workspace約1.24倍、更新スパイク増大。公開しない（BM-0064）。 |
| WAVL | 1～4 KiBの合成入力 | AVL比0.523～0.551倍。初期ゲートで不採用とし、Silesia測定には進まない（BM-0067）。 |
| Sparse HashTree | 4～64 MiB | 小さいpoolの最良候補でもHashChain比0.903、0.945、0.920倍で、pool枯渇が残る（BM-0069）。 |
| 再利用ゲート付きSparse | 4～64 MiB | pool枯渇は減るが、最良のHashChain比は0.848、0.847、0.873倍（BM-0071）。 |
| 不変スナップショットSparse | 4～64 MiB | 可変Sparseには全36組で勝ち、HashChain比は0.727→0.811→0.935倍。窓拡大に伴う改善傾向はあるが、総合では未達（BM-0074）。 |

特に不変スナップショットは、大きな窓で再検討する根拠を残した。一方、
64 MiBでの差はなお約6.5%あり、64 MiBを超える窓の速度・メモリは未測定で
ある。差分候補数も大きく、合成入力で試したdelta budgetは事前条件を
満たさなかった（BM-0075）。現行の公開selectorに候補を追加しない。
BM-0074の比較対象はbucket拡大とbest-length probe採用より前のHashChainで
あり、0.935倍という値は現在の既定経路との差を表さない。
将来の再評価では、より大きな窓を許すresource profile、同じExact性、
workspace上限、現在のHashChainを対照とする全Corpusでの勝敗、更新スパイク
を別途検証する。

HashChain自身については、mnemonic prefix mixerを合成試験の事前ゲートで
不採用とした（BM-0078）。bucket上限を65,536から262,144へ変更した結果、
4/16/64 MiBのSilesia探索単体で総合速度比は1.032/1.118/1.126倍となり、
必要workspaceは同条件で1,572,864バイト増えた（BM-0083～BM-0084）。
その後、best-length probeを既定のHashChainに採用した。4 MiBのcontextual
rANS全体では12ファイルの圧縮時間中央値合計が277.490秒から124.460秒に
短縮したが、`x-ray`は約0.66%遅かった（BM-0097）。どちらの変更も
トークンとアーカイブのバイト列、形式ID、C ABIを変更しない。
