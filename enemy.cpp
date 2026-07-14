#include "enemy.h"
#include "renderer.h"
#include "resource_manager.h"
#include "math_helper.h"
#include "player.h"
#include "manager.h"
#include "camera.h"
#include "shockwave.h"
#include "game_rule.h"
#include "score_popup.h"
#include "game_constants.h"

void Enemy::Init()
{
    m_Position = XMFLOAT3(0.0f, -0.5f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_Size     = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_UprightTimer = 0;
    m_IsExplosive  = false;
    m_IsLightning  = false;
    m_IsDefeatedCounted = false;
    m_ScoreValue   = Constants::Enemy::DEFAULT_SCORE;

    m_Texture = ResourceManager::GetTexture("enemy.png");
    
    // �R���|�[�l���g�w���ł̕`��p�����[�^������
    m_RenderComponent = RenderComponent("enemy.png", MeshType::Cube, true);
}

void Enemy::Uninit()
{
}

void Enemy::Update()
{
    if (m_EnemyState == EnemyState::FLYING) {
        // ���C�i��C��R�j�ŏ��X�Ɍ���������i���C��������Ĕ�т�����h�~�j
        MathHelper::ScaleXZ(m_Velocity, Constants::Enemy::FLYING_AIR_RESISTANCE);
        // �d�͂̓K�p�iY���̗����j
        m_VelocityY -= Constants::Enemy::FLYING_GRAVITY; 

        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;
        m_Position.y += m_VelocityY;

        // �X�P�[���ɉ��������ʂ̍�����v�Z�i�߂荞�ݖh�~�j
        float baseOffset = (m_Size.y * m_Scale.y - 1.0f) * 0.5f;
        float flightFloorY = -0.3f + baseOffset;

        // �n�ʁi���j�ւ̒��n�N�����v
        if (m_Position.y < flightFloorY) {
            // �ڒn�����u�Ԃ̏����i�������̏\���ȑ��x������ꍇ�j
            if (m_VelocityY < -0.05f) {
                // ���剻�G�l�~�[���@������ꂽ�ۂ̃J�����V�F�C�N�ƃq�b�g�X�g�b�v
                if (m_Scale.x > 2.0f) {
                    if (g_Camera) {
                        float impactShake = abs(m_VelocityY) * 1.5f;
                        if (impactShake > 0.6f) impactShake = 0.6f;
                        g_Camera->Shake(impactShake, 15);
                    }
                    Manager::AddHitStop(10);
                }
            }

            m_Position.y = flightFloorY;
            m_VelocityY = 0.0f;
        }

        // ���x���\���ɗ����A�����������n�ʂɒ��n���Ă���Ȃ�NORMAL�ɖ߂��i����G�l�~�[�͏��Łj
        // ��������������������G�� game_scene ���Ŕ����������邽�߁A�����ł͑J�ڂ��Ȃ�
        if (m_Position.y <= flightFloorY && abs(m_Velocity.x) < 0.05f && abs(m_Velocity.z) < 0.05f) {
            if (!m_IsExplosive) {
                if (m_Scale.x > 2.0f) {
                    // ���剻�G�l�~�[�͔�яI����ăX�s�[�h��0�ɂȂ�������ł�����
                    m_EnemyState = EnemyState::DEFEATED;
                    m_Velocity   = XMFLOAT3(0.0f, 0.0f, 0.0f);
                    m_VelocityY  = 0.0f;

                    // �X�s�[�h��0�ɂȂ�A���ł���u�Ԃɑ��d�g��Ռ��g�𔭐�������iY���W��n�ʂ̍����ɋ����Œ肵�A�K�͂ƃf�B���C��g��j
                    XMFLOAT3 shockPos = m_Position;
                    shockPos.y = -0.95f; // �n�ʂ̍����Ɋ��S�N�����v

                    ShockwaveSystem::AddShockwave(shockPos, 15.0f, 1.8f, 0.9f, 0.0f, 32, 1.4f, 0);
                    ShockwaveSystem::AddShockwave(shockPos, 11.0f, 1.8f, 0.9f, 0.0f, 26, 0.9f, 7);
                    ShockwaveSystem::AddShockwave(shockPos, 7.0f, 1.8f, 0.9f, 0.0f, 20, 0.5f, 14);
                } else {
                    m_Position.y = -0.5f + baseOffset; // �Î~���ɖ{���̒n�ʂ̍����ɖ���������
                    m_Velocity   = XMFLOAT3(0, 0, 0);
                    m_VelocityY  = 0.0f;
                    m_EnemyState = EnemyState::NORMAL;
                    m_UprightTimer = 60;  // 1�b��ɂ������N���オ��悤�Ƀ^�C�}�[��Z�b�g
                }
            }
            // ���������̏ꍇ�� FLYING �̂܂� �� game_scene �̒��n+���x0�`�F�b�N�Ŕ���������
        }

        m_Rotation.x += 0.2f;
        m_Rotation.z += 0.15f;
    }
    else if (m_EnemyState == EnemyState::DEFEATED) {
        m_Scale *= 0.85f;                                  // 3�������ɏk���I
        m_Rotation += DirectX::XMFLOAT3(0.5f, 0.5f, 0.5f); // 3�������ɍ�����]�I
        MathHelper::ScaleXZ(m_Velocity, 0.9f);             // ���C�ɂ�錸��

        // ������Ԋ�������������ێ����ăX���C�h������
        m_Velocity.x *= 0.9f;
        m_Velocity.z *= 0.9f;
        
        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;

        if (m_Scale.x < 0.05f) {
            SetDestroy(); // ���S�ɏ���
        }
    }
    else if (m_EnemyState == EnemyState::NORMAL) {
        if (m_IsSandbag) {
            m_Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
            m_SandbagLife--;
            if (m_SandbagLife <= 0) {
                Defeat();
                m_EnemyState = EnemyState::DEFEATED;
                return;
            }
        }
        else {
            if (m_UprightTimer > 0) {
                m_UprightTimer--;
            }
            else {
                m_Rotation.x = MathHelper::Lerp(m_Rotation.x, 0.0f, 0.1f);
                m_Rotation.z = MathHelper::Lerp(m_Rotation.z, 0.0f, 0.1f);

                MathHelper::ClearIfNearZero(m_Rotation.x);
                MathHelper::ClearIfNearZero(m_Rotation.z);
            }
        }
    }
    else if (m_EnemyState == EnemyState::VACUUMED) {
        // �͂�ł���G�𒆐S�Ƀu���b�N�z�[���̂悤�Ɍ��]���鋓��
        Player* player = Manager::GetGameObject<Player>();
        if (player) {
                // �v���C���[���X�s����Ԃ�I���Ă�����ʏ�ɖ߂�
                if (player->GetState() != PlayerState::SPINNING) {
                    m_EnemyState = EnemyState::NORMAL;
                    return;
                }

                // �u���b�N�z�[���̒��S = �͂�ł���G�̈ʒu�i�Ȃ���΃v���C���[�ʒu�j
                Enemy* grabbed = player->GetGrabbedEnemy();
                DirectX::XMFLOAT3 pivotPos = grabbed
                    ? grabbed->GetPosition()
                    : player->GetPosition();

                DirectX::XMFLOAT3 diff = m_Position - pivotPos;
                float dist = MathHelper::Length(diff);

                // ���X�ɒ͂܂�Ă���G�Ɉ����񂹂�i���a 2.5 �Ɏ����j
                float targetDist = 2.5f;
                dist = MathHelper::Lerp(dist, targetDist, 0.06f);

                // �͂�ł���G�̃X�s�����x�Ō��]������
                float angle = atan2f(diff.x, diff.z);
                angle += player->GetAngularVelocity();

                m_Position.x = pivotPos.x + sinf(angle) * dist;
                m_Position.z = pivotPos.z + cosf(angle) * dist;
                m_Position.y = pivotPos.y + 0.3f; // �͂�ł���G�̍����ɍ��킹�ĕ�����

                m_Velocity = DirectX::XMFLOAT3(0, 0, 0); // �z�����܂�Ă���Ԃ͑��x��[���ɂ���

                // �z������Ă��鉉�o�Ŏ��]������
                m_Rotation.y += 0.15f;
            }
        }
    else if (m_EnemyState == EnemyState::BLOWN_AWAY) {
        // �����Ő�����΂���镨������
        MathHelper::ScaleXZ(m_Velocity, 0.95f); // ��C��R
        m_VelocityY -= 0.02f; // �d��
        
        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;
        m_Position.y += m_VelocityY;
        
        // �X�P�[���ɉ��������ʂ̍�����v�Z�i�߂荞�ݖh�~�j
        float baseOffset = (m_Size.y * m_Scale.y - 1.0f) * 0.5f;
        float floorY = -0.5f + baseOffset;
        if (m_Position.y < floorY) {
            m_Position.y = floorY;
            m_VelocityY = 0.0f;
        }
        
        // ��������]����
        m_Rotation.x += 0.3f;
        m_Rotation.y += 0.2f;
        m_Rotation.z += 0.25f;
        
        // �n�ʂɌ��˂��āA���x���\���ɒx���Ȃ����猂�j��ԁiDEFEATED�j�ɂ��ď��ł�����
        if (m_Position.y <= floorY && abs(m_Velocity.x) < 0.05f && abs(m_Velocity.z) < 0.05f) {
            m_EnemyState = EnemyState::DEFEATED;
            m_Velocity = DirectX::XMFLOAT3(0, 0, 0);
        }
    }
}

void Enemy::Draw()
{
    // �ʏ�p�X�̏ꍇ�� RenderSystem ���ňꊇ�`�悷�邽�ߖ{�͕̂`�悵�Ȃ����A
    // �d�������im_IsLightning�j������ꍇ�͎��͂ɃX�p�[�N�G�t�F�N�g��`�悷��
    if (!Renderer::IsShadowMode() && !Renderer::IsOutlineMode())
    {
        if (m_IsLightning)
        {
            Player* player = Manager::GetGameObject<Player>();
            if (player)
            {
                // �G�̒��S������͂̋󒆂� 2?3�{ �قǃr���r���ƉΉԂ�U�炷
                XMFLOAT3 start = m_Position;
                start.y += 0.3f; // ���S�t��

                int sparks = 2 + (rand() % 2); 
                for (int i = 0; i < sparks; i++)
                {
                    float angle = ((float)rand() / RAND_MAX) * XM_2PI;
                    float pitch = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * XM_PIDIV4; // �㉺45�x
                    float dist = 0.8f + ((float)rand() / RAND_MAX) * 1.2f; // ����0.8?2.0m

                    XMFLOAT3 end = XMFLOAT3(
                        start.x + sinf(angle) * cosf(pitch) * dist,
                        start.y + sinf(pitch) * dist,
                        start.z + cosf(angle) * cosf(pitch) * dist
                    );

                    // �p�`�p�`���ł���V�A���̃C�i�Y�}��`��
                    player->DrawLightningBolt(start, end, 0.02f, XMFLOAT4(0.0f, 2.0f, 2.8f, 1.0f));
                }
            }
        }
        return;
    }

    XMMATRIX worldMatrix =
        XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
        XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z) *
        XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    Renderer::SetWorldMatrix(worldMatrix);

    Renderer::DrawCube(worldMatrix, m_Texture);
}

