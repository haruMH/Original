#include "player_combat.h"
#include "player.h"
#include "player_movement.h"
#include "math_helper.h"
#include "player_controller.h"
#include "collision.h"
#include "manager.h"
#include "camera.h"
#include "game_constants.h"
#include "enemy.h"
#include "boss_enemy.h"
#include "enemy_bullet.h"
#include "shockwave.h"

using namespace DirectX;

// =================================================================
// Constructor / Destructor
// =================================================================
PlayerCombat::PlayerCombat(Player* owner)
    : m_Owner(owner)
{
}

PlayerCombat::~PlayerCombat()
{
}

// =================================================================
// Initialize
// =================================================================
void PlayerCombat::Init()
{
}

// =================================================================
// Update Frame
// =================================================================
void PlayerCombat::Update()
{
    // Update guard status
    if (m_Owner->GetDamageTimer() <= 0 && m_Owner->GetState() == PlayerState::IDLE && !m_Owner->GetGrabbedEnemy()) {
        if (PlayerController::IsGuardAction()) {
            m_Owner->IncrementGuardTimer();
        } else {
            m_Owner->SetGuardTimer(0);
        }
    } else {
        m_Owner->SetGuardTimer(0);
    }

    // Update Lockon
    if (Manager::IsSlowMotionActive()) {
        FindLockOnTarget();
    } else {
        m_Owner->SetLockOnTarget(nullptr);
        m_Owner->SetLockOnFrame(0);
        m_Owner->SetWarpSlashCount(0);
        m_Owner->SetCanWarpSlash(true);
    }

    m_Owner->IncrementMarkerTimer();

    // Check states if not stunned
    if (m_Owner->GetDamageTimer() <= 0) {
        switch (m_Owner->GetState()) {
        case PlayerState::IDLE:
            UpdateIdle();
            break;
        case PlayerState::GRABBED:
            UpdateGrabbed();
            break;
        case PlayerState::SPINNING:
            UpdateSpinning();
            break;
        }
    }
}

