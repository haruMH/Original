#include "player_movement.h"
#include "player.h"
#include "player_controller.h"
#include "math_helper.h"
#include "input.h"
#include "collision.h"
#include "manager.h"
#include "camera.h"
#include "game_constants.h"
#include "enemy.h"
#include "boss_enemy.h"
#include "sound_manager.h"
#include "dust_system.h"
#include <algorithm>

using namespace DirectX;

namespace {
    // 傾き（Pitch, Yaw, Roll）およびスケールに応じた、底面の全角が地面（GROUND_Y）に接するプレイヤー中心高さを計算
    float CalculateGroundCenterY(float pitch, float yaw, float roll, const XMFLOAT3& scale, float groundY = -1.0f)
    {
        // 底面4頂点のローカルオフセット（直方体の下半分）
        float hx = 0.5f * scale.x;
        float hy = 0.5f * scale.y;
        float hz = 0.5f * scale.z;

        XMVECTOR corners[4] = {
            XMVectorSet(-hx, -hy, -hz, 0.0f),
            XMVectorSet( hx, -hy, -hz, 0.0f),
            XMVectorSet(-hx, -hy,  hz, 0.0f),
            XMVectorSet( hx, -hy,  hz, 0.0f),
        };

        // 本体の回転行列
        XMMATRIX rotM = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

        float minY = 0.0f;
        for (int i = 0; i < 4; ++i) {
            XMVECTOR rotated = XMVector3Transform(corners[i], rotM);
            float y = XMVectorGetY(rotated);
            if (i == 0 || y < minY) {
                minY = y;
            }
        }

        // 中心高さ = groundY - minY + 微小マージン（床面とのZファイティング防止・底面輪郭線の完全露出用）
        const float MARGIN_Y = 0.02f;
        return groundY - minY + MARGIN_Y;
    }
}

// =================================================================
// Constructor / Destructor
// =================================================================
PlayerMovement::PlayerMovement(Player* owner)
    : m_Owner(owner)
{
}

PlayerMovement::~PlayerMovement()
{
}

// =================================================================
// Initialize
// =================================================================
void PlayerMovement::Init()
{
    m_VelocityY = 0.0f;
    m_IsJumping = false;
    m_JumpCount = 0;
    
    m_ScaleVelocityX = 0.0f;
    m_ScaleVelocityY = 0.0f;
    m_ScaleVelocityZ = 0.0f;
    
    m_DashTimer = 0;
    m_DashCooldown = 0;
    m_DashDirection = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_IsDashing = false;
    m_DashGhosts.clear();
    
    m_KnockbackVelocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_MoveAnimation = 0.0f;

    m_CurrentPitch = 0.0f;
    m_CurrentRoll = 0.0f;
    m_PitchVelocity = 0.0f;
    m_RollVelocity = 0.0f;
    m_RunDustTimer = 0;
}

