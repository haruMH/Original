#pragma once

// =================================================================
// ゲーム定数定義ヘッダー (game_constants.h)
// =================================================================
// ゲームのバランスや挙動を調整するための定数を一元管理します。
// プログラミング学習時の値調整や、ゲームの面白さを左右するパラメータはすべてここに集約します。

#include <DirectXMath.h>

namespace Constants
{
    // 構造体定義
    struct ColorRGB { unsigned char R, G, B; };

    // カメラに関する調整パラメータ
    namespace Camera
    {
        // 視体積・プロジェクション設定
        const float FOV                     = DirectX::XM_PIDIV4; // 画角（視野角: 約45度）
        const float NEAR_CLIP               = 0.1f;               // ニアクリップ距離（これより近いものは非描画）
        const float FAR_CLIP                = 1000.0f;            // ファークリップ距離（これより遠いものは非描画）

        // プレイヤー追従・距離・オフセット
        const float DEFAULT_DISTANCE        = 6.0f;               // プレイヤーとカメラの基準距離
        const float HEIGHT_OFFSET           = 1.5f;               // プレイヤー基準のカメラ高さオフセット
        const float TARGET_HEIGHT_OFFSET    = 1.2f;               // プレイヤー注視点（胸元）の高さオフセット

        // 初期角度（ラジアン）
        const float DEFAULT_ANGLE_X         = 0.2f;               // 初期ピッチ角（上下角度: やや見下ろし）
        const float DEFAULT_ANGLE_Y         = 0.0f;               // 初期ヨー角（左右角度: 正面向き）

        // ピッチ角制限（プレイヤー追従時: 上下回転の可動域）
        const float ANGLE_X_LIMIT_MAX       = 1.4f;               // 見上げ限界（ラジアン）
        const float ANGLE_X_LIMIT_MIN       = -0.3f;              // 見下ろし限界（ラジアン）

        // 操作感度・回転速度
        const float DEFAULT_SENSITIVITY     = 0.002f;             // マウス操作感度の初期値
        const float KEY_ROTATION_SPEED      = 0.03f;              // キーボード（Q/E/矢印キー）操作時の回転速度

        // 壁との衝突・遮蔽防止（カメラ押し戻し）
        const float COLLISION_MARGIN        = 0.3f;               // 壁衝突時の手前マージン距離
        const float MIN_DISTANCE            = 0.5f;               // 壁衝突時のプレイヤーとの最小距離

        // カットシーン補間
        const float CUTSCENE_LERP_SPEED     = 0.1f;               // カットシーン移行／解除時の補間速度
        const float CUTSCENE_RESET_ANGLE_X  = 0.2f;               // カットシーン終了時の復旧ピッチ角
        const float CUTSCENE_RESET_ANGLE_Y  = 0.0f;               // カットシーン終了時の復旧ヨー角

        // カメラシェイク（振動演出）
        const float SHAKE_DAMPING           = 0.7f;               // 毎フレームの振動減衰率 (0.0〜1.0)

        // 垂直方向追従のスムージング（歩行揺れ・接地バウンスによるカメラのガタつき吸収: 0.0=追従なし, 1.0=即時）
        const float VERTICAL_SMOOTH         = 0.25f;

        // フリーカメラ（プレイヤー不在時 / デバッグ用）
        const float FREE_CAM_MOVE_SPEED     = 0.15f;              // フリーカメラ移動速度
        const float FREE_CAM_TARGET_DIST    = 5.0f;               // フリーカメラ注視点距離
        const float FREE_CAM_ANGLE_X_MAX    = 1.4f;               // フリーカメラ時の見上げ限界
        const float FREE_CAM_ANGLE_X_MIN    = -1.4f;              // フリーカメラ時の見下ろし限界
    }

    // プレイヤーに関するパラメータ
    namespace Player
    {
        // ステータス関連
        const int   MAX_HP                  = 5;        // プレイヤーの最大HP
        const int   DAMAGE_STUN_DURATION    = 30;       // 被弾時の気絶（スタン）フレーム数
        const int   INVINCIBLE_DURATION     = 60;       // 被弾後の無敵フレーム数

