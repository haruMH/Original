#include "player_visual.h"
#include "player.h"
#include "player_movement.h"
#include "math_helper.h"
#include "renderer.h"
#include "manager.h"
#include "camera.h"
#include "enemy.h"
#include "boss_enemy.h"
#include "game_constants.h"
#include "player_combat.h"
#include "aura_effect.h"
#include <algorithm>

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
DirectX::XMFLOAT3 PlayerVisual::GetDeformScale() const
{
    XMFLOAT3 s = m_Owner->GetScale();
    float h = 1.0f + m_Stretch;
    float w = 1.0f / sqrtf(h);   // 体積をおおむね保つ
    return XMFLOAT3(s.x * w, s.y * h, s.z * w);
}

DirectX::XMFLOAT3 PlayerVisual::GetDeformOffset() const
{
    // 足元が地面から浮かない/沈まないよう、伸縮した分だけ中心を上下にずらす
    return XMFLOAT3(0.0f, m_Owner->GetScale().y * m_Stretch * 0.5f, 0.0f);
}

void PlayerVisual::Update()
{
    // --- 伸縮演出（見た目のみ。当たり判定・威力には影響しない） ---
    {
        XMFLOAT3 pos = m_Owner->GetPosition();
        if (m_HasPrevPos) {
            float dx = pos.x - m_PrevPos.x;
            float dz = pos.z - m_PrevPos.z;
            m_MoveSpeed = sqrtf(dx * dx + dz * dz);
        }
        m_PrevPos = pos;
        m_HasPrevPos = true;
        m_DeformTime += 1.0f;

        int state = (int)m_Owner->GetState();
        int stage = m_Owner->GetCombatModule() ? m_Owner->GetCombatModule()->GetChargeStage() : 0;

        // 状態切替の瞬間: 掴み=ぐっと潰れる / 回転開始=縮む / 投げ・解除=反動で一気に伸びる
        if (m_LastState != -1 && state != m_LastState) {
            if (state == (int)PlayerState::GRABBED)       m_StretchVel -= 0.10f;
            else if (state == (int)PlayerState::SPINNING) m_StretchVel -= 0.06f;
            else if (m_LastState == (int)PlayerState::SPINNING || m_LastState == (int)PlayerState::GRABBED) {
                m_Stretch = -0.22f;      // 溜めた分だけ潰してから
                m_StretchVel = 0.05f;    // 反動で伸びる
            }
        }
        // 段階が上がるたびに弾む
        if (stage > m_LastStage && state == (int)PlayerState::SPINNING) m_StretchVel += 0.07f + 0.02f * stage;
        m_LastState = state;
        m_LastStage = stage;

        // 目標値: 呼吸 + 走りの上下動 + 回転中は遠心力で潰れ気味に小刻みに揺れる
        float speedF = (std::min)(m_MoveSpeed / 0.25f, 1.0f);
        float target = sinf(m_DeformTime * 0.06f) * 0.025f
                     + sinf(m_DeformTime * 0.45f) * 0.035f * speedF;
        if (state == (int)PlayerState::SPINNING) {
            float r = stage / 4.0f;
            target += -0.10f * r + sinf(m_DeformTime * 1.1f) * 0.02f * r;
        }

        // ばね・ダンパー（フレーム単位）
        m_StretchVel += (target - m_Stretch) * 0.20f;
        m_StretchVel *= 0.78f;
        m_Stretch += m_StretchVel;
        m_Stretch = (std::max)(-0.40f, (std::min)(0.40f, m_Stretch));
    }

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
// チャージ段階ごとの発光色（0=通常スピンのゴールド）
static XMFLOAT3 ChargeStageColor(int stage)
{
    return AuraEffect::StageColor(stage);
}

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

    // カットシーン中でなく、かつジャスト回避（Warp Slash突撃可能）中、またはタックル有効時のみロックオンサイトを描画
    if (!Manager::IsCutsceneActive()) {
        bool canWarpSlash = Manager::IsSlowMotionActive() && m_Owner->CanWarpSlash() && m_Owner->GetWarpSlashCount() < 3;
        bool canTackle    = m_Owner->GetTackleTimer() > 0;
        if ((canWarpSlash || canTackle) && m_Owner->GetLockOnTarget()) {
            DrawLockOnReticle(m_Owner->GetLockOnTarget());
        }
    }

    // 正面方向を明示するフェイスパーツ（両目）を描画
    DrawEyes();
}

