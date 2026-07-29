#pragma once
#include "gameobject.h"

// =================================================================
// 磁力旋回弾 (MagneticBullet)
// ボスの周囲を螺旋周回し、戦闘開始時にプレイヤーに向けて射出される
// =================================================================
class MagneticBullet : public GameObject
{
private:
    GameObject* m_BossTarget = nullptr; // 磁力中心のボス
    float m_Angle = 0.0f;               // 周回角度（ラジアン）
    float m_CurrentRadius = 0.0f;       // 現在の半径

    // 旋回運動パラメータ
    float m_TargetRadius = 3.5f;        // 安定時の周回半径
    float m_RotSpeed = 0.05f;           // 回転速度 (ラジアン/フレーム)
    float m_AttractFactor = 0.02f;      // 磁力の強さ (引き寄せ係数: 0.0〜1.0)
    float m_YOffset = 1.5f;             // ボスに対する高さオフセット

    // 射出状態パラメータ
    bool             m_IsLaunched = false; // 射出されたか
    DirectX::XMFLOAT3 m_Direction = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f); // 射出方向
    float            m_LaunchSpeed = 0.12f; // 射出後の移動速度
    int              m_Life = 300;          // 射出後の寿命 (フレーム数)
    DirectX::XMFLOAT3 m_EmissiveColor;      // 自己発光カラー

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override; // GameObjectの純粋仮想関数の実装

    // 初期パラメータ設定
    void Setup(GameObject* boss, float startAngle, float startRadius, float targetRadius, float rotSpeed, float attractFactor, float yOffset);

    // プレイヤーに向かって射出する
    void Launch(DirectX::XMFLOAT3 targetPos);

    bool IsLaunched() const { return m_IsLaunched; }
    
    // GameObject の仮想関数のオーバーライド
    DirectX::XMFLOAT3 GetEmissive() const override { return m_EmissiveColor; }
    ObjectType GetObjectType() const override { return ObjectType::Bullet; }

    static ObjectType GetStaticType() { return ObjectType::Bullet; }
};