// =================================================================
// IDLE State Combat Update
// =================================================================
void PlayerCombat::UpdateIdle()
{
    // Tackle Warp Attack
    if (m_Owner->GetTackleTimer() > 0 && PlayerController::IsGrabOrThrowAction()) {
        Enemy* target = nullptr;
        
        if (m_Owner->GetLockOnTarget() && !m_Owner->GetLockOnTarget()->IsDestroy()) {
            target = m_Owner->GetLockOnTarget();
        } else {
            float nearestDist = 15.0f;
            for (GameObject* obj : Manager::GetGameObjectList()) {
                if (!obj || obj == m_Owner || obj->IsDestroy()) continue;
                if (obj->GetObjectType() != ObjectType::Enemy && obj->GetObjectType() != ObjectType::Boss) continue;
                Enemy* e = static_cast<Enemy*>(obj);
                if (e->GetEnemyState() == EnemyState::DEFEATED) continue;
                
                float dist = MathHelper::Length(e->GetPosition() - m_Owner->GetPosition());
                if (dist < nearestDist) {
                    nearestDist = dist;
                    target = e;
                }
            }
        }
        
        if (target) {
            XMFLOAT3 targetPos = target->GetPosition();
            XMFLOAT3 startPos = m_Owner->GetPosition();
            
            XMVECTOR vStart = XMLoadFloat3(&startPos);
            XMVECTOR vTarget = XMLoadFloat3(&targetPos);
            XMVECTOR vDiff = vTarget - vStart;
            vDiff = XMVectorSetY(vDiff, 0.0f);
            float dist = XMVectorGetX(XMVector3Length(vDiff));
            
            XMVECTOR vDir = (dist > 0.001f) ? (vDiff / dist) : XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
            
            float targetRadius = target->GetRadius();
            float warpDistance = targetRadius + 0.8f;
            
            XMVECTOR vWarpPos = vTarget - vDir * warpDistance;
            XMFLOAT3 warpPos;
            XMStoreFloat3(&warpPos, vWarpPos);
            warpPos.y = m_Owner->GetPosition().y;
            
            m_Owner->SetPosition(warpPos);
            XMFLOAT3 dir;
            XMStoreFloat3(&dir, vDir);
            
            XMFLOAT3 rot = m_Owner->GetRotation();
            rot.y = atan2f(dir.x, dir.z);
            m_Owner->SetRotation(rot);
            
            m_Owner->SetScale(XMFLOAT3(1.8f, 0.4f, 1.8f));
            if (m_Owner->GetMovementModule()) {
                m_Owner->GetMovementModule()->SetScaleVelocity(0.08f, -0.1f, 0.08f);
            }
            
            ShockwaveSystem::AddShockwave(warpPos, 5.0f, 0.0f, 2.5f, 4.0f, 20, 1.5f, 0);
            
            if (target->GetObjectType() == ObjectType::Boss) {
                BossEnemy* boss = static_cast<BossEnemy*>(target);
                HitInfo hitInfo;
                hitInfo.damage = 3;
                hitInfo.hitSourcePos = m_Owner->GetPosition();
                boss->OnHit(hitInfo);
            } else {
                XMFLOAT3 pushVel = XMFLOAT3(dir.x * 1.8f, 0.6f, dir.z * 1.8f);
                target->SetVelocity(pushVel);
                target->SetEnemyState(EnemyState::BLOWN_AWAY);
                target->Defeat(0.0f, 1.0f, 2.0f);
            }
            
            Manager::AddHitStop(12);
            if (g_Camera) {
                g_Camera->Shake(0.35f, 12);
            }
            
            m_Owner->SetTackleTimer(0);
            return;
        }
    }

    // Warp Slash Counter (during Witch Time / Slow Motion)
    if (Manager::IsSlowMotionActive() && m_Owner->CanWarpSlash() && m_Owner->GetLockOnTarget() && m_Owner->GetWarpSlashCount() < 3) {
        if (PlayerController::IsGrabOrThrowAction()) {
            bool hasGrabTarget = false;
            float grabRange = Constants::Player::GRAB_RANGE;
            for (GameObject* obj : Manager::GetGameObjectList()) {
                if (!obj || obj == m_Owner || obj->IsDestroy()) continue;
                if (obj->GetObjectType() != ObjectType::Enemy) continue;
                Enemy* e = static_cast<Enemy*>(obj);
                if (e->GetEnemyState() != EnemyState::NORMAL) continue;

                float dist = MathHelper::Length(e->GetPosition() - m_Owner->GetPosition());
                if (dist < grabRange) {
                    hasGrabTarget = true;
                    break;
                }
            }

            if (!hasGrabTarget) {
                m_Owner->IncrementWarpSlashCount();
                Enemy* target = m_Owner->GetLockOnTarget();
                XMFLOAT3 targetPos = target->GetPosition();
                XMFLOAT3 startPos = m_Owner->GetPosition();
            
                XMVECTOR vStart = XMLoadFloat3(&startPos);
                XMVECTOR vTarget = XMLoadFloat3(&targetPos);
                XMVECTOR vDiff = vTarget - vStart;
                vDiff = XMVectorSetY(vDiff, 0.0f);
                float dist = XMVectorGetX(XMVector3Length(vDiff));
                
                XMVECTOR vDir = (dist > 0.001f) ? (vDiff / dist) : XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
                
                float halfWidth = target->GetSize().x * target->GetScale().x * 0.5f;
                float warpDistance = halfWidth + 1.0f;
                
                XMVECTOR vWarpPos = vTarget - vDir * warpDistance;
                XMFLOAT3 warpPos;
                XMStoreFloat3(&warpPos, vWarpPos);
                warpPos.y = m_Owner->GetPosition().y;
                
                m_Owner->SetPosition(warpPos);
                m_Owner->SetScale(XMFLOAT3(1.8f, 0.4f, 1.8f));
                
                XMFLOAT3 dir;
                XMStoreFloat3(&dir, vDir);
                
                XMFLOAT3 rot = m_Owner->GetRotation();
                rot.y = atan2f(dir.x, dir.z);
                m_Owner->SetRotation(rot);
                
                XMFLOAT3 boltStart = startPos;
                XMFLOAT3 boltEnd = targetPos;
                boltStart.y += 0.3f;
                boltEnd.y += 0.3f;
                m_Owner->AddLightningEffect(boltStart, boltEnd);
                m_Owner->AddLightningEffect(XMFLOAT3(boltStart.x + 0.1f, boltStart.y, boltStart.z), XMFLOAT3(boltEnd.x + 0.1f, boltEnd.y, boltEnd.z));
                m_Owner->AddLightningEffect(XMFLOAT3(boltStart.x - 0.1f, boltStart.y, boltStart.z), XMFLOAT3(boltEnd.x - 0.1f, boltEnd.y, boltEnd.z));
                
                ShockwaveSystem::AddShockwave(warpPos, 4.0f, 0.0f, 2.5f, 4.0f, 20, 1.5f, 0);
                
                if (target->GetObjectType() == ObjectType::Boss) {
                    BossEnemy* boss = static_cast<BossEnemy*>(target);
                    HitInfo hitInfo;
                    hitInfo.damage = 4;
                    hitInfo.hitSourcePos = m_Owner->GetPosition();
                    boss->OnHit(hitInfo);
                } else {
                    XMFLOAT3 pushVel = XMFLOAT3(dir.x * 1.6f, 0.6f, dir.z * 1.6f);
                    target->SetVelocity(pushVel);
                    target->SetEnemyState(EnemyState::BLOWN_AWAY);
                    target->SetLightning(true);
                    target->Defeat(0.0f, 2.0f, 3.0f);
                }
                
                Manager::AddHitStop(15);
                if (g_Camera) {
                    g_Camera->Shake(0.45f, 15);
                }
                
                Manager::StartSlowMotion(90);
                return;
            }
        }
    }

    // Try grabbing a nearby enemy
    if (PlayerController::IsGrabOrThrowAction()) {
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
            m_Owner->SetGrabbedEnemy(nearest);
        }
    }
}