        // 移動・ジャンプ関連
        const float MOVE_SPEED              = 0.1f;     // 通常移動速度
        const float JUMP_VELOCITY           = 0.22f;    // ジャンプの初期上向き速度
        const float GRAVITY                 = 0.008f;   // 毎フレームかかる重力の強さ
        const int   MAX_JUMP_COUNT          = 2;        // 最大空中ジャンプ回数（2段ジャンプ）

        // 歩行・移動時の姿勢傾き（バンク・前傾）
        const float MOVE_TILT_ROLL          = 0.05f;    // 左右移動（A/D）時のバンク傾き角（過剰な傾き・カメラ揺れを抑制）
        const float MOVE_TILT_PITCH         = 0.20f;    // 前進時の前傾姿勢角度
        const float MOVE_TILT_SPRING_K      = 0.15f;    // 傾き復元のばね係数
        const float MOVE_TILT_DAMPING       = 0.85f;    // 傾き復元の減衰係数（ブレを抑制）

        // スプリング物理（もちもち変形演出用）
        const float SPRING_K                = 0.18f;    // ばね定数（復元力の強さ）
        const float DAMPING                 = 0.78f;    // 減衰係数（揺れの収まりやすさ）

        // ダッシュ・回避関連
        const int   DASH_DURATION           = 15;       // ダッシュ継続フレーム数
        const int   DASH_COOLDOWN           = 45;       // ダッシュ再使用可能までの時間（フレーム）
        const int   DASH_INVINCIBLE_TIME    = 20;       // ダッシュ中の無敵フレーム数
        const float DASH_SPEED              = 0.35f;    // ダッシュ時の移動速度（通常移動の3.5倍）

        // ガード・パリィ関連
        const int   PARRY_ACCEPT_DURATION   = 12;       // パリィが成立する入力直後の受付フレーム数

        // 戦闘アクション・スキル関連
        const float GRAB_RANGE              = 4.0f;     // 敵を掴むことができる最大距離
        const float MIN_SPIN_SPEED          = 0.0f;     // スピンの最小旋回速度
        const float MAX_SPIN_SPEED          = 0.32f;    // スピンの最大旋回速度
        const float SPIN_ACCELERATION       = 0.008f;   // スピンの回転加速速度
        const float THROW_FORCE             = 1.5f;     // 敵を投げ飛ばす時の初期速度
        const int   TACKLE_ACTIVE_DURATION  = 90;       // ボス弾回避後のタックル有効時間（フレーム）
        const int   TACKLE_DAMAGE           = 3;        // タックル攻撃がボスに与えるダメージ
        const int   WARP_SLASH_DAMAGE       = 4;        // 雷電テレポートスラッシュがボスに与えるダメージ
        const int   SPIN_SWEEP_DAMAGE       = 1;        // 回転なぎ払いがボスに与えるダメージ
        const int   SPIN_BOSS_HIT_INTERVAL  = 24;       // スピンなぎ払いがボスに連続ヒットする最小間隔フレーム（約0.4秒）
        const int   THROW_SPIN_MAX_BONUS    = 2;        // 最高速回転投げ時の通常敵追加ボーナスダメージ
        const float THROW_SPIN_SPECIAL_MULT = 1.6f;     // 最高速回転投げ時の特殊エネミー（サンドバッグ/属性）ダメージ倍率

        // 雷電エフェクト関連
        const int   LIGHTNING_TIMER         = 8;        // 放電エフェクトの表示フレーム数
    }

    // 敵キャラ一般に関するパラメータ
    namespace Enemy
    {
        const int   DEFAULT_SCORE           = 100;      // 敵を倒した時に獲得できる標準スコア
        const float GIGANT_SCALE            = 5.0f;     // 巨大化アイテム使用時の敵の拡大率
        const float FLYING_AIR_RESISTANCE   = 0.94f;    // 投げ飛ばされた敵の空気抵抗（XZ減速率）
        const float FLYING_GRAVITY          = 0.015f;   // 投げ飛ばされた敵にかかる重力
    }

