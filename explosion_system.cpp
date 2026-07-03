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

// ─────────────────────────────────────────────
// 爆発を発生させ周囲の敵を吹き飛ばす
// ─────────────────────────────────────────────
void ExplosionSystem::TriggerExplosion(const DirectX::XMFLOAT3& center)
{
    float explosionRadius = Constants::Explosion::RADIUS; // 爆発の有効半径
    float baseForce       = Constants::Explosion::BASE_FORCE;  // 爆風の基本威力

    // カメラシェイクで爆発のインパクトを演出
    if (g_Camera) {
        g_Camera->Shake(1.2f, 25);
    }

    // 爆発多重波紋（ビジュアルエフェクトのみ、Y座標を地面に這わせ、時間差で3本の赤い波紋が広がる）
    XMFLOAT3 shockPos = center;
    shockPos.y = -0.95f; // 地面の高さに完全クランプ

    ShockwaveSystem::AddShockwave(shockPos, explosionRadius,        2.5f, 0.3f, 0.0f, 30, 0.0f, 0);
    ShockwaveSystem::AddShockwave(shockPos, explosionRadius * 0.75f, 2.5f, 0.3f, 0.0f, 24, 0.0f, 6);
    ShockwaveSystem::AddShockwave(shockPos, explosionRadius * 0.50f, 2.5f, 0.3f, 0.0f, 18, 0.0f, 12);

    // マネージャーからオブジェクト一覧を取得して走査
    for (GameObject* obj : Manager::GetGameObjectList()) {
        if (!obj) continue;
        
        if (obj->GetObjectType() != ObjectType::Enemy) continue;
        Enemy* enemy = static_cast<Enemy*>(obj);
        if (enemy->IsDestroy()) continue;

        EnemyState oldState = enemy->GetEnemyState();
        // すでに撃破済み、または既に吹き飛んでいる敵は除外
        if (oldState == EnemyState::DEFEATED || oldState == EnemyState::BLOWN_AWAY) continue;

        XMFLOAT3 ePos = enemy->GetPosition();
        float dx = ePos.x - center.x;
        float dy = ePos.y - center.y;
        float dz = ePos.z - center.z;
        float distSq = dx * dx + dy * dy + dz * dz;

        // 爆風の範囲内に入っているか判定
        if (distSq < explosionRadius * explosionRadius) {
            float dist = sqrtf(distSq);
            if (dist < 0.01f) dist = 0.01f;

            // 距離減衰（中心に近いほど強い力を受ける）
            float attenuation = (explosionRadius - dist) / explosionRadius;

            // XZ平面での吹き飛ぶ方向ベクトル
            XMFLOAT3 dir = XMFLOAT3(dx / dist, 0.0f, dz / dist);

            // 爆風速度ベクトル（水平ベクトル ＋ 打ち上げ力）
            float force = baseForce * attenuation;
            XMFLOAT3 vel = XMFLOAT3(dir.x * force, 1.0f * attenuation + 0.4f, dir.z * force);

            // ボスは Defeat() を直接呼ばず、フェーズ保護を経由した ApplyBossDamage でダメージを与える
            if (enemy->GetObjectType() == ObjectType::Boss) {
                BossEnemy* boss = static_cast<BossEnemy*>(enemy);
                boss->ApplyBossDamage(Constants::Explosion::BOSS_DAMAGE, center); // 爆発ダメージ
                // ボスは吹き飛ばさない
            } else {
                // 撃破処理（爆発・赤色ポップアップ）
                enemy->Defeat(2.5f, 0.2f, 0.0f);

                // 敵に爆風の速度と吹き飛び状態を設定
                enemy->SetVelocity(vel);
                enemy->SetEnemyState(EnemyState::BLOWN_AWAY);
            }
        }
    }
}
