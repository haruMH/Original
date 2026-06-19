#include "item.h"
#include "renderer.h"
#include "resource_manager.h"

// =================================================================
// 初期化処理
// =================================================================
void Item::Init()
{
    // 描画用のコンポーネント設定
    m_RenderComponent.visible = false; // インスタンス一括描画から除外（個別でマテリアルカラーを変更するため）
    m_RenderComponent.meshType = MeshType::Cube;
    // リリースビルド時は Assets/texture/ サブフォルダのパスを使用する
#ifdef NDEBUG
    m_RenderComponent.textureKey = "Assets/texture/player.png"; // テクスチャはプレイヤー用のものを流用
#else
    m_RenderComponent.textureKey = "player.png"; // テクスチャはプレイヤー用のものを流用
#endif
    
    // アイテムらしいサイズに調整（一回り小さく）
    SetScale(DirectX::XMFLOAT3(0.5f, 0.5f, 0.5f));
    
    // 初期位置
    SetPosition(DirectX::XMFLOAT3(0.0f, 0.5f, 5.0f));

    // デフォルトタイプ
    m_Type = ItemType::VACUUM;
}

// =================================================================
// 解放処理
// =================================================================
// 日本語コメントで文字化けのないように記述
void Item::Uninit()
{
}

// =================================================================
// 更新処理
// =================================================================
void Item::Update()
{
    // アイテムをその場でくるくる回転させる演出
    DirectX::XMFLOAT3 rot = GetRotation();
    rot.y += m_RotationSpeed * 0.016f; // 毎フレーム少しずつ回転
    rot.x += m_RotationSpeed * 0.008f;
    SetRotation(rot);
}

// =================================================================
// 描画処理
// =================================================================
void Item::Draw()
{
    using namespace DirectX;

    // ワールド行列の作成
    XMMATRIX scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
    XMMATRIX translation = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    XMMATRIX world = scale * rotation * translation;

    if (Renderer::IsShadowMode()) {
        world = world * Renderer::GetShadowMatrix();
    }
    Renderer::SetWorldMatrix(world);

    if (!Renderer::IsShadowMode() && !Renderer::IsOutlineMode()) {
        // 通常描画パスのときだけ、アイテムの種類に応じた光る色（エミッシブ）を設定
        MATERIAL material;
        ZeroMemory(&material, sizeof(material));
        material.Diffuse = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
        material.Ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
        material.Specular = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        material.Shininess = 50.0f;
        material.TextureEnable = TRUE;
        
        switch (m_Type) {
        case ItemType::VACUUM:
            // 吸引アイテムは妖しく光る紫色
            material.Emission = XMFLOAT4(1.5f, 0.0f, 2.0f, 1.0f);
            break;
        case ItemType::GIGANT:
            // 巨大化アイテムは警告のような赤色
            material.Emission = XMFLOAT4(2.2f, 0.2f, 0.0f, 1.0f);
            break;
        case ItemType::LIGHTNING:
            // 雷電アイテムは眩しく光るシアン（青緑）
            material.Emission = XMFLOAT4(0.0f, 1.8f, 2.5f, 1.0f);
            break;
        }
        Renderer::SetMaterial(material);
        Renderer::SetTexture(ResourceManager::GetTexture(m_RenderComponent.textureKey.c_str()));
    }

    Renderer::SetupCubeDraw();
    Renderer::GetDeviceContext()->Draw(36, 0);
}
