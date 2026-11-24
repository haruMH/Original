# GM31 DX11_Original 専用ルール（グローバル規約に追加）

## ゲーム概要
遠心力テーマの3Dアクション。核ループ＝「掴む → 回す(スピン) → 投げる」。ボス戦・ハブ・進行/セーブ・アイテムあり。

## 設計方針（最重要）
- 威力の源は**スピン量(チャージ)**。回転せずに強くなる抜け道を作らない。
- 新要素(アイテム/ボス/演出)は核ループを強化するものに限る。単なる数値アップは避ける。
- 回転せず投げる＝ダメージ無し・ノックバックのみ(脱出手段、クールダウンあり)。

## ディレクトリ地図（ここから探索を始める）
- Core: manager(シーン/オブジェクト/演出), game_constants.h(定数), progression/save
- Player: player.h(PlayerState), player_combat(掴み/スピン/投げ), player_movement, player_visual
- Enemy: enemy(EnemyState: FLYING等), boss_enemy, boss_phase_attack, enemy_affix(爆発/雷)
- Stage: collision_system(衝突・スピンなぎ払い), field/wall/prop
- Graphics: render_system(インスタンス描画), renderer / UI: score_hud(HUD全般)

## 既知の落とし穴
- `Player::GetAngularVelocity()` は回転角の差分から毎フレーム算出(player_movement.cpp)。投擲威力の根拠に使うとスピン無しでも威力が乗る。威力は `m_CurrentSpinSpeed` を根拠にする。
- 投擲ダメージは `Enemy::GetThrowSpinRatio()`(0〜1)を collision_system.cpp が参照。
- ボスは `Manager::GetGameObject<BossEnemy>()` でキャッシュ返却(全探索しない)。

## バランス変更ルール
- `game_constants.h` の既存値は変更しない。新規は追加のみ。変更が必要なら事前確認。

## クォータ節約ルール（本プロジェクト）
- 探索は上の地図を起点に、関数単位(offset/limit)で読む。プロジェクト全体Grep・Explore/Planサブエージェントは使わない。
- 大きな.cpp(collision_system, score_hud, boss_enemy, gameplay_scene等)は全体Read禁止。Grepで行番号特定→該当範囲のみ。
- ビルドは各Step完了時に1回。報告は「エラー有無」のみ。エラー時は該当行だけ抽出して修正(同一エラーは最大2回)。警告・ログ全文は出さない。
- プレイ検証・スクリーンショット・ブラウザ操作はしない(手触り確認はユーザーが行う)。
- 変更はEditの差分のみ。コード再掲・長い解説なし。報告は変更ファイル名＋要点1〜2行。
- 区切りごとに「/clear 推奨」を添える。再開に必要な情報は下の「現在の作業」に更新する。
- 3ファイル以上の変更は着手前に3行以内の計画を提示。

## ブランチ運用ルール
- 新しい作業(機能追加・調整・修正)を始めるときは、着手前に新ブランチ(feature/xxx)を切る。現在ブランチが別作業のままなら、切るかどうかをユーザーに確認する。
- 前の作業が未コミットなら、先にコミット/退避の扱いを確認してからブランチを切る。

## 後回しメモ（実装後に遊んでから再検討。完了したら削除）
- 発射方向: 実装済み(feature/tangent-throw)。回転中に放すボタンを押すと「リリース予約」になり、敵が狙い(カメラ正面+近くの対象への吸い付き)を通過する瞬間に放つ。値は game_constants.h の AIM_ASSIST_* / RELEASE_MAX_WAIT_FRAMES。手触り確認待ち。
- 回転が速いほど接線ブレを小さく(高威力=狙いやすい)。ただし回転中のリスクは高く。
- 難易度は最高速度ではなく「チャージ到達時間」「エイム補助強度」「リスクの重さ」で調整。
- 回転中のなぎ払い(HandleSpinSweep)を消すかはStep3後に再判断(現状は維持)。

## 現在の作業
回転必須化: Step1 非回転投げ(ノックバックのみ+クールダウン)+抜け道封じ → Step2 4段階チャージ(30/60/90/100%保証、4段階目に爆発等) → Step3 チャージ演出+回転中リスク → Step4 クールダウン/段階UI(score_hud)。
詳細プラン: C:\Users\HARU17\.claude\plans\reply-4-tidy-allen.md
進捗: Step0〜4すべて完了(ビルド成功・手触り確認待ち)。チャージ演出は放電→オーラ(Source/Effect/aura_effect、Manager::Drawから描画)に変更。UIは score_hud の DrawChargeGauge(右下: 4分割チャージ+クールダウンバー)。次: 遊んで調整、後回しメモ(接線発射/難易度)の再検討。

