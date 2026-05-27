#include "wall.h"
#include "renderer.h"
#include "resource_manager.h"

void Wall::Init()
{
    m_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_Size     = XMFLOAT3(1.0f, 1.0f, 1.0f);

    m_Texture = ResourceManager::GetTexture("grid.png");

    // コンポーネント指向での描画パラメータ初期化
    m_RenderComponent = RenderComponent("grid.png", MeshType::Cube, true);
}

void Wall::Uninit()
{
}

void Wall::Update()
{
    // 壁は動かないため特に処理なし
}

void Wall::Draw()
{
    // 通常・シャドウ・アウトラインの描画は RenderSystem 側で一括描画するため、
    // ここでの個別描画（36頂点）は行いません。
}
