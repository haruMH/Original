#pragma once
#include "enemy.h"

// =================================================================
// 攻撃エネミークラス
// =================================================================
class AttackingEnemy : public Enemy
{
private:
    static constexpr float ATTACK_SPEED    = 0.025f; // プレイヤーへの追尾移動速度
    static constexpr float SHOOT_RANGE     = 15.0f;  // 射撃を開始する距離
    static constexpr int   BASE_COOLDOWN   = 120;    // 射撃クールダウンの基本フレーム数（2秒）
    static constexpr float JUMP_FORCE      = 0.18f;  // ジャンプ時の上方向初速
    static constexpr float GRAVITY         = 0.008f; // 重力加速度
    static constexpr float AVOID_RADIUS    = 2.2f;   // 敵同士が回避を始める距離
    static constexpr float AVOID_FORCE     = 1.8f;   // 回避行動の強さ

    int m_ShootCooldown = 0; // 射撃クールダウンタイマー

public:
    void Init()   override;
    void Update() override;

    // 自発光（Emissive）情報の取得（赤色に強く発光させる）
    XMFLOAT3 GetEmissive() const override { return XMFLOAT3(1.5f, 0.0f, 0.0f); }
};
