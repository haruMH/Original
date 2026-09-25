#pragma once
#include "gameobject.h"
#include "progression_types.h"

// =================================================================
// 謨ｵ謦・ｴ譎ゅ↓繝峨Ο繝・・縺吶ｋ邏譚舌が繝悶ず繧ｧ繧ｯ繝・// =================================================================
class MaterialDrop : public GameObject
{
private:
    MaterialType        m_MaterialType = MaterialType::SCRAP_IRON;
    float               m_BaseY = 0.0f;
    float               m_FloatTimer = 0.0f;
    float               m_RotationSpeed = 3.0f;
    int                 m_LifeFrames = 0;
    DirectX::XMFLOAT3   m_Velocity = { 0.0f, 0.0f, 0.0f };
    bool                m_IsAttracting = false;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    void Setup(MaterialType type, const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& initialVel);
    MaterialType GetMaterialType() const { return m_MaterialType; }

    static ObjectType GetStaticType() { return ObjectType::MaterialDrop; }
    ObjectType GetObjectType() const override { return GetStaticType(); }
};
