#pragma once
#include <directxmath.h>
#include <d3d11.h>
#include "enemy.h"
#include "game_constants.h"
#include <vector>

enum class BossState {
    NORMAL,            // 通常行動状態
    CHARGE_WINDUP,     // 突進前のタメ予兆状態
    CHARGE_RUSH,       // 突進中
    CHARGE_STUN,       // 壁激突後のスタン（隙）状態
    SLAM_WINDUP,       // 大ジャンプ前のタメ予兆状態
    SLAM_AIR,          // 上空への滞空・着地地点狙い状態
    SLAM_RECOVERY,     // 着地グラウンドスラム後の隙状態
    RETURN_TO_CENTER,  // 特殊攻撃開始前の定位置復帰ジャンプ
    PHASE_TRANSITION,  // 特別攻撃（フェーズ移行演出）状態
    DEFEATED           // 撃破
};

// 金色地響き衝撃波の波面衝突判定用構造体
struct BossShockwave {
    DirectX::XMFLOAT3 position;
    int   timer;       // 経過フレーム数 (0 -> maxTimer)
    int   maxTimer;    // 最大フレーム数 (40)
    float maxRadius;   // 最大半径 (25.0f)
    bool  hasDamaged;  // すでにダメージを与えたか
};

// 弾幕予測および魔力チャージ情報の共有構造体 (フェーズ1用)
struct BossPhase1ChargeInfo {
    bool  isCharging;
    int   relativeTimer;
    float angleOffset;
};

// =================================================================
// 巨大ボスエネミー (BossEnemy)
// =================================================================
class BossEnemy : public Enemy
{
private:
    int   m_HP = 60;
    int   m_MaxHP = 60;
    int   m_DamageFlashTimer = 0; // 被弾被弾フラッシュタイマー
    int   m_SpinHitCooldownTimer = 0; // スピンなぎ払い被弾クールダウンタイマー
    int   m_AttackTimer = 0;      // 攻撃クールダウンタイマー
    int   m_AttackPattern = 0;    // 攻撃パターンインデックス

    // 段階的フェーズ移行用
    struct BossPhase {
        int hpThreshold;
        int phaseIndex;
        bool triggered;
    };
    std::vector<BossPhase> m_Phases;

    BossState m_BossState        = BossState::NORMAL;
    int   m_PhaseAttackTimer     = 0;     // 特別攻撃進行用タイマー
    int   m_PhaseIndex           = 0;     // 現在の特別フェーズ (1, 2, 3)
    bool  m_IsInvincible         = false; // 無敵フラグ
    DirectX::XMFLOAT3 m_PhaseTargetPos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f); // 特別攻撃ターゲット座標
    int   m_LightningVisualTimer = 0;     // 落雷A（プレイヤー狙い）ビジュアルタイマー
    std::vector<BossShockwave> m_ActiveShockwaves; // アクティブな地響き衝撃波リスト

    // ランダム落雷B の着弾座標と残り表示フレーム
    struct RandomLightning {
        DirectX::XMFLOAT3 pos;  // 着弾座標
        int               timer; // 残り表示フレーム（0になったら削除）
    };
    std::vector<RandomLightning> m_RandomLightnings; // 描画用ランダム落雷リスト

    // 特殊攻撃前の定位置復帰ジャンプ制御用変数
    DirectX::XMFLOAT3 m_JumpStartPos  = DirectX::XMFLOAT3(0.0f, Constants::Boss::GROUND_Y, 0.0f);
    DirectX::XMFLOAT3 m_JumpTargetPos = DirectX::XMFLOAT3(0.0f, Constants::Boss::GROUND_Y, 0.0f);
    int               m_JumpTimer     = 0;
    int               m_JumpDuration  = 45;

    // 突進タックル制御用変数
    DirectX::XMFLOAT3 m_ChargeDir               = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);
    float             m_ChargeTraveled          = 0.0f;
    int               m_ChargeTimer             = 0;
    bool              m_HasDamagedPlayerInCharge = false;

    // 大ジャンプ・グラウンドスラム制御用変数
    DirectX::XMFLOAT3 m_SlamStartPos            = DirectX::XMFLOAT3(0.0f, Constants::Boss::GROUND_Y, 0.0f);
    DirectX::XMFLOAT3 m_SlamTargetPos           = DirectX::XMFLOAT3(0.0f, Constants::Boss::GROUND_Y, 0.0f);
    int               m_SlamTimer               = 0;

    void UpdateBossAI();
    void UpdateReturnJump();
    void StartChargeAttack();
    void UpdateChargeWindup();
    void UpdateChargeRush();
    void UpdateChargeStun();
    void StartSlamAttack();
    void UpdateSlamWindup();
    void UpdateSlamAir();
    void UpdateSlamRecovery();
    void Fire3WaySpread();
    void FireRapidShot();
    void SpawnDroppingObjects(int count, const DirectX::XMFLOAT3& baseDir, float spreadAngle);

    // 特別攻撃パターン
    BossPhase1ChargeInfo GetPhase1ChargeInfo() const;

    void PerformPhaseAttack();
    void PerformPhase1Attack();
    void PerformPhase2Attack();
    void PerformPhase3Attack();
    void DrawBarrierEffect();
    void ApplyBossDamage(int damage, const DirectX::XMFLOAT3& hitSourcePos);

public:
    void Init() override;
    void Update() override;
    void Draw() override;

    void OnHit(const HitInfo& info) override;
    bool CanReceiveSpinHit() const { return m_SpinHitCooldownTimer <= 0; }
    void TriggerSpinHitCooldown(int frames = Constants::Player::SPIN_BOSS_HIT_INTERVAL) { m_SpinHitCooldownTimer = frames; }
    int  GetHP() const { return m_HP; }
    int  GetMaxHP() const { return m_MaxHP; }
    bool IsInvincible() const { return m_IsInvincible; }
    static ObjectType GetStaticType() { return ObjectType::Boss; }
    ObjectType GetObjectType() const override { return GetStaticType(); }
    DirectX::XMFLOAT3 GetEmissive() const override;
};
