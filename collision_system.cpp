#include "collision_system.h"
#include <list>
#include "manager.h"
#include "player.h"
#include "enemy.h"
#include "item.h"
#include "wall.h"
#include "camera.h"
#include "collision.h"
#include "math_helper.h"
#include "game_rule.h"
#include "explosion_system.h"

// ─────────────────────────────────────────────
// 衝突判定および物理連鎖の更新処理
// ─────────────────────────────────────────────
void CollisionSystem::Update()
{
    // プレイヤーを取得
    Player* player = Manager::GetGameObject<Player>();
    if (!player) return;

    XMFLOAT3 pPos = player->GetPosition();
    const std::list<GameObject*>& gameObjects = Manager::GetGameObjectList();

    // ─── プレイヤーとアイテムの衝突判定（吸引アイテム取得） ───
    for (GameObject* obj : gameObjects) {
        if (!obj || obj->IsDestroy()) continue;
        Item* item = dynamic_cast<Item*>(obj);
        if (item) {
            XMFLOAT3 iPos = item->GetPosition();
            float dx = pPos.x - iPos.x;
            float dy = pPos.y - iPos.y;
            float dz = pPos.z - iPos.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dist < 1.2f) {
                player->SetHasVacuumItem(true);
                item->SetDestroy();
                OutputDebugStringA("[CollisionSystem] 吸引アイテムを取得しました！\n");
            }
        }
    }

    // ─── 飛んでいる敵 → 他の敵・壁への連鎖衝突 ────────────────
    std::list<Enemy*> flyingEnemies;
    for (GameObject* obj : gameObjects) {
        Enemy* e = dynamic_cast<Enemy*>(obj);
        if (e && !e->IsDestroy() && e->GetEnemyState() == EnemyState::FLYING)
            flyingEnemies.push_back(e);
    }

    // フリーズ防止：1フレームに発生する爆発は1回のみに制限する
    bool explosionThisFrame = false;

    for (Enemy* flying : flyingEnemies) {
        if (flying->IsDestroy()) continue; // 既に撃破済みならスキップ
        if (flying->GetEnemyState() != EnemyState::FLYING) continue;
        XMFLOAT3 fPos = flying->GetPosition();

        // ─── 着地 + 速度がほぼ0になったら爆発トリガー ───
        if (!explosionThisFrame && flying->IsExplosive() && fPos.y <= -0.3f) {
            XMFLOAT3 curVel = flying->GetVelocity();
            float speedSq = curVel.x * curVel.x + curVel.z * curVel.z;
            if (speedSq < 0.04f) {
                ExplosionSystem::TriggerExplosion(fPos);
                flying->SetEnemyState(EnemyState::DEFEATED);
                flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
                explosionThisFrame = true;
                continue; // この敵の衝突判定は終了
            }
        }

        for (GameObject* obj : gameObjects) {
            if (obj == flying || obj->IsDestroy()) continue;

            // --- 壁との衝突判定 ---
            Wall* wall = dynamic_cast<Wall*>(obj);
            if (wall) {
                if (Collision::CheckAABB(flying, wall)) {
                    if (!explosionThisFrame && flying->IsExplosive()) {
                        ExplosionSystem::TriggerExplosion(flying->GetPosition());
                        flying->SetEnemyState(EnemyState::DEFEATED);
                        flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
                        explosionThisFrame = true;
                    } else {
                        GameRule::OnEnemyDefeated(flying->GetScoreValue());
                        flying->SetEnemyState(EnemyState::DEFEATED);
                        XMFLOAT3 oldVel = flying->GetVelocity();
                        flying->SetVelocity(XMFLOAT3(oldVel.x * -0.3f, 0.1f, oldVel.z * -0.3f));
                    }

                    Manager::AddHitStop(6); 
                    if (g_Camera) g_Camera->Shake(0.15f, 10); 
                    break;
                }
                continue;
            }

            // --- 他の敵との衝突判定 ---
            Enemy* target = dynamic_cast<Enemy*>(obj);
            if (!target) continue;

            EnemyState targetState = target->GetEnemyState();
            if (targetState == EnemyState::DEFEATED || targetState == EnemyState::BLOWN_AWAY) continue;

            if (!Collision::CheckSphere(flying, target)) continue; // 衝突なし
            XMFLOAT3 tPos = target->GetPosition();

            // 爆弾属性の敵同士の衝突は爆発しない
            if (flying->IsExplosive() && target->IsExplosive()) continue;

            // 爆弾状態の敵が通常の敵（NORMAL）に当たったら即爆発！
            if (!explosionThisFrame && flying->IsExplosive() && targetState == EnemyState::NORMAL) {
                ExplosionSystem::TriggerExplosion(flying->GetPosition());
                flying->SetEnemyState(EnemyState::DEFEATED);
                flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
                explosionThisFrame = true;
                break;
            }

            // ─── 通常の敵に衝突 ───
            if (target->GetEnemyState() == EnemyState::NORMAL) {
                XMFLOAT3 dir = MathHelper::Normalize(tPos - fPos);
                
                // ぶつかられた敵（target）をさらに高く上に吹き飛ばす
                XMFLOAT3 vel = dir * 0.4f;
                vel.y = 0.35f;
                target->SetVelocity(vel);
                target->SetEnemyState(EnemyState::FLYING);
                
                float rotY = atan2f(-dir.x, -dir.z);
                target->SetRotation(XMFLOAT3(0.0f, rotY, 0.0f));

                Manager::AddHitStop(8); 
                if (g_Camera) g_Camera->Shake(0.3f, 12); 
                break;
            }
        }
    }
}
