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
#include "boss_enemy.h"
#include "game_constants.h"

namespace {
    // 空間分割用のグリッド構造体
    struct CollisionGrid {
        static constexpr float CELL_SIZE = 5.0f;
        static constexpr int GRID_COLS = 24; // -60.0f から 60.0f までカバー
        static constexpr int GRID_ROWS = 24;
        static constexpr float GRID_MIN_X = -60.0f;
        static constexpr float GRID_MIN_Z = -60.0f;

        std::vector<Enemy*> cells[GRID_ROWS][GRID_COLS];

        void Clear() {
            for (int r = 0; r < GRID_ROWS; ++r) {
                for (int c = 0; c < GRID_COLS; ++c) {
                    cells[r][c].clear();
                }
            }
        }

        void Register(Enemy* enemy) {
            if (!enemy) return;
            XMFLOAT3 pos = enemy->GetPosition();
            int col = static_cast<int>(floorf((pos.x - GRID_MIN_X) / CELL_SIZE));
            int row = static_cast<int>(floorf((pos.z - GRID_MIN_Z) / CELL_SIZE));

            col = (std::max)(0, (std::min)(col, GRID_COLS - 1));
            row = (std::max)(0, (std::min)(row, GRID_ROWS - 1));

            cells[row][col].push_back(enemy);
        }
    };

    // グリッドの実体
    CollisionGrid g_CollisionGrid;
}

// namespace LightningConfig は削除され、Constants::Lightning (game_constants.h) に統合されました。

