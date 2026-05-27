#include "game_scene.h"
#include <list>
#include <sstream>
#include <fstream>
#include "camera.h"
#include "player.h"
#include "enemy.h"
#include "item.h"
#include "wall.h"
#include "field.h"
#include "renderer.h"
#include "input.h"
#include "manager.h"
#include "collision.h"
#include "player_controller.h"
#include "math_helper.h"
// ─────────────────────────────────────────────
// 初期化
// ─────────────────────────────────────────────
void GameScene::Init() {
    m_Score         = 0;
    m_DefeatedCount = 0;
    m_TotalEnemies  = 0;
    m_IsGameClear   = false;
    m_IsGameOver    = false;

    // 描画システムの初期化
    bool initRes = m_RenderSystem.Init(Renderer::GetDevice());
    if (!initRes) {
        MessageBoxA(nullptr, "RenderSystem Init Failed.", "Error", MB_OK | MB_ICONERROR);
    }

    // プレイヤー生成
    Player *player = new Player();
    player->Init();
    player->SetPosition(XMFLOAT3(0.0f, -0.5f, 0.0f));
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

    // 敵を多め（4x4の計16体）に生成して、ド派手な連鎖反応が起きるようにする！
    for (int x = -2; x <= 1; x++) {
        for (int z = -2; z <= 1; z++) {
            Enemy *enemy = new Enemy();
            enemy->Init();
            // プレイヤーの前方エリアに高密度な配置を作る
            float posX = (float)x * 3.2f + 1.0f;
            float posZ = (float)z * 3.2f - 7.0f; 
            enemy->SetPosition(XMFLOAT3(posX, -0.5f, posZ));
            m_GameObjectList.push_back(enemy);
            m_TotalEnemies++;                        // 敵の合計数をカウント
        }
    }

    // 吸引力付きスピンを発動するためのパワーアップアイテムを生成
    Item *item = new Item();
    item->Init();
    item->SetPosition(XMFLOAT3(0.0f, 0.5f, 4.0f)); // プレイヤーの前方付近に配置
    m_GameObjectList.push_back(item);

    // デバッグ：初期情報を出力
    OutputDebugStringA("[GameScene] Init完了 - 敵の合計数: ");
    OutputDebugStringA(std::to_string(m_TotalEnemies).c_str());
    OutputDebugStringA("\n");
}

// ─────────────────────────────────────────────
// 終了処理
// ─────────────────────────────────────────────
void GameScene::Uninit() {
    m_RenderSystem.Uninit();
    Scene::Uninit();
}

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

    // 従来の即時クリア判定はフリーズ（演出中の静止）防止のため、Update側の遅延クリア判定に移行しました
}

// ─────────────────────────────────────────────
// 爆発を発生させ周囲の敵を吹き飛ばす（超強化版）
// ─────────────────────────────────────────────
void GameScene::TriggerExplosion(const DirectX::XMFLOAT3& center)
{
    // ブラックホール爆発：規模をフリーズ防止のために適切に調整（18.0f -> 12.0f, 4.0f -> 2.2f）
    float explosionRadius = 12.0f; // 爆発の有効半径
    float baseForce = 1.2f;        // 爆風の基本威力

    // カメラシェイクで爆発のインパクトを演出
    if (g_Camera) {
        g_Camera->Shake(1.2f, 25);
    }

    for (GameObject* obj : m_GameObjectList) {
        Enemy* enemy = dynamic_cast<Enemy*>(obj);
        if (!enemy || enemy->IsDestroy()) continue;

        EnemyState oldState = enemy->GetEnemyState();
        // すでに撃破済み、または既に吹き飛んでいる敵は除外（消滅できなくなるバグやフリーズを防止）
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

            // まだ倒されていない敵であれば、爆風に巻き込まれた時点で撃破スコアを加算
            OnEnemyDefeated(enemy->GetScoreValue());

            // 敵に爆風 of 速度と吹き飛び状態を設定
            enemy->SetVelocity(vel);
            enemy->SetEnemyState(EnemyState::BLOWN_AWAY);
        }
    }
}

