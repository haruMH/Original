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