// =================================================================
// 正面方向を明示するフェイスパーツ（両目）の描画
// =================================================================
void PlayerVisual::DrawEyes()
{
    if (!m_Owner || !m_Owner->GetRenderComponent().visible) return;

    XMFLOAT3 pPos = m_Owner->GetPosition();
    XMFLOAT3 pRot = m_Owner->GetRotation();
    XMFLOAT3 pScale = GetDeformScale();         // 伸縮演出後のスケール（目・クレストも一緒に伸縮）
    XMFLOAT3 pOff   = GetDeformOffset();

    // プレイヤーの本体姿勢マトリクス
    XMMATRIX parentMatrix = XMMatrixScaling(pScale.x, pScale.y, pScale.z) *
                            XMMatrixRotationRollPitchYaw(pRot.x, pRot.y, pRot.z) *
                            XMMatrixTranslation(pPos.x + pOff.x, pPos.y + pOff.y, pPos.z + pOff.z);

    Renderer::SetupCubeDraw();

    // 目の外枠マテリアル
    MATERIAL eyeBaseMat;
    ZeroMemory(&eyeBaseMat, sizeof(eyeBaseMat));
    eyeBaseMat.Diffuse       = XMFLOAT4(0.05f, 0.05f, 0.08f, 1.0f);
    eyeBaseMat.Ambient       = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    eyeBaseMat.Specular      = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    eyeBaseMat.Emission      = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    eyeBaseMat.Shininess     = 0.0f;
    eyeBaseMat.TextureEnable = FALSE;
    eyeBaseMat.RimPower      = 0.0f;

    // 光る瞳マテリアル
    MATERIAL pupilMat;
    ZeroMemory(&pupilMat, sizeof(pupilMat));
    pupilMat.Diffuse       = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    pupilMat.Ambient       = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    pupilMat.Specular      = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    pupilMat.Shininess     = 0.0f;
    pupilMat.TextureEnable = FALSE;
    pupilMat.RimPower      = 0.0f;

    if (m_Owner->GetDamageTimer() > 0) {
        // 被弾時：赤く点滅発光
        pupilMat.Emission = XMFLOAT4(2.5f, 0.3f, 0.3f, 1.0f);
    } else if (m_Owner->GetState() == PlayerState::SPINNING) {
        // スピン時：ゴールド発光
        XMFLOAT3 sc = ChargeStageColor(m_Owner->GetCombatModule() ? m_Owner->GetCombatModule()->GetChargeStage() : 0);
        pupilMat.Emission = XMFLOAT4(sc.x, sc.y, sc.z, 1.0f);
    } else {
        // 通常時：鮮やかなネオンシアン発光（正面方向が遠目からでも明瞭に視認可能）
        pupilMat.Emission = XMFLOAT4(0.4f, 2.0f, 2.5f, 1.0f);
    }

    // 1. 目の外枠
    XMMATRIX leftEyeLocal = XMMatrixScaling(0.12f, 0.22f, 0.05f) * XMMatrixTranslation(-0.20f, 0.12f, 0.51f);
    Renderer::SetWorldMatrix(leftEyeLocal * parentMatrix);
    Renderer::SetMaterial(eyeBaseMat);
    Renderer::GetDeviceContext()->Draw(36, 0);

    XMMATRIX rightEyeLocal = XMMatrixScaling(0.12f, 0.22f, 0.05f) * XMMatrixTranslation(0.20f, 0.12f, 0.51f);
    Renderer::SetWorldMatrix(rightEyeLocal * parentMatrix);
    Renderer::GetDeviceContext()->Draw(36, 0);

    // 2. 光る瞳
    XMMATRIX leftPupilLocal = XMMatrixScaling(0.08f, 0.14f, 0.06f) * XMMatrixTranslation(-0.20f, 0.14f, 0.52f);
    Renderer::SetWorldMatrix(leftPupilLocal * parentMatrix);
    Renderer::SetMaterial(pupilMat);
    Renderer::GetDeviceContext()->Draw(36, 0);

    XMMATRIX rightPupilLocal = XMMatrixScaling(0.08f, 0.14f, 0.06f) * XMMatrixTranslation(0.20f, 0.14f, 0.52f);
    Renderer::SetWorldMatrix(rightPupilLocal * parentMatrix);
    Renderer::GetDeviceContext()->Draw(36, 0);

    // 3. 頭頂部の前方インジケーター（見下ろし・斜め後方カメラからでも前方向が一目で視認できるサイバークレスト）
    XMMATRIX crestCenter = XMMatrixScaling(0.08f, 0.04f, 0.36f) * XMMatrixTranslation(0.0f, 0.51f, 0.22f);
    Renderer::SetWorldMatrix(crestCenter * parentMatrix);
    Renderer::SetMaterial(pupilMat);
    Renderer::GetDeviceContext()->Draw(36, 0);

    // 前方を強調するシェブロン翼（左・右）
    XMMATRIX crestLeft = XMMatrixScaling(0.06f, 0.04f, 0.16f) * 
                         XMMatrixRotationY(XM_PIDIV4) * 
                         XMMatrixTranslation(-0.08f, 0.51f, 0.30f);
    Renderer::SetWorldMatrix(crestLeft * parentMatrix);
    Renderer::GetDeviceContext()->Draw(36, 0);

    XMMATRIX crestRight = XMMatrixScaling(0.06f, 0.04f, 0.16f) * 
                          XMMatrixRotationY(-XM_PIDIV4) * 
                          XMMatrixTranslation(0.08f, 0.51f, 0.30f);
    Renderer::SetWorldMatrix(crestRight * parentMatrix);
    Renderer::GetDeviceContext()->Draw(36, 0);
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

// =================================================================
// ロックオン対象専用のターゲットサイト（枠・ポインター・エイムライン）描画
// =================================================================
void PlayerVisual::DrawLockOnReticle(Enemy* target)
{
    if (Manager::IsCutsceneActive() || !target || target->IsDestroy() || target->GetEnemyState() == EnemyState::DEFEATED) {
        m_Owner->SetLockOnTarget(nullptr);
        return;
    }

    bool isBoss = (target->GetObjectType() == ObjectType::Boss);
    XMFLOAT3 tPos = target->GetPosition();
    float radius = target->GetRadius();

    // ターゲットの中心高さ
    float tCenterY = tPos.y + (isBoss ? radius * 0.8f : radius * 0.5f);
    XMFLOAT3 tCenter = XMFLOAT3(tPos.x, tCenterY, tPos.z);

    // 発光カラー（通常敵: 高輝度シアン、ボス: 黄金アンバー）
    XMFLOAT4 reticleColor = isBoss ? XMFLOAT4(3.8f, 2.6f, 0.3f, 1.0f) : XMFLOAT4(0.0f, 3.4f, 3.8f, 1.0f);

    // マテリアル設定（高輝度エミッション）
    MATERIAL mat;
    ZeroMemory(&mat, sizeof(mat));
    mat.Diffuse       = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    mat.Ambient       = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    mat.Specular      = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    mat.Emission      = reticleColor;
    mat.Shininess     = 0.0f;
    mat.TextureEnable = FALSE;
    mat.RimPower      = 0.0f;
    Renderer::SetMaterial(mat);
    Renderer::SetupCubeDraw();

    // カメラの姿勢に合わせてビルボード化
    float camPitch = g_Camera ? g_Camera->GetAngleX() : 0.0f;
    float camYaw   = g_Camera ? g_Camera->GetAngleY() : 0.0f;
    XMMATRIX billboardRot = XMMatrixRotationRollPitchYaw(camPitch, camYaw, 0.0f);
    XMMATRIX targetTrans  = XMMatrixTranslation(tCenter.x, tCenter.y, tCenter.z);

    // パルスによる枠サイズの拡縮
    int timer = m_Owner->GetMarkerTimer();
    float pulse = sinf((float)timer * 0.2f) * 0.04f;
    float boxHalf = radius * (1.30f + pulse);
    float thickness = 0.065f; // 見失わないように太さを大幅アップ

    // ヘルパー: ビルボード基準で四角形パーツを描画
    auto DrawPart = [&](float localX, float localY, float scaleX, float scaleY) {
        XMMATRIX partWorld = XMMatrixScaling(scaleX, scaleY, thickness) *
                             XMMatrixTranslation(localX, localY, 0.0f) *
                             billboardRot *
                             targetTrans;
        Renderer::SetWorldMatrix(partWorld);
        Renderer::GetDeviceContext()->Draw(36, 0);
    };

    // --- 1. 完全な外枠メインフレーム（4辺全体を結ぶ太い四角形） ---
    // 上辺
    DrawPart(0.0f, boxHalf, boxHalf * 2.0f + thickness, thickness);
    // 下辺
    DrawPart(0.0f, -boxHalf, boxHalf * 2.0f + thickness, thickness);
    // 左辺
    DrawPart(-boxHalf, 0.0f, thickness, boxHalf * 2.0f);
    // 右辺
    DrawPart(boxHalf, 0.0f, thickness, boxHalf * 2.0f);

    // --- 2. 4隅の強化HUDブラケット（立体アクセント） ---
    float bracketThick = thickness * 1.5f;
    float armLen = boxHalf * 0.38f;

    // 右上
    DrawPart(boxHalf - armLen * 0.5f, boxHalf, armLen, bracketThick);
    DrawPart(boxHalf, boxHalf - armLen * 0.5f, bracketThick, armLen);
    // 左上
    DrawPart(-boxHalf + armLen * 0.5f, boxHalf, armLen, bracketThick);
    DrawPart(-boxHalf, boxHalf - armLen * 0.5f, bracketThick, armLen);
    // 右下
    DrawPart(boxHalf - armLen * 0.5f, -boxHalf, armLen, bracketThick);
    DrawPart(boxHalf, -boxHalf + armLen * 0.5f, bracketThick, armLen);
    // 左下
    DrawPart(-boxHalf + armLen * 0.5f, -boxHalf, armLen, bracketThick);
    DrawPart(-boxHalf, -boxHalf + armLen * 0.5f, bracketThick, armLen);

    // --- 3. 中央の回転クロスヘア（エイム十字） ---
    float crossAngle = (float)timer * 0.06f;
    float crossArm = boxHalf * 0.26f;
    XMMATRIX crossRot = XMMatrixRotationZ(crossAngle);

    XMMATRIX crossHWorld = XMMatrixScaling(crossArm * 2.0f, thickness * 0.85f, thickness * 0.85f) *
                           crossRot * billboardRot * targetTrans;
    Renderer::SetWorldMatrix(crossHWorld);
    Renderer::GetDeviceContext()->Draw(36, 0);

    XMMATRIX crossVWorld = XMMatrixScaling(thickness * 0.85f, crossArm * 2.0f, thickness * 0.85f) *
                           crossRot * billboardRot * targetTrans;
    Renderer::SetWorldMatrix(crossVWorld);
    Renderer::GetDeviceContext()->Draw(36, 0);

    // --- 4. 頭上の▼ターゲットポインター ---
    float headHoverY = tPos.y + radius * 2.0f + 0.40f + sinf((float)timer * 0.15f) * 0.08f;
    XMMATRIX pointerWorld = XMMatrixScaling(0.18f, 0.26f, 0.18f) *
                           XMMatrixRotationRollPitchYaw(XM_PI, (float)timer * 0.08f, 0.0f) *
                           XMMatrixTranslation(tPos.x, headHoverY, tPos.z);
    Renderer::SetWorldMatrix(pointerWorld);
    Renderer::GetDeviceContext()->Draw(36, 0);
}