    // 雷電チェインライトニング（電撃連鎖）に関するパラメータ
    namespace Lightning
    {
        const int   MAX_CHAIN               = 5;        // 雷電が敵から敵へと連鎖する最大回数
        const float CHAIN_RADIUS            = 8.0f;     // 雷電が次に連鎖する敵を探す索敵半径(メートル)
        const float PUSH_FORCE_XZ           = 0.8f;     // 雷電に当たった敵が水平方向に吹き飛ぶ強さ
        const float PUSH_FORCE_Y            = 0.4f;     // 雷電に当たった敵が垂直方向に吹き飛ぶ強さ
        const int   CHAIN_DAMAGE            = 3;        // 連鎖電撃がボスに与えるダメージ
    }

    // 爆発エフェクト・爆風に関するパラメータ
    namespace Explosion
    {
        const float RADIUS                  = 12.0f;    // 爆風の有効半径
        const float BASE_FORCE              = 1.2f;     // 爆風による吹き飛ばし基本威力
        const int   BOSS_DAMAGE             = 4;        // 爆風がボスに与えるダメージ
    }

    // ボスエネミーに関するパラメータ
    namespace Boss
    {
        const int   HP                      = 60;       // ボスの初期最大HP
        const int   SCORE                   = 1500;     // ボス撃破時のスコア
        const float GROUND_Y                = 1.8f;    // ボスの接地基準Y座標（床面-1.0fとのZファイティング防止・アウトライン完全露出用）

        // フェーズ移行閾値 (HP)
        const int   PHASE1_HP_THRESHOLD     = 42;       // 第1フェーズ移行のHP閾値
        const int   PHASE2_HP_THRESHOLD     = 30;       // 第2フェーズ移行のHP閾値
        const int   PHASE3_HP_THRESHOLD     = 18;       // 第3フェーズ移行のHP閾値

        // ボス攻撃・落雷関連
        const float STRIKE_DAMAGE_RADIUS    = 2.0f;     // プレイヤー狙い落雷の当たり判定半径
        const int   STRIKE_DAMAGE           = 1;        // プレイヤー狙い落雷のダメージ値
        const int   STRIKE_WARNING_FRAMES   = 65;       // プレイヤー狙い落雷の警告猶予フレーム（着弾までの時間）
        const int   STRIKE_INTERVAL_FRAMES  = 100;      // プレイヤー狙い落雷の発生間隔（フレーム数）
        const float STAGE_HALF_WIDTH        = 26.0f;    // ランダム落雷が発生するステージの半幅（拡大部屋サイズに合わせて調整）
        const float PHASE2_BAR_LENGTH       = 45.0f;    // フェーズ2回転ビームの長さ
        const int   RANDOM_STRIKE_MIN       = 3;        // ランダム落雷の最小発生箇所数
        const int   RANDOM_STRIKE_EXTRA     = 3;        // ランダム落雷の追加発生箇所数
        const int   ATTACK_START_FRAME      = 350;      // 落雷攻撃が開始するフレーム数（フェーズ移行後）
        const int   ATTACK_DURATION_FRAME   = 550;      // 落雷攻撃が持続するフレーム数

        // 通常AI行動サイクル（HP低下による加速と距離判定）
        const int   ATTACK_INTERVAL_BASE    = 110;      // 初期状態の攻撃間隔フレーム
        const int   ATTACK_INTERVAL_PHASE1  = 95;       // フェーズ1時の攻撃間隔フレーム
        const int   ATTACK_INTERVAL_PHASE2  = 80;       // フェーズ2時の攻撃間隔フレーム
        const int   ATTACK_INTERVAL_PHASE3  = 65;       // フェーズ3時の攻撃間隔フレーム
        const float CLOSE_RANGE_THRESHOLD   = 8.0f;     // 近距離判定（迎撃行動の優先閾値）
        
