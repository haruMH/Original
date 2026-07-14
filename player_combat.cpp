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
// �R���X�g���N�^ / �f�X�g���N�^
// =================================================================
PlayerCombat::PlayerCombat(Player* owner)
    : m_Owner(owner)
{
}

PlayerCombat::~PlayerCombat()
{
}

// =================================================================
// ������
// =================================================================
void PlayerCombat::Init()
{
}

// =================================================================
// ���t���[���X�V����
// =================================================================
void PlayerCombat::Update()
{
    // �K�[�h��Ԃ̍X�V�i�X�^�����łȂ��A���G��͂�ł��Ȃ��ʏ��Ԃ̂Ƃ��̂݉\�j
    if (m_Owner->m_DamageTimer <= 0 && m_Owner->m_State == PlayerState::IDLE && !m_Owner->m_GrabbedEnemy) {
        if (PlayerController::IsGuardAction()) {
            m_Owner->m_GuardTimer++;
        } else {
            m_Owner->m_GuardTimer = 0;
        }
    } else {
        m_Owner->m_GuardTimer = 0;
    }

    // ���b�N�I���̍X�V�i�X���[���[�V�������̂ݗL���j
    if (Manager::IsSlowMotionActive()) {
        FindLockOnTarget();
    } else {
        m_Owner->m_LockOnTarget = nullptr;
        m_Owner->m_LockOnFrame = 0;
        m_Owner->m_WarpSlashCount = 0;
        m_Owner->m_CanWarpSlash = true;
    }

    m_Owner->m_MarkerTimer++;

    // ���т�X�^�����łȂ���΁A�e��Ԃ̍X�V��s��
    if (m_Owner->m_DamageTimer <= 0) {
        switch (m_Owner->m_State) {
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
// IDLE�i�ʏ�j��Ԃ̐퓬�X�V
// =================================================================
void PlayerCombat::UpdateIdle()
{
    // �^�b�N���L�����ɍ��N���b�N �� �G�̖ڂ̑O�Ƀe���|�[�g���ă^�b�N���I
    if (m_Owner->m_TackleTimer > 0 && PlayerController::IsGrabOrThrowAction()) {
        Enemy* target = nullptr;
        
        // ���b�N�I���Ώۂ�D��
        if (m_Owner->m_LockOnTarget && !m_Owner->m_LockOnTarget->IsDestroy()) {
            target = m_Owner->m_LockOnTarget;
        } else {
            // �߂��̓G�����
            float nearestDist = 15.0f;
            for (GameObject* obj : Manager::GetGameObjectList()) {
                if (!obj || obj == m_Owner || obj->IsDestroy()) continue;
                if (obj->GetObjectType() != ObjectType::Enemy && obj->GetObjectType() != ObjectType::Boss) continue;
                Enemy* e = static_cast<Enemy*>(obj);
                if (e->GetEnemyState() == EnemyState::DEFEATED) continue;
                
                float dist = MathHelper::Length(e->GetPosition() - m_Owner->m_Position);
                if (dist < nearestDist) {
                    nearestDist = dist;
                    target = e;
                }
            }
        }
        
        // �^�b�N���e���|�[�g���s
        if (target) {
            XMFLOAT3 targetPos = target->GetPosition();
            XMFLOAT3 startPos = m_Owner->m_Position;
            
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
            warpPos.y = m_Owner->m_Position.y; // �ڒn������ێ�
            
            m_Owner->m_Position = warpPos;
            XMFLOAT3 dir;
            XMStoreFloat3(&dir, vDir);
            m_Owner->m_Rotation.y = atan2f(dir.x, dir.z);
            
            // �������ό`���o
            m_Owner->m_Scale.y = 0.4f;
            m_Owner->m_Scale.x = 1.8f;
            m_Owner->m_Scale.z = 1.8f;
            m_Owner->m_Movement->SetScaleVelocity(0.08f, -0.1f, 0.08f);
            
            // �Ռ��g����
            ShockwaveSystem::AddShockwave(warpPos, 5.0f, 0.0f, 2.5f, 4.0f, 20, 1.5f, 0);
            
            // �_���[�W�K�p
            if (target->GetObjectType() == ObjectType::Boss) {
                BossEnemy* boss = static_cast<BossEnemy*>(target);
                boss->ApplyBossDamage(3, m_Owner->m_Position);
            } else {
                XMFLOAT3 pushVel = XMFLOAT3(dir.x * 1.8f, 0.6f, dir.z * 1.8f);
                target->SetVelocity(pushVel);
                target->SetEnemyState(EnemyState::BLOWN_AWAY);
                target->Defeat(0.0f, 1.0f, 2.0f);
            }
            
            // �C���p�N�g���o
            Manager::AddHitStop(12);
            if (g_Camera) {
                g_Camera->Shake(0.35f, 12);
            }
            
            m_Owner->m_TackleTimer = 0;
            return;
        }
    }

    // ���d�e���|�[�g�X���b�V����������i�X���[�������b�N�I�������U����3�񖢖��j
    if (Manager::IsSlowMotionActive() && m_Owner->m_CanWarpSlash && m_Owner->m_LockOnTarget && m_Owner->m_WarpSlashCount < 3) {
        if (PlayerController::IsGrabOrThrowAction()) {
            bool hasGrabTarget = false;
            float grabRange = Constants::Player::GRAB_RANGE;
            for (GameObject* obj : Manager::GetGameObjectList()) {
                if (!obj || obj == m_Owner || obj->IsDestroy()) continue;
                if (obj->GetObjectType() != ObjectType::Enemy) continue;
                Enemy* e = static_cast<Enemy*>(obj);
                if (e->GetEnemyState() != EnemyState::NORMAL) continue;

                float dist = MathHelper::Length(e->GetPosition() - m_Owner->m_Position);
                if (dist < grabRange) {
                    hasGrabTarget = true;
                    break;
                }
            }

            // �͂ݗL���͈͓�ɓG�����Ȃ��ꍇ�̂݁A�e���|�[�g�X���b�V������
            if (!hasGrabTarget) {
                m_Owner->m_WarpSlashCount++;
                Enemy* target = m_Owner->m_LockOnTarget;
                XMFLOAT3 targetPos = target->GetPosition();
                XMFLOAT3 startPos = m_Owner->m_Position;
            
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
                warpPos.y = m_Owner->m_Position.y;
                
                m_Owner->m_Position = warpPos;
                m_Owner->m_Scale.y = 0.4f;
                m_Owner->m_Scale.x = 1.8f;
                m_Owner->m_Scale.z = 1.8f;
                
                XMFLOAT3 dir;
                XMStoreFloat3(&dir, vDir);
                m_Owner->m_Rotation.y = atan2f(dir.x, dir.z);
                
                // ���d�G�t�F�N�g�̒ǉ�
                XMFLOAT3 boltStart = startPos;
                XMFLOAT3 boltEnd = targetPos;
                boltStart.y += 0.3f;
                boltEnd.y += 0.3f;
                m_Owner->AddLightningEffect(boltStart, boltEnd);
                m_Owner->AddLightningEffect(XMFLOAT3(boltStart.x + 0.1f, boltStart.y, boltStart.z), XMFLOAT3(boltEnd.x + 0.1f, boltEnd.y, boltEnd.z));
                m_Owner->AddLightningEffect(XMFLOAT3(boltStart.x - 0.1f, boltStart.y, boltStart.z), XMFLOAT3(boltEnd.x - 0.1f, boltEnd.y, boltEnd.z));
                
                // �Ռ��g
                ShockwaveSystem::AddShockwave(warpPos, 4.0f, 0.0f, 2.5f, 4.0f, 20, 1.5f, 0);
                
                // �_���[�W�K�p
                if (target->GetObjectType() == ObjectType::Boss) {
                    BossEnemy* boss = static_cast<BossEnemy*>(target);
                    boss->ApplyBossDamage(4, m_Owner->m_Position);
                } else {
                    XMFLOAT3 pushVel = XMFLOAT3(dir.x * 1.6f, 0.6f, dir.z * 1.6f);
                    target->SetVelocity(pushVel);
                    target->SetEnemyState(EnemyState::BLOWN_AWAY);
                    target->SetLightning(true);
                    target->Defeat(0.0f, 2.0f, 3.0f);
                }
                
                // �C���p�N�g
                Manager::AddHitStop(15);
                if (g_Camera) {
                    g_Camera->Shake(0.45f, 15);
                }
                
                Manager::StartSlowMotion(90);
                return;
            }
        }
    }

    // �ʏ�̓G�̒͂ݏ���
    if (PlayerController::IsGrabOrThrowAction()) {
        float grabRange = Constants::Player::GRAB_RANGE;
        Enemy* nearest  = nullptr;
        float  nearestDist = grabRange;

        for (GameObject* obj : Manager::GetGameObjectList()) {
            if (!obj || obj == m_Owner || obj->IsDestroy()) continue;
            if (obj->GetObjectType() != ObjectType::Enemy) continue;
            Enemy* e = static_cast<Enemy*>(obj);
            if (e->GetEnemyState() != EnemyState::NORMAL) continue;

            XMFLOAT3 toEnemy = e->GetPosition() - m_Owner->m_Position;
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
// GRABBED�i�G�͂݁j��Ԃ̐퓬�X�V
// =================================================================
void PlayerCombat::UpdateGrabbed()
{
    // �E�N���b�N�F�X�s���J�n
    if (m_Owner->m_GrabbedEnemy && PlayerController::IsSpinToggleAction()) {
        m_Owner->m_State = PlayerState::SPINNING;
        m_Owner->m_CurrentSpinSpeed = Constants::Player::MIN_SPIN_SPEED;
    }

    // ���N���b�N�F���ړ�����
    if (m_Owner->m_GrabbedEnemy && PlayerController::IsGrabOrThrowAction()) {
        Throw();
        return;
    }


    if (!m_Owner->m_GrabbedEnemy) {
        m_Owner->m_State = PlayerState::IDLE;
    }
}

// =================================================================
// SPINNING�i�U��񂵁j��Ԃ̐퓬�X�V
// =================================================================
void PlayerCombat::UpdateSpinning()
{
    // �X�s������
    float targetSpinSpeed = Constants::Player::MAX_SPIN_SPEED;
    m_Owner->m_CurrentSpinSpeed += (targetSpinSpeed - m_Owner->m_CurrentSpinSpeed) * Constants::Player::SPIN_ACCELERATION;

    m_Owner->m_Rotation.y += m_Owner->m_CurrentSpinSpeed;
    if (m_Owner->m_Rotation.y > XM_2PI) m_Owner->m_Rotation.y -= XM_2PI;

    if (g_Camera) {
        float dynamicShake = 0.01f + m_Owner->m_CurrentSpinSpeed * 0.15f;
        g_Camera->Shake(dynamicShake, 2);
    }

    // �z������
    if (m_Owner->m_HasVacuumItem) {
        float vacuumRadius = 8.0f;
        float vacuumForce = 0.03f;

        for (GameObject* obj : Manager::GetGameObjectList()) {
            if (!obj || obj == m_Owner || obj == m_Owner->m_GrabbedEnemy) continue;

            if (obj->GetObjectType() == ObjectType::Enemy) {
                Enemy* enemy = static_cast<Enemy*>(obj);
                if (enemy->GetEnemyState() == EnemyState::NORMAL || enemy->GetEnemyState() == EnemyState::CHASING) {
                    XMFLOAT3 enemyPos = enemy->GetPosition();
                    XMFLOAT3 diff = m_Owner->m_Position - enemyPos;
                    float distSq = MathHelper::LengthSq(diff);

                    if (distSq < vacuumRadius * vacuumRadius && distSq > 0.01f) {
                        float dist = sqrtf(distSq);
                        XMFLOAT3 dir = diff / dist;

                        float spinDirSign = (m_Owner->m_AngularVelocity >= 0.0f) ? 1.0f : -1.0f;
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

    // ���N���b�N�F������
    if (PlayerController::IsGrabOrThrowAction()) {
        Throw();
        return;
    }

    // �E�N���b�N�F�͂ݏ�Ԃɖ߂�i�L�����Z���j
    if (PlayerController::IsSpinToggleAction()) {
        m_Owner->m_State = PlayerState::GRABBED;
        m_Owner->m_CurrentSpinSpeed = Constants::Player::MIN_SPIN_SPEED;
    }


    if (!m_Owner->m_GrabbedEnemy) {
        m_Owner->m_State = PlayerState::IDLE;
        m_Owner->m_CurrentSpinSpeed = Constants::Player::MIN_SPIN_SPEED;
    }
}

// =================================================================
// �͂�ł���G�𓊂���΂�
// =================================================================
void PlayerCombat::Throw()
{
    if (m_Owner->m_GrabbedEnemy) {
        float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;
        m_Owner->m_Rotation.y = camYaw;

        XMFLOAT3 fwdF = XMFLOAT3(sinf(m_Owner->m_Rotation.y), 0.0f, cosf(m_Owner->m_Rotation.y));

        float baseThrowSpeed = Constants::Player::THROW_FORCE;
        float speedBoost = abs(m_Owner->m_AngularVelocity) * 6.0f; 
        float totalSpeed = baseThrowSpeed + speedBoost;

        XMFLOAT3 throwVelocity = fwdF * totalSpeed;

        if (m_Owner->m_HasGigantItem) {
            throwVelocity.y = -0.3f - speedBoost * 0.6f;
        }

        m_Owner->m_GrabbedEnemy->SetVelocity(throwVelocity);
        m_Owner->m_GrabbedEnemy->SetEnemyState(EnemyState::FLYING);

        // �A�C�e�����ʂ̓K�p
        if (m_Owner->m_HasVacuumItem) {
            m_Owner->m_GrabbedEnemy->SetExplosive(true);
        }
        if (m_Owner->m_HasLightningItem) {
            m_Owner->m_GrabbedEnemy->SetLightning(true);
        }

        // ���]���̓G���Ďˏo
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

        // �t���O����
        m_Owner->m_HasVacuumItem = false;
        m_Owner->m_HasGigantItem = false;
        m_Owner->m_HasLightningItem = false;

        m_Owner->m_GrabbedEnemy = nullptr;
        m_Owner->m_State = PlayerState::IDLE;
        m_Owner->m_IsAutoSpinning = false;

        // �L�k���o
        m_Owner->m_Scale.y = 0.5f;
        m_Owner->m_Scale.x = 1.6f;
        m_Owner->m_Scale.z = 1.6f;

        // �J�����V�F�C�N
        float throwShake = 0.08f + abs(m_Owner->m_AngularVelocity) * 1.8f;
        if (throwShake > 0.40f) throwShake = 0.40f;
        g_Camera->Shake(throwShake, 12);
    }
}

// =================================================================
// ��e�_���[�W�K�p
// =================================================================
void PlayerCombat::ApplyDamage(int damage, const DirectX::XMFLOAT3& enemyPos)
{
    // �f�o�b�O�p�̖��G���[�h�܂��͒ʏ�̖��G���Ԓ��̏ꍇ�̓_���[�W�𖳌���
    if (Constants::Debug::INVINCIBLE_PLAYER || m_Owner->m_InvincibleTimer > 0 || m_Owner->m_HP <= 0) return;

    m_Owner->m_HP -= damage;
    if (m_Owner->m_HP < 0) m_Owner->m_HP = 0;

    m_Owner->m_Scale.y = 0.5f;
    m_Owner->m_Scale.x = 1.7f;
    m_Owner->m_Scale.z = 1.7f;

    m_Owner->m_DamageTimer = Constants::Player::DAMAGE_STUN_DURATION * 2; // 60�t���[��
    m_Owner->m_InvincibleTimer = Constants::Player::INVINCIBLE_DURATION * 3; // 180�t���[��

    if (m_Owner->m_GrabbedEnemy) {
        m_Owner->m_GrabbedEnemy->SetEnemyState(EnemyState::NORMAL);
        m_Owner->m_GrabbedEnemy->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
        m_Owner->m_GrabbedEnemy = nullptr;
    }
    m_Owner->m_State = PlayerState::IDLE;

    // �m�b�N�o�b�N�v�Z
    DirectX::XMFLOAT3 diff = m_Owner->m_Position - enemyPos;
    diff.y = 0.0f;
    float dist = MathHelper::Length(diff);
    if (dist > 0.001f) {
        diff.x /= dist;
        diff.z /= dist;
    } else {
        diff = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);
    }

    XMFLOAT3 knockback = DirectX::XMFLOAT3(diff.x * 0.35f, 0.0f, diff.z * 0.35f);
    m_Owner->m_Movement->ApplyKnockback(knockback);

    if (g_Camera) {
        g_Camera->Shake(0.35f, 12);
    }
    Manager::AddHitStop(5);
}

// =================================================================
// �p���B�������̃J�E���^�[���s
// =================================================================
void PlayerCombat::ExecuteParryCounter(DirectX::XMFLOAT3 bulletPos)
{
    // �T���h�o�b�O�G�̐���
    Enemy* sandbag = Manager::AddGameObject<Enemy>();
    if (sandbag) {
        XMFLOAT3 spawnPos = bulletPos;
        spawnPos.y = -0.5f;
        sandbag->SetPosition(spawnPos);
        sandbag->SetScale(XMFLOAT3(1.2f, 1.2f, 1.2f));
        sandbag->SetSandbag(true);
    }

    // �������ό`
    m_Owner->m_Scale.y = 1.3f;
    m_Owner->m_Scale.x = 0.8f;
    m_Owner->m_Scale.z = 0.8f;
    m_Owner->m_Movement->SetScaleVelocity(-0.04f, 0.08f, -0.04f);

    // �ΉԃC�i�Y�}�O�Ղ̐���
    XMFLOAT3 boltStart = m_Owner->m_Position;
    boltStart.y += 0.3f;
    XMFLOAT3 boltEnd = bulletPos;
    boltEnd.y += 0.3f;
    m_Owner->AddLightningEffect(boltStart, boltEnd);

    // �Ռ��g
    XMFLOAT3 shockPos = bulletPos;
    shockPos.y = -0.95f;
    ShockwaveSystem::AddShockwave(shockPos, 3.0f, 1.5f, 0.8f, 0.0f, 16, 0.0f, 0);

    // �q�b�g���o
    Manager::AddHitStop(8);
    if (g_Camera) {
        g_Camera->Shake(0.2f, 10);
    }

    Manager::StartSlowMotion(90);
    m_Owner->DisableWarpSlash();
}

// =================================================================
// ���b�N�I���ΏےT���i�X���[���̂݁j
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
    float minCos = 0.7071f; // cos(45�x)

    for (GameObject* obj : Manager::GetGameObjectList()) {
        if (!obj || obj == m_Owner || obj->IsDestroy()) continue;
        ObjectType type = obj->GetObjectType();
        if (type != ObjectType::Enemy && type != ObjectType::Boss) continue;

        Enemy* enemy = static_cast<Enemy*>(obj);
        EnemyState state = enemy->GetEnemyState();
        if (state == EnemyState::DEFEATED || state == EnemyState::GRABBED || state == EnemyState::VACUUMED) continue;
        if (enemy == m_Owner->m_GrabbedEnemy) continue;

        XMFLOAT3 enemyPos = enemy->GetPosition();
        XMVECTOR vEnemyPos = XMLoadFloat3(&enemyPos);

        float dist = MathHelper::Length(enemyPos - m_Owner->m_Position);
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

    if (bestTarget != m_Owner->m_LockOnTarget) {
        m_Owner->m_LockOnTarget = bestTarget;
        m_Owner->m_LockOnFrame = 0;
    } else if (m_Owner->m_LockOnTarget != nullptr) {
        m_Owner->m_LockOnFrame++;
    }
}
