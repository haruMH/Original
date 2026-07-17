#pragma once
#include <DirectXMath.h>

class Enemy;

// =================================================================
// Enemy Affix Interface (基底インターフェース)
// =================================================================
class IEnemyAffix
{
public:
    virtual ~IEnemyAffix() = default;
    virtual void Update(Enemy* enemy) {}
    virtual DirectX::XMFLOAT3 GetEmissive() const { return {0.0f, 0.0f, 0.0f}; }
    virtual bool IsExplosive() const { return false; }
    virtual bool IsLightning() const { return false; }
    virtual bool IsSandbag()   const { return false; }
};

// --- Explosive Affix ---
class ExplosiveAffix : public IEnemyAffix
{
private:
    // ゲーム内フレームカウンタ（2-2 対応）
    // GetTickCount()（実時間）ではなくフレーム数を使うことで、
    // ヒットストップ・スローモーション中に他の演出と同様に減速・停止する。
    int m_FrameCount = 0;

public:
    void Update(Enemy* enemy) override
    {
        // Update() はゲームロジックと連動して呼ばれるため、
        // ヒットストップ中は呼び出しが減速・停止し、演出の一貫性が保たれる
        m_FrameCount++;
    }

    DirectX::XMFLOAT3 GetEmissive() const override;

    bool IsExplosive() const override { return true; }
};

// --- Lightning Affix ---
class LightningAffix : public IEnemyAffix
{
public:
    DirectX::XMFLOAT3 GetEmissive() const override
    {
        // 水色グロー
        return {0.0f, 1.8f, 2.5f};
    }
    bool IsLightning() const override { return true; }
};

// --- Sandbag Affix ---
class SandbagAffix : public IEnemyAffix
{
private:
    int m_Life = 300;

public:
    DirectX::XMFLOAT3 GetEmissive() const override
    {
        // 緑グロー
        return {0.2f, 1.8f, 0.2f};
    }
    bool IsSandbag() const override { return true; }
    void Update(Enemy* enemy) override; // enemy_affix.cpp で定義（2-3 対応）
};
