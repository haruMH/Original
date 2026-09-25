#include "material_drop.h"
#include "manager.h"
#include "player.h"
#include "progression_manager.h"
#include "sound_manager.h"
#include "item_notification_hud.h"
#include <cmath>

using namespace DirectX;

void MaterialDrop::Init()
{
    m_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(0.35f, 0.35f, 0.35f);
    m_Size     = XMFLOAT3(0.35f, 0.35f, 0.35f);
    m_BaseY    = 0.0f;
    m_FloatTimer = 0.0f;
    m_LifeFrames = 0;
    m_IsAttracting = false;
    m_Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

    m_RenderComponent = RenderComponent("wall_comic_block.png", MeshType::Cube, true);
}

void MaterialDrop::Uninit()
{
}

void MaterialDrop::Setup(MaterialType type, const XMFLOAT3& pos, const XMFLOAT3& initialVel)
{
    m_MaterialType = type;
    m_Position = pos;
    m_BaseY = pos.y;
    m_Velocity = initialVel;

    std::string texKey = "wall_comic_block.png";
    switch (type)
    {
    case MaterialType::SCRAP_IRON:
        texKey = "wall_comic_block.png";
        break;
    case MaterialType::ENERGY_CELL:
        texKey = "wall_comic_cyan.png";
        break;
    case MaterialType::EXPLOSIVE_POWDER:
        texKey = "wall_comic_crimson.png";
        break;
    case MaterialType::LIGHTNING_CORE:
        texKey = "wall_cyber_neon.png";
        break;
    case MaterialType::TITAN_ARMOR:
        texKey = "wall_comic_gold.png";
        break;
    case MaterialType::TITAN_CORE:
        texKey = "wall_honeycomb.png";
        m_Scale = XMFLOAT3(0.5f, 0.5f, 0.5f);
        break;
    default:
        break;
    }

    m_RenderComponent.textureKey = texKey;
}

void MaterialDrop::Update()
{
    m_LifeFrames++;

    if (fabsf(m_Velocity.x) > 0.001f || fabsf(m_Velocity.z) > 0.001f || fabsf(m_Velocity.y) > 0.001f)
    {
        m_Position.x += m_Velocity.x;
        m_Position.y += m_Velocity.y;
        m_Position.z += m_Velocity.z;

        m_Velocity.y -= 0.015f;
        m_Velocity.x *= 0.92f;
        m_Velocity.z *= 0.92f;

        if (m_Position.y <= 0.0f)
        {
            m_Position.y = 0.0f;
            m_BaseY = 0.0f;
            m_Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
        }
    }
    else
    {
        m_FloatTimer += 0.05f;
        m_Position.y = m_BaseY + sinf(m_FloatTimer) * 0.12f + 0.15f;
    }

    m_Rotation.y += m_RotationSpeed * (1.0f / 60.0f);
    m_Rotation.x = sinf(m_FloatTimer * 0.7f) * 0.2f;

    Player* player = Manager::GetGameObject<Player>();
    if (!player) return;

    XMFLOAT3 pPos = player->GetPosition();
    float dx = pPos.x - m_Position.x;
    float dy = (pPos.y + 0.5f) - m_Position.y;
    float dz = pPos.z - m_Position.z;
    float distSq = dx * dx + dy * dy + dz * dz;

    float attractRadius = ProgressionManager::HasMaterialVacuum() ? 6.5f : 2.8f;
    float attractRadiusSq = attractRadius * attractRadius;

    if (distSq < attractRadiusSq || m_IsAttracting)
    {
        m_IsAttracting = true;

        float dist = sqrtf(distSq);
        if (dist > 0.0001f)
        {
            float speed = 0.35f + (6.5f / (dist + 0.5f)) * 0.08f;
            m_Position.x += (dx / dist) * speed;
            m_Position.y += (dy / dist) * speed;
            m_Position.z += (dz / dist) * speed;
        }

        if (dist < 0.6f)
        {
            ProgressionManager::AddSessionMaterial(m_MaterialType, 1);

            XMFLOAT3 col = ProgressionData::GetMaterialColor(m_MaterialType);
            const char* matNameUtf8 = ProgressionData::GetMaterialNameUtf8(m_MaterialType);
            ItemNotificationHUD::Add(matNameUtf8, 1, XMFLOAT4(col.x, col.y, col.z, 1.0f));

            SoundManager::PlaySE(SoundID::SE_ITEM_PICKUP);

            SetDestroy();
        }
    }
}

void MaterialDrop::Draw()
{
}
