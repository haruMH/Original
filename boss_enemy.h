#pragma once
#include "enemy.h"

// =================================================================
// 巨大ボスエネミー (BossEnemy)
// =================================================================
class BossEnemy : public Enemy
{
private:
    int   m_HP = 15;
    int   m_MaxHP = 15;
    int   m_DamageFlashTimer = 0; // 被弾被弾フラッシュタイマー
    int   m_AttackTimer = 0;      // 攻撃クールダウンタイマー
    int   m_AttackPattern = 0;    // 攻撃パターンインデックス

    void UpdateBossAI();
    void Fire3WaySpread();
    void FireRapidShot();

public:
    void Init() override;
    void Update() override;
    void Draw() override;

    void ApplyBossDamage(int damage, const DirectX::XMFLOAT3& hitSourcePos);
    int  GetHP() const { return m_HP; }
    int  GetMaxHP() const { return m_MaxHP; }
    ObjectType GetObjectType() const override { return ObjectType::Boss; }
    DirectX::XMFLOAT3 GetEmissive() const override;
};
