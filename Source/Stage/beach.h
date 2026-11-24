#pragma once
#include "gameobject.h"
#include <wrl/client.h>

// =================================================================
// 砂浜と海 (Beach)
// =================================================================
// タイトル用の背景。砂浜は緩い傾斜の地形メッシュ（砂粒の絵はCPUで生成）、海は y=-1 の水面（sea_ps で波・泡・反射を描く）。
// 岸線は z=SHORE_Z 付近（x でうねる）。海は +z 側、砂浜は -z 側。
class Beach : public GameObject
{
public:
    void Init() override;
    void Uninit() override;
    void Update() override {}
    void Draw() override;

    // 砂浜の高さ（プレイヤーの足元合わせ用）。水面より下は海底の高さ
    static float GroundY(float x, float z);
    // 岸線のZ座標（xでうねる。sea_ps.hlsl の ShoreZ と同じ式）
    static float ShoreZ(float x);

    static ObjectType GetStaticType() { return ObjectType::Field; }
    ObjectType GetObjectType() const override { return GetStaticType(); }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer>              m_SandVB;
    Microsoft::WRL::ComPtr<ID3D11Buffer>              m_SeaVB;
    Microsoft::WRL::ComPtr<ID3D11Buffer>              m_SeaCB;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_SandTex;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader*  m_SandPS = nullptr;
    ID3D11PixelShader*  m_SeaPS = nullptr;
    ID3D11InputLayout*  m_VertexLayout = nullptr;
    ID3D11VertexShader* m_SeaVS = nullptr;       // 波で頂点を動かす海面用
    ID3D11InputLayout*  m_SeaLayout = nullptr;
    int m_SandVertexCount = 0;
    int m_SeaVertexCount = 0;

    void CreateSandTexture();
};
