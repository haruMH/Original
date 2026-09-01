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

using namespace DirectX;

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

            m_Owner->SetScale(XMFLOAT3(1.8f, 0.5f, 1.8f));

            m_ScaleVelocityY = -0.1f;
            m_ScaleVelocityX = 0.08f;
            m_ScaleVelocityZ = 0.08f;
            
            m_Owner->SetInvincibleTimer(Constants::Player::DASH_INVINCIBLE_TIME);
        }
    }

    // Dash update
    if (m_DashTimer > 0) {
        m_DashTimer--;
        
        float dashSpeed = Constants::Player::DASH_SPEED;
        XMFLOAT3 nextPos = m_Owner->GetPosition();
        nextPos.x += m_DashDirection.x * dashSpeed;
        nextPos.z += m_DashDirection.z * dashSpeed;

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
    XMFLOAT3 curPos = m_Owner->GetPosition();
    if (curPos.y > -0.5f || m_VelocityY != 0.0f) {
        m_VelocityY -= Constants::Player::GRAVITY;
        curPos.y += m_VelocityY;
        m_Owner->SetPosition(curPos);

        XMFLOAT3 curScale = m_Owner->GetScale();
        if (m_VelocityY > 0.01f) {
            curScale.y += (1.0f + m_VelocityY * 1.5f - curScale.y) * 0.2f;
            curScale.x += (1.0f - m_VelocityY * 0.75f - curScale.x) * 0.2f;
            curScale.z += (1.0f - m_VelocityY * 0.75f - curScale.z) * 0.2f;
            m_Owner->SetScale(curScale);
        }

        curPos = m_Owner->GetPosition();
        if (curPos.y <= -0.5f) {
            curPos.y = -0.5f;
            m_Owner->SetPosition(curPos);
            m_VelocityY = 0.0f;
            m_IsJumping = false;
            m_JumpCount = 0;

            m_Owner->SetScale(XMFLOAT3(1.3f, 0.5f, 1.3f));

            m_ScaleVelocityY = -0.1f;
            m_ScaleVelocityX = 0.05f;
            m_ScaleVelocityZ = 0.05f;
        }
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
    float speed = sqrtf(actualVel.x * actualVel.x + actualVel.y * actualVel.y + actualVel.z * actualVel.z);
    float dt = 1.0f / 60.0f;

    if (speed > 0.005f) {
        m_MoveAnimation += speed * dt * 30.0f;
        XMFLOAT3 curScale = m_Owner->GetScale();
        curScale.y += sinf(m_MoveAnimation * 3.0f) * 0.03f;
        curScale.x -= sinf(m_MoveAnimation * 3.0f) * 0.015f;
        curScale.z -= sinf(m_MoveAnimation * 3.0f) * 0.015f;
        m_Owner->SetScale(curScale);
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
        XMFLOAT3 nextPos = m_Owner->GetPosition();
        nextPos.x += moveDir.x * speed;
        nextPos.z += moveDir.z * speed;

        if (!Collision::CheckAABBCollision(m_Owner, nextPos, Manager::GetGameObjectList())) {
            m_Owner->SetPosition(nextPos);
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
