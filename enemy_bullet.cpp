#include "enemy_bullet.h"
#include "renderer.h"
#include "resource_manager.h"
#include "player.h"
#include "manager.h"
#include "math_helper.h"
#include "collision.h"
#include "enemy.h"
#include "wall.h"
#include "camera.h"
#include "shockwave.h"
#include "boss_enemy.h"
#include "score_popup.h"
#include "game_rule.h"

// ������������������������������������������������������������������������������������������
// ������
// ������������������������������������������������������������������������������������������
void EnemyBullet::Init()
{
    m_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(0.4f, 0.4f, 0.4f); // �e�͏�����
    m_Size     = XMFLOAT3(0.4f, 0.4f, 0.4f);
    m_Life     = BULLET_LIFE;
    m_Speed    = BULLET_SPEED;
    m_Destroy  = false;
    m_IsPlayerOwned = false;
    m_EmissiveColor = XMFLOAT3(2.5f, 0.5f, 0.0f);

    // �`��ɂ� enemy.png �𗬗p���A�C���X�^���X�`��ŕ`��
    // �����[�X�r���h���� Assets/texture/ �T�u�t�H���_����ǂݍ���
#ifdef NDEBUG
    m_RenderComponent = RenderComponent("Assets/texture/enemy.png", MeshType::Cube, true);
#else
    m_RenderComponent = RenderComponent("enemy.png", MeshType::Cube, true);
#endif
}

// ������������������������������������������������������������������������������������������
// �I������
// ������������������������������������������������������������������������������������������
void EnemyBullet::Uninit()
{
}

