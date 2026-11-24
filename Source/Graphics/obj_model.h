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

    // 全 ObjModel で共有しているリソース（軽量ピクセルシェーダー）の解放。アプリ終了時に呼ぶ
    static void ReleaseSharedResources();

    // 自然物(木・茂み・花・岩)の表示/非表示（FPSの原因切り分け用）
    static void SetFoliageHidden(bool hidden);

private:
    // MTLマテリアル読み込みヘルパー
    // matTex: マテリアル名 → そのマテリアルの map_Kd（木の樹皮と葉のように、1モデルに複数テクスチャがある場合用）
    void LoadMtl(const std::string& mtlPath, std::unordered_map<std::string, DirectX::XMFLOAT4>& materials, std::string& outTexture,
                 std::unordered_map<std::string, std::string>& matTex);

    // マテリアルごとの描画範囲（頂点の並びを usemtl の切り替わりで区切る）
    struct SubMesh {
        int start = 0;
        int count = 0;
        ID3D11ShaderResourceView* tex = nullptr;   // 所有しない（m_TextureSRV か m_ExtraTextures のどれか）
        ID3D11ShaderResourceView* nrm = nullptr;   // 法線マップ(map_Bump)。所有は m_ExtraTextures。中世の家などのみ
    };
    std::unordered_map<std::string, std::string> m_MatNormal;   // マテリアル名 → 法線マップのファイル名
    std::vector<SubMesh>                      m_SubMeshes;
    std::vector<ID3D11ShaderResourceView*>    m_ExtraTextures;   // マテリアル個別のテクスチャ（所有）

    ID3D11Buffer*              m_VertexBuffer = nullptr;
    ID3D11RasterizerState*     m_RasterizerStateNoCull = nullptr;
    ID3D11ShaderResourceView*  m_TextureSRV = nullptr;
    int                        m_VertexCount = 0;
    bool                       m_UseFoliageShader = false;   // 自然物(木・茂み・花)は軽いシェーダーで描く
    bool                       m_UseNormalMap = false;       // 中世の家などは法線マップ対応シェーダーで描く
};
