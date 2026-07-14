#include "explosion_system.h"
#include "enemy.h"
#include "boss_enemy.h"
#include "camera.h"
#include "game_rule.h"
#include "manager.h"
#include "math_helper.h"
#include "score_popup.h"
#include "shockwave.h"
#include "game_constants.h"

// ������������������������������������������������������������������������������������������
// �����𔭐��������͂̓G�𐁂���΂�
// ������������������������������������������������������������������������������������������
void ExplosionSystem::TriggerExplosion(const DirectX::XMFLOAT3& center)
{
    float explosionRadius = Constants::Explosion::RADIUS; // �����̗L�����a
    float baseForce       = Constants::Explosion::BASE_FORCE;  // �����̊�{�З�

    // �J�����V�F�C�N�Ŕ����̃C���p�N�g����o
    if (g_Camera) {
        g_Camera->Shake(1.2f, 25);
    }

    // �������d�g��i�r�W���A���G�t�F�N�g�̂݁AY���W��n�ʂɔ��킹�A���ԍ���3�{�̐Ԃ��g�䂪�L����j
    XMFLOAT3 shockPos = center;
    shockPos.y = -0.95f; // �n�ʂ̍����Ɋ��S�N�����v

    ShockwaveSystem::AddShockwave(shockPos, explosionRadius,        2.5f, 0.3f, 0.0f, 30, 0.0f, 0);
    ShockwaveSystem::AddShockwave(shockPos, explosionRadius * 0.75f, 2.5f, 0.3f, 0.0f, 24, 0.0f, 6);
    ShockwaveSystem::AddShockwave(shockPos, explosionRadius * 0.50f, 2.5f, 0.3f, 0.0f, 18, 0.0f, 12);

    // �}�l�[�W���[����G�L���b�V�����X�g��擾���đ���
    for (Enemy* enemy : Manager::GetEnemyList()) {
        if (!enemy || enemy->IsDestroy()) continue;

        EnemyState oldState = enemy->GetEnemyState();
        // ���łɌ��j�ς݁A�܂��͊��ɐ������ł���G�͏��O
        if (oldState == EnemyState::DEFEATED || oldState == EnemyState::BLOWN_AWAY) continue;

        XMFLOAT3 ePos = enemy->GetPosition();
        float dx = ePos.x - center.x;
        float dy = ePos.y - center.y;
        float dz = ePos.z - center.z;
        float distSq = dx * dx + dy * dy + dz * dz;

        // �����͈͓̔�ɓ����Ă��邩����
        if (distSq < explosionRadius * explosionRadius) {
            float dist = sqrtf(distSq);
            if (dist < 0.01f) dist = 0.01f;

            // ���������i���S�ɋ߂��قǋ����͂�󂯂�j
            float attenuation = (explosionRadius - dist) / explosionRadius;

            // XZ���ʂł̐�����ԕ����x�N�g��
            XMFLOAT3 dir = XMFLOAT3(dx / dist, 0.0f, dz / dist);

            // �������x�x�N�g���i�����x�N�g�� �{ �ł��グ�́j
            float force = baseForce * attenuation;
            XMFLOAT3 vel = XMFLOAT3(dir.x * force, 1.0f * attenuation + 0.4f, dir.z * force);

            HitInfo hitInfo;
            hitInfo.hitSourcePos = center;
            if (enemy->GetObjectType() == ObjectType::Boss) {
                hitInfo.damage = Constants::Explosion::BOSS_DAMAGE;
                enemy->OnHit(hitInfo);
            } else {
                hitInfo.damage = 1;
                hitInfo.knockbackVel = vel;
                hitInfo.popupColor = {2.5f, 0.2f, 0.0f};
                enemy->OnHit(hitInfo);
            }
        }
    }
}
