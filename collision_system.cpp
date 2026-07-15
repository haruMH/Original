#include "collision_system.h"
#include <list>
#include <algorithm> // std::find を使うため
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
#include "spatial_grid.h"
#include "boss_enemy.h"

// namespace LightningConfig は削除され、Constants::Lightning (game_constants.h) に統合されました。

// ─────────────────────────────────────────────
// チェインライトニング（電撃連鎖）の処理
// ─────────────────────────────────────────────
static void TriggerChainLightning(const XMFLOAT3& startPos, Player* player)
{
    XMFLOAT3 currentPos = startPos;
    
    // 連鎖回数と索敵半径の設定
    const int maxChain = Constants::Lightning::MAX_CHAIN;
    float chainRadius = Constants::Lightning::CHAIN_RADIUS; // 連鎖する半径

    std::vector<Enemy*> chainedEnemies;
    chainedEnemies.reserve(maxChain);

    for (int chain = 0; chain < maxChain; chain++) {
        Enemy* nearest = nullptr;
        float nearestDistSq = chainRadius * chainRadius;

        // SpatialGrid の FindNearbyEnemies で周囲の敵を絞り込む
        std::vector<Enemy*> candidates;
        SpatialGrid::GetInstance().FindNearbyEnemies(currentPos, chainRadius, candidates);

        for (Enemy* enemy : candidates) {
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
            HitInfo hitInfo;
            hitInfo.hitSourcePos = currentPos;
            if (nearest->GetObjectType() == ObjectType::Boss) {
                hitInfo.damage = 3; // 連鎖ダメージは3
                nearest->OnHit(hitInfo);
            } else {
                hitInfo.damage = 1;
                hitInfo.knockbackVel = pushVel;
                hitInfo.setLightning = true;
                hitInfo.popupColor = {0.0f, 1.5f, 2.5f};
                nearest->OnHit(hitInfo);
            }
            
            chainedEnemies.push_back(nearest);
            currentPos = nextPos; // 次の連鎖の開始点にする
        } else {
            break; // 近くに敵がいなければ連鎖終了
        }
    }
}

// ─────────────────────────────────────────────
// 飛行エネミーが対象に衝突したときの衝撃を解決する
// 爆発・電撃・通常の3パターンを一か所に集約して重複を排除する。
// 戻り値：衝突処理を実施した場合は true。
// ─────────────────────────────────────────────
static bool ResolveFlyingEnemyImpact(
    Enemy*      flying,
    GameObject* target,
    Player*     player,
    bool&       explosionThisFrame)
{
    // ─── 1. 爆発属性の衝突 ───
    if (!explosionThisFrame && flying->IsExplosive()) {
        // 爆弾同士の衝突は爆発しない（呼び出し元でガード済みの場合もあるが念のため）
        if (target->GetObjectType() == ObjectType::Enemy) {
            Enemy* te = static_cast<Enemy*>(target);
            if (te->IsExplosive()) return false;
        }
        ExplosionSystem::TriggerExplosion(flying->GetPosition());
        flying->SetEnemyState(EnemyState::DEFEATED);
        flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
        explosionThisFrame = true;
        return true;
    }

    // ─── 2. 電撃属性の衝突 ───
    if (flying->IsLightning()) {
        TriggerChainLightning(flying->GetPosition(), player);

        HitInfo hitInfo;
        hitInfo.hitSourcePos = flying->GetPosition();
        hitInfo.setLightning = true;
        hitInfo.popupColor = {0.0f, 1.5f, 2.5f};

        if (target->GetObjectType() == ObjectType::Boss) {
            hitInfo.damage = Constants::Lightning::CHAIN_DAMAGE;
        } else {
            hitInfo.damage = 1;
            hitInfo.knockbackVel = XMFLOAT3(0.0f, 0.0f, 0.0f);
        }
        target->OnHit(hitInfo);

        flying->SetEnemyState(EnemyState::DEFEATED);
        flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
        Manager::AddHitStop(10);
        if (g_Camera) g_Camera->Shake(0.35f, 12);
        return true;
    }

    // ─── 3. 通常衝突 ───
    if (target->GetObjectType() == ObjectType::Boss) {
        int dmg = Constants::Boss::THROW_NORMAL_DAMAGE;
        if (flying->IsSandbag()) {
            dmg = Constants::Boss::THROW_SANDBAG_DAMAGE;
        } else if (flying->GetScale().x > 2.0f) {
            dmg = Constants::Boss::THROW_GIGANT_DAMAGE;
        }
        
        HitInfo hitInfo;
        hitInfo.damage = dmg;
        hitInfo.hitSourcePos = flying->GetPosition();
        target->OnHit(hitInfo);

        flying->SetEnemyState(EnemyState::DEFEATED);
        flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
        Manager::AddHitStop(10);
        if (g_Camera) g_Camera->Shake(0.4f, 15);
        return true;
    }

    // 通常エネミーへの玉突き
    {
        XMFLOAT3 fPos = flying->GetPosition();
        XMFLOAT3 tPos = target->GetPosition();
        XMFLOAT3 dir = MathHelper::Normalize(tPos - fPos);

        HitInfo hitInfo;
        hitInfo.damage = 1;
        hitInfo.hitSourcePos = fPos;

        if (flying->GetScale().x > 2.0f) {
            // 巨大エネミーによる投げ：ぶつかられた敵は撃破される
            XMFLOAT3 vel = dir * 0.8f;
            vel.y = 0.4f;
            hitInfo.knockbackVel = vel;
            hitInfo.popupColor = {2.5f, 0.7f, 0.0f};
            target->OnHit(hitInfo);
            Manager::AddHitStop(10);
            if (g_Camera) g_Camera->Shake(0.4f, 15);
        } else {
            // 通常サイズ：玉突きして撃破
            XMFLOAT3 vel = dir * 0.5f;
            vel.y = 0.35f;
            hitInfo.knockbackVel = vel;
            target->OnHit(hitInfo);
            Manager::AddHitStop(8);
            if (g_Camera) g_Camera->Shake(0.3f, 12);
        }

        float rotY = atan2f(-dir.x, -dir.z);
        if (target->GetObjectType() == ObjectType::Enemy) {
            static_cast<Enemy*>(target)->SetRotation(XMFLOAT3(0.0f, rotY, 0.0f));
        }
    }
    return true;
}

