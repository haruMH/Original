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
// �R���X�g���N�^ / �f�X�g���N�^
// =================================================================
PlayerMovement::PlayerMovement(Player* owner)
    : m_Owner(owner)
{
}

PlayerMovement::~PlayerMovement()
{
}

// =================================================================
// ������
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
// ���t���[���X�V����
// =================================================================
void PlayerMovement::Update()
{
    XMFLOAT3 oldPos = m_Owner->m_Position;
    float oldRotY = m_Owner->m_Rotation.y;

    // �_�b�V���̃N�[���_�E���X�V
    if (m_DashCooldown > 0) {
        m_DashCooldown--;
    }

    // �c���i�S�[�X�g�j�̍X�V
    UpdateGhosts();

    // �_�b�V����������
    // (�X�^�����łȂ��A���_�b�V�����łȂ��A���N�[���_�E���łȂ�)
    if (m_Owner->m_DamageTimer <= 0 && m_DashTimer <= 0 && m_DashCooldown <= 0) {
        if (PlayerController::IsDashAction()) {
            float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;
            XMFLOAT3 moveDir = PlayerController::GetMoveDirection(camYaw);
            
            // �ړ����͂��Ȃ��ꍇ�̓v���C���[�̐��ʕ����Ƀ_�b�V��
            if (MathHelper::LengthSq(moveDir) < 0.001f) {
                moveDir = XMFLOAT3(sinf(m_Owner->m_Rotation.y), 0.0f, cosf(m_Owner->m_Rotation.y));
            }
            
            // ���K��
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

            // �������ό`���o�F�i�s�����ɒ����L�т�悤�ɐݒ�
            m_Owner->m_Scale.y = 0.5f;
            m_Owner->m_Scale.x = 1.8f;
            m_Owner->m_Scale.z = 1.8f;

            m_ScaleVelocityY = -0.1f;
            m_ScaleVelocityX = 0.08f;
            m_ScaleVelocityZ = 0.08f;
            
            // �_�b�V�����̖��G���Ԑݒ�
            m_Owner->m_InvincibleTimer = Constants::Player::DASH_INVINCIBLE_TIME;
        }
    }

    // ������������������������������������������������������������������������������������������
    // �_�b�V�����̋�������
    // ������������������������������������������������������������������������������������������
    if (m_DashTimer > 0) {
        m_DashTimer--;
        
        // �����ړ�
        float dashSpeed = Constants::Player::DASH_SPEED;
        XMFLOAT3 nextPos = m_Owner->m_Position;
        nextPos.x += m_DashDirection.x * dashSpeed;
        nextPos.z += m_DashDirection.z * dashSpeed;

        // �^�b�N���L�����͏Փ˔���ƍU��������s��
        if (m_Owner->m_TackleTimer > 0) {
            Enemy* hitTarget = nullptr;
            
            // 1. ���b�N�I���Ώۂ�D��
            if (m_Owner->m_LockOnTarget && !m_Owner->m_LockOnTarget->IsDestroy()) {
                XMFLOAT3 diff = m_Owner->m_Position - m_Owner->m_LockOnTarget->GetPosition();
                diff.y = 0.0f;
                float dist = MathHelper::Length(diff);
                if (dist < 1.0f + m_Owner->m_LockOnTarget->GetRadius()) {
                    hitTarget = m_Owner->m_LockOnTarget;
                }
            }
            
            // 2. ���͂̓G�𑖍�
            if (!hitTarget) {
                for (GameObject* obj : Manager::GetGameObjectList()) {
                    if (!obj || obj->IsDestroy() || obj == m_Owner) continue;
                    if (obj->GetObjectType() != ObjectType::Enemy && obj->GetObjectType() != ObjectType::Boss) continue;
                    Enemy* e = static_cast<Enemy*>(obj);
                    if (e->GetEnemyState() == EnemyState::DEFEATED) continue;
                    
                    XMFLOAT3 diff = m_Owner->m_Position - e->GetPosition();
                    diff.y = 0.0f;
                    float dist = MathHelper::Length(diff);
                    if (dist < 1.0f + e->GetRadius()) {
                        hitTarget = e;
                        break;
                    }
                }
            }
            
            // �_���[�W�E������΂��K�p
            if (hitTarget) {
                if (hitTarget->GetObjectType() == ObjectType::Boss) {
                    BossEnemy* bossE = static_cast<BossEnemy*>(hitTarget);
                    bossE->ApplyBossDamage(3, m_Owner->m_Position);
                } else {
                    XMFLOAT3 pushDir = hitTarget->GetPosition() - m_Owner->m_Position;
                    pushDir.y = 0.0f;
                    float len = MathHelper::Length(pushDir);
                    if (len > 0.001f) { pushDir.x /= len; pushDir.z /= len; }
                    else { pushDir = XMFLOAT3(0.0f, 0.0f, 1.0f); }
                    
                    XMFLOAT3 pushVel = XMFLOAT3(pushDir.x * 1.8f, 0.6f, pushDir.z * 1.8f);
                    hitTarget->SetVelocity(pushVel);
                    hitTarget->SetEnemyState(EnemyState::BLOWN_AWAY);
                    hitTarget->Defeat(0.0f, 1.0f, 2.0f);
                }
                
                // �q�b�g���o
                Manager::AddHitStop(10);
                if (g_Camera) {
                    g_Camera->Shake(0.25f, 10);
                }
                
                m_Owner->m_TackleTimer = 0; // �^�b�N������
            }
        }

        // �ǂƂ̏Փˉ��
        if (!Collision::CheckAABBCollision(m_Owner, nextPos, Manager::GetGameObjectList())) {
            m_Owner->m_Position = nextPos;
        }

        // �c�������i3�t���[�����Ɓj
        if (m_DashTimer % 3 == 0) {
            DashGhost ghost;
            ghost.Position = m_Owner->m_Position;
            ghost.Rotation = m_Owner->m_Rotation;
            ghost.Scale = m_Owner->m_Scale;
            ghost.Alpha = 0.8f;
            m_DashGhosts.push_back(ghost);
        }

        // �_�b�V���I�����̃o�E���h���o
        if (m_DashTimer == 0) {
            m_IsDashing = false;
            m_Owner->m_Scale.y = 1.5f;
            m_Owner->m_Scale.x = 0.7f;
            m_Owner->m_Scale.z = 0.7f;
            m_ScaleVelocityY = 0.08f;
            m_ScaleVelocityX = -0.04f;
            m_ScaleVelocityZ = -0.04f;
        }


        // �_�b�V������������̌����U����X�V
        UpdateSpringPhysics();
        return;
    }

    // ������������������������������������������������������������������������������������������
    // ��e�C��i�X�^���j���̃m�b�N�o�b�N����
    // ������������������������������������������������������������������������������������������
    if (m_Owner->m_DamageTimer > 0) {
        XMFLOAT3 nextPos = m_Owner->m_Position;
        nextPos.x += m_KnockbackVelocity.x;
        nextPos.z += m_KnockbackVelocity.z;
        
        m_KnockbackVelocity.x *= 0.9f;
        m_KnockbackVelocity.z *= 0.9f;
        
        if (!Collision::CheckAABBCollision(m_Owner, nextPos, Manager::GetGameObjectList())) {
            m_Owner->m_Position = nextPos;
        }
    }

    // ������������������������������������������������������������������������������������������
    // �ʏ�ړ��ƃW�����v���͂̍X�V
    // ������������������������������������������������������������������������������������������
    if (m_Owner->m_DamageTimer <= 0) {
        // �X�y�[�X�L�[�ŃW�����v�i2�i�W�����v�Ή��j
        if (Input::GetKeyTrigger(0x20) && m_JumpCount < Constants::Player::MAX_JUMP_COUNT) {
            m_VelocityY = Constants::Player::JUMP_VELOCITY;
            m_IsJumping = true;
            m_JumpCount++;

            // ���ݐ؂莞�̂��������o
            m_Owner->m_Scale.y = 0.6f;
            m_Owner->m_Scale.x = 1.3f;
            m_Owner->m_Scale.z = 1.3f;

            m_ScaleVelocityY = 0.1f;
            m_ScaleVelocityX = -0.05f;
            m_ScaleVelocityZ = -0.05f;
        }

        // �ʏ�ړ������̌Ăяo��
        UpdateNormalMovement();
    }

    // ������������������������������������������������������������������������������������������
    // �d�͂Ɛڒn����̓K�p
    // ������������������������������������������������������������������������������������������
    if (m_Owner->m_Position.y > -0.5f || m_VelocityY != 0.0f) {
        m_VelocityY -= Constants::Player::GRAVITY;
        m_Owner->m_Position.y += m_VelocityY;

        // �󒆂ł̈����L�΂����o�i�c���j
        if (m_VelocityY > 0.01f) {
            m_Owner->m_Scale.y += (1.0f + m_VelocityY * 1.5f - m_Owner->m_Scale.y) * 0.2f;
            m_Owner->m_Scale.x += (1.0f - m_VelocityY * 0.75f - m_Owner->m_Scale.x) * 0.2f;
            m_Owner->m_Scale.z += (1.0f - m_VelocityY * 0.75f - m_Owner->m_Scale.z) * 0.2f;
        }

        // ���n����
        if (m_Owner->m_Position.y <= -0.5f) {
            m_Owner->m_Position.y = -0.5f;
            m_VelocityY = 0.0f;
            m_IsJumping = false;
            m_JumpCount = 0;

            // ���n���ׂ̒ꉉ�o
            m_Owner->m_Scale.y = 0.5f;
            m_Owner->m_Scale.x = 1.3f;
            m_Owner->m_Scale.z = 1.3f;

            m_ScaleVelocityY = -0.1f;
            m_ScaleVelocityX = 0.05f;
            m_ScaleVelocityZ = 0.05f;
        }
    }

    // �p���x�i���񑬓x�j�̌v�Z
    float diff = m_Owner->m_Rotation.y - oldRotY;
    while (diff < -XM_PI) diff += XM_2PI;
    while (diff > XM_PI)  diff -= XM_2PI;
    m_Owner->m_AngularVelocity = diff;

    // ������������������������������������������������������������������������������������������
    // �X�v�����O�����ɂ������������ƕ�s�h��
    // ������������������������������������������������������������������������������������������
    UpdateSpringPhysics();

    // 1�t���[����XZ�ړ��ʂ���ړ����x��Z�o
    XMFLOAT3 actualVel = XMFLOAT3(m_Owner->m_Position.x - oldPos.x, m_Owner->m_Position.y - oldPos.y, m_Owner->m_Position.z - oldPos.z);
    float speed = sqrtf(actualVel.x * actualVel.x + actualVel.y * actualVel.y + actualVel.z * actualVel.z);
    float dt = 1.0f / 60.0f;

    if (speed > 0.005f) {
        m_MoveAnimation += speed * dt * 30.0f;
        m_Owner->m_Scale.y += sinf(m_MoveAnimation * 3.0f) * 0.03f;
        m_Owner->m_Scale.x -= sinf(m_MoveAnimation * 3.0f) * 0.015f;
        m_Owner->m_Scale.z -= sinf(m_MoveAnimation * 3.0f) * 0.015f;
    }

    // �Փ˔���ɂ��߂荞�݂̉����߂����
    Collision::ResolveAABBCollision(m_Owner, Manager::GetGameObjectList());
}

