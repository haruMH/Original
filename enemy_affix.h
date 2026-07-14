#pragma once
#include <directxmath.h>
#include <memory>
#include <windows.h>
#include <math.h>

class Enemy;

// =================================================================
// Enemy Affix Interface
// =================================================================
class IEnemyAffix
{
public:
    virtual ~IEnemyAffix() = default;
    virtual void Update(Enemy* enemy) {}
    virtual DirectX::XMFLOAT3 GetEmissive() const { return {0.0f, 0.0f, 0.0f}; }
    virtual bool IsExplosive() const { return false; }
    virtual bool IsLightning() const { return false; }
    virtual bool IsSandbag() const { return false; }
};

// --- Explosive Affix ---
class ExplosiveAffix : public IEnemyAffix
{
public:
    DirectX::XMFLOAT3 GetEmissive() const override
    {
        // Pulse red glow (0.8 to 2.2)
        float pulse = 1.5f + sinf(static_cast<float>(GetTickCount()) * 0.015f) * 0.7f;
        return {pulse, pulse * 0.1f, pulse * 0.1f};
    }
    bool IsExplosive() const override { return true; }
};

// --- Lightning Affix ---
class LightningAffix : public IEnemyAffix
{
public:
    DirectX::XMFLOAT3 GetEmissive() const override
    {
        // Light blue glow
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
        // Green glow
        return {0.2f, 1.8f, 0.2f};
    }
    bool IsSandbag() const override { return true; }
    void Update(Enemy* enemy) override;
};
