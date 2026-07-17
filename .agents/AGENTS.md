# TOON SLASHER 設計・リファクタリングガイドライン

このファイルは、本プロジェクト（TOON SLASHER）におけるコードのリファクタリングおよび新規実装において、AIが常に従うべき共通ルールを定めたものです。

---

## 1. 絶対に守るべき制約（Do Not Break Rules）

1. **ゲームの見た目・挙動を変えない。** `game_constants.h` の数値、当たり判定の仕様、演出タイミング（ヒットストップ・スローモーション・カメラシェイクの発生条件）はバグ修正が目的の場合を除き変更しない。リファクタリングは「同じ動作をより良い構造で」実現することが目的であり、ゲームバランスの変更ではない。
2. **シェーダーとの整合性を壊さない。** `renderer.h` の `VERTEX_3D` / `MATERIAL` / `LIGHT` / `POSTEFFECT` / `WaterParamCB` などの構造体は対応する `.hlsl` の `cbuffer` / `struct` とレイアウトが一致している。C++側の構造体を変更する場合は、対応する `.hlsl` ファイルも必ず同時に更新し、フィールドの追加・削除・並び替え・パディング変更を行う際は16バイトアライメントを崩さないこと。
3. **一度に全ファイルを書き換えない。** 巨大な一括リライトはビルドを壊すリスクが高い。段階的手順に従い、1フェーズごとに「コンパイルが通る状態」を維持すること。
4. **既存の公開インターフェース（他クラスから呼ばれるpublicメソッド）を変更する場合は、呼び出し元すべてを検索し、同一コミット内で追従修正する。** 片方だけの修正は禁止。
5. **`Debug::START_FROM_BOSS` のような開発用フラグの挙動を変更する際は、リリースビルドとデバッグビルドで異なる値になるよう明示的にガードすること。**

---

## 2. 可読性 (Readability) — 弱点と修正ルール

### 2-1. `CollisionSystem::Update()` が単一関数で500行超、責務が混在している
- 以下のプライベート静的関数に分割すること。各関数は60行以内を目安とする。
  - `HandlePlayerItemPickup(Player*, const std::vector<Item*>&)`
  - `HandleSpinSweep(Player*, CollisionGrid&)`
  - `HandleFlyingVsWall(Enemy* flying, const std::vector<Wall*>&, bool& explosionThisFrame)`
  - `HandleFlyingVsBoss(Enemy* flying, BossEnemy*, bool& explosionThisFrame)`
  - `HandleFlyingVsEnemy(Enemy* flying, CollisionGrid&, bool& explosionThisFrame)`
  - `TriggerChainLightning(...)` はそのまま維持。

### 2-2. 「爆発／電撃／通常」のダメージ分岐が3箇所（壁・ボス・敵）でほぼコピペされている
- `ResolveFlyingEnemyImpact(Enemy* flying, GameObject* target, bool& explosionThisFrame)` のような単一関数に集約し、上記3箇所すべてから呼び出す形にする。

### 2-3. `game_constants.h` に一元化する方針があるのに、マジックナンバーが個別ファイルに再出現している
- `game_constants.h` に `Constants::Collision::GRID_CELL_SIZE` などの名前空間を追加し、マジックナンバーを移動する。同じ意味の数値が2箇所に存在する状態をゼロにすること。

### 2-4. デバッグログ（`OutputDebugStringA`）がロジックに直接埋め込まれ、文字列結合コードとゲームロジックが視覚的に区別できない
- `LOG_INFO(fmt, ...)` のようなマクロ/インライン関数でラップし、`NDEBUG` 時は空文字列に展開されるようにする。ログ呼び出しは必ず単独行に置き、ロジック用の変数とログ用の一時変数を混在させない。

### 2-5. `Player` のモジュール分割（`PlayerMovement`/`PlayerCombat`/`PlayerVisual`）が `friend class` + 直接メンバアクセスで実装されており、実質的にカプセル化されていない
- `friend` 宣言を撤廃し、`Player` に意図の明確な public メソッドを用意し、各モジュールはそれらのメソッド経由でのみ状態を変更する。

### 2-6. 命名の不統一: `Player::ApplyDamage` と `BossEnemy::ApplyBossDamage`
- 概念的に同じ「ダメージを受ける/与える」操作の名前を `OnHit` 等に統一する。

### 2-7. テクスチャパス解決の `#ifdef NDEBUG` 分岐が最低7ファイルにコピペされている
- パス解決ロジックを `ResourceManager::GetTexture()` の内部一箇所に集約する。各`GameObject`派生クラスは常にベース名（例: `"player.png"`) のみを渡せばよい状態にする。

---

## 3. 処理速度 (Performance) — 弱点と修正ルール

### 3-1. `Manager::GetGameObject<BossEnemy>()` が毎フレーム・飛行エネミー毎にO(n)全探索している
- `m_CachedBoss` をManagerに追加してO(1)化するか、`CollisionSystem::Update()` の冒頭で一度だけボスを取得し、ループへは引数として渡す。

