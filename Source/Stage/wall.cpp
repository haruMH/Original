#include "wall.h"
#include "renderer.h"
#include "resource_manager.h"

// =================================================================
// 初期化
// =================================================================
void Wall::Init()
{
    m_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_Size     = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_Emissive = XMFLOAT3(0.0f, 0.0f, 0.0f);

    m_Texture = ResourceManager::GetTexture("wall.png");

    // コンポーネント指向での描画パラメータ初期化（インスタンスバッチ描画の対象にする）
    m_RenderComponent = RenderComponent("wall.png", MeshType::Cube, true);
}

// =================================================================
// 終了処理
// =================================================================
void Wall::Uninit()
{
}

// =================================================================
// 更新処理
// =================================================================
void Wall::Update()
{
    // 本編の壁は静的オブジェクトのため、毎フレ1ームの更新は行いません
}

// =================================================================
// 個別描画処理
// =================================================================
void Wall::Draw()
{
    // 通常・シャドウ・アウトラインの描画は RenderSystem 側で一括描画するため、
    // ここでの個別描画（DrawIndexed）は行いません。
}

// =================================================================
// テクスチャ設定
// =================================================================
void Wall::SetTexture(std::string_view textureName)
{
    m_Texture = ResourceManager::GetTexture(textureName);
    m_RenderComponent.textureKey = std::string(textureName);
}