// ─────────────────────────────────────────────
// 更新処理
// ─────────────────────────────────────────────
void GameScene::Update() {
    // クリア後は更新しない
    if (m_IsGameClear) return;

    Scene::Update();

    // 敵の位置を出力するデバッグログ
    int enemyIdx = 0;
    for (GameObject* obj : m_GameObjectList) {
        Enemy* e = dynamic_cast<Enemy*>(obj);
        if (e && !e->IsDestroy()) {
            XMFLOAT3 ePos = e->GetPosition();
            char dbgMsg[256];
            sprintf_s(dbgMsg, "[Debug] 敵%d 位置: (%.2f, %.2f, %.2f)\n", enemyIdx++, ePos.x, ePos.y, ePos.z);
            OutputDebugStringA(dbgMsg);
        }
    }

    // プレイヤーを取得
    Player* player = nullptr;
    for (GameObject* obj : m_GameObjectList) {
        player = dynamic_cast<Player*>(obj);
        if (player) break;
    }
    if (!player) return;

    XMFLOAT3 pPos = player->GetPosition();

    // ─── プレイヤーとアイテムの衝突判定（吸引アイテム取得） ───
    for (GameObject* obj : m_GameObjectList) {
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
                OutputDebugStringA("[GameScene] 吸引アイテムを取得しました！\n");
            }
        }
    }

    // ─── 飛んでいる敵 → 他の敵・壁への連鎖衝突 ────────────────
    std::list<Enemy*> flyingEnemies;
    for (GameObject* obj : m_GameObjectList) {
        Enemy* e = dynamic_cast<Enemy*>(obj);
        if (e && !e->IsDestroy() && e->GetEnemyState() == EnemyState::FLYING)
            flyingEnemies.push_back(e);
    }

    // フリーズ防止：1フレームに発生する爆発は1回のみに制限する
    bool explosionThisFrame = false;

    for (Enemy* flying : flyingEnemies) {
        if (flying->IsDestroy()) continue;                           // 既に撃破済みならスキップ
        // 爆発属性の敵が既に爆発したステートならスキップ（FLYING以外に遷移済み）
        if (flying->GetEnemyState() != EnemyState::FLYING) continue;
        XMFLOAT3 fPos = flying->GetPosition();

        // ─── 着地 + 速度がほぼ0になったら爆発トリガー（爆弾化している場合） ───
        // 「速度が0になったら爆発する」仕様：着地済みかつ水平速度が閾値以下で爆発
        if (!explosionThisFrame && flying->IsExplosive() && fPos.y <= -0.3f) {
            XMFLOAT3 curVel = flying->GetVelocity();
            float speedSq = curVel.x * curVel.x + curVel.z * curVel.z;
            // 着地していて、水平速度が十分に落ちたら（塊が止まる瞬間に）爆発！
            if (speedSq < 0.04f) {
                TriggerExplosion(fPos);
                flying->SetEnemyState(EnemyState::DEFEATED);
                flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
                explosionThisFrame = true;
                continue; // この敵の衝突判定は終了
            }
        }

        for (GameObject* obj : m_GameObjectList) {
            if (obj == flying || obj->IsDestroy()) continue;

            // --- 壁との衝突判定 ---
            Wall* wall = dynamic_cast<Wall*>(obj);
            if (wall) {
                // 壁との当たり判定（球ではなく直方体 / AABB で正確に判定）
                if (Collision::CheckAABB(flying, wall)) {
                    char dbgMsg[256];
                    sprintf_s(dbgMsg, "[Debug] 敵が壁と衝突しました！（衝突済み） 位置: (%.2f, %.2f, %.2f)\n", fPos.x, fPos.y, fPos.z);
                    OutputDebugStringA(dbgMsg);

                    if (!explosionThisFrame && flying->IsExplosive()) {
                        // 爆弾化した敵が壁に衝突した場合は爆発を誘発
                        TriggerExplosion(flying->GetPosition());
                        flying->SetEnemyState(EnemyState::DEFEATED);
                        flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
                        explosionThisFrame = true;
                    } else {
                        OnEnemyDefeated(flying->GetScoreValue());
                        
                        // 瞬時に消さず、撃破状態（DEFEATED）にして縮小消滅演出を行う
                        flying->SetEnemyState(EnemyState::DEFEATED);
                        XMFLOAT3 oldVel = flying->GetVelocity();
                        flying->SetVelocity(XMFLOAT3(oldVel.x * -0.3f, 0.1f, oldVel.z * -0.3f));
                    }

                    Manager::AddHitStop(6); // 壁衝突時のヒットストップ
                    g_Camera->Shake(0.15f, 10); // 壁衝突のカメラシェイク
                    break;
                }
                continue;
            }

            // --- 他の敵との衝突判定 ---
            Enemy* target = dynamic_cast<Enemy*>(obj);
            if (!target) continue;

            // すでに倒されているか吹き飛んでいる敵は衝突判定をスキップ（不必要な衝突連鎖や負荷を防止）
            EnemyState targetState = target->GetEnemyState();
            if (targetState == EnemyState::DEFEATED || targetState == EnemyState::BLOWN_AWAY) continue;

            if (!Collision::CheckSphere(flying, target)) continue; // 衝突なし
            XMFLOAT3 tPos = target->GetPosition();

            // 爆弾属性の敵同士の衝突は爆発しない（連鎖フリーズ防止）
            if (flying->IsExplosive() && target->IsExplosive()) continue;

            // 爆弾状態の敵が通常の敵（NORMAL）に当たったら即爆発！
            if (!explosionThisFrame && flying->IsExplosive() && targetState == EnemyState::NORMAL) {
                TriggerExplosion(flying->GetPosition());
                flying->SetEnemyState(EnemyState::DEFEATED);
                flying->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
                explosionThisFrame = true;
                break;
            }

            // ─── 通常の敵に衝突 ───
            if (target->GetEnemyState() == EnemyState::NORMAL) {
                // MathHelper::Normalize で正規化（XMVECTOR 変換不要）
                XMFLOAT3 dir = MathHelper::Normalize(tPos - fPos);
                
                // ぶつかられた敵（target）をさらに高く上に吹き飛ばす
                XMFLOAT3 vel = dir * 0.4f;
                vel.y = 0.35f;
                target->SetVelocity(vel);
                target->SetEnemyState(EnemyState::FLYING);
                
                // ぶつかられた敵に、ぶつかってきた方向（飛んできた敵の方向）を向かせる
                float rotY = atan2f(-dir.x, -dir.z);
                target->SetRotation(XMFLOAT3(0.0f, rotY, 0.0f));

                // 飛んでいる自分（flying）は消滅せず、速度も変えずにそのまま飛び続ける

                Manager::AddHitStop(8); // 敵同士の衝突時のヒットストップ（少し短めにしてテンポを良くする）
                g_Camera->Shake(0.3f, 12); // 連鎖衝突時のダイナミックなカメラシェイク
                break;
            }
        }
    }

    // ─── 遅延ゲームクリア判定 ─────────────────────
    // すべての敵の撃破数が目標に達し、かつ敵の消滅演出（DEFEATED 状態の縮小消滅）が
    // すべて完了してシーン内から Enemy オブジェクトが完全にいなくなったらクリアとする。
    if (!m_IsGameClear && m_DefeatedCount >= m_TotalEnemies) {
        bool enemyExists = false;
        for (GameObject* obj : m_GameObjectList) {
            if (dynamic_cast<Enemy*>(obj)) {
                enemyExists = true;
                break;
            }
        }

        if (!enemyExists) {
            m_IsGameClear = true;
            OutputDebugStringA("[GameScene] *** ゲームクリア! ***\n");
        }
    }
}

