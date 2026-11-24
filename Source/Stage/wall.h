#pragma once
#include "gameobject.h"

// =================================================================
// Wall object class
// =================================================================
class Wall : public GameObject
{
private:
    ID3D11ShaderResourceView* m_Texture = nullptr;
    DirectX::XMFLOAT3         m_Emissive = { 0.0f, 0.0f, 0.0f };

public:
    void Init()   override;
    void Uninit() override;
    void Update() override;
    void Draw()   override;
    
    static ObjectType GetStaticType() { return ObjectType::Wall; }
    ObjectType GetObjectType() const override { return GetStaticType(); }
    
    void SetVisible(bool visible) { m_RenderComponent.visible = visible; }
    void SetOutline(bool outline) { m_RenderComponent.outline = outline; }
    void SetTexture(std::string_view textureName);
    void SetEmissive(const DirectX::XMFLOAT3& emissive) { m_Emissive = emissive; }
    DirectX::XMFLOAT3 GetEmissive() const override { return m_Emissive; }

    // 四方（北・南・東・西）の外周境界壁を一括生成するヘルパー
    // visible=false で見えない当たり判定だけの壁にできる（拠点: 遠景を見せるため）
    static void CreateBoundaryWalls(float roomHalfSize, float wallHeight, float wallCenterY, float thickness, bool visible = true);
};