// =================================================================
// �ʏ�ړ�����
// =================================================================
void PlayerMovement::UpdateNormalMovement()
{
    float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;
    XMFLOAT3 moveDir = PlayerController::GetMoveDirection(camYaw);

    // �ړ����͂�����ꍇ�̂ݍ��W�Ɖ�]��X�V
    if (MathHelper::LengthSq(moveDir) > 0.001f) {
        float speed = Constants::Player::MOVE_SPEED;
        XMFLOAT3 nextPos = m_Owner->m_Position;
        nextPos.x += moveDir.x * speed;
        nextPos.z += moveDir.z * speed;

        // �ǂƂ̏Փ˂��Ȃ���Έړ�����
        if (!Collision::CheckAABBCollision(m_Owner, nextPos, Manager::GetGameObjectList())) {
            m_Owner->m_Position = nextPos;
        }

        // �X�s���i�����E�蓮�j���łȂ��ꍇ�̂݁A�ړ������֌�����ς���
        if (m_Owner->m_State != PlayerState::SPINNING) {
            float targetYaw = atan2f(moveDir.x, moveDir.z);
            m_Owner->m_Rotation.y = MathHelper::LerpAngle(m_Owner->m_Rotation.y, targetYaw, 0.15f);
        }
    }
}

// =================================================================
// �����������̃X�v�����O�������Z
// =================================================================
void PlayerMovement::UpdateSpringPhysics()
{
    float springK = Constants::Player::SPRING_K;
    float damping = Constants::Player::DAMPING;

    // X��
    float forceX = (1.0f - m_Owner->m_Scale.x) * springK;
    m_ScaleVelocityX += forceX;
    m_ScaleVelocityX *= damping;
    m_Owner->m_Scale.x += m_ScaleVelocityX;

    // Y��
    float forceY = (1.0f - m_Owner->m_Scale.y) * springK;
    m_ScaleVelocityY += forceY;
    m_ScaleVelocityY *= damping;
    m_Owner->m_Scale.y += m_ScaleVelocityY;

    // Z��
    float forceZ = (1.0f - m_Owner->m_Scale.z) * springK;
    m_ScaleVelocityZ += forceZ;
    m_ScaleVelocityZ *= damping;
    m_Owner->m_Scale.z += m_ScaleVelocityZ;
}

// =================================================================
// �_�b�V���c���̎����Ǘ�
// =================================================================
void PlayerMovement::UpdateGhosts()
{
    for (auto it = m_DashGhosts.begin(); it != m_DashGhosts.end(); ) {
        it->Alpha -= 0.08f; // ���t���[���s�����x�����
        if (it->Alpha <= 0.0f) {
            it = m_DashGhosts.erase(it);
        } else {
            it++;
        }
    }
}
