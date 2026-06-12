#pragma once
#include "gameobject.h"

// =================================================================
// エネミーの弾クラス
// =================================================================
class EnemyBullet : public GameObject
{
private:
    static constexpr float BULLET_SPEED = 0.20f; // 移動速度（少し避けやすく調整）
    static constexpr int   BULLET_LIFE  = 180;   // 寿命（フレーム数。3秒）

    XMFLOAT3 m_Direction = XMFLOAT3(0.0f, 0.0f, 1.0f); // 移動方向
    float    m_Speed     = BULLET_SPEED;                // 現在の速度
    int      m_Life      = BULLET_LIFE;                 // 残り寿命

public:
    void Init()   override;
    void Uninit() override;
    void Update() override;
    void Draw()   override;

    // 移動方向の設定
    void SetDirection(XMFLOAT3 dir) { m_Direction = dir; }

    // 自発光（Emissive）情報の取得（オレンジ色に強く発光させる）
    XMFLOAT3 GetEmissive() const override { return XMFLOAT3(2.5f, 0.5f, 0.0f); }
    ObjectType GetObjectType() const override { return ObjectType::Bullet; }
};