// =================================================================
// Update Frame
// =================================================================
void PlayerMovement::Update()
{
    XMFLOAT3 oldPos = m_Owner->GetPosition();
    float oldRotY = m_Owner->GetRotation().y;

    // Update dash cooldown
    if (m_DashCooldown > 0) {
        m_DashCooldown--;
    }

    // Update ghosts
    UpdateGhosts();

    // Trigger dash check
    if (m_Owner->GetDamageTimer() <= 0 && m_DashTimer <= 0 && m_DashCooldown <= 0) {
        if (PlayerController::IsDashAction()) {
            float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;
            XMFLOAT3 moveDir = PlayerController::GetMoveDirection(camYaw);
            
            if (MathHelper::LengthSq(moveDir) < 0.001f) {
                moveDir = XMFLOAT3(sinf(m_Owner->GetRotation().y), 0.0f, cosf(m_Owner->GetRotation().y));
            }
            
            float len = sqrtf(moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z);
            if (len > 0.001f) {
                moveDir.x /= len;
                moveDir.y /= len;
                moveDir.z /= len;
            }

            m_DashDirection = moveDir;
            m_DashTimer = Constants::Player::DASH_DURATION;
            m_DashCooldown = Constants::Player::DASH_COOLDOWN;
            m_IsDashing = true;

            SoundManager::PlaySE(SoundID::SE_DODGE);

            m_Owner->SetScale(XMFLOAT3(1.8f, 0.5f, 1.8f));

            m_ScaleVelocityY = -0.1f;
            m_ScaleVelocityX = 0.08f;
            m_ScaleVelocityZ = 0.08f;
            
            m_Owner->SetInvincibleTimer(Constants::Player::DASH_INVINCIBLE_TIME);

            // ダッシュ噴射土煙
            DustSystem::AddDashDust(m_Owner->GetPosition(), m_DashDirection);

            // ダッシュ時のスピード感演出（ラジアルブラー＆色収差）
            Manager::TriggerRadialBlur(0.35f);
            Manager::TriggerChromaticAberration(0.20f);
        }
    }

    // Dash update
    if (m_DashTimer > 0) {
        m_DashTimer--;
        
        float dashSpeed = Constants::Player::DASH_SPEED;
        XMFLOAT3 nextPos = m_Owner->GetPosition();
        nextPos.x += m_DashDirection.x * dashSpeed;
        nextPos.z += m_DashDirection.z * dashSpeed;

        // ダッシュ中も足元から勢いよく煙を引く
        if (m_DashTimer % 2 == 0) {
            DustSystem::AddRunDust(m_Owner->GetPosition(), m_DashDirection);
        }

        if (m_Owner->GetTackleTimer() > 0) {
            Enemy* hitTarget = nullptr;
            
            if (m_Owner->GetLockOnTarget() && !m_Owner->GetLockOnTarget()->IsDestroy()) {
                XMFLOAT3 diff = m_Owner->GetPosition() - m_Owner->GetLockOnTarget()->GetPosition();
                diff.y = 0.0f;
                float dist = MathHelper::Length(diff);
                if (dist < 1.0f + m_Owner->GetLockOnTarget()->GetRadius()) {
                    hitTarget = m_Owner->GetLockOnTarget();
                }
            }
            
            if (!hitTarget) {
                for (GameObject* obj : Manager::GetGameObjectList()) {
                    if (!obj || obj->IsDestroy() || obj == m_Owner) continue;
                    if (obj->GetObjectType() != ObjectType::Enemy && obj->GetObjectType() != ObjectType::Boss) continue;
                    Enemy* e = static_cast<Enemy*>(obj);
                    if (e->GetEnemyState() == EnemyState::DEFEATED) continue;
                    
                    XMFLOAT3 diff = m_Owner->GetPosition() - e->GetPosition();
                    diff.y = 0.0f;
                    float dist = MathHelper::Length(diff);
                    if (dist < 1.0f + e->GetRadius()) {
                        hitTarget = e;
                        break;
                    }
                }
            }
            
            if (hitTarget) {
                if (hitTarget->GetObjectType() == ObjectType::Boss) {
                    BossEnemy* bossE = static_cast<BossEnemy*>(hitTarget);
                    HitInfo hitInfo;
                    hitInfo.damage = 3;
                    hitInfo.hitSourcePos = m_Owner->GetPosition();
                    bossE->OnHit(hitInfo);
                } else {
                    XMFLOAT3 pushDir = hitTarget->GetPosition() - m_Owner->GetPosition();
                    pushDir.y = 0.0f;
                    float len = MathHelper::Length(pushDir);
                    if (len > 0.001f) { pushDir.x /= len; pushDir.z /= len; }
                    else { pushDir = XMFLOAT3(0.0f, 0.0f, 1.0f); }
                    
                    XMFLOAT3 pushVel = XMFLOAT3(pushDir.x * 1.8f, 0.6f, pushDir.z * 1.8f);
                    hitTarget->SetVelocity(pushVel);
                    hitTarget->SetEnemyState(EnemyState::BLOWN_AWAY);
                    hitTarget->Defeat(0.0f, 1.0f, 2.0f);
                }
                
                Manager::AddHitStop(10);
                if (g_Camera) {
                    g_Camera->Shake(0.25f, 10);
                }
                
                m_Owner->SetTackleTimer(0);
            }
        }

        if (!Collision::CheckAABBCollision(m_Owner, nextPos, Manager::GetGameObjectList())) {
            m_Owner->SetPosition(nextPos);
        }

        if (m_DashTimer % 3 == 0) {
            DashGhost ghost;
            ghost.Position = m_Owner->GetPosition();
            ghost.Rotation = m_Owner->GetRotation();
            ghost.Scale = m_Owner->GetScale();
            ghost.Alpha = 0.8f;
            m_DashGhosts.push_back(ghost);
        }

        if (m_DashTimer == 0) {
            m_IsDashing = false;
            m_Owner->SetScale(XMFLOAT3(0.7f, 1.5f, 0.7f));
            m_ScaleVelocityY = 0.08f;
            m_ScaleVelocityX = -0.04f;
            m_ScaleVelocityZ = -0.04f;
        }

        UpdateSpringPhysics();
        // ダッシュ中も地上であれば最新スケールに合わせて接地高さを維持
        if (!m_IsJumping && m_VelocityY == 0.0f) {
            XMFLOAT3 rot = m_Owner->GetRotation();
            float exactGroundY = CalculateGroundCenterY(rot.x, rot.y, rot.z, m_Owner->GetScale(), -1.0f);
            XMFLOAT3 p = m_Owner->GetPosition();
            p.y = exactGroundY;
            m_Owner->SetPosition(p);
        }
        return;
    }

    // Damage knockback update
    if (m_Owner->GetDamageTimer() > 0) {
        XMFLOAT3 nextPos = m_Owner->GetPosition();
        nextPos.x += m_KnockbackVelocity.x;
        nextPos.z += m_KnockbackVelocity.z;
        
        m_KnockbackVelocity.x *= 0.9f;
        m_KnockbackVelocity.z *= 0.9f;
        
        if (!Collision::CheckAABBCollision(m_Owner, nextPos, Manager::GetGameObjectList())) {
            m_Owner->SetPosition(nextPos);
        }
    }

    // Normal movement & jump update
    if (m_Owner->GetDamageTimer() <= 0) {
        if (Input::GetKeyTrigger(0x20) && m_JumpCount < Constants::Player::MAX_JUMP_COUNT) {
            m_VelocityY = Constants::Player::JUMP_VELOCITY;
            m_IsJumping = true;
            m_JumpCount++;

            m_Owner->SetScale(XMFLOAT3(1.3f, 0.6f, 1.3f));

            m_ScaleVelocityY = 0.1f;
            m_ScaleVelocityX = -0.05f;
            m_ScaleVelocityZ = -0.05f;
        }

        UpdateNormalMovement();
    }

    // Gravity & Ground checks
    const float GROUND_Y = -1.0f; // 地面の高さ
    XMFLOAT3 curScale = m_Owner->GetScale();
    XMFLOAT3 curRot = m_Owner->GetRotation();
    // 傾き（Pitch, Yaw, Roll）およびスケールに応じた厳密な接地中心高さを計算
    float targetGroundY = CalculateGroundCenterY(m_CurrentPitch, curRot.y, m_CurrentRoll, curScale, GROUND_Y);

    XMFLOAT3 curPos = m_Owner->GetPosition();
    if (curPos.y > targetGroundY || m_VelocityY != 0.0f) {
        m_VelocityY -= Constants::Player::GRAVITY;
        curPos.y += m_VelocityY;
        m_Owner->SetPosition(curPos);

        if (m_VelocityY > 0.01f) {
            curScale.y += (1.0f + m_VelocityY * 1.5f - curScale.y) * 0.2f;
            curScale.x += (1.0f - m_VelocityY * 0.75f - curScale.x) * 0.2f;
            curScale.z += (1.0f - m_VelocityY * 0.75f - curScale.z) * 0.2f;
            m_Owner->SetScale(curScale);
        }

        curPos = m_Owner->GetPosition();
        if (curPos.y <= targetGroundY) {
            bool wasInAir = (m_VelocityY < -0.05f || m_IsJumping);
            curPos.y = targetGroundY;
            m_Owner->SetPosition(curPos);
            m_VelocityY = 0.0f;
            m_IsJumping = false;
            m_JumpCount = 0;

            m_Owner->SetScale(XMFLOAT3(1.3f, 0.5f, 1.3f));

            m_ScaleVelocityY = -0.1f;
            m_ScaleVelocityX = 0.05f;
            m_ScaleVelocityZ = 0.05f;

            // 着地時の砂埃エフェクト（地面高さで発生）
            if (wasInAir) {
                DustSystem::AddLandDust(curPos, 8);
            }
        }
    } else {
        // 地上移動中も傾きやスケール変形による沈み込みを解消し、常に地面の上に接地させる
        curPos.y = targetGroundY;
        m_Owner->SetPosition(curPos);
    }

    // Angular velocity
    float diff = m_Owner->GetRotation().y - oldRotY;
    while (diff < -XM_PI) diff += XM_2PI;
    while (diff > XM_PI)  diff -= XM_2PI;
    m_Owner->SetAngularVelocity(diff);

    // Spring physics update
    UpdateSpringPhysics();

    curPos = m_Owner->GetPosition();
    XMFLOAT3 actualVel = XMFLOAT3(curPos.x - oldPos.x, curPos.y - oldPos.y, curPos.z - oldPos.z);
    float horizontalSpeed = sqrtf(actualVel.x * actualVel.x + actualVel.z * actualVel.z);
    float dt = 1.0f / 60.0f;

    if (horizontalSpeed > 0.005f) {
        m_MoveAnimation += horizontalSpeed * dt * 30.0f;
        XMFLOAT3 curScale = m_Owner->GetScale();
        curScale.y += sinf(m_MoveAnimation * 3.0f) * 0.03f;
        curScale.x -= sinf(m_MoveAnimation * 3.0f) * 0.015f;
        curScale.z -= sinf(m_MoveAnimation * 3.0f) * 0.015f;
        m_Owner->SetScale(curScale);

        // 走行中の足元砂埃（接地中かつ通常走行時）
        bool isGrounded = (!m_IsJumping && m_VelocityY == 0.0f);
        if (isGrounded && horizontalSpeed > 0.02f && m_Owner->GetState() != PlayerState::SPINNING) {
            m_RunDustTimer++;
            if (m_RunDustTimer >= 4) { // 約0.06秒ごとにリズミカルにポコッと発生
                m_RunDustTimer = 0;
                DustSystem::AddRunDust(curPos, actualVel);
            }
        }
    } else {
        m_RunDustTimer = 0;
    }

    // ロロロロ風の体幹傾き（Tilt / Lean）計算
    float targetPitch = 0.0f;
    float targetRoll  = 0.0f;

    if (m_Owner->GetState() != PlayerState::SPINNING) {
        if (horizontalSpeed > 0.01f) {
            float yaw = m_Owner->GetRotation().y;
            float cosY = cosf(yaw);
            float sinY = sinf(yaw);

            // ローカル前方・横方向の速度成分
            float forwardComp = (actualVel.x * sinY + actualVel.z * cosY) / (Constants::Player::MOVE_SPEED + 0.001f);
            float rightComp   = (actualVel.x * cosY - actualVel.z * sinY) / (Constants::Player::MOVE_SPEED + 0.001f);

            forwardComp = (std::max)(-1.2f, (std::min)(1.2f, forwardComp));
            rightComp   = (std::max)(-1.2f, (std::min)(1.2f, rightComp));

            // 前傾姿勢（前進時に前へ傾き、歩行リズムで微小に上下振動）
            float bob = sinf(m_MoveAnimation * 3.0f) * 0.02f;
            targetPitch = forwardComp * Constants::Player::MOVE_TILT_PITCH + bob;

            // バンク（左右移動 A/D 時の適度な傾き: 過度な傾きによる画面ブレを防止）
            targetRoll = -rightComp * Constants::Player::MOVE_TILT_ROLL;
        }

        // スプリング補間（移動時は滑らかに傾き、静止時は安定して立ち直る）
        float springK = Constants::Player::MOVE_TILT_SPRING_K;
        float damping = Constants::Player::MOVE_TILT_DAMPING;

        float diffPitch = targetPitch - m_CurrentPitch;
        m_PitchVelocity = (m_PitchVelocity + diffPitch * springK) * damping;
        m_CurrentPitch += m_PitchVelocity;

        float diffRoll = targetRoll - m_CurrentRoll;
        m_RollVelocity = (m_RollVelocity + diffRoll * springK) * damping;
        m_CurrentRoll += m_RollVelocity;

        XMFLOAT3 rot = m_Owner->GetRotation();
        rot.x = m_CurrentPitch;
        rot.z = m_CurrentRoll;
        m_Owner->SetRotation(rot);
    } else {
        m_CurrentPitch *= 0.8f;
        m_CurrentRoll  *= 0.8f;
        m_PitchVelocity = 0.0f;
        m_RollVelocity  = 0.0f;
        XMFLOAT3 rot = m_Owner->GetRotation();
        rot.x = m_CurrentPitch;
        rot.z = m_CurrentRoll;
        m_Owner->SetRotation(rot);
    }

    // 傾き・スケール更新後の最新姿勢に基づいて、接地中であれば地面の上に即座に再密着させる
    if (!m_IsJumping && m_VelocityY == 0.0f) {
        XMFLOAT3 rot = m_Owner->GetRotation();
        float exactGroundY = CalculateGroundCenterY(rot.x, rot.y, rot.z, m_Owner->GetScale(), -1.0f);
        XMFLOAT3 p = m_Owner->GetPosition();
        p.y = exactGroundY;
        m_Owner->SetPosition(p);
    }

    // Resolve push out of collision
    Collision::ResolveAABBCollision(m_Owner, Manager::GetGameObjectList());
}

