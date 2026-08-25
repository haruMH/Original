#include "player_visual.h"
#include "player.h"
#include "player_movement.h"
#include "math_helper.h"
#include "renderer.h"
#include "manager.h"
#include "camera.h"
#include "enemy.h"
#include "game_constants.h"

using namespace DirectX;

// =================================================================
// Constructor / Destructor
// =================================================================
PlayerVisual::PlayerVisual(Player* owner)
    : m_Owner(owner)
{
}

PlayerVisual::~PlayerVisual()
{
}

// =================================================================
// Initialize
// =================================================================
void PlayerVisual::Init()
{
}

// =================================================================
// Update Frame
// =================================================================
void PlayerVisual::Update()
{
    // Update lightning effects timers
    auto& effects = m_Owner->GetLightningEffects();
    for (auto it = effects.begin(); it != effects.end(); ) {
        it->Timer--;
        if (it->Timer <= 0) {
            it = effects.erase(it);
        } else {
            it++;
        }
    }
}

// =================================================================
// Draw Elements
// =================================================================
void PlayerVisual::Draw()
{
    if (Renderer::IsShadowMode() || Renderer::IsOutlineMode()) return;

    // Draw dash ghosts
    if (m_Owner->GetMovementModule()) {
        const auto& ghosts = m_Owner->GetMovementModule()->GetDashGhosts();
        if (!ghosts.empty()) {
            Renderer::SetupCubeDraw();

            for (const auto& ghost : ghosts) {
                XMMATRIX world = XMMatrixScaling(ghost.Scale.x, ghost.Scale.y, ghost.Scale.z) * 
                                 XMMatrixRotationRollPitchYaw(ghost.Rotation.x, ghost.Rotation.y, ghost.Rotation.z) * 
                                 XMMatrixTranslation(ghost.Position.x, ghost.Position.y, ghost.Position.z);
                Renderer::SetWorldMatrix(world);

                MATERIAL ghostMat;
                ZeroMemory(&ghostMat, sizeof(ghostMat));
                ghostMat.Diffuse        = XMFLOAT4(0.0f, 0.5f, 1.0f, ghost.Alpha);
                ghostMat.Ambient        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
                ghostMat.Specular       = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
                ghostMat.Emission       = XMFLOAT4(0.0f, 0.6f * ghost.Alpha, 1.5f * ghost.Alpha, 1.0f);
                ghostMat.Shininess      = 0.0f;
                ghostMat.TextureEnable  = FALSE;
                ghostMat.RimPower       = 0.0f;
                Renderer::SetMaterial(ghostMat);

                Renderer::GetDeviceContext()->Draw(36, 0);
            }
        }
    }

    // Draw plasma shield effect
    if (m_Owner->IsGuardActive()) {
        XMFLOAT3 center = m_Owner->GetPosition();
        center.y += 0.3f;

        XMFLOAT4 sparkColor = m_Owner->IsParryActive() 
            ? XMFLOAT4(0.0f, 2.5f, 4.0f, 1.0f) 
            : XMFLOAT4(0.0f, 1.0f, 2.5f, 1.0f);

        int sparks = m_Owner->IsParryActive() ? 4 : 2;
        for (int i = 0; i < sparks; i++) {
            float angle = ((float)rand() / RAND_MAX) * XM_2PI;
            float pitch = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * XM_PIDIV4;
            float radius = 1.0f;

            XMFLOAT3 start = XMFLOAT3(
                center.x + sinf(angle) * cosf(pitch) * radius,
                center.y + sinf(pitch) * radius,
                center.z + cosf(angle) * cosf(pitch) * radius
            );

            float angle2 = angle + XM_PIDIV4 + (((float)rand() / RAND_MAX) * 0.1f);
            XMFLOAT3 end = XMFLOAT3(
                center.x + sinf(angle2) * cosf(pitch) * radius,
                center.y + sinf(pitch) * radius,
                center.z + cosf(angle2) * cosf(pitch) * radius
            );

            DrawLightningBolt(start, end, 0.02f, sparkColor);
        }
    }

    // Draw stun discharge effects
    if (m_Owner->GetDamageTimer() > 0) {
        XMFLOAT3 start = m_Owner->GetPosition();
        start.y += 0.3f;

        int sparks = 2 + (rand() % 2); 
        for (int i = 0; i < sparks; i++) {
            float angle = ((float)rand() / RAND_MAX) * XM_2PI;
            float pitch = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * XM_PIDIV4;
            float dist = 0.8f + ((float)rand() / RAND_MAX) * 1.2f;

            XMFLOAT3 end = XMFLOAT3(
                start.x + sinf(angle) * cosf(pitch) * dist,
                start.y + sinf(pitch) * dist,
                start.z + cosf(angle) * cosf(pitch) * dist
            );

            DrawLightningBolt(start, end, 0.02f, XMFLOAT4(2.8f, 0.0f, 2.0f, 1.0f));
        }
    }

    // Draw aim guide laser when grabbing an enemy
    if (m_Owner->GetGrabbedEnemy()) {
        float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;
        XMFLOAT3 curPos = m_Owner->GetPosition();

        XMMATRIX guideWorld = XMMatrixScaling(0.04f, 0.04f, 8.0f) * 
                              XMMatrixTranslation(0.0f, 0.0f, 4.0f) * 
                              XMMatrixRotationRollPitchYaw(0.0f, camYaw, 0.0f) *
                              XMMatrixTranslation(curPos.x, curPos.y + 0.3f, curPos.z);

        Renderer::SetWorldMatrix(guideWorld);

        MATERIAL guideMaterial;
        ZeroMemory(&guideMaterial, sizeof(guideMaterial));
        guideMaterial.Diffuse        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        guideMaterial.Ambient        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        guideMaterial.Specular       = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        guideMaterial.Emission       = XMFLOAT4(0.0f, 1.8f, 0.5f, 1.0f);
        guideMaterial.Shininess      = 0.0f;
        guideMaterial.TextureEnable  = FALSE;
        guideMaterial.RimPower       = 0.0f;
        Renderer::SetMaterial(guideMaterial);

        Renderer::SetupCubeDraw();
        Renderer::GetDeviceContext()->Draw(36, 0);
    }

    // Draw indicator marker on nearest enemy
    if (m_Owner->GetState() == PlayerState::IDLE) {
        float grabRange = Constants::Player::GRAB_RANGE;
        Enemy* nearest  = nullptr;
        float  nearestDist = grabRange;

        for (GameObject* obj : Manager::GetGameObjectList()) {
            if (!obj || obj == m_Owner || obj->IsDestroy()) continue;
            if (obj->GetObjectType() != ObjectType::Enemy) continue;
            Enemy* e = static_cast<Enemy*>(obj);
            if (e->GetEnemyState() != EnemyState::NORMAL) continue;

            XMFLOAT3 toEnemy = e->GetPosition() - m_Owner->GetPosition();
            float dist = MathHelper::Length(toEnemy);
            if (dist < nearestDist) {
                nearestDist = dist;
                nearest = e;
            }
        }

        if (nearest) {
            XMFLOAT3 enemyPos = nearest->GetPosition();
            float radius = nearest->GetRadius();
            
            float hoverY = enemyPos.y + radius + 0.3f;
            hoverY += sinf((float)m_Owner->GetMarkerTimer() * 0.1f) * 0.08f;

            XMMATRIX markerWorld = XMMatrixScaling(0.12f, 0.12f, 0.12f) * 
                                   XMMatrixRotationRollPitchYaw((float)m_Owner->GetMarkerTimer() * 0.05f, (float)m_Owner->GetMarkerTimer() * 0.05f, 0.0f) * 
                                   XMMatrixTranslation(enemyPos.x, hoverY, enemyPos.z);

            Renderer::SetWorldMatrix(markerWorld);

            MATERIAL markerMaterial;
            ZeroMemory(&markerMaterial, sizeof(markerMaterial));
            markerMaterial.Diffuse        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            markerMaterial.Ambient        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            markerMaterial.Specular       = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
            markerMaterial.Emission       = XMFLOAT4(0.0f, 2.5f, 2.5f, 1.0f);
            markerMaterial.Shininess      = 0.0f;
            markerMaterial.TextureEnable  = FALSE;
            markerMaterial.RimPower       = 0.0f;
            Renderer::SetMaterial(markerMaterial);

            Renderer::SetupCubeDraw();
            Renderer::GetDeviceContext()->Draw(36, 0);
        }
    }

    // Draw active lightning effects
    const auto& effects = m_Owner->GetLightningEffects();
    if (!effects.empty()) {
        for (const auto& effect : effects) {
            for (const auto& segment : effect.Segments) {
                DrawLightningBolt(segment.Start, segment.End, 0.03f, XMFLOAT4(0.0f, 2.2f, 3.0f, 1.0f));
            }
        }
    }
}