// ─────────────────────────────────────────────
// プレイヤーとアイテムの衝突判定（各種アイテム取得）
// ─────────────────────────────────────────────
static void HandlePlayerItemPickup(Player* player, const std::vector<GameObject*>& items)
{
    XMFLOAT3 pPos = player->GetPosition();
    for (GameObject* obj : items) {
        Item* item = static_cast<Item*>(obj);
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
}

// ─────────────────────────────────────────────
// プレイヤーのスピン中におけるなぎ払い判定
// ─────────────────────────────────────────────
static void HandleSpinSweep(Player* player, SpatialGrid& grid)
{
    if (player->GetState() != PlayerState::SPINNING) return;

    Enemy* grabbed = player->GetGrabbedEnemy();
    if (!grabbed || grabbed->IsDestroy()) return;

    XMFLOAT3 gPos = grabbed->GetPosition();
    float gRadius = grabbed->GetRadius();

    // 周囲セルの敵とだけ衝突判定を行う（SpatialGrid::GetCell 経由）
    int centerCol = static_cast<int>(floorf((gPos.x - Constants::Collision::GRID_MIN_X) / Constants::Collision::GRID_CELL_SIZE));
    int centerRow = static_cast<int>(floorf((gPos.z - Constants::Collision::GRID_MIN_Z) / Constants::Collision::GRID_CELL_SIZE));

    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            int r = centerRow + dr;
            int c = centerCol + dc;

            for (Enemy* enemy : grid.GetCell(r, c)) {
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

                    // ボスかどうかの判定
                    HitInfo hitInfo;
                    hitInfo.hitSourcePos = grabbed->GetPosition();
                    hitInfo.knockbackVel = pushVel;
                    if (enemy->GetObjectType() == ObjectType::Boss) {
                        hitInfo.damage = Constants::Player::SPIN_SWEEP_DAMAGE;
                        enemy->OnHit(hitInfo);
                    } else {
                        hitInfo.damage = 1;
                        enemy->OnHit(hitInfo);
                    }

                    // ヒットインパクト演出（ヒットストップとカメラ揺れ）
                    Manager::AddHitStop(6);
                    if (g_Camera) g_Camera->Shake(0.18f + spinSpeed * 0.4f, 8);
                }
            }
        }
    }
}

