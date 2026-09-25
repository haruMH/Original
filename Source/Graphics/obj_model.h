#pragma once
#include "main.h"
#include "renderer.h"
#include <string>
#include <vector>
#include <unordered_map>

// =================================================================
// OBJ 3Dモデル読み込み・描画クラス
// =================================================================
class ObjModel
{
public:
    ObjModel();
    ~ObjModel();

    // OBJファイルおよび対応するMTLファイルを読み込む
    bool Load(const std::string& objPath, const std::string& mtlPath = "");

    // 指定のワールド行列で描画（シェーダー・ライティング・影・アウトライン共通対応）
    void Draw(const DirectX::XMMATRIX& worldMatrix);

    // リソース解放
    void Uninit();

    int GetVertexCount() const { return m_VertexCount; }

private:
    // MTLマテリアル読み込みヘルパー
    void LoadMtl(const std::string& mtlPath, std::unordered_map<std::string, DirectX::XMFLOAT4>& materials);

    ID3D11Buffer*          m_VertexBuffer = nullptr;
    ID3D11RasterizerState* m_RasterizerStateNoCull = nullptr;
    int                    m_VertexCount = 0;
};