        // 突進タックル関連
        const float CHARGE_SPEED            = 0.52f;    // 突進速度
        const int   CHARGE_WINDUP_FRAMES    = 45;       // 突進前のタメ予兆フレーム数（約0.75秒）
        const int   CHARGE_STUN_FRAMES      = 90;       // 壁激突後のスタン（気絶・隙）フレーム数（約1.5秒）
        const float CHARGE_HIT_RADIUS       = 3.2f;     // 突進時のプレイヤー衝突判定半径
        const int   CHARGE_DAMAGE           = 1;        // 突進直撃ダメージ
        const float CHARGE_MAX_DISTANCE     = 55.0f;    // 突進の最大移動距離

        // 大ジャンプ・グラウンドスラム関連
        const int   SLAM_WINDUP_FRAMES      = 40;       // ジャンプ前のタメ予兆フレーム数
        const int   SLAM_AIR_FRAMES         = 60;       // 滞空・狙い・落下前フレーム数
        const float SLAM_FALL_SPEED         = 0.85f;    // 急降下速度
        const float SLAM_SHOCKWAVE_RADIUS   = 7.0f;     // 着地衝撃波の判定半径
        const int   SLAM_SHOCKWAVE_DAMAGE   = 1;        // 着地衝撃波のダメージ
        const int   SLAM_RECOVERY_FRAMES    = 45;       // 着地後の隙フレーム数
        const float SLAM_JUMP_HEIGHT        = 24.0f;    // 大ジャンプ滞空高度
        const int   SLAM_RISE_FRAMES        = 18;       // 上昇フレーム数
        const int   SLAM_LOCK_TARGET_FRAMES = 38;       // 着地点確定フレーム数

        // 撃破後のリザルト移行ディレイと演出
        const int   CLEAR_DELAY_FRAMES          = 120;      // ボス撃破からクリア画面遷移までの待ち時間（フレーム）
        const int   DEFEAT_SLOW_MOTION_DURATION = 120;      // ボス撃破時のスローモーション持続フレーム数
        const float DEFEAT_FLASH_FADE_SPEED     = 0.035f;   // ボス撃破フラッシュのフェード速度

        // 投擲衝突ダメージ
        const int   THROW_NORMAL_DAMAGE     = 1;        // 通常敵をボスに投げつけた時のダメージ
        const int   THROW_GIGANT_DAMAGE     = 3;        // 巨大化敵をボスに投げつけた時のダメージ
        const int   THROW_SANDBAG_DAMAGE    = 6;        // サンドバッグをボスに投げつけた時のダメージ
    }

    // 磁力旋回弾に関するパラメータ
    namespace MagneticBullet
    {
        const int   DAMAGE      = 1;    // プレイヤーに接触した際のダメージ量
        const float KNOCKBACK   = 0.2f; // 接触時のノックバック速度
    }

    // UI・画面表示に関するパラメータ
    namespace UI
    {
        namespace BossHP
        {
            // バーのサイズと画面マージン
            const float BAR_WIDTH           = 640.0f;   // HPバーの横幅
            const float BAR_HEIGHT          = 80.0f;    // HPバーの高さ
            const float SCREEN_MARGIN_Y     = 10.0f;    // 画面下部からの余白ピクセル数

            // HPバー配色定義
            const ColorRGB HP_HIGH          = { 0, 255, 100 };   // 高HP時（満タン）のカラー（明るい緑）
            const ColorRGB HP_MID           = { 255, 220, 0 };   // 中HP時のカラー（黄色）
            const ColorRGB HP_LOW           = { 255, 40, 40 };   // 低HP時（瀕死）のカラー（赤）
            
            const ColorRGB BAR_BG           = { 40, 25, 25 };    // HPゲージの空部分（背景）のカラー（暗い赤）
            const ColorRGB BAR_BORDER       = { 160, 50, 50 };   // HPゲージの枠線のカラー（赤サビ色）
            