// =================================================================
// GRABBED State Update
// =================================================================
void PlayerCombat::UpdateGrabbed()
{
    if (m_Owner->GetGrabbedEnemy() && PlayerController::IsSpinToggleAction()) {
        m_Owner->SetState(PlayerState::SPINNING);
        m_Owner->SetCurrentSpinSpeed(Constants::Player::MIN_SPIN_SPEED);
    }

    if (m_Owner->GetGrabbedEnemy() && PlayerController::IsGrabOrThrowAction()) {
        Throw();
        return;
    }

    if (!m_Owner->GetGrabbedEnemy()) {
        m_Owner->SetState(PlayerState::IDLE);
    }
}

// =================================================================
// SPINNING State Update
// =================================================================
void PlayerCombat::UpdateSpinning()
{
    float targetSpinSpeed = Constants::Player::MAX_SPIN_SPEED;
    float currentSpeed = m_Owner->GetCurrentSpinSpeed();
    currentSpeed += (targetSpinSpeed - currentSpeed) * Constants::Player::SPIN_ACCELERATION;
    m_Owner->SetCurrentSpinSpeed(currentSpeed);

    XMFLOAT3 rot = m_Owner->GetRotation();
    rot.y += currentSpeed;
    if (rot.y > XM_2PI) rot.y -= XM_2PI;
    m_Owner->SetRotation(rot);

    if (g_Camera) {
        float dynamicShake = 0.01f + currentSpeed * 0.15f;
        g_Camera->Shake(dynamicShake, 2);
    }

    // Vacuum absorption effect
    if (m_Owner->HasVacuumItem()) {
        float vacuumRadius = 8.0f;
        float vacuumForce = 0.03f;

        for (GameObject* obj : Manager::GetGameObjectList()) {
            if (!obj || obj == m_Owner || obj == m_Owner->GetGrabbedEnemy()) continue;

            if (obj->GetObjectType() == ObjectType::Enemy) {
                Enemy* enemy = static_cast<Enemy*>(obj);
                if (enemy->GetEnemyState() == EnemyState::NORMAL || enemy->GetEnemyState() == EnemyState::CHASING) {
                    XMFLOAT3 enemyPos = enemy->GetPosition();
                    XMFLOAT3 diff = m_Owner->GetPosition() - enemyPos;
                    float distSq = MathHelper::LengthSq(diff);

                    if (distSq < vacuumRadius * vacuumRadius && distSq > 0.01f) {
                        float dist = sqrtf(distSq);
                        XMFLOAT3 dir = diff / dist;

                        float spinDirSign = (m_Owner->GetAngularVelocity() >= 0.0f) ? 1.0f : -1.0f;
                        XMFLOAT3 tangent = XMFLOAT3(-dir.z, 0.0f, dir.x) * spinDirSign * 0.3f;
                        XMFLOAT3 force = (dir + tangent) * vacuumForce;

                        XMFLOAT3 vel = enemy->GetVelocity();
                        vel.x += force.x;
                        vel.z += force.z;
                        enemy->SetVelocity(vel);

                        enemy->SetEnemyState(EnemyState::VACUUMED);
                    }
                }
            }
        }
    }

    if (PlayerController::IsGrabOrThrowAction()) {
        Throw();
        return;
    }

    if (PlayerController::IsSpinToggleAction()) {
        m_Owner->SetState(PlayerState::GRABBED);
        m_Owner->SetCurrentSpinSpeed(Constants::Player::MIN_SPIN_SPEED);
    }

    if (!m_Owner->GetGrabbedEnemy()) {
        m_Owner->SetState(PlayerState::IDLE);
        m_Owner->SetCurrentSpinSpeed(Constants::Player::MIN_SPIN_SPEED);
    }
}

