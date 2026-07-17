// enemy_affix.cpp
// Enemy Affix の実装（2-3 対応）
// SandbagAffix::Update() および各Affixの非インラインメソッドをここに集約する。

#include "enemy_affix.h"
#include "enemy.h"
#include <cmath>

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