// =================================================================
// Normal Movement
// =================================================================
void PlayerMovement::UpdateNormalMovement()
{
    float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;
    XMFLOAT3 moveDir = PlayerController::GetMoveDirection(camYaw);

    if (MathHelper::LengthSq(moveDir) > 0.001f) {
        float speed = Constants::Player::MOVE_SPEED;
        XMFLOAT3 curPos = m_Owner->GetPosition();

        // 1. まず進行方向そのまま（斜め含む）の移動を試みる
        XMFLOAT3 nextPosXZ = curPos;
        nextPosXZ.x += moveDir.x * speed;
        nextPosXZ.z += moveDir.z * speed;

        if (!Collision::CheckAABBCollision(m_Owner, nextPosXZ, Manager::GetGameObjectList())) {
            m_Owner->SetPosition(nextPosXZ);
        } else {
            // 2. 衝突した場合はX軸・Z軸それぞれ単独のスライド移動を試み、引っかかり・スタックを防ぐ
            XMFLOAT3 nextPosX = curPos;
            nextPosX.x += moveDir.x * speed;
            if (!Collision::CheckAABBCollision(m_Owner, nextPosX, Manager::GetGameObjectList())) {
                curPos.x = nextPosX.x;
            }

            XMFLOAT3 nextPosZ = curPos;
            nextPosZ.z += moveDir.z * speed;
            if (!Collision::CheckAABBCollision(m_Owner, nextPosZ, Manager::GetGameObjectList())) {
                curPos.z = nextPosZ.z;
            }

            m_Owner->SetPosition(curPos);
        }

        if (m_Owner->GetState() != PlayerState::SPINNING) {
            float targetYaw = atan2f(moveDir.x, moveDir.z);
            XMFLOAT3 rot = m_Owner->GetRotation();
            rot.y = MathHelper::LerpAngle(rot.y, targetYaw, 0.15f);
            m_Owner->SetRotation(rot);
        }
    }
}

