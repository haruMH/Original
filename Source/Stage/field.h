#pragma once
#include "gameobject.h"

class Field : public GameObject
{
private:
    ID3D11Buffer*       m_VertexBuffer = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader*  m_PixelShader = nullptr;
    ID3D11InputLayout*  m_VertexLayout = nullptr;
    ID3D11ShaderResourceView* m_Texture = nullptr;
    ID3D11ShaderResourceView* m_SkyTexture = nullptr; // 反射フォールバック用スカイテクスチャ

    int m_VertexCount = 0;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;
    static ObjectType GetStaticType() { return ObjectType::Field; }
    ObjectType GetObjectType() const override { return GetStaticType(); }
};

enum class SkyType {
    Day = 0,
    Sunset,
    Night,
    Max
};

// スカイボックス / スカイドーム (Skybox / Skydome) クラス
class Skybox : public GameObject
{
private:
    ID3D11Buffer*       m_VertexBuffer = nullptr;
    ID3D11Buffer*       m_IndexBuffer  = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader*  m_PixelShader  = nullptr;
    ID3D11InputLayout*  m_VertexLayout = nullptr;
    int                 m_IndexCount   = 0;

    ID3D11ShaderResourceView* m_Textures[(int)SkyType::Max] = {};
    SkyType             m_CurrentType = SkyType::Day;
    float               m_RotationAngle = 0.0f;
    float               m_RotationSpeed = 0.008f; // ゆっくりとした自転速度 (rad/s)

    void CreateDomeMesh(int slices, int stacks, float radius);

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    void SetSkyType(SkyType type) { m_CurrentType = type; }
    SkyType GetSkyType() const { return m_CurrentType; }

    static ObjectType GetStaticType() { return ObjectType::Skybox; }
    ObjectType GetObjectType() const override { return GetStaticType(); }
};