### 拠点の見栄え強化(feature/field-visual。進行中)
- 実装済み: ブルーム強化(HDR R11G11B10)、SSAO半解像度、夜の蛍(ambient_effect)、焚き火/かがり火の点光源(最大4灯, b7)、草(grass_system, カメラ中心2層)、丘と木と芝敷物(Stage/hill_terrain)、空の山(sky_ps)、外周は見えない壁+柵。
- 素材: Assets/model/nature(Quaternius Stylized Nature MegaKit, CC0)。ライセンス記録は Assets/CREDITS.md(素材追加時に必ず追記)。ObjModelは複数テクスチャ+アルファテスト対応、/nature/ は foliage_ps(軽量PS)で描画。PropModelは同一OBJを共有キャッシュ。
- 未コミット: GPU/CPU計測(Renderer::GpuMark、Manager::s_DbgCpu*)、拠点の確認用オーバーレイとF5/F6/F7切替(hub_scene.cpp の「確認用」。確認後に削除)、草の2層化、foliage_ps、自然物の本配置。
- FPS問題(フルスクリーン36〜42): CPUは update0.1ms/draw-submit0.3ms で問題なし。GPUの重さは **標準 pixelShader.hlsl** に集中(地面 ground 3〜10ms、物体 objects 5〜16ms。影16タップPCF+床ノイズ+点光源4灯ループ)。草・丘・自然物を個別にOFFしても各+3FPSのみ。次の手: pixelShader の影を8タップ化/距離で省略、床ノイズの簡略化、点光源ループの条件省略、必要なら内部解像度スケール。
- FPS対策(未コミット・手触り確認待ち): pixelShader の影を4点下見→半影のみ16点、床ノイズをFogEnd付近で省略、点光源の半径外スキップ。renderScale を実装(設定の「レンダリング解像度」50〜100%。Renderer::ApplyRenderScaleIfChanged、シーン/深度/ポストを縮小し合成で拡大+CAS。scale<1のみ合成結果をシーン解像度のLDR一時画像へ出し、オーラ等もそこへ描いてから upscale_ps で画面へ拡大。深度コピー方式はimgui区間+3.4msで廃止)。HUD(ScoreHUD)はシーンRT内なので縮小時はやや柔らかい。計測: Wallpaper Engine 稼働中は約10fps低下(38→46〜50)。post(合成)4.4msは中身の割に重く待ち時間混入の疑い。
- テクスチャのBC7化: 大きい14枚(nature 2048px×5+1024×2、壁1024×7)を AssetConverter で .dds 化し同フォルダへ配置(pngは残す)。Renderer::CreateTexture が同名 .dds を優先(BC1/3/7、sRGB指定はUNORMとして扱う。変換時に sRGB出力だと色が変わるため注意)。効果は未測定。
- 最大の原因: ゲームが内蔵GPU(Intel UHD)で動いていた(D3D11CreateDeviceのアダプタ未指定)。main.cpp に NvOptimusEnablement/AmdPowerXpressRequestHighPerformance を追加して専用GPUへ。以降のFPS/GPU計測はこれ以前の値と比較不可(再測定すること)。
- 中世の村(Medieval Village MegaKit Standard, CC0): Assets/model/medieval/ に必要な部品のみ取り込み(mtlのmap_Kdは textures/ 相対に書き換え済み。元は Downloads に残存)。hub_scene.cpp の自然物ブロック内 `house()` で、壁2mモジュールを組んだ家(6x8m, 等倍)を拠点の四隅に4軒+荷車/木箱(奥)。屋根の原点=家の中心に当たり判定(中には入れない)。向き・高さ・ドア位置は確認済み(壁の外向き=ローカル+z、屋根原点y=2.6、三角壁y=3.12、ドア板は-0.51ずらす)。
- 中世の家の法線マップ: obj_model が /medieval/ のモデルだけ map_Bump を読みスロット1へ、Shaders/medieval_ps.hlsl(pixelShader+法線マップ。接線はddx/ddy)で描く。調整は medieval_ps.hlsl 先頭の NORMAL_STRENGTH(凹凸の強さ)と NORMAL_FLIP_Y(凹凸が逆に見えたら0にする)。取り込みスクリプトは medieval の mtl を書き換える(map_Kd/map_Bump は textures/ 相対)。
- ファイル整理: ステップ1(素材取り込み)完了。ステップ2完了(.hlsl21個を Shaders/ へ移動。vcxprojのInclude修正のみで、.cso出力先は$(ProjectDir)直下のまま=実行時パス不変。fade_systemの.hlsl予備パスも修正。新規シェーダーは Shaders/ に置く)。確認用オーバーレイ(F5〜F7)は削除済み。未着手のステップ3: ルートのログ類を logs/ へ、Assets/model の Kenney素材を survival_kit/ に整理(コード内パス文字列も修正)。各段階でビルド+起動確認してコミット。
- 注意: Releaseは x64\Release で実行され Assets のコピーが漏れることがある。ObjModel は ../ ../../ を探すフォールバックあり。drive-download(63モデル)とMedieval Village MegaKit(Standard)は Downloads 内に未取り込み(使う時は CREDITS.md に追記)。
- 拠点の更新: 床を自作の石畳(Assets/texture/cobble*.png、Field::SetFloorTexture で hub のみ。medieval_ps で法線マップ)に変更。旧Kenney系プロップ(柵/木箱/樽/テント等)は hub_scene から全削除(炎と松明の光・ゲート柱・サンドバッグは残置)。中世の家は house(...,nx,nz,deco) で4種(6x8/8x8/4x6/6x10)＋煙突/雨戸/丸ドア。中世テクスチャ12枚はBC7 .dds化(ロード短縮。obj_modelは法線も.dds優先)。草は3層(近/遠/超遠 66mまで)で、遠景層は内縁から徐々に大きくなる(grass_vs の ramp)。未確認: 煙突の位置、4m幅の屋根の高さ、草の境目。
- 拠点レイアウト変更: 中央に幅6mの道(Field::SetRoad, road*.png)を南北に通し、左右に6棟(塔3階/二階建て含む)を道向きに配置。ゲートは北端(0,19)の大きな門(アウトラインなし: RenderComponent::outline)。鍛冶台(-12,13)・サンドバッグ(13,14)に移動。アウトラインは非一様スケールを補正。丘の遠景は針葉樹のみ(hill_terrain CONIFER_RATIO)。foliage_ps は FACET_SMOOTH で法線を上向きに寄せる。