// =================================================================
// Elastic Spring Calculation
// =================================================================
void PlayerMovement::UpdateSpringPhysics()
{
    float springK = Constants::Player::SPRING_K;
    float damping = Constants::Player::DAMPING;

    XMFLOAT3 scale = m_Owner->GetScale();

    // X
    float forceX = (1.0f - scale.x) * springK;
    m_ScaleVelocityX += forceX;
    m_ScaleVelocityX *= damping;
    scale.x += m_ScaleVelocityX;

    // Y
    float forceY = (1.0f - scale.y) * springK;
    m_ScaleVelocityY += forceY;
    m_ScaleVelocityY *= damping;
    scale.y += m_ScaleVelocityY;

    // Z
    float forceZ = (1.0f - scale.z) * springK;
    m_ScaleVelocityZ += forceZ;
    m_ScaleVelocityZ *= damping;
    scale.z += m_ScaleVelocityZ;

    m_Owner->SetScale(scale);
}

// =================================================================
// Ghost Management
// =================================================================
void PlayerMovement::UpdateGhosts()
{
    for (auto it = m_DashGhosts.begin(); it != m_DashGhosts.end(); ) {
        it->Alpha -= 0.08f;
        if (it->Alpha <= 0.0f) {
            it = m_DashGhosts.erase(it);
        } else {
            it++;
        }
    }
}