### 3-2. `AttackingEnemy` の回避行動が独自にO(n)全走査を行い、`CollisionSystem` の空間分割グリッドと二重実装になっている
- `CollisionGrid` を `SpatialGrid` として汎用ユーティリティに昇格させ、`CollisionSystem`・`AttackingEnemy`（回避行動）・`TriggerChainLightning`（連鎖索敵）が同じ空間分割データ構造を共有するようにする。

### 3-3. Shadow/Normal/Outlineの3描画パスで、視錐台カリング＋描画リスト構築が重複計算されている
- Normalパスで計算した「カリング後の描画対象リスト」を保持しておき、Outlineパスではそれを再利用する。

### 3-4. `Manager::Draw()` 内で、同じ「オブジェクト種別による除外条件」がShadow/Outlineパスでコピペされている
- 同一条件をラムダ/ヘルパー関数（例: `Manager::DrawIndividualObjects`）に切り出す。

### 3-5. `UnregisterCategory` が `std::vector::erase`（O(n)）でカテゴリリストから除去している
- swap-and-pop方式に統一するか、`unordered_map` を補助的に持つ設計に変更し、O(1)化する。

---

## 4. 拡張性 (Extensibility) — 弱点と修正ルール

### 4-1. `Manager` が「神クラス」化しており、新しいオブジェクト種別の追加が複数箇所への手作業の変更を要求する
- `std::unordered_map<ObjectType, std::vector<GameObject*>>` に置き換え、型を意識しない汎用実装にする。

### 4-2. `Enemy` の亜種フラグ（`m_IsExplosive`/`m_IsLightning`/`m_IsSandbag`）が基底クラスに直接ベタ書きされ、`CollisionSystem` 側の分岐も線形に増え続ける設計になっている
- `IEnemyAffix`（属性）インターフェースを導入し、属性ごとの動作をポリモーフィズム化する。

### 4-3. `BossEnemy` のフェーズ管理が bool×3 + int + 巨大if/elseの手組みステートマシンになっている
- フェーズをデータ化（`BossPhase` 構造体など）し、現在HPに応じて該当フェーズをリストから探して実行するデータ駆動型に変更する。

### 4-4. ダメージを受ける処理が型ごとにバラバラで、共通インターフェースが存在しない
- `GameObject` に仮想関数 `virtual void OnHit(const HitInfo& info)` を追加し、各クラスでこれをオーバーライドする形に統一する。

### 4-5. `GameObject`派生クラスの追加時、`Manager`ヘッダの前方宣言・friend宣言・カテゴリ登録の3箇所を手動で編集する必要がある
- カテゴリ管理のマップ化とカプセル化（friend廃止）によって、新しいオブジェクト追加時の変更を最小化（ObjectType列挙型の追加とソースコードの追加のみ）する。

### 4-6. オブジェクト破棄ロジックが4箇所にほぼ同じ内容で重複している
- `Manager::DestroyObjectsIf` のような単一の破棄関数に統一する。さらに、手動管理を `std::unique_ptr` 管理に置き換える。

### 4-7. `PlayerMovement::Update()` と `GameplayScene::UpdateGameplay()` の両方が `Collision::ResolveGrabPhysics()` を異なる引数で呼び出し、責務が重複している
- 掴んだ敵の位置同期を `GameplayScene::UpdateGameplay()` 側（オフセット 0.8f）に一本化し、`PlayerMovement::Update()` 側の重複呼び出しを削除する。

---

## 5. 推奨する段階的リファクタリング手順

1. **フェーズ1（副作用のない整理）**: 2-3（マジックナンバー集約）, 2-7（テクスチャパス集約）, 2-4（ログマクロ化）。
2. **フェーズ2（パフォーマンス）**: 3-1（ボスキャッシュ）, 3-5（swap-and-pop統一）。
3. **フェーズ3（重複ロジックの統合）**: 2-1/2-2（CollisionSystem分割）, 3-3/3-4（描画パス共通化）, 4-6（破棄ロジック統一）, 4-7（Grab物理の一本化）。
4. **フェーズ4（構造的な拡張性改善）**: 4-1（カテゴリmap化）, 4-4（OnHitインターフェース）, 4-2（Affixパターン）, 4-3（Bossフェーズのデータ化）。
5. **フェーズ5（カプセル化の是正）**: 2-5（Player friend撤廃）, 4-5（Manager friend撤廃）。
6. **フェーズ6（共有ユーティリティ化）**: 3-2（SpatialGridの共通化）。

---

## 6. AI出力スタイルに関するルール（共通認識）

1. **思考プロセスの省略**
   - ユーザーへの最終出力では、原則として結論と修正コードのみを極めて簡潔に出力し、推論の痕跡、前提の解説、余計な説明などはすべて省略してください。
2. **解説の例外**
   - ユーザーから明示的に「解説」や「どのように思考したか」を尋ねられた場合のみ、思考プロセスや設計思想を詳細に説明してください。

