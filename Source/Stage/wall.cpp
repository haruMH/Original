#include "wall.h"
#include "manager.h"
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

// =================================================================
// 四方（北・南・東・西）の外周境界壁を一括生成
// =================================================================
void Wall::CreateBoundaryWalls(float roomHalfSize, float wallHeight, float wallCenterY, float thickness)
{
    const float halfThick = thickness * 0.5f;
    const float spanLength = roomHalfSize * 2.0f + thickness * 2.0f;

    // 北壁
    Wall* wallN = Manager::AddGameObject<Wall>();
    wallN->SetPosition(XMFLOAT3(0.0f, wallCenterY, roomHalfSize + halfThick));
    wallN->SetScale(XMFLOAT3(spanLength, wallHeight, thickness));

    // 南壁
    Wall* wallS = Manager::AddGameObject<Wall>();
    wallS->SetPosition(XMFLOAT3(0.0f, wallCenterY, -(roomHalfSize + halfThick)));
    wallS->SetScale(XMFLOAT3(spanLength, wallHeight, thickness));

    // 東壁
    Wall* wallE = Manager::AddGameObject<Wall>();
    wallE->SetPosition(XMFLOAT3(roomHalfSize + halfThick, wallCenterY, 0.0f));
    wallE->SetScale(XMFLOAT3(thickness, wallHeight, spanLength));

    // 西壁
    Wall* wallW = Manager::AddGameObject<Wall>();
    wallW->SetPosition(XMFLOAT3(-(roomHalfSize + halfThick), wallCenterY, 0.0f));
    wallW->SetScale(XMFLOAT3(thickness, wallHeight, spanLength));
}