            const ColorRGB TEXT_HP          = { 220, 220, 220 }; // HP数値テキスト（60/60など）のカラー
            const ColorRGB TEXT_LABEL       = { 255, 80, 80 };   // "BOSS"ラベルテキストのカラー
            
            const ColorRGB PANEL_BG         = { 20, 10, 30 };    // ボスHPバーパネル全体の背景カラー（深紫色）
        }

        namespace PlayerHP
        {
            // バーのサイズと画面パラメータ
            const float BAR_WIDTH           = 400.0f;   // プレイヤーHPバーの横幅
            const float BAR_HEIGHT          = 60.0f;    // プレイヤーHPバーの高さ
            const float SCREEN_POS_X        = 220.0f;   // 画面上のX中心座標
            const float SCREEN_POS_Y        = 45.0f;    // 画面上のY中心座標

            // HPバー配色定義
            const ColorRGB HP_HIGH          = { 0, 230, 255 };   // 高HP時のカラー（シアン/青緑）
            const ColorRGB HP_MID           = { 255, 200, 0 };   // 中HP時のカラー（黄色）
            const ColorRGB HP_LOW           = { 255, 50, 50 };   // 低HP時のカラー（赤）
            
            const ColorRGB BAR_BG           = { 20, 30, 45 };    // HPゲージの空部分のカラー
            const ColorRGB BAR_BORDER       = { 40, 140, 200 };  // HPゲージの枠線カラー
            
            const ColorRGB TEXT_HP          = { 230, 240, 255 }; // HP数値テキスト（5 / 5など）のカラー
            const ColorRGB TEXT_LABEL       = { 50, 200, 255 };  // "PLAYER"ラベルテキストのカラー
            
            const ColorRGB PANEL_BG         = { 10, 20, 35 };    // プレイヤーHPバーパネル全体の背景カラー（深青色）
        }
    }

    // 空間分割（コリジョングリッド）に関するパラメータ
    namespace Collision
    {
        constexpr float GRID_CELL_SIZE  = 5.0f;
        constexpr int   GRID_COLS       = 24;
        constexpr int   GRID_ROWS       = 24;
        constexpr float GRID_MIN_X      = -60.0f;
        constexpr float GRID_MIN_Z      = -60.0f;
    }

    // 影（丸影／ドロップシャドウ）に関する調整パラメータ
    namespace Shadow
    {
        // プレイヤーの影
        const float PLAYER_RADIUS_SCALE     = 1.85f;    // プレイヤーの影の大きさ倍率
        const float PLAYER_ALPHA            = 0.78f;    // プレイヤーの影の黒さ (0.0=透明 〜 1.0=完全な黒)

        // 通常エネミーの影
        const float ENEMY_RADIUS_SCALE      = 1.70f;    // 通常敵の影の大きさ倍率
        const float ENEMY_ALPHA             = 0.72f;    // 通常敵の影の黒さ (0.0〜1.0)

        // ボスの影
        const float BOSS_RADIUS_SCALE       = 1.50f;    // ボスの影の大きさ（小さく: 1.2〜1.4 / 大きく: 1.8〜2.0）
        const float BOSS_ALPHA              = 0.95f;    // ボスの影の黒さ（★0.95fで重厚感のある漆黒）

        // アイテムの影
        const float ITEM_RADIUS             = 1.30f;    // アイテムの影の大きさ
        const float ITEM_ALPHA              = 0.60f;    // アイテムの影の黒さ (0.0〜1.0)
    }

    // ステージ構築・配置に関するパラメータ
    namespace Stage
    {
        // ボスステージ設定
        const float BOSS_ROOM_SIZE          = 28.0f;    // ボス部屋の広さ（四方の壁の距離・横幅56mに拡大）
        const float BOSS_SPAWN_OFFSET_Z     = 10.0f;    // ボスの初期出現位置のZ座標
        const float BOSS_WALL_HEIGHT        = 8.5f;     // ボス部屋の壁の高さ（高くしてプレイヤーの飛び出しを防止）
        const float BOSS_WALL_Y             = 3.25f;    // ボス部屋の壁のY中心座標（底面-1.0f、上面7.5f）
        const float BOSS_WALL_THICKNESS     = 3.0f;     // ボス部屋の壁の厚み（トンネリング貫通防止）