// ─────────────────────────────────────────────
// チェインライトニング（電撃連鎖）の処理
// ─────────────────────────────────────────────
static void TriggerChainLightning(const XMFLOAT3& startPos, Player* player)
{
    const std::vector<GameObject*>& gameObjects = Manager::GetGameObjectList();
    XMFLOAT3 currentPos = startPos;
    
    // 連鎖回数と索敵半径の設定
    const int maxChain = Constants::Lightning::MAX_CHAIN;
    float chainRadius = Constants::Lightning::CHAIN_RADIUS; // 連鎖する半径

    std::vector<Enemy*> chainedEnemies;

    for (int chain = 0; chain < maxChain; chain++) {
        Enemy* nearest = nullptr;
        float nearestDistSq = chainRadius * chainRadius;

        // グリッド座標を算出して近接セルのみを走査（半径8mに対しセルサイズ5mのため周囲2セル分を探索）
        int centerCol = static_cast<int>(floorf((currentPos.x - CollisionGrid::GRID_MIN_X) / CollisionGrid::CELL_SIZE));
        int centerRow = static_cast<int>(floorf((currentPos.z - CollisionGrid::GRID_MIN_Z) / CollisionGrid::CELL_SIZE));

        for (int dr = -2; dr <= 2; ++dr) {
            for (int dc = -2; dc <= 2; ++dc) {
                int r = centerRow + dr;
                int c = centerCol + dc;
                if (r >= 0 && r < CollisionGrid::GRID_ROWS && c >= 0 && c < CollisionGrid::GRID_COLS) {
                    for (Enemy* enemy : g_CollisionGrid.cells[r][c]) {
                        if (!enemy || enemy->IsDestroy()) continue;

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
                }
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
            // 放射状かつ上空へ吹き飛ばす（LightningConfig から取得した強さを適用）
            XMFLOAT3 pushVel = XMFLOAT3(
                dir.x * Constants::Lightning::PUSH_FORCE_XZ, 
                Constants::Lightning::PUSH_FORCE_Y, 
                dir.z * Constants::Lightning::PUSH_FORCE_XZ
            );
            // ボスの場合は吹き飛ばさず、Defeat()も直接呼ばずにダメージ処理を行う
            if (nearest->GetObjectType() == ObjectType::Boss) {
                BossEnemy* boss = static_cast<BossEnemy*>(nearest);
                boss->ApplyBossDamage(3, currentPos); // 連鎖ダメージは3
            } else {
                nearest->SetVelocity(pushVel);
                nearest->SetEnemyState(EnemyState::BLOWN_AWAY);
                nearest->SetLightning(true); // スパーク放電を有効化

                // 撃破処理（チェインライトニング）
                nearest->Defeat(0.0f, 1.5f, 2.5f);
            }
            
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
    const std::vector<GameObject*>& gameObjects = Manager::GetGameObjectList();

    // 1回の走査で判定対象ごとに事前分類およびグリッド登録を行い、高速化
    std::vector<Enemy*> enemies;
    std::vector<Wall*> walls;
    std::vector<Item*> items;

    enemies.reserve(32);
    walls.reserve(16);
    items.reserve(8);

    g_CollisionGrid.Clear();

    for (GameObject* obj : gameObjects) {
        if (!obj || obj->IsDestroy()) continue;
        ObjectType type = obj->GetObjectType();
        if (type == ObjectType::Enemy) {
            Enemy* enemy = static_cast<Enemy*>(obj);
            enemies.push_back(enemy);
            g_CollisionGrid.Register(enemy);
        } else if (type == ObjectType::Boss) {
            enemies.push_back(static_cast<Enemy*>(obj));
            // ボスはセル跨ぎ判定を行うため、グリッドには登録しない
        } else if (type == ObjectType::Wall) {
            walls.push_back(static_cast<Wall*>(obj));
        } else if (type == ObjectType::Item) {
            items.push_back(static_cast<Item*>(obj));
        }
    }

    // ─── プレイヤーとアイテムの衝突判定（各種アイテム取得） ───
    for (Item* item : items) {
        XMFLOAT3 iPos = item->GetPosition();
        float dx = pPos.x - iPos.x;
        float dy = pPos.y - iPos.y;
        float dz = pPos.z - iPos.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        // 平方根を用いず2乗距離（1.2fの2乗 = 1.44f）で高速判定
        if (distSq < 1.44f) {
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

    // ─── プレイヤーがスピン中のなぎ払い判定 ───
    if (player->GetState() == PlayerState::SPINNING) {
        Enemy* grabbed = player->GetGrabbedEnemy();
        if (grabbed && !grabbed->IsDestroy()) {
            XMFLOAT3 gPos = grabbed->GetPosition();
            float gRadius = grabbed->GetRadius();

            // 周囲9セルの敵とだけ衝突判定を行う
            int centerCol = static_cast<int>(floorf((gPos.x - CollisionGrid::GRID_MIN_X) / CollisionGrid::CELL_SIZE));
            int centerRow = static_cast<int>(floorf((gPos.z - CollisionGrid::GRID_MIN_Z) / CollisionGrid::CELL_SIZE));

            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    int r = centerRow + dr;
                    int c = centerCol + dc;
                    if (r >= 0 && r < CollisionGrid::GRID_ROWS && c >= 0 && c < CollisionGrid::GRID_COLS) {
                        for (Enemy* enemy : g_CollisionGrid.cells[r][c]) {
                            if (!enemy || enemy->IsDestroy() || enemy == grabbed) continue;

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

                                // ボスかどうかの分岐
                                if (enemy->GetObjectType() == ObjectType::Boss) {
                                    BossEnemy* boss = static_cast<BossEnemy*>(enemy);
                                    boss->ApplyBossDamage(Constants::Player::SPIN_SWEEP_DAMAGE, grabbed->GetPosition()); // スピンなぎ払いダメージ
                                } else {
                                    // 吹き飛ばす
                                    enemy->SetVelocity(pushVel);
                                    enemy->SetEnemyState(EnemyState::BLOWN_AWAY);

                                    // 撃破処理（スピンなぎ払い）
                                    enemy->Defeat();
                                }

                                // ヒットインパクト演出（ヒットストップとカメラ揺れ）
                                Manager::AddHitStop(6);
                                if (g_Camera) g_Camera->Shake(0.18f + spinSpeed * 0.4f, 8);
                            }
                        }
                    }
                }
            }
        }
    }

    // ─── 飛んでいる敵 → 他の敵・壁への連鎖衝突 ────────────────
    std::vector<Enemy*> flyingEnemies;
    for (Enemy* e : enemies) {
        if (e && e->GetEnemyState() == EnemyState::FLYING) {
            flyingEnemies.push_back(e);
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

        // --- 壁との衝突判定 ---
        for (Wall* wall : walls) {
            if (wall->IsDestroy()) continue;
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
                    // 撃破処理（壁衝突）
                    flying->Defeat();
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
        }

        // 壁との衝突で既に倒された状態（DEFEATED）になった場合は、他の敵との判定は行わない
        if (flying->GetEnemyState() == EnemyState::DEFEATED) continue;

        bool targetHit = false; // ループ脱出用フラグ

        // --- ボスとの衝突判定を個別に行う（グリッドのセル跨ぎによる貫通バグを完全に防ぐ） ---
        BossEnemy* boss = Manager::GetGameObject<BossEnemy>();
        if (boss && !boss->IsDestroy()) {
            EnemyState targetState = boss->GetEnemyState();
            if (targetState != EnemyState::DEFEATED && targetState != EnemyState::BLOWN_AWAY) {
                if (Collision::CheckSphere(flying, boss)) {
                    bool hitHandled = false;

                    // 1. 爆弾状態の敵との衝突
                    if (!explosionThisFrame && flying->IsExplosive()) {
                        ExplosionSystem::TriggerExplosion(flying->GetPosition());
                        flying->SetEnemyState(EnemyState::DEFEATED);
                        flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
                        explosionThisFrame = true;
                        hitHandled = true;
                    }

                    // 2. 電撃状態の敵との衝突
                    if (!hitHandled && flying->IsLightning()) {
                        TriggerChainLightning(flying->GetPosition(), player);
                        boss->ApplyBossDamage(Constants::Lightning::CHAIN_DAMAGE, flying->GetPosition());
                        flying->SetEnemyState(EnemyState::DEFEATED);
                        flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
                        Manager::AddHitStop(10);
                        if (g_Camera) g_Camera->Shake(0.35f, 12);
                        hitHandled = true;
                    }

                    // 3. 通常の衝突
                    if (!hitHandled) {
                        int dmg = Constants::Boss::THROW_NORMAL_DAMAGE;
                        if (flying->IsSandbag()) {
                            dmg = Constants::Boss::THROW_SANDBAG_DAMAGE;
                        } else if (flying->GetScale().x > 2.0f) {
                            dmg = Constants::Boss::THROW_GIGANT_DAMAGE;
                        }
                        boss->ApplyBossDamage(dmg, flying->GetPosition());

                        flying->SetEnemyState(EnemyState::DEFEATED);
                        flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));

                        Manager::AddHitStop(10);
                        if (g_Camera) g_Camera->Shake(0.4f, 15);
                        hitHandled = true;
                    }

                    if (hitHandled) {
                        targetHit = true;
                    }
                }
            }
        }

        if (targetHit) continue; // ボスに衝突した場合はグリッド走査をスキップ

        // --- 他の敵との衝突判定 ---
        // 飛行エネミーの周囲 9 セルのグリッドから判定対象エネミーを抽出
        int centerCol = static_cast<int>(floorf((fPos.x - CollisionGrid::GRID_MIN_X) / CollisionGrid::CELL_SIZE));
        int centerRow = static_cast<int>(floorf((fPos.z - CollisionGrid::GRID_MIN_Z) / CollisionGrid::CELL_SIZE));

        targetHit = false; // ループ脱出用フラグ

        for (int dr = -1; dr <= 1 && !targetHit; ++dr) {
            for (int dc = -1; dc <= 1 && !targetHit; ++dc) {
                int r = centerRow + dr;
                int c = centerCol + dc;
                if (r >= 0 && r < CollisionGrid::GRID_ROWS && c >= 0 && c < CollisionGrid::GRID_COLS) {
                    for (Enemy* target : g_CollisionGrid.cells[r][c]) {
                        if (target == flying || target->IsDestroy()) continue;

                        EnemyState targetState = target->GetEnemyState();
                        if (targetState == EnemyState::DEFEATED || targetState == EnemyState::BLOWN_AWAY) continue;

                        if (!Collision::CheckSphere(flying, target)) continue; // 衝突なし
                        XMFLOAT3 tPos = target->GetPosition();

                        // 爆弾属性の敵同士の衝突は爆発しない
                        if (flying->IsExplosive() && target->IsExplosive()) continue;

                        // 爆弾状態の敵が通常の敵（NORMAL/CHASING）に当たったら即爆発！
                        if (!explosionThisFrame && flying->IsExplosive() && (targetState == EnemyState::NORMAL || targetState == EnemyState::CHASING)) {
                            ExplosionSystem::TriggerExplosion(flying->GetPosition());
                            flying->SetEnemyState(EnemyState::DEFEATED);
                            flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
                            explosionThisFrame = true;
                            targetHit = true;
                            break;
                        }

                        // 電撃属性を持つエネミーの通常エネミー衝突でチェインライトニング発動
                        if (flying->IsLightning() && (targetState == EnemyState::NORMAL || targetState == EnemyState::CHASING)) {

                            // ボスへの電撃衝突は Defeat() を直接呼ばず、ダメージ経由で処理する
                            if (target->GetObjectType() == ObjectType::Boss) {
                                BossEnemy* boss = static_cast<BossEnemy*>(target);
                                TriggerChainLightning(flying->GetPosition(), player);
                                boss->ApplyBossDamage(Constants::Lightning::CHAIN_DAMAGE, flying->GetPosition()); // 電撃投げダメージ
                                flying->SetEnemyState(EnemyState::DEFEATED);
                                flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
                                Manager::AddHitStop(10);
                                if (g_Camera) g_Camera->Shake(0.35f, 12);
                                targetHit = true;
                                break;
                            }

                            TriggerChainLightning(flying->GetPosition(), player);
                            // 電撃属性は維持したまま撃破消滅へ移行し、スパークを散らし続ける

                            // 投げた本人も撃破
                            flying->SetEnemyState(EnemyState::DEFEATED);
                            flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));

                            // ぶつかった対象も撃破
                            target->SetEnemyState(EnemyState::DEFEATED);
                            target->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
                            target->SetLightning(true); // 対象エネミーもスパーク放電させる
                            // 撃破処理（電撃衝突）
                            target->Defeat(0.0f, 1.5f, 2.5f);

                            Manager::AddHitStop(10);
                            if (g_Camera) g_Camera->Shake(0.35f, 12);
                            targetHit = true;
                            break;
                        }

                        // ─── 通常の敵またはボスに衝突 ───
                        if (target->GetObjectType() == ObjectType::Boss) {
                            BossEnemy* boss = static_cast<BossEnemy*>(target);
                            int dmg = Constants::Boss::THROW_NORMAL_DAMAGE;
                            if (flying->IsSandbag()) {
                                dmg = Constants::Boss::THROW_SANDBAG_DAMAGE;
                            } else if (flying->GetScale().x > 2.0f) {
                                dmg = Constants::Boss::THROW_GIGANT_DAMAGE;
                            }
                            boss->ApplyBossDamage(dmg, flying->GetPosition());

                            flying->SetEnemyState(EnemyState::DEFEATED);
                            flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));

                            Manager::AddHitStop(10);
                            if (g_Camera) g_Camera->Shake(0.4f, 15);
                            targetHit = true;
                            break;
                        }
                        else if (targetState == EnemyState::NORMAL || targetState == EnemyState::CHASING || targetState == EnemyState::VACUUMED) {
                            XMFLOAT3 dir = MathHelper::Normalize(tPos - fPos);
                            
                            // 投げられたエネミーが巨大化している場合、ぶつかられた敵はダメージを受けて撃破される
                            if (flying->GetScale().x > 2.0f) {
                                XMFLOAT3 vel = dir * 0.8f;
                                vel.y = 0.4f;
                                target->SetVelocity(vel);
                                target->SetEnemyState(EnemyState::BLOWN_AWAY); // 撃退吹き飛び状態
                                // 撃破処理（ギガント投げ撃退）
                                target->Defeat(2.5f, 0.7f, 0.0f);

                                Manager::AddHitStop(10); 
                                if (g_Camera) g_Camera->Shake(0.4f, 15);
                            } else {
                                // 通常サイズのエネミーの場合：玉突きしてダメージを与えて撃破
                                XMFLOAT3 vel = dir * 0.5f;
                                vel.y = 0.35f;
                                target->SetVelocity(vel);
                                target->SetEnemyState(EnemyState::BLOWN_AWAY);
                                target->Defeat(); // 撃破処理を適用

                                Manager::AddHitStop(8); 
                                if (g_Camera) g_Camera->Shake(0.3f, 12); 
                            }
                            
                            float rotY = atan2f(-dir.x, -dir.z);
                            target->SetRotation(XMFLOAT3(0.0f, rotY, 0.0f));
                            targetHit = true;
                            break;
                        }
                    }
                }
            }
        }
    }
}
