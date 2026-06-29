#pragma once

// =================================================================
// ゲーム定数定義ヘッダー (game_constants.h)
// =================================================================
// ゲームのバランスや挙動を調整するための定数を一元管理します。
// プログラミング学習時の値調整や、ゲームの面白さを左右するパラメータはすべてここに集約します。

namespace Constants
{
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

        // 雷電エフェクト関連
        const int   LIGHTNING_TIMER         = 8;        // 放電エフェクトの表示フレーム数
    }

    // 敵キャラに関するパラメータ
    namespace Enemy
    {
        const int   DEFAULT_SCORE           = 100;      // 敵を倒した時に獲得できる標準スコア
        const float GIGANT_SCALE            = 5.0f;     // 巨大化アイテム使用時の敵の拡大率
        const float FLYING_AIR_RESISTANCE   = 0.94f;    // 投げ飛ばされた敵の空気抵抗（XZ減速率）
        const float FLYING_GRAVITY          = 0.015f;   // 投げ飛ばされた敵にかかる重力
        
        // ボスエネミーのパラメータ
        const int   BOSS_HP                 = 60;       // ボスの初期最大HP
        const int   BOSS_SCORE              = 1500;     // ボス撃破時のスコア
    }
}