// =================================================================
// Throw Object
// =================================================================
void PlayerCombat::Throw()
{
    if (m_Owner->GetGrabbedEnemy()) {
        float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;
        
        XMFLOAT3 rot = m_Owner->GetRotation();
        rot.y = camYaw;
        m_Owner->SetRotation(rot);

        XMFLOAT3 fwdF = XMFLOAT3(sinf(rot.y), 0.0f, cosf(rot.y));

        float baseThrowSpeed = Constants::Player::THROW_FORCE;
        float speedBoost = abs(m_Owner->GetAngularVelocity()) * 6.0f; 
        float totalSpeed = baseThrowSpeed + speedBoost;

        XMFLOAT3 throwVelocity = fwdF * totalSpeed;

        if (m_Owner->HasGigantItem()) {
            throwVelocity.y = -0.3f - speedBoost * 0.6f;
        }

        m_Owner->GetGrabbedEnemy()->SetVelocity(throwVelocity);
        m_Owner->GetGrabbedEnemy()->SetEnemyState(EnemyState::FLYING);

        if (m_Owner->HasVacuumItem()) {
            m_Owner->GetGrabbedEnemy()->SetExplosive(true);
        }
        if (m_Owner->HasLightningItem()) {
            m_Owner->GetGrabbedEnemy()->SetLightning(true);
        }

        // Throw vacuumed enemies as well
        for (GameObject* obj : Manager::GetGameObjectList()) {
            if (!obj) continue;
            if (obj->GetObjectType() != ObjectType::Enemy) continue;
            Enemy* enemy = static_cast<Enemy*>(obj);
            if (enemy->GetEnemyState() == EnemyState::VACUUMED) {
                float finalSpeed = totalSpeed * 0.9f;
                XMFLOAT3 vacuumThrowVel = fwdF * finalSpeed;

                enemy->SetVelocity(vacuumThrowVel);
                enemy->SetEnemyState(EnemyState::FLYING);
                enemy->SetExplosive(true);
            }
        }

        m_Owner->SetHasVacuumItem(false);
        m_Owner->SetHasGigantItem(false);
        m_Owner->SetHasLightningItem(false);

        m_Owner->SetGrabbedEnemy(nullptr);
        m_Owner->SetState(PlayerState::IDLE);
        m_Owner->SetAutoSpinning(false);

        m_Owner->SetScale(XMFLOAT3(1.6f, 0.5f, 1.6f));

        float throwShake = 0.08f + abs(m_Owner->GetAngularVelocity()) * 1.8f;
        if (throwShake > 0.40f) throwShake = 0.40f;
        g_Camera->Shake(throwShake, 12);
    }
}

// =================================================================
// Apply Damage
// =================================================================
void PlayerCombat::OnHit(const HitInfo& info)
{
    if (Constants::Debug::INVINCIBLE_PLAYER || m_Owner->GetInvincibleTimer() > 0 || m_Owner->GetHP() <= 0) return;

    m_Owner->SetHP(m_Owner->GetHP() - info.damage);
    if (m_Owner->GetHP() < 0) m_Owner->SetHP(0);

    m_Owner->SetScale(XMFLOAT3(1.7f, 0.5f, 1.7f));

    m_Owner->SetDamageTimer(Constants::Player::DAMAGE_STUN_DURATION * 2);
    m_Owner->SetInvincibleTimer(Constants::Player::INVINCIBLE_DURATION * 3);

    if (m_Owner->GetGrabbedEnemy()) {
        m_Owner->GetGrabbedEnemy()->SetEnemyState(EnemyState::NORMAL);
        m_Owner->GetGrabbedEnemy()->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
        m_Owner->SetGrabbedEnemy(nullptr);
    }
    m_Owner->SetState(PlayerState::IDLE);

    DirectX::XMFLOAT3 diff = m_Owner->GetPosition() - info.hitSourcePos;
    diff.y = 0.0f;
    float dist = MathHelper::Length(diff);
    if (dist > 0.001f) {
        diff.x /= dist;
        diff.z /= dist;
    } else {
        diff = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);
    }

    XMFLOAT3 knockback = DirectX::XMFLOAT3(diff.x * 0.35f, 0.0f, diff.z * 0.35f);
    if (m_Owner->GetMovementModule()) {
        m_Owner->GetMovementModule()->ApplyKnockback(knockback);
    }

    if (g_Camera) {
        g_Camera->Shake(0.35f, 12);
    }
    Manager::AddHitStop(5);
}