// =================================================================
// Draw Lightning Bolt
// =================================================================
void PlayerVisual::DrawLightningBolt(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color)
{
    if (Renderer::IsShadowMode() || Renderer::IsOutlineMode()) return;

    DrawLightningBoltInternal(start, end, 0.02f, XMFLOAT4(0.0f, 1.0f, 2.5f, 1.0f), true, 0);
    DrawLightningBoltInternal(start, end, 0.022f, XMFLOAT4(0.5f, 1.8f, 2.5f, 1.0f), true, 1);
    DrawLightningBoltInternal(start, end, 0.015f, XMFLOAT4(1.8f, 2.2f, 2.5f, 1.0f), false, 2);
}

void PlayerVisual::DrawLightningBoltInternal(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color, bool drawBranches, int seedOffset)
{
    XMVECTOR pA = XMLoadFloat3(&start);
    XMVECTOR pB = XMLoadFloat3(&end);
    XMVECTOR dir = pB - pA;
    float len = XMVectorGetX(XMVector3Length(dir));
    if (len < 0.01f) return;

    const int segmentsCount = 5;
    std::vector<XMVECTOR> points;
    points.push_back(pA);

    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR tangentX = XMVector3Cross(XMVector3Normalize(dir), up);
    if (XMVectorGetX(XMVector3Length(tangentX)) < 0.01f) {
        tangentX = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }
    tangentX = XMVector3Normalize(tangentX);
    XMVECTOR tangentY = XMVector3Normalize(XMVector3Cross(XMVector3Normalize(dir), tangentX));

    for (int i = 1; i < segmentsCount; i++) {
        float ratio = (float)i / segmentsCount;
        XMVECTOR pt = pA + dir * ratio;

        float noiseScale = len * 0.07f;
        if (noiseScale > 0.35f) noiseScale = 0.35f;

        for (int k = 0; k < seedOffset; k++) {
            rand();
        }

        float offsetX = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * noiseScale;
        float offsetY = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * noiseScale;

        pt += tangentX * offsetX + tangentY * offsetY;
        points.push_back(pt);
    }
    points.push_back(pB);

    for (size_t i = 0; i < points.size() - 1; i++) {
        XMVECTOR segmentDir = points[i+1] - points[i];
        float segmentLen = XMVectorGetX(XMVector3Length(segmentDir));
        if (segmentLen > 0.001f) {
            XMVECTOR midPoint = (points[i] + points[i+1]) * 0.5f;

            XMVECTOR zAxis = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
            XMVECTOR targetDir = XMVector3Normalize(segmentDir);
            XMVECTOR rotAxis = XMVector3Cross(zAxis, targetDir);
            float rotAxisLen = XMVectorGetX(XMVector3Length(rotAxis));

            XMMATRIX rotation;
            if (rotAxisLen < 0.001f) {
                float dot = XMVectorGetX(XMVector3Dot(zAxis, targetDir));
                if (dot < 0.0f) {
                    rotation = XMMatrixRotationY(XM_PI);
                } else {
                    rotation = XMMatrixIdentity();
                }
            } else {
                float dot = XMVectorGetX(XMVector3Dot(zAxis, targetDir));
                float angle = acosf(dot);
                rotation = XMMatrixRotationAxis(rotAxis, angle);
            }

            float flicker = 0.8f + ((float)rand() / RAND_MAX) * 0.4f; 
            XMMATRIX scale = XMMatrixScaling(thickness * flicker, thickness * flicker, segmentLen);
            XMMATRIX translation = XMMatrixTranslationFromVector(midPoint);
            XMMATRIX world = scale * rotation * translation;

            Renderer::SetWorldMatrix(world);

            MATERIAL segmentMaterial;
            ZeroMemory(&segmentMaterial, sizeof(segmentMaterial));
            segmentMaterial.Diffuse        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            segmentMaterial.Ambient        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            segmentMaterial.Specular       = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
            segmentMaterial.Emission       = color;
            segmentMaterial.Shininess      = 0.0f;
            segmentMaterial.TextureEnable  = FALSE;
            segmentMaterial.RimPower       = 0.0f;
            Renderer::SetMaterial(segmentMaterial);

            Renderer::SetupCubeDraw();
            Renderer::GetDeviceContext()->Draw(36, 0);
        }
    }
}
