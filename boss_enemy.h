#pragma once
#include "enemy.h"
#include <vector>

enum class BossState {
    NORMAL,            // 通常行動状態
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

// =================================================================
// 巨大ボスエネミー (BossEnemy)
// =================================================================
class BossEnemy : public Enemy
{
private:
    int   m_HP = 60;
    int   m_MaxHP = 60;
    int   m_DamageFlashTimer = 0; // 被弾被弾フラッシュタイマー
    int   m_AttackTimer = 0;      // 攻撃クールダウンタイマー
    int   m_AttackPattern = 0;    // 攻撃パターンインデックス

    // 段階的フェーズ移行用
    BossState m_BossState        = BossState::NORMAL;
    int   m_PhaseAttackTimer     = 0;     // 特別攻撃進行用タイマー
    int   m_PhaseIndex           = 0;     // 現在の特別フェーズ (1, 2, 3)
    bool  m_IsInvincible         = false; // 無敵フラグ
    bool  m_Phase1Triggered      = false; // フェーズ1実行フラグ
    bool  m_Phase2Triggered      = false; // フェーズ2実行フラグ
    bool  m_Phase3Triggered      = false; // フェーズ3実行フラグ
    DirectX::XMFLOAT3 m_PhaseTargetPos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f); // 特別攻撃ターゲット座標
    int   m_LightningVisualTimer = 0;     // 落雷A（プレイヤー狙い）ビジュアルタイマー
    std::vector<BossShockwave> m_ActiveShockwaves; // アクティブな地響き衝撃波リスト

    // ランダム落雷B の着弾座標と残り表示フレーム
    struct RandomLightning {
        DirectX::XMFLOAT3 pos;  // 着弾座標
        int               timer; // 残り表示フレーム（0になったら削除）
    };
    std::vector<RandomLightning> m_RandomLightnings; // 描画用ランダム落雷リスト

    void UpdateBossAI();
    void Fire3WaySpread();
    void FireRapidShot();

    // 特別攻撃パターン
    void PerformPhaseAttack();
    void PerformPhase1Attack();
    void PerformPhase2Attack();
    void PerformPhase3Attack();
    void DrawBarrierEffect();

public:
    void Init() override;
    void Update() override;
    void Draw() override;

    void ApplyBossDamage(int damage, const DirectX::XMFLOAT3& hitSourcePos);
    int  GetHP() const { return m_HP; }
    int  GetMaxHP() const { return m_MaxHP; }
    bool IsInvincible() const { return m_IsInvincible; }
    static ObjectType GetStaticType() { return ObjectType::Boss; }
    ObjectType GetObjectType() const override { return GetStaticType(); }
    DirectX::XMFLOAT3 GetEmissive() const override;
};
