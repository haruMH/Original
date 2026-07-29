// enemy_affix.cpp
// Enemy Affix の実装（2-3 対応）
// SandbagAffix::Update() および各Affixの非インラインメソッドをここに集約する。

#include "enemy_affix.h"
#include "enemy.h"
#include <cmath>
#include "player.h"
#include "explosion_system.h"
#include "collision_system.h"
#include "camera.h"
#include "manager.h"
#include "game_constants.h"
#include "score_popup.h"

// ─────────────────────────────────────────────
// ExplosiveAffix (2-2 対応)
// ─────────────────────────────────────────────
DirectX::XMFLOAT3 ExplosiveAffix::GetEmissive() const
{
    // 赤い脈動グロー（明度 0.8〜2.2 を周期的に変化させる）
    // 係数 0.06f ≒ 約 105 フレームで 1 周期（60fps で約 1.75 秒）
    float pulse = 1.5f + std::sin(static_cast<float>(m_FrameCount) * 0.06f) * 0.7f;
    return DirectX::XMFLOAT3(pulse, pulse * 0.1f, pulse * 0.1f);
}

bool ExplosiveAffix::OnImpact(Enemy* flying, GameObject* target, Player* player, bool& explosionThisFrame)
{
    if (explosionThisFrame) return false;

    // 爆弾同士の衝突は爆発しない
    if (target->GetObjectType() == ObjectType::Enemy) {
        Enemy* te = static_cast<Enemy*>(target);
        if (te->IsExplosive()) return false;
    }

    ExplosionSystem::TriggerExplosion(flying->GetPosition());
    flying->SetEnemyState(EnemyState::DEFEATED);
    flying->SetVelocity(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
    explosionThisFrame = true;
    return true;
}

// ─────────────────────────────────────────────
// LightningAffix
// ─────────────────────────────────────────────
bool LightningAffix::OnImpact(Enemy* flying, GameObject* target, Player* player, bool& explosionThisFrame)
{
    CollisionSystem::TriggerChainLightning(flying->GetPosition(), player);

    HitInfo hitInfo;
    hitInfo.hitSourcePos = flying->GetPosition();
    hitInfo.setLightning = true;
    hitInfo.popupColor = {0.0f, 1.5f, 2.5f};

    if (target->GetObjectType() == ObjectType::Boss) {
        hitInfo.damage = Constants::Lightning::CHAIN_DAMAGE;
    } else {
        hitInfo.damage = 1;
        hitInfo.knockbackVel = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    }
    target->OnHit(hitInfo);

    flying->SetEnemyState(EnemyState::DEFEATED);
    flying->SetVelocity(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
    Manager::AddHitStop(10);
    if (g_Camera) g_Camera->Shake(0.35f, 12);
    return true;
}

// ─────────────────────────────────────────────
// SandbagAffix
// ─────────────────────────────────────────────
void SandbagAffix::Update(Enemy* enemy)
{
    // 通常状態のときのみ寿命をカウントダウンする
    // （被弾中・被投げ中は Enemy 側が別途状態を管理するため除外）
    if (enemy->GetEnemyState() == EnemyState::NORMAL) {
        enemy->SetVelocity(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
        m_Life--;
        if (m_Life <= 0) {
            enemy->Defeat();
            enemy->SetEnemyState(EnemyState::DEFEATED);
        }
    }
}
