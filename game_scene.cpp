#include "game_scene.h"
#include <list>
#include <sstream>
#include "camera.h"
#include "player.h"
#include "enemy.h"
#include "wall.h"
#include "field.h"
#include "renderer.h"
#include "input.h"

// ─────────────────────────────────────────────
// 初期化
// ─────────────────────────────────────────────
void GameScene::Init() {
    m_Score         = 0;
    m_DefeatedCount = 0;
    m_TotalEnemies  = 0;
    m_IsGameClear   = false;
    m_IsGameOver    = false;

    // プレイヤー生成
    Player *player = new Player();
    player->Init();
    player->SetPosition(XMFLOAT3(0.0f, 0.0f, 0.0f));
    m_GameObjectList.push_back(player);

    // フィールド生成
    Field *field = new Field();
    field->Init();
    m_GameObjectList.push_back(field);

    // 壁（専用クラス）
    Wall *wall = new Wall();
    wall->Init();
    wall->SetPosition(XMFLOAT3(3.0f, 1.5f, 3.0f));
    wall->SetScale(XMFLOAT3(5.0f, 5.0f, 5.0f));
    m_GameObjectList.push_back(wall);

    // 敵を3体生成
    for (int i = 0; i < 3; i++) {
        Enemy *enemy = new Enemy();
        enemy->Init();
        enemy->SetPosition(XMFLOAT3((float)(i * 4 - 4), -0.5f, -3.0f));
        m_GameObjectList.push_back(enemy);
        m_TotalEnemies++;                        // 敵の合計数をカウント
    }

    // デバッグ：初期情報を出力
    OutputDebugStringA("[GameScene] Init完了 - 敵の合計数: ");
    OutputDebugStringA(std::to_string(m_TotalEnemies).c_str());
    OutputDebugStringA("\n");
}

// ─────────────────────────────────────────────
// 終了処理
// ─────────────────────────────────────────────
void GameScene::Uninit() { Scene::Uninit(); }

// ─────────────────────────────────────────────
// 撃破時の共通処理（スコア加算・クリア判定）
// ─────────────────────────────────────────────
void GameScene::OnEnemyDefeated(int scoreValue)
{
    m_Score += scoreValue;
    m_DefeatedCount++;

    // デバッグ出力
    std::string msg = "[GameScene] 撃破! スコア: " + std::to_string(m_Score)
                    + " / 撃破数: " + std::to_string(m_DefeatedCount)
                    + " / 合計: "  + std::to_string(m_TotalEnemies) + "\n";
    OutputDebugStringA(msg.c_str());

    // ─── ゲームクリア判定 ─────────────────────────
    if (!m_IsGameClear && m_DefeatedCount >= m_TotalEnemies) {
        m_IsGameClear = true;
        OutputDebugStringA("[GameScene] *** ゲームクリア! ***\n");
        // TODO: クリアシーンへの遷移はSTEP 4で実装
    }
}