// ������������������������������������������������������������������������������������������
// �X�V����
// ������������������������������������������������������������������������������������������
void EnemyBullet::Update()
{
    // �����ړ�
    m_Position.x += m_Direction.x * m_Speed;
    m_Position.y += m_Direction.y * m_Speed;
    m_Position.z += m_Direction.z * m_Speed;

    Player* player = Manager::GetGameObject<Player>();

    // ������ �@ �v���C���[���ˏ�ԁi�����e�j���ǂ����̕��� ������
    if (!m_IsPlayerOwned)
    {
        // === �G�̒e�F�v���C���[�Ƃ̏Փ˔��� ===
        if (player)
        {
            XMFLOAT3 diff = m_Position - player->GetPosition();
            float dist = MathHelper::Length(diff);
            float limitDist = GetRadius() + player->GetRadius() * 0.7f;

            if (dist < limitDist)
            {
                if (player->IsParryActive())
                {
                    player->ExecuteParryCounter(m_Position);
                    SetDestroy();
                    return;
                }
                else if (player->IsJustDodgeActive())
                {
                    m_IsPlayerOwned = true;
                    m_Speed *= 1.2f;
                    SetDamage(2);
                    m_EmissiveColor = XMFLOAT3(0.0f, 1.8f, 2.5f);
                    m_Direction = player->GetForwardVector();
                    m_Life = BULLET_LIFE;

                    Manager::StartSlowMotion(180);

                    Manager::AddHitStop(12);
                    if (g_Camera)
                    {
                        g_Camera->Shake(0.3f, 15);
                    }

                    ShockwaveSystem::AddShockwave(player->GetPosition(), 10.0f, 0.0f, 3.5f, 5.0f, 30, 0.0f, 0);

                    player->ResetDashCooldown();
                    player->DisableWarpSlash();
                    // �W���X�g��𐬌����Ƀ^�b�N����L�����i���u�e�E�{�X�e��킸�j
                    player->EnableTackle(90);

                    GameRule::AddScore(150);
                    ScorePopupSystem::AddPopup(player->GetPosition().x, player->GetPosition().y + 1.0f, player->GetPosition().z, 150, 0.0f, 0.8f, 1.5f);

                    return;
                }
                else if (player->IsDashing())
                {
                    if (m_IsBossBullet)
                    {
                        // �{�X�̒e��_�b�V�����F�^�b�N���U����L����
                        player->EnableTackle(90); // 1.5�b�ԃ^�b�N���L��
                        ShockwaveSystem::AddShockwave(player->GetPosition(), 5.0f, 0.0f, 2.0f, 3.0f, 20, 0.0f, 0);
                        if (g_Camera) g_Camera->Shake(0.2f, 8);
                    }
                    // �ʏ�G�̒e�͂��̂܂܂��蔲���i�^�b�N�������j
                    return;
                }
                else if (player->IsGuardActive())
                {
                    if (m_IsBossBullet)
                    {
                        // �{�X�̒e��ʏ�K�[�h�Ŏ󂯂��獂�_���[�W�Ŕ��˔���
                        m_IsPlayerOwned = true;
                        m_Speed *= 1.5f;
                        SetDamage(4); // �W���X�g�h�b�W��2��荂��4�_���[�W
                        m_EmissiveColor = XMFLOAT3(1.5f, 0.0f, 2.5f); // ���F�Ŕ��˒e����ʂ��₷��
                        m_Direction = player->GetForwardVector();
                        m_Life = BULLET_LIFE;

                        Manager::AddHitStop(3);
                        if (g_Camera) g_Camera->Shake(0.2f, 8);
                        ShockwaveSystem::AddShockwave(player->GetPosition(), 6.0f, 0.0f, 2.5f, 4.0f, 20, 0.0f, 0);
                        return;
                    }
                    else
                    {
                        // �ʏ�K�[�h�F�_���[�W�����A�e����
                        Manager::AddHitStop(2);
                        SetDestroy();
                        return;
                    }
                }
                else
                {
                    // ������ ��e�i�ʏ�_���[�W�j ������
                    HitInfo hitInfo;
                    hitInfo.damage = 1;
                    hitInfo.hitSourcePos = m_Position;
                    player->OnHit(hitInfo);
                    SetDestroy();
                    return;
                }
            }
        }
    }
    else
    {
        // === ���˂��ꂽ�����̒e�F�G�l�~�[�iEnemy�j�Ƃ̏Փ˔��� ===
        for (GameObject* obj : Manager::GetGameObjectList())
        {
            if (!obj || obj->IsDestroy() || obj == this || obj == player) continue;
            if (obj->GetObjectType() != ObjectType::Enemy && obj->GetObjectType() != ObjectType::Boss) continue;

            Enemy* enemy = static_cast<Enemy*>(obj);

            // ���łɓ|����Ă���G�l�~�[�͑ΏۊO
            EnemyState eState = enemy->GetEnemyState();
            if (eState == EnemyState::DEFEATED || eState == EnemyState::BLOWN_AWAY || eState == EnemyState::VACUUMED) continue;

            // �e�ƓG�̋�������
            XMFLOAT3 diff = m_Position - enemy->GetPosition();
            float dist = MathHelper::Length(diff);
            float limitDist = GetRadius() + enemy->GetRadius();

            if (dist < limitDist)
            {
                HitInfo hitInfo;
                hitInfo.hitSourcePos = m_Position;
                if (enemy->GetObjectType() == ObjectType::Boss)
                {
                    hitInfo.damage = GetDamage();
                    enemy->OnHit(hitInfo);
                }
                else
                {
                    float force = 0.35f;
                    XMFLOAT3 pushVel = XMFLOAT3(m_Direction.x * force, 0.18f, m_Direction.z * force);
                    hitInfo.damage = 1;
                    hitInfo.knockbackVel = pushVel;
                    enemy->OnHit(hitInfo);
                }

                // �e�͏���
                SetDestroy();
                return;
            }
        }
    }

    // �ǂƂ̏Փ˔���i�ǂɓ�����������ŁB�����e�ł���l�j
    for (GameObject* obj : Manager::GetGameObjectList())
    {
        if (!obj || obj->IsDestroy() || obj == this || obj == player) continue;
        
        // �G�A�{�X�A�n�ʁA���̒e�ۂ͏��O�i�ǂȂǂ̏�Q���݂̂ƏՓ˂�����j
        ObjectType type = obj->GetObjectType();
        if (type == ObjectType::Enemy || 
            type == ObjectType::Boss || 
            type == ObjectType::Field || 
            type == ObjectType::Bullet) 
        {
            continue;
        }

        if (Collision::CheckAABB(this, obj))
        {
            SetDestroy();
            return;
        }
    }

    // �����̃J�E���g�_�E��
    m_Life--;
    if (m_Life <= 0)
    {
        SetDestroy();
    }
}

// ������������������������������������������������������������������������������������������
// �`�揈���i�V���h�E/�A�E�g���C���p�X�p�j
// ������������������������������������������������������������������������������������������
void EnemyBullet::Draw()
{
    // �ʏ�`��� RenderSystem ���ňꊇ�ōs���邽�߁A�����ł͉�����Ȃ�
    if (!Renderer::IsShadowMode() && !Renderer::IsOutlineMode()) return;

    XMMATRIX worldMatrix =
        XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
        XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z) *
        XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    Renderer::SetWorldMatrix(worldMatrix);

    ID3D11ShaderResourceView* tex = ResourceManager::GetTexture("enemy.png");
    Renderer::DrawCube(worldMatrix, tex);
}