// ─────────────────────────────────────────────
// 飛行エネミー vs 壁の衝突判定
// 壁に衝突した場合、飛行エネミーを撃破または跳ね返す。
// ─────────────────────────────────────────────
static void HandleFlyingVsWall(
    Enemy*                         flying,
    const std::vector<GameObject*>& walls,
    Player*                        player,
    bool&                          explosionThisFrame)
{
    for (GameObject* obj : walls) {
        Wall* wall = static_cast<Wall*>(obj);
        if (wall->IsDestroy()) continue;
        if (!Collision::CheckAABB(flying, wall)) continue;

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

// ─────────────────────────────────────────────
// 飛行エネミー vs ボスの衝突判定
// グリッドのセル跨ぎによる貫通バグを防ぐため個別に処理する。
// 戻り値：衝突が起きた場合は true。
// ─────────────────────────────────────────────
static bool HandleFlyingVsBoss(
    Enemy*    flying,
    Player*   player,
    bool&     explosionThisFrame)
{
    BossEnemy* boss = Manager::GetGameObject<BossEnemy>();
    if (!boss || boss->IsDestroy()) return false;

    EnemyState targetState = boss->GetEnemyState();
    if (targetState == EnemyState::DEFEATED || targetState == EnemyState::BLOWN_AWAY) return false;

    if (!Collision::CheckSphere(flying, boss)) return false;

    return ResolveFlyingEnemyImpact(flying, boss, player, explosionThisFrame);
}

// ─────────────────────────────────────────────
// 飛行エネミー vs 他のエネミーの衝突判定（グリッド利用）
// ─────────────────────────────────────────────
static void HandleFlyingVsEnemy(
    Enemy*          flying,
    Player*         player,
    SpatialGrid&    grid,
    bool&           explosionThisFrame)
{
    XMFLOAT3 fPos = flying->GetPosition();
    int centerCol = static_cast<int>(floorf((fPos.x - Constants::Collision::GRID_MIN_X) / Constants::Collision::GRID_CELL_SIZE));
    int centerRow = static_cast<int>(floorf((fPos.z - Constants::Collision::GRID_MIN_Z) / Constants::Collision::GRID_CELL_SIZE));

    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            int r = centerRow + dr;
            int c = centerCol + dc;

            for (Enemy* target : grid.GetCell(r, c)) {
                if (target == flying || target->IsDestroy()) continue;

                EnemyState targetState = target->GetEnemyState();
                if (targetState == EnemyState::DEFEATED || targetState == EnemyState::BLOWN_AWAY) continue;

                if (!Collision::CheckSphere(flying, target)) continue;

                // 爆弾属性の敵同士の衝突は爆発しない
                if (flying->IsExplosive() && target->IsExplosive()) continue;

                // 爆弾・電撃・通常の分岐を ResolveFlyingEnemyImpact に委譲する
                // （通常エネミーの状態が NORMAL / CHASING / VACUUMED のとき衝突処理を行う）
                if (targetState != EnemyState::NORMAL && targetState != EnemyState::CHASING && targetState != EnemyState::VACUUMED) continue;

                if (ResolveFlyingEnemyImpact(flying, target, player, explosionThisFrame)) {
                    return; // 1フレームに1衝突まで
                }
            }
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

    const std::vector<GameObject*>& enemies = Manager::GetCategoryList(ObjectType::Enemy);
    const std::vector<GameObject*>& walls   = Manager::GetCategoryList(ObjectType::Wall);
    const std::vector<GameObject*>& items   = Manager::GetCategoryList(ObjectType::Item);

    // 共有 SpatialGrid を今フレーム用にリセットし、全生存エネミーを登録する
    SpatialGrid& grid = SpatialGrid::GetInstance();
    grid.Clear();

    for (GameObject* obj : enemies) {
        Enemy* enemy = static_cast<Enemy*>(obj);
        if (enemy && !enemy->IsDestroy() && enemy->GetObjectType() == ObjectType::Enemy) {
            grid.Register(enemy);
        }
    }

    // ─── プレイヤーとアイテムの衝突判定 ───
    HandlePlayerItemPickup(player, items);

    // ─── スピン中のなぎ払い判定 ───
    HandleSpinSweep(player, grid);

    // ─── 飛んでいる敵 → 他の敵・壁への連鎖衝突 ───
    std::vector<Enemy*> flyingEnemies;
    flyingEnemies.reserve(enemies.size());
    for (GameObject* obj : enemies) {
        Enemy* e = static_cast<Enemy*>(obj);
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
        HandleFlyingVsWall(flying, walls, player, explosionThisFrame);

        // 壁との衝突で既に倒された状態になった場合は、他の敵との判定は行わない
        if (flying->GetEnemyState() == EnemyState::DEFEATED) continue;

        // --- ボスとの衝突判定 ---
        if (HandleFlyingVsBoss(flying, player, explosionThisFrame)) continue;

        // --- 他の敵との衝突判定 ---
        HandleFlyingVsEnemy(flying, player, grid, explosionThisFrame);
    }
}
