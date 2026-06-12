#include "collision_system.h"
#include <list>
#include <algorithm> // std::findを使うため
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
#include "score_popup.h"

// ─────────────────────────────────────────────
// チェインライトニング（電撃連鎖）の処理
// ─────────────────────────────────────────────
static void TriggerChainLightning(const XMFLOAT3& startPos, Player* player)
{
    const std::list<GameObject*>& gameObjects = Manager::GetGameObjectList();
    XMFLOAT3 currentPos = startPos;
    
    // 連鎖回数
    const int maxChain = 5;
    float chainRadius = 8.0f; // 連鎖する半径

    std::vector<Enemy*> chainedEnemies;

    for (int chain = 0; chain < maxChain; chain++) {
        Enemy* nearest = nullptr;
        float nearestDistSq = chainRadius * chainRadius;

        for (GameObject* obj : gameObjects) {
            if (!obj || obj->IsDestroy() || obj == player) continue;
            if (obj->GetObjectType() != ObjectType::Enemy) continue;
            Enemy* enemy = static_cast<Enemy*>(obj);

            // すでに撃破済み、または今回の連鎖リストに含まれている敵は除外
            EnemyState eState = enemy->GetEnemyState();
            if (eState == EnemyState::DEFEATED || eState == EnemyState::BLOWN_AWAY) continue;
            if (std::find(chainedEnemies.begin(), chainedEnemies.end(), enemy) != chainedEnemies.end()) continue;

            XMFLOAT3 ePos = enemy->GetPosition();
            float dx = ePos.x - currentPos.x;
            float dy = ePos.y - currentPos.y;
            float dz = ePos.z - currentPos.z;
            float distSq = dx * dx + dy * dy + dz * dz;

            if (distSq < nearestDistSq) {
                nearestDistSq = distSq;
                nearest = enemy;
            }
        }

        if (nearest) {
            XMFLOAT3 nextPos = nearest->GetPosition();
            
            // イナズマラインをプレイヤーに登録して描画させる
            XMFLOAT3 lineStart = currentPos;
            XMFLOAT3 lineEnd = nextPos;
            lineStart.y += 0.3f;
            lineEnd.y += 0.3f;
            player->AddLightningEffect(lineStart, lineEnd);

            // 対象の敵を吹き飛ばす
            XMFLOAT3 dir = MathHelper::Normalize(nextPos - currentPos);
            // 放射状かつ上空へ吹き飛ばす
            XMFLOAT3 pushVel = XMFLOAT3(dir.x * 0.8f, 0.4f, dir.z * 0.8f);
            nearest->SetVelocity(pushVel);
            nearest->SetEnemyState(EnemyState::BLOWN_AWAY);
            nearest->SetLightning(true); // スパーク放電を有効化

            // スコア加算（チェインライトニング擃鉖）
            GameRule::OnEnemyDefeated(nearest->GetScoreValue());
            // シアン色ポップアップ（電撃連鎖演出）
            XMFLOAT3 nPos = nearest->GetPosition();
            ScorePopupSystem::AddPopup(
                nPos.x, nPos.y + 1.0f, nPos.z,
                nearest->GetScoreValue(),
                0.0f, 1.5f, 2.5f
            );
            
            chainedEnemies.push_back(nearest);
            currentPos = nextPos; // 次の連鎖の開始点にする
        } else {
            break; // 近くに敵がいなければ連鎖終了
        }
    }
}

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

    // ─── プレイヤーとアイテムの衝突判定（各種アイテム取得） ───
    for (GameObject* obj : gameObjects) {
        if (!obj || obj->IsDestroy()) continue;
        if (obj->GetObjectType() == ObjectType::Item) {
            Item* item = static_cast<Item*>(obj);
            XMFLOAT3 iPos = item->GetPosition();
            float dx = pPos.x - iPos.x;
            float dy = pPos.y - iPos.y;
            float dz = pPos.z - iPos.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dist < 1.2f) {
                switch (item->GetItemType()) {
                case ItemType::VACUUM:
                    player->SetHasVacuumItem(true);
                    OutputDebugStringA("[CollisionSystem] 吸引アイテムを取得しました！\n");
                    break;
                case ItemType::GIGANT:
                    player->SetHasGigantItem(true);
                    OutputDebugStringA("[CollisionSystem] 巨大化アイテムを取得しました！\n");
                    break;
                case ItemType::LIGHTNING:
                    player->SetHasLightningItem(true);
                    OutputDebugStringA("[CollisionSystem] 雷電アイテムを取得しました！\n");
                    break;
                }
                item->SetDestroy();
            }
        }
    }

    // ─── プレイヤーがスピン中のなぎ払い判定 ───
    if (player->GetState() == PlayerState::SPINNING) {
        Enemy* grabbed = player->GetGrabbedEnemy();
        if (grabbed && !grabbed->IsDestroy()) {
            XMFLOAT3 gPos = grabbed->GetPosition();
            float gRadius = grabbed->GetRadius();

            for (GameObject* obj : gameObjects) {
                if (!obj || obj->IsDestroy() || obj == player || obj == grabbed) continue;
                if (obj->GetObjectType() != ObjectType::Enemy) continue;
                Enemy* enemy = static_cast<Enemy*>(obj);

                // 対象エネミーがすでに倒されていたら除外
                EnemyState eState = enemy->GetEnemyState();
                if (eState == EnemyState::DEFEATED || eState == EnemyState::BLOWN_AWAY || eState == EnemyState::VACUUMED) continue;

                // 球体同士の衝突判定
                XMFLOAT3 ePos = enemy->GetPosition();
                float dx = ePos.x - gPos.x;
                float dy = ePos.y - gPos.y;
                float dz = ePos.z - gPos.z;
                float distSq = dx * dx + dy * dy + dz * dz;

                float minDist = gRadius + enemy->GetRadius();
                if (distSq < minDist * minDist) {
                    // なぎ払い衝突！
                    float dist = sqrtf(distSq);
                    if (dist < 0.01f) dist = 0.01f;
                    XMFLOAT3 dir = XMFLOAT3(dx / dist, 0.0f, dz / dist);

                    // 回転速度に応じた吹き飛ばし力
                    float spinSpeed = abs(player->GetAngularVelocity());
                    float force = 0.4f + spinSpeed * 2.2f; // 最低でもそこそこ飛ぶ
                    XMFLOAT3 pushVel = XMFLOAT3(dir.x * force, 0.25f, dir.z * force);

                    // 回転接線方向の力を少し加算
                    float spinDirSign = (player->GetAngularVelocity() >= 0.0f) ? 1.0f : -1.0f;
                    XMFLOAT3 tangent = XMFLOAT3(-dir.z, 0.0f, dir.x) * spinDirSign * force * 0.4f;
                    pushVel.x += tangent.x;
                    pushVel.z += tangent.z;

                    // 吹き飛ばす
                    enemy->SetVelocity(pushVel);
                    enemy->SetEnemyState(EnemyState::BLOWN_AWAY);

                    // 擃鉖スコア（スピンなぎ払い）
                    GameRule::OnEnemyDefeated(enemy->GetScoreValue());
                    // 黄金色ポップアップ（スピンなぎ払い撞撃）
                    XMFLOAT3 ePopPos = enemy->GetPosition();
                    ScorePopupSystem::AddPopup(
                        ePopPos.x, ePopPos.y + 1.0f, ePopPos.z,
                        enemy->GetScoreValue()
                    );

                    // ヒットインパクト演出（ヒットストップとカメラ揺れ）
                    Manager::AddHitStop(6);
                    if (g_Camera) g_Camera->Shake(0.18f + spinSpeed * 0.4f, 8);
                }
            }
        }
    }

    // ─── 飛んでいる敵 → 他の敵・壁への連鎖衝突 ────────────────
    std::list<Enemy*> flyingEnemies;
    for (GameObject* obj : gameObjects) {
        if (obj && !obj->IsDestroy() && obj->GetObjectType() == ObjectType::Enemy) {
            Enemy* e = static_cast<Enemy*>(obj);
            if (e->GetEnemyState() == EnemyState::FLYING) {
                flyingEnemies.push_back(e);
            }
        }
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
            if (obj->GetObjectType() == ObjectType::Wall) {
                Wall* wall = static_cast<Wall*>(obj);
                if (Collision::CheckAABB(flying, wall)) {
                    bool triggeredLightning = false;
                    if (flying->IsLightning()) {
                        TriggerChainLightning(flying->GetPosition(), player);
                        // 電撃属性は維持したまま撃破消滅へ移行し、スパークを散らし続ける
                        triggeredLightning = true;
                    }

                    if (!triggeredLightning && !explosionThisFrame && flying->IsExplosive()) {
                        ExplosionSystem::TriggerExplosion(flying->GetPosition());
                        flying->SetEnemyState(EnemyState::DEFEATED);
                        flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
                        explosionThisFrame = true;
                    } else {
                        GameRule::OnEnemyDefeated(flying->GetScoreValue());
                        // 黄金色ポップアップ（壁路激突擃鉖）
                        XMFLOAT3 fPopPos = flying->GetPosition();
                        ScorePopupSystem::AddPopup(
                            fPopPos.x, fPopPos.y + 1.0f, fPopPos.z,
                            flying->GetScoreValue()
                        );
                        flying->SetEnemyState(EnemyState::DEFEATED);
                        if (flying->GetScale().x > 2.0f) {
                            flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f)); // 巨大エネミーは壁衝突時に跳ね返らない
                        } else {
                            XMFLOAT3 oldVel = flying->GetVelocity();
                            flying->SetVelocity(XMFLOAT3(oldVel.x * -0.3f, 0.1f, oldVel.z * -0.3f));
                        }
                    }

                    Manager::AddHitStop(6); 
                    if (g_Camera) g_Camera->Shake(0.15f, 10); 
                    break;
                }
                continue;
            }

            // --- 他の敵との衝突判定 ---
            if (obj->GetObjectType() != ObjectType::Enemy) continue;
            Enemy* target = static_cast<Enemy*>(obj);

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

            // 電撃属性を持つエネミーの通常エネミー衝突でチェインライトニング発動
            if (flying->IsLightning() && targetState == EnemyState::NORMAL) {
                TriggerChainLightning(flying->GetPosition(), player);
                // 電撃属性は維持したまま撃破消滅へ移行し、スパークを散らし続ける

                // 投げた本人も撃破
                flying->SetEnemyState(EnemyState::DEFEATED);
                flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));

                // ブつかった対象も擃鉖
                target->SetEnemyState(EnemyState::DEFEATED);
                target->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
                target->SetLightning(true); // 対象エネミーもスパーク放電させる
                GameRule::OnEnemyDefeated(target->GetScoreValue());
                // シアン色ポップアップ（電撃連鎖で擃鉖）
                XMFLOAT3 tLightPos = target->GetPosition();
                ScorePopupSystem::AddPopup(
                    tLightPos.x, tLightPos.y + 1.0f, tLightPos.z,
                    target->GetScoreValue(),
                    0.0f, 1.5f, 2.5f
                );

                Manager::AddHitStop(10);
                if (g_Camera) g_Camera->Shake(0.35f, 12);
                break;
            }

            // ─── 通常の敵に衝突 ───
            if (target->GetEnemyState() == EnemyState::NORMAL) {
                XMFLOAT3 dir = MathHelper::Normalize(tPos - fPos);
                
                // 投げられたエネミーが巨大化している場合、ぶつかられた敵はダメージを受けて撃破される
                if (flying->GetScale().x > 2.0f) {
                    XMFLOAT3 vel = dir * 0.8f;
                    vel.y = 0.4f;
                    target->SetVelocity(vel);
                    target->SetEnemyState(EnemyState::BLOWN_AWAY); // 擃鉖吹き飛び状態
                    GameRule::OnEnemyDefeated(target->GetScoreValue()); // スコア加算
                    // オレンジ色ポップアップ（ギガント投げ擃鉖）
                    XMFLOAT3 tPos2 = target->GetPosition();
                    ScorePopupSystem::AddPopup(
                        tPos2.x, tPos2.y + 1.0f, tPos2.z,
                        target->GetScoreValue(),
                        2.5f, 0.7f, 0.0f
                    );

                    Manager::AddHitStop(10); 
                    if (g_Camera) g_Camera->Shake(0.4f, 15);
                } else {
                    // 通常サイズのエネミーの場合：単なる玉突き（生存して吹き飛ぶ）
                    XMFLOAT3 vel = dir * 0.4f;
                    vel.y = 0.35f;
                    target->SetVelocity(vel);
                    target->SetEnemyState(EnemyState::FLYING);

                    Manager::AddHitStop(8); 
                    if (g_Camera) g_Camera->Shake(0.3f, 12); 
                }
                
                float rotY = atan2f(-dir.x, -dir.z);
                target->SetRotation(XMFLOAT3(0.0f, rotY, 0.0f));
                break;
            }
        }
    }
}