// ������������������������������������������������������������������������������������������
// ���j�����i��d�J�E���g�h�~�@�\�t���j
// ������������������������������������������������������������������������������������������
void Enemy::Defeat(float colorR, float colorG, float colorB)
{
    if (m_IsDefeatedCounted) return;
    m_IsDefeatedCounted = true;

    // �X�R�A���Z�ƌ��j���C���N�������g
    GameRule::OnEnemyDefeated(m_ScoreValue);

    // �X�R�A�|�b�v�A�b�v�\��
    ScorePopupSystem::AddPopup(m_Position.x, m_Position.y + 1.0f, m_Position.z, m_ScoreValue, colorR, colorG, colorB);
}

XMFLOAT3 Enemy::GetEmissive() const
{
    if (m_IsSandbag) {
        return XMFLOAT3(3.0f, 2.0f, 0.0f);
    }
    return GameObject::GetEmissive();
}

void Enemy::OnHit(const HitInfo& info)
{
    if (m_EnemyState == EnemyState::DEFEATED || m_EnemyState == EnemyState::BLOWN_AWAY) return;

    SetVelocity(info.knockbackVel);
    SetEnemyState(EnemyState::BLOWN_AWAY);

    if (info.setLightning) {
        SetLightning(true);
    }

    Defeat(info.popupColor.x, info.popupColor.y, info.popupColor.z);
}