// =================================================================
// Parry Counter
// =================================================================
void PlayerCombat::ExecuteParryCounter(DirectX::XMFLOAT3 bulletPos)
{
    Enemy* sandbag = Manager::AddGameObject<Enemy>();
    if (sandbag) {
        XMFLOAT3 spawnPos = bulletPos;
        spawnPos.y = -0.5f;
        sandbag->SetPosition(spawnPos);
        sandbag->SetScale(XMFLOAT3(1.2f, 1.2f, 1.2f));
        sandbag->SetSandbag(true);
    }

    m_Owner->SetScale(XMFLOAT3(0.8f, 1.3f, 0.8f));
    if (m_Owner->GetMovementModule()) {
        m_Owner->GetMovementModule()->SetScaleVelocity(-0.04f, 0.08f, -0.04f);
    }

    XMFLOAT3 boltStart = m_Owner->GetPosition();
    boltStart.y += 0.3f;
    XMFLOAT3 boltEnd = bulletPos;
    boltEnd.y += 0.3f;
    m_Owner->AddLightningEffect(boltStart, boltEnd);

    XMFLOAT3 shockPos = bulletPos;
    shockPos.y = -0.95f;
    ShockwaveSystem::AddShockwave(shockPos, 3.0f, 1.5f, 0.8f, 0.0f, 16, 0.0f, 0);

    Manager::AddHitStop(8);
    if (g_Camera) {
        g_Camera->Shake(0.2f, 10);
    }

    Manager::StartSlowMotion(90);
    m_Owner->DisableWarpSlash();
}

// =================================================================
// Find Lockon Target
// =================================================================
void PlayerCombat::FindLockOnTarget()
{
    if (!g_Camera) return;

    XMFLOAT3 camPos = g_Camera->GetPosition();
    XMFLOAT3 camFwd = g_Camera->GetForward();

    XMVECTOR vCamFwd = XMVector3Normalize(XMLoadFloat3(&camFwd));
    XMVECTOR vCamPos = XMLoadFloat3(&camPos);

    Enemy* bestTarget = nullptr;
    float maxCos = -1.0f;
    float maxDistance = 30.0f;
    float minCos = 0.7071f;

    for (GameObject* obj : Manager::GetGameObjectList()) {
        if (!obj || obj == m_Owner || obj->IsDestroy()) continue;
        ObjectType type = obj->GetObjectType();
        if (type != ObjectType::Enemy && type != ObjectType::Boss) continue;

        Enemy* enemy = static_cast<Enemy*>(obj);
        EnemyState state = enemy->GetEnemyState();
        if (state == EnemyState::DEFEATED || state == EnemyState::GRABBED || state == EnemyState::VACUUMED) continue;
        if (enemy == m_Owner->GetGrabbedEnemy()) continue;

        XMFLOAT3 enemyPos = enemy->GetPosition();
        XMVECTOR vEnemyPos = XMLoadFloat3(&enemyPos);

        float dist = MathHelper::Length(enemyPos - m_Owner->GetPosition());
        if (dist > maxDistance) continue;

        XMVECTOR vToEnemy = XMVector3Normalize(vEnemyPos - vCamPos);
        float cosAngle = XMVectorGetX(XMVector3Dot(vToEnemy, vCamFwd));

        if (cosAngle >= minCos) {
            if (cosAngle > maxCos) {
                maxCos = cosAngle;
                bestTarget = enemy;
            }
        }
    }

    if (bestTarget != m_Owner->GetLockOnTarget()) {
        m_Owner->SetLockOnTarget(bestTarget);
        m_Owner->SetLockOnFrame(0);
    } else if (m_Owner->GetLockOnTarget() != nullptr) {
        m_Owner->IncrementLockOnFrame();
    }
}
