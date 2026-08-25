#pragma once

// =================================================================
// ゲーム定数定義ヘッダー (game_constants.h)
// =================================================================
// ゲームのバランスや挙動を調整するための定数を一元管理します。
// プログラミング学習時の値調整や、ゲームの面白さを左右するパラメータはすべてここに集約します。

namespace Constants
{
    // 構造体定義
    struct ColorRGB { unsigned char R, G, B; };

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

        // フェーズ移行閾値 (HP)
        const int   PHASE1_HP_THRESHOLD     = 42;       // 第1フェーズ移行のHP閾値
        const int   PHASE2_HP_THRESHOLD     = 30;       // 第2フェーズ移行のHP閾値
        const int   PHASE3_HP_THRESHOLD     = 18;       // 第3フェーズ移行のHP閾値

        // ボス攻撃・落雷関連
        const float STRIKE_DAMAGE_RADIUS    = 4.0f;     // プレイヤー狙い落雷の当たり判定半径
        const int   STRIKE_DAMAGE           = 1;        // プレイヤー狙い落雷のダメージ値
        const float STAGE_HALF_WIDTH        = 17.0f;    // ランダム落雷が発生するステージの半幅
        const int   RANDOM_STRIKE_MIN       = 3;        // ランダム落雷の最小発生箇所数
        const int   RANDOM_STRIKE_EXTRA     = 3;        // ランダム落雷の追加発生箇所数
        const int   ATTACK_START_FRAME      = 350;      // 落雷攻撃が開始するフレーム数（フェーズ移行後）
        const int   ATTACK_DURATION_FRAME   = 550;      // 落雷攻撃が持続するフレーム数
        
        // 撃破後のリザルト移行ディレイ
        const int   CLEAR_DELAY_FRAMES      = 120;      // ボス撃破からクリア画面遷移までの待ち時間（フレーム）

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

    // ステージ構築・配置に関するパラメータ
    namespace Stage
    {
        // ボスステージ設定
        const float BOSS_ROOM_SIZE          = 18.0f;    // ボス部屋の広さ（四方の壁の距離）
        const float BOSS_SPAWN_OFFSET_Z     = 10.0f;    // ボスの初期出現位置のZ座標

        // 通常ステージ設定
        const float PLAYER_START_POS_Z      = 8.0f;     // 通常ステージでのプレイヤー開始Z座標
        const int   ENEMY_GRID_COLS         = 4;        // モブ敵グリッド配置の列数
        const int   ENEMY_GRID_ROWS         = 4;        // モブ敵グリッド配置の行数
        const float ENEMY_SPAWN_INTERVAL    = 6.0f;     // 敵同士の配置間隔
        const float WALL_DEFAULT_POS_X      = -8.0f;
        const float WALL_DEFAULT_POS_Y      = 1.5f;
        const float WALL_DEFAULT_POS_Z      = -7.0f;
        const float WALL_DEFAULT_SCALE      = 5.0f;

        const float ENEMY_GRID_SPAWN_OFFSET_X = 1.0f;
        const float ENEMY_GRID_SPAWN_OFFSET_Z = -7.0f;
        const float ENEMY_GRID_SPAWN_INTERVAL_XZ = 3.2f;

        // アイテム配置
        const float ITEM_SPAWN_Y            = 0.5f;
        const float ITEM_BOSS_VACUUM_Z      = -15.0f;
        const float ITEM_BOSS_GIGANT_X      = -4.0f;
        const float ITEM_BOSS_GIGANT_Z      = -15.0f;
        const float ITEM_BOSS_LIGHTNING_X   = 4.0f;
        const float ITEM_BOSS_LIGHTNING_Z   = -15.0f;

        const float ITEM_NORMAL_VACUUM_Z    = 4.0f;
        const float ITEM_NORMAL_GIGANT_X    = -4.0f;
        const float ITEM_NORMAL_GIGANT_Z    = 4.0f;
        const float ITEM_NORMAL_LIGHTNING_X = 2.0f;
        const float ITEM_NORMAL_LIGHTNING_Z = 6.0f;
    }

    // デバッグ・テスト用パラメータ
    namespace Debug
    {
#ifdef NDEBUG
        const bool  START_FROM_BOSS         = false;    // リリース時は強制的に通常ステージから開始
        const bool  INVINCIBLE_PLAYER       = false;    // リリース時は無敵化を強制無効
#else
        const bool  START_FROM_BOSS         = true;    // trueにするとゲーム開始時にいきなりボス戦からスタートします
        const bool  INVINCIBLE_PLAYER       = false;    // trueにするとプレイヤーが無敵になります（テスト用）
#endif
    }
}
