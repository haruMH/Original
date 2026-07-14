#include "enemy.h"
#include "enemy_affix.h"
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
    m_Affix        = nullptr;
    m_IsDefeatedCounted = false;
    m_ScoreValue   = Constants::Enemy::DEFAULT_SCORE;

    m_Texture = ResourceManager::GetTexture("enemy.png");
    
    // R|[lgwł̕`p[^
    m_RenderComponent = RenderComponent("enemy.png", MeshType::Cube, true);
}

void Enemy::Uninit()
{
}

void Enemy::Update()
{
    if (m_Affix) m_Affix->Update(this);

    if (m_EnemyState == EnemyState::FLYING) {
        // �E��E��E�C�E�i�E��E�C�E��E�R�E�j�E�ŏ��E�X�E�Ɍ��E��E��E��E��E��E��E��E�i�E��E��E�C�E��E��E��E��E��E��E��E�Ĕ�т��E��E��E��E�h�E�~�E�j
        MathHelper::ScaleXZ(m_Velocity, Constants::Enemy::FLYING_AIR_RESISTANCE);
        // �E�d�E�͂̓K�E�p�E�iY�E��E��E�̗��E��E��E�j
        m_VelocityY -= Constants::Enemy::FLYING_GRAVITY; 

        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;
        m_Position.y += m_VelocityY;

        // �E�X�E�P�E�[�E��E��E�ɉ��E��E��E��E��E��E��E�ʂ̍��E��E��E��E�v�E�Z�E�i�E�߂荞�E�ݖh�E�~�E�j
        float baseOffset = (m_Size.y * m_Scale.y - 1.0f) * 0.5f;
        float flightFloorY = -0.3f + baseOffset;

        // �E�n�E�ʁi�E��E��E�j�E�ւ̒��E�n�E�N�E��E��E��E��E�v
        if (m_Position.y < flightFloorY) {
            // �E�ڒn�E��E��E��E��E�u�E�Ԃ̏��E��E��E�i�E��E��E��E��E��E��E�̏\�E��E��E�ȑ��E�x�E��E��E��E��E��E�ꍁE��j
            if (m_VelocityY < -0.05f) {
                // �E��E��E�剻�E�G�E�l�E�~�E�[�E��E��E�@�E��E��E���E��E�ꂽ�E�ۂ̃J�E��E��E��E��E�V�E�F�E�C�E�N�E�ƃq�E�b�E�g�E�X�E�g�E�b�E�v
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

        // �E��E��E�x�E��E��E�\�E��E��E�ɗ��E��E��E�A�E��E��E���E��E��E��E��E��E��E�n�E�ʂɒ��E�n�E��E��E�Ă��E��E�Ȃ�NORMAL�E�ɖ߂��E�i�E��E��E��E�G�E�l�E�~�E�[�E�͏��E�Łj
        // �E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E�G�E��E� game_scene �E��E��E�Ŕ��E��E��E��E��E��E��E��E��E�邽�E�߁A�E��E��E��E��E�ł͑J�E�ڂ��E�Ȃ�
        if (m_Position.y <= flightFloorY && abs(m_Velocity.x) < 0.05f && abs(m_Velocity.z) < 0.05f) {
            if (!IsExplosive()) {
                if (m_Scale.x > 2.0f) {
                    // �E��E��E�剻�E�G�E�l�E�~�E�[�E�͔�яI�E��E��E��E�ăX�E�s�E�[�E�h�E��E�0�E�ɂȂ��E��E��E��E��E��E�ł��E��E��E��E�
                    m_EnemyState = EnemyState::DEFEATED;
                    m_Velocity   = XMFLOAT3(0.0f, 0.0f, 0.0f);
                    m_VelocityY  = 0.0f;

                    // �E�X�E�s�E�[�E�h�E��E�0�E�ɂȂ�A�E��E��E�ł��E��E�u�E�Ԃɑ��E�d�E�g�E��E�Ռ��E�g�E�𔭐��E��E��E��E��E��E�iY�E��E��E�W�E��E�n�E�ʂ̍��E��E��E�ɋ��E��E��E�Œ肵�E�A�E�K�E�͂ƃf�E�B�E��E��E�C�E��E�g�E��E�j
                    XMFLOAT3 shockPos = m_Position;
                    shockPos.y = -0.95f; // �E�n�E�ʂ̍��E��E��E�Ɋ��E�S�E�N�E��E��E��E��E�v

                    ShockwaveSystem::AddShockwave(shockPos, 15.0f, 1.8f, 0.9f, 0.0f, 32, 1.4f, 0);
                    ShockwaveSystem::AddShockwave(shockPos, 11.0f, 1.8f, 0.9f, 0.0f, 26, 0.9f, 7);
                    ShockwaveSystem::AddShockwave(shockPos, 7.0f, 1.8f, 0.9f, 0.0f, 20, 0.5f, 14);
                } else {
                    m_Position.y = -0.5f + baseOffset; // �E�Î~�E��E��E�ɖ{�E��E��E�̒n�E�ʂ̍��E��E��E�ɖ��E��E��E��E��E��E��E��E�
                    m_Velocity   = XMFLOAT3(0, 0, 0);
                    m_VelocityY  = 0.0f;
                    m_EnemyState = EnemyState::NORMAL;
                    m_UprightTimer = 60;  // 1�E�b�E��E�ɂ��E��E��E��E��E�N�E��E��E�オ�E��E�悤�E�Ƀ^�E�C�E�}�E�[�E��E�Z�E�b�E�g
                }
            }
            // �E��E��E��E��E��E��E��E��E�̏ꍇ�E��E� FLYING �E�̂܂� �E��E� game_scene �E�̒��E�n+�E��E��E�x0�E�`�E�F�E�b�E�N�E�Ŕ��E��E��E��E��E��E��E��E�
        }

        m_Rotation.x += 0.2f;
        m_Rotation.z += 0.15f;
    }
    else if (m_EnemyState == EnemyState::DEFEATED) {
        m_Scale *= 0.85f;                                  // 3�E��E��E��E��E��E��E�ɏk�E��E��E�I
        m_Rotation += DirectX::XMFLOAT3(0.5f, 0.5f, 0.5f); // 3�E��E��E��E��E��E��E�ɍ��E��E��E��E�]�E�I
        MathHelper::ScaleXZ(m_Velocity, 0.9f);             // �E��E��E�C�E�ɂ�錸�E��E�

        // �E��E��E��E��E��E�Ԋ��E��E��E��E��E��E��E��E��E��E��E��E�ێ��E��E��E�ăX�E��E��E�C�E�h�E��E��E��E��E��E�
        m_Velocity.x *= 0.9f;
        m_Velocity.z *= 0.9f;
        
        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;

        if (m_Scale.x < 0.05f) {
            SetDestroy(); // �E��E��E�S�E�ɏ��E��E�
        }
    }
    else if (m_EnemyState == EnemyState::NORMAL) {
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
    else if (m_EnemyState == EnemyState::VACUUMED) {
        // �E�͂�ł��E��E�G�E��ES�E�Ƀu�E��E��E�b�E�N�E�z�E�[�E��E��E�̂悤�E�Ɍ��E�]�E��E��E�鋓��E�
        Player* player = Manager::GetGameObject<Player>();
        if (player) {
                // �E�v�E��E��E�C�E��E��E�[�E��E��E�X�E�s�E��E��E��E�Ԃ�I�E��E��E�Ă��E��E��E��E�ʏ�ɖ߂�
                if (player->GetState() != PlayerState::SPINNING) {
                    m_EnemyState = EnemyState::NORMAL;
                    return;
                }

                // �E�u�E��E��E�b�E�N�E�z�E�[�E��E��E�̒��E�S = �E�͂�ł��E��E�G�E�̈ʒu�E�i�E�Ȃ��E��E�΃v�E��E��E�C�E��E��E�[�E�ʒu�E�j
                Enemy* grabbed = player->GetGrabbedEnemy();
                DirectX::XMFLOAT3 pivotPos = grabbed
                    ? grabbed->GetPosition()
                    : player->GetPosition();

                DirectX::XMFLOAT3 diff = m_Position - pivotPos;
                float dist = MathHelper::Length(diff);

                // �E��E��E�X�E�ɒ͂܂�Ă��E��E�G�E�Ɉ��E��E��E�񂹂�i�E��E��E�a 2.5 �E�Ɏ��E��E��E�j
                float targetDist = 2.5f;
                dist = MathHelper::Lerp(dist, targetDist, 0.06f);

                // �E�͂�ł��E��E�G�E�̃X�E�s�E��E��E��E��E�x�E�Ō��E�]�E��E��E��E��E��E�
                float angle = atan2f(diff.x, diff.z);
                angle += player->GetAngularVelocity();

                m_Position.x = pivotPos.x + sinf(angle) * dist;
                m_Position.z = pivotPos.z + cosf(angle) * dist;
                m_Position.y = pivotPos.y + 0.3f; // �E�͂�ł��E��E�G�E�̍��E��E��E�ɍ��E�����E�ĕ��E��E��E��E�

                m_Velocity = DirectX::XMFLOAT3(0, 0, 0); // �E�z�E��E��E��E��E�܂�Ă��E��E�Ԃ͑��E�x�E��E�[�E��E��E�ɂ��E��E�

                // �E�z�E��E��E��E��E��E�Ă��E�鉉�o�E�Ŏ��E�]�E��E��E��E��E��E�
                m_Rotation.y += 0.15f;
            }
        }
    else if (m_EnemyState == EnemyState::BLOWN_AWAY) {
        // �E��E��E��E��E�Ő��E��E��E��E�΂��E��E�镨�E��E��E��E��E��E�
        MathHelper::ScaleXZ(m_Velocity, 0.95f); // �E��E�C�E��E�R
        m_VelocityY -= 0.02f; // �E�d�E��E�
        
        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;
        m_Position.y += m_VelocityY;
        
        // �E�X�E�P�E�[�E��E��E�ɉ��E��E��E��E��E��E��E�ʂ̍��E��E��E��E�v�E�Z�E�i�E�߂荞�E�ݖh�E�~�E�j
        float baseOffset = (m_Size.y * m_Scale.y - 1.0f) * 0.5f;
        float floorY = -0.5f + baseOffset;
        if (m_Position.y < floorY) {
            m_Position.y = floorY;
            m_VelocityY = 0.0f;
        }
        
        // �E��E��E��E��E��E��E��E�]�E��E��E��E�
        m_Rotation.x += 0.3f;
        m_Rotation.y += 0.2f;
        m_Rotation.z += 0.25f;
        
        // �E�n�E�ʂɌ��E�˂��E�āA�E��E��E�x�E��E��E�\�E��E��E�ɒx�E��E��E�Ȃ��E��E��E�猂�j�E��E�ԁiDEFEATED�E�j�E�ɂ��E�ď��E�ł��E��E��E��E�
        if (m_Position.y <= floorY && abs(m_Velocity.x) < 0.05f && abs(m_Velocity.z) < 0.05f) {
            m_EnemyState = EnemyState::DEFEATED;
            m_Velocity = DirectX::XMFLOAT3(0, 0, 0);
        }
    }
}

void Enemy::Draw()
{
    // �E�ʏ�p�E�X�E�̏ꍇ�E��E� RenderSystem �E��E��E�ňꊇ�E�`�E�悷�E�邽�E�ߖ{�E�͕̂`�E�悵�E�Ȃ��E��E��E�A
    // �E�d�E��E��E��E��E��E��E�im_IsLightning�E�j�E��E��E��E��E��E�ꍁE��͎��E�͂ɃX�E�p�E�[�E�N�E�G�E�t�E�F�E�N�E�g�E��E�`�E�悷�E��E�
    if (!Renderer::IsShadowMode() && !Renderer::IsOutlineMode())
    {
        if (IsLightning())
        {
            Player* player = Manager::GetGameObject<Player>();
            if (player)
            {
                // �E�G�E�̒��E�S�E��E��E��E��E��E�͂̋󒆂� 2?3�E�{ �E�قǃr�E��E��E�r�E��E��E�ƉΉԂ�U�E�炷
                XMFLOAT3 start = m_Position;
                start.y += 0.3f; // �E��E��E�S�E�t�E��E�

                int sparks = 2 + (rand() % 2); 
                for (int i = 0; i < sparks; i++)
                {
                    float angle = ((float)rand() / RAND_MAX) * XM_2PI;
                    float pitch = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * XM_PIDIV4; // �E�㉺45�E�x
                    float dist = 0.8f + ((float)rand() / RAND_MAX) * 1.2f; // �E��E��E��E�0.8?2.0m

                    XMFLOAT3 end = XMFLOAT3(
                        start.x + sinf(angle) * cosf(pitch) * dist,
                        start.y + sinf(pitch) * dist,
                        start.z + cosf(angle) * cosf(pitch) * dist
                    );

                    // �E�p�E�`�E�p�E�`�E��E��E�ł��E��E�V�E�A�E��E��E�̃C�E�i�E�Y�E�}�E��E�`�E��E�
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

// �E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E�
// �E��E��E�j�E��E��E��E��E�i�E��E�d�E�J�E�E�E��E��E�g�E�h�E�~�E�@�E�\�E�t�E��E��E�j
// �E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E��E�
void Enemy::Defeat(float colorR, float colorG, float colorB)
{
    if (m_IsDefeatedCounted) return;
    m_IsDefeatedCounted = true;

    // �E�X�E�R�E�A�E��E��E�Z�E�ƌ��E�j�E��E��E�C�E��E��E�N�E��E��E��E��E��E��E�g
    GameRule::OnEnemyDefeated(m_ScoreValue);

    // �E�X�E�R�E�A�E�|�E�b�E�v�E�A�E�b�E�v�E�\�E��E�
    ScorePopupSystem::AddPopup(m_Position.x, m_Position.y + 1.0f, m_Position.z, m_ScoreValue, colorR, colorG, colorB);
}

XMFLOAT3 Enemy::GetEmissive() const
{
    if (m_Affix) {
        return m_Affix->GetEmissive();
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

bool Enemy::IsExplosive() const
{
    return m_Affix ? m_Affix->IsExplosive() : false;
}

void Enemy::SetExplosive(bool explosive)
{
    if (explosive) {
        m_Affix = std::make_shared<ExplosiveAffix>();
    } else if (IsExplosive()) {
        m_Affix = nullptr;
    }
}

bool Enemy::IsLightning() const
{
    return m_Affix ? m_Affix->IsLightning() : false;
}

void Enemy::SetLightning(bool lightning)
{
    if (lightning) {
        m_Affix = std::make_shared<LightningAffix>();
    } else if (IsLightning()) {
        m_Affix = nullptr;
    }
}

bool Enemy::IsSandbag() const
{
    return m_Affix ? m_Affix->IsSandbag() : false;
}

void Enemy::SetSandbag(bool enable)
{
    if (enable) {
        m_Affix = std::make_shared<SandbagAffix>();
    } else if (IsSandbag()) {
        m_Affix = nullptr;
    }
}

void SandbagAffix::Update(Enemy* enemy)
{
    if (enemy->GetEnemyState() == EnemyState::NORMAL) {
        enemy->SetVelocity(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
        m_Life--;
        if (m_Life <= 0) {
            enemy->Defeat();
            enemy->SetEnemyState(EnemyState::DEFEATED);
        }
    }
}
