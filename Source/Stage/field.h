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

    ID3D11ShaderResourceView* m_NormalTexture = nullptr;  // 床の法線マップ（指定時のみ。法線マップ対応シェーダーで描く）
    ID3D11PixelShader*        m_NormalPS       = nullptr;

    // 道（床の上に貼る長方形1枚。SetRoad で指定したときだけ描く）
    ID3D11Buffer*             m_RoadVB = nullptr;
    ID3D11ShaderResourceView* m_RoadTexture = nullptr;
    ID3D11ShaderResourceView* m_RoadNormal  = nullptr;

    int m_VertexCount = 0;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    // 床の絵柄と法線マップを差し替える（Assets/texture 内のファイル名）。未指定なら従来の grid.png
    void SetFloorTexture(const char* albedoName, const char* normalName);
    // 床の上に道を1本通す（ワールド座標 x0〜x1, z0〜z1 の長方形。絵柄は3mで1周）。法線マップ付き
    void SetRoad(float x0, float x1, float z0, float z1, const char* albedoName, const char* normalName);

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
    ID3D11Buffer*       m_SkyBuffer    = nullptr;   // sky_ps 用定数バッファ
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

    // 空を即座に切り替える（ライトも即時に切り替わる）
    void SetSkyType(SkyType type);
    SkyType GetSkyType() const { return m_CurrentType; }

    static ObjectType GetStaticType() { return ObjectType::Skybox; }
    ObjectType GetObjectType() const override { return GetStaticType(); }
};