// ─────────────────────────────────────────────
// 描画処理（ステート・リーク修正版）
// ─────────────────────────────────────────────
void GameScene::Draw() {
    XMMATRIX cameraView = Renderer::GetViewMatrix();
    XMMATRIX cameraProj = Renderer::GetProjectionMatrix();

    LIGHT currentLight = Renderer::GetLight();
    XMVECTOR lightDir = XMLoadFloat4(&currentLight.Direction);
    XMVECTOR lightPos = XMVector3Normalize(lightDir) * -20.0f;
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos,
        XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    XMMATRIX lightProj = XMMatrixOrthographicLH(30.0f, 30.0f, 1.0f, 50.0f);
    Renderer::SetShadowVPMatrix(lightView * lightProj);
    Renderer::SetViewMatrix(lightView);
    Renderer::SetProjectionMatrix(lightProj);

    // === 1. シャドウパス描画（フィールドを除く） ===
    Renderer::BeginShadowPass();
    Renderer::SetupCubeDraw(); // キューブ共通アセットをバインド
    for (GameObject* obj : m_GameObjectList) {
        // フィールド、エネミー、プレイヤー、壁を除外して個別描画
        if (!dynamic_cast<Field*>(obj) && 
            !dynamic_cast<Enemy*>(obj) && 
            !dynamic_cast<Player*>(obj) && 
            !dynamic_cast<Wall*>(obj)) {
            obj->Draw();
            // 各オブジェクトの内部でトポロジーが変更された場合を考慮してLISTに戻す
            Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
    }
    // エネミー、プレイヤー、壁のインスタンスシャドウ描画を実行
    m_RenderSystem.RenderCubeInstances(Renderer::GetDeviceContext(), m_GameObjectList, RenderPass::Shadow);
    Renderer::EndShadowPass();

    // === 2. 通常描画パス ===
    Renderer::SetViewMatrix(cameraView);
    Renderer::SetProjectionMatrix(cameraProj);
    Renderer::Begin();

    // 先に床(Field)を描画（床は独自のバッファ・シェーダーで、内部でTRIANGLESTRIPに切り替わる）
    for (GameObject* obj : m_GameObjectList) {
        if (dynamic_cast<Field*>(obj)) { obj->Draw(); }
    }

    // 【重要】床がTRIANGLESTRIPに変更したステートを、ここで明示的に通常のLISTに戻す！
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 再度キューブ共通アセットをバインドして、一括描画以外のオブジェクトを個別描画
    Renderer::SetupCubeDraw();
    for (GameObject* obj : m_GameObjectList) {
        // フィールド、エネミー、壁を除外して個別描画（プレイヤーはエイムガイド描画のために呼ぶ）
        if (!dynamic_cast<Field*>(obj) && 
            !dynamic_cast<Enemy*>(obj) && 
            !dynamic_cast<Wall*>(obj)) {
            obj->Draw();
            // プレイヤーや壁のDraw後にトポロジーをLISTへ安全に戻す
            Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
    }

    // キューブオブジェクトの一括インスタンシング描画
    m_RenderSystem.RenderCubeInstances(Renderer::GetDeviceContext(), m_GameObjectList, RenderPass::Normal);
    // インスタンシング描画関数から抜けた後のトポロジーをLISTに矯正
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // === 3. アウトライン描画（フィールドを除く） ===
    Renderer::BeginOutlinePass();
    // RenderCubeInstances() がスロット0/1の頂点バッファを nullptr にクリアするため、
    // アウトラインパスの前に必ず SetupCubeDraw() でキューブ VB を再バインドする。
    Renderer::SetupCubeDraw();
    for (GameObject* obj : m_GameObjectList) {
        // フィールド、エネミー、プレイヤー、壁を除外して個別描画
        if (!dynamic_cast<Field*>(obj) && 
            !dynamic_cast<Enemy*>(obj) && 
            !dynamic_cast<Player*>(obj) && 
            !dynamic_cast<Wall*>(obj)) {
            obj->Draw();
            Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
    }
    // キューブオブジェクトのインスタンスアウトライン描画を実行
    m_RenderSystem.RenderCubeInstances(Renderer::GetDeviceContext(), m_GameObjectList, RenderPass::Outline);
    Renderer::EndOutlinePass();

    if (g_Camera) g_Camera->Draw();

    Renderer::End();
}