// ─────────────────────────────────────────────
// 更新処理
// ─────────────────────────────────────────────
void GameScene::Update() {
    // クリア後は更新しない
    if (m_IsGameClear) return;

    Scene::Update();

    // プレイヤーを取得
    Player* player = nullptr;
    for (GameObject* obj : m_GameObjectList) {
        player = dynamic_cast<Player*>(obj);
        if (player) break;
    }
    if (!player) return;

    XMFLOAT3 pPos = player->GetPosition();

    // ─── マウス左クリック：つかみ / 投げ ──────────────────
    if (Input::GetKeyTrigger(VK_LBUTTON)) {
        if (player->GetGrabbedEnemy()) {
            // すでにつかんでいる → 投げる
            player->Throw();
        } else {
            // 近くの敵をつかむ
            float grabRange  = 2.5f;
            Enemy* closest   = nullptr;
            float  closestDist = grabRange;
            for (GameObject* obj : m_GameObjectList) {
                Enemy* e = dynamic_cast<Enemy*>(obj);
                if (!e || e->IsDestroy()) continue;
                if (e->GetEnemyState() != EnemyState::NORMAL) continue;

                XMFLOAT3 ePos = e->GetPosition();
                float dx = pPos.x - ePos.x;
                float dz = pPos.z - ePos.z;
                float dist = sqrtf(dx*dx + dz*dz);
                if (dist < closestDist) {
                    closestDist = dist;
                    closest = e;
                }
            }
            if (closest) {
                closest->SetEnemyState(EnemyState::GRABBED);
                player->SetGrabbedEnemy(closest);
            }
        }
    }

    // ─── 振り回し中の敵と周囲の敵との衝突判定 ──────────────
    if (player->GetGrabbedEnemy()) {
        Enemy*   grabbed = player->GetGrabbedEnemy();
        XMFLOAT3 gPos    = grabbed->GetPosition();

        for (GameObject* obj : m_GameObjectList) {
            Enemy* target = dynamic_cast<Enemy*>(obj);
            if (!target || target->IsDestroy() || target == grabbed) continue;
            if (target->GetEnemyState() != EnemyState::NORMAL) continue;

            XMFLOAT3 tPos = target->GetPosition();
            float dx = gPos.x - tPos.x;
            float dy = gPos.y - tPos.y;
            float dz = gPos.z - tPos.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dist < (grabbed->GetRadius() + target->GetRadius())) {
                XMVECTOR vToTarget = XMVector3Normalize(
                    XMLoadFloat3(&tPos) - XMLoadFloat3(&pPos));
                XMFLOAT3 dir;
                XMStoreFloat3(&dir, vToTarget);

                // 遠心力ブースト：回転速度が速いほど威力が上がる
                float force = 0.5f + abs(player->GetAngularVelocity()) * 5.0f;
                target->SetVelocity(XMFLOAT3(dir.x * force, 0.4f, dir.z * force));
                target->SetEnemyState(EnemyState::FLYING);
            }
        }
    }

    // ─── 飛んでいる敵 → 他の敵・壁への連鎖衝突 ────────────
    std::list<Enemy*> flyingEnemies;
    for (GameObject* obj : m_GameObjectList) {
        Enemy* e = dynamic_cast<Enemy*>(obj);
        if (e && !e->IsDestroy() && e->GetEnemyState() == EnemyState::FLYING)
            flyingEnemies.push_back(e);
    }

    for (Enemy* flying : flyingEnemies) {
        if (flying->IsDestroy()) continue;                           // 既に撃破済みならスキップ
        XMFLOAT3 fPos = flying->GetPosition();

        for (GameObject* obj : m_GameObjectList) {
            if (obj == flying || obj->IsDestroy()) continue;

            // --- 壁との衝突判定 ---
            Wall* wall = dynamic_cast<Wall*>(obj);
            if (wall) {
                XMFLOAT3 wPos = wall->GetPosition();
                float dx = fPos.x - wPos.x;
                float dy = fPos.y - wPos.y;
                float dz = fPos.z - wPos.z;
                float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                
                // 壁の大きさを考慮した当たり判定（簡易）
                float collisionRadius = flying->GetRadius() + wall->GetRadius();
                if (dist < collisionRadius) {
                    OnEnemyDefeated(flying->GetScoreValue());
                    flying->SetDestroy();
                    break;
                }
                continue;
            }

            // --- 他の敵との衝突判定 ---
            Enemy* target = dynamic_cast<Enemy*>(obj);
            if (!target) continue;

            XMFLOAT3 tPos  = target->GetPosition();
            float dx = fPos.x - tPos.x;
            float dy = fPos.y - tPos.y;
            float dz = fPos.z - tPos.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);

            float collisionRadius = flying->GetRadius() + target->GetRadius();
            if (dist >= collisionRadius) continue;                   // 衝突なし

            // ─── 通常の敵に衝突 → 連鎖させて自分は撃破 ──────────
            if (target->GetEnemyState() == EnemyState::NORMAL) {
                XMVECTOR vHitDir = XMVector3Normalize(
                    XMLoadFloat3(&tPos) - XMLoadFloat3(&fPos));
                XMFLOAT3 dir;
                XMStoreFloat3(&dir, vHitDir);
                target->SetVelocity(XMFLOAT3(dir.x * 0.5f, 0.4f, dir.z * 0.5f));
                target->SetEnemyState(EnemyState::FLYING);
                OnEnemyDefeated(flying->GetScoreValue());
                flying->SetDestroy();
                break;
            }
        }
    }
}

// ─────────────────────────────────────────────
// 描画処理
// ─────────────────────────────────────────────
void GameScene::Draw() {
    XMMATRIX cameraView = Renderer::GetViewMatrix();
    XMMATRIX cameraProj = Renderer::GetProjectionMatrix();

    LIGHT currentLight = Renderer::GetLight();
    XMVECTOR lightDir  = XMLoadFloat4(&currentLight.Direction);
    XMVECTOR lightPos  = XMVector3Normalize(lightDir) * -20.0f;
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos,
        XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    XMMATRIX lightProj = XMMatrixOrthographicLH(30.0f, 30.0f, 1.0f, 50.0f);
    Renderer::SetShadowVPMatrix(lightView * lightProj);
    Renderer::SetViewMatrix(lightView);
    Renderer::SetProjectionMatrix(lightProj);

    // シャドウパス描画（フィールドを除く）
    Renderer::BeginShadowPass();
    for (GameObject *obj : m_GameObjectList) {
        if (!dynamic_cast<Field *>(obj)) obj->Draw();
    }
    Renderer::EndShadowPass();

    // 通常描画
    Renderer::SetViewMatrix(cameraView);
    Renderer::SetProjectionMatrix(cameraProj);
    Renderer::Begin();
    for (GameObject *obj : m_GameObjectList) { obj->Draw(); }

    // アウトライン描画（フィールドを除く）
    Renderer::BeginOutlinePass();
    for (GameObject *obj : m_GameObjectList) {
        if (!dynamic_cast<Field*>(obj)) { obj->Draw(); }
    }
    Renderer::EndOutlinePass();

    if (g_Camera) g_Camera->Draw();
    Renderer::End();
}