        // 通常ステージ設定（60m×60m バトルアリーナ）
        const float NORMAL_ROOM_SIZE        = 30.0f;    // 通常アリーナの広さ（四方の壁の距離・横幅60m）
        const float NORMAL_WALL_HEIGHT      = 8.5f;     // 外周壁の高さ
        const float NORMAL_WALL_Y           = 3.25f;    // 外周壁の中心Y座標
        const float NORMAL_WALL_THICKNESS   = 3.0f;     // 外周壁の厚み
        const float PLAYER_START_POS_Z      = -20.0f;   // 通常ステージでのプレイヤー開始Z座標（南側から北向きに戦闘開始）

        // 通常ステージの障害物（柱・バリケード）設定
        const float PILLAR_POS_X            = 10.0f;    // 左右ピラーのXオフセット
        const float PILLAR_POS_Y            = 2.0f;     // ピラーの中心Y座標
        const float PILLAR_POS_Z            = 6.0f;     // ピラーのZ座標
        const float PILLAR_SCALE_XZ         = 3.0f;     // ピラーの太さ
        const float PILLAR_SCALE_Y          = 6.0f;     // ピラーの高さ

        const float BARRICADE_POS_X         = 12.0f;    // 左右バリケードのXオフセット
        const float BARRICADE_POS_Y         = 0.4f;     // バリケードの中心Y座標（飛び越え可能）
        const float BARRICADE_POS_Z         = -6.0f;    // バリケードのZ座標
        const float BARRICADE_WIDTH         = 7.0f;     // バリケードの横幅
        const float BARRICADE_HEIGHT        = 2.8f;     // バリケードの高さ
        const float BARRICADE_THICKNESS     = 2.0f;     // バリケードの厚み

        // アイテム配置
        const float ITEM_SPAWN_Y            = 0.5f;
        const float ITEM_BOSS_VACUUM_Z      = -15.0f;
        const float ITEM_BOSS_GIGANT_X      = -4.0f;
        const float ITEM_BOSS_GIGANT_Z      = -15.0f;
        const float ITEM_BOSS_LIGHTNING_X   = 4.0f;
        const float ITEM_BOSS_LIGHTNING_Z   = -15.0f;

        const float ITEM_NORMAL_VACUUM_Z    = 24.0f;    // 最奥（敵陣奥のリワード）
        const float ITEM_NORMAL_GIGANT_X    = -18.0f;   // 左翼
        const float ITEM_NORMAL_GIGANT_Z    = 0.0f;
        const float ITEM_NORMAL_LIGHTNING_X = 18.0f;    // 右翼
        const float ITEM_NORMAL_LIGHTNING_Z = 0.0f;

        // タイトル画面の背景壁設定（浮きの解消や高さ調整用）
        const float TITLE_WALL_DIST         = 20.0f;    // 中央からの距離
        const float TITLE_WALL_POS_Y        = 1.0f;     // 壁の中心Y座標（小さくすると下がり、大きくすると浮きます）
        const float TITLE_WALL_HEIGHT       = 4.5f;     // 壁の高さ（Scale Y）
        const float TITLE_WALL_WIDTH        = 8.0f;     // 壁の横幅
        const float TITLE_WALL_THICKNESS    = 1.0f;     // 壁の厚み
    }

    // デバッグ・テスト用パラメータ
    namespace Debug
    {
#ifdef NDEBUG
        const bool  START_FROM_BOSS         = true;    // リリース時は強制的に通常ステージから開始
        const bool  INVINCIBLE_PLAYER       = false;    // リリース時は無敵化を強制無効
#else
        const bool  START_FROM_BOSS         = true;   // 通常ステージからスタート（ボス戦直行テスト時はtrue）
        const bool  INVINCIBLE_PLAYER       = false;    // trueにするとプレイヤーが無敵になります（テスト用）
#endif
    }
}
