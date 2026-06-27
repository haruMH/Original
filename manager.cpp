#include "manager.h"
#include <string>
#include "camera.h"
#include "input.h"
#include "main.h"
#include "player.h"
#include "renderer.h"
#include "resource_manager.h"
#include "collision.h"
#include "enemy.h"
#include "attacking_enemy.h"
#include "boss_enemy.h"
#include "field.h"
#include "wall.h"
#include "item.h"
#include "game_rule.h"
#include "collision_system.h"
#include "score_popup.h"
#include "score_hud.h"
#include "shockwave.h"

// 静的メンバ変数の実体定義
std::vector<GameObject*> Manager::m_GameObjects;
std::vector<GameObject*> Manager::m_UpdateObjects;
RenderSystem           Manager::m_RenderSystem;
int                    Manager::m_HitStopFrames = 0;
int                    Manager::m_SlowMotionTimer = 0;
int                    Manager::m_SlowMotionDuration = 0;
Scene                  Manager::m_CurrentScene = Scene::TITLE;

//  デバッグ用切り替えマクロ: 1にすると最初からボス戦、0にすると通常の16体ステージから始まります
#define START_FROM_BOSS 0

bool                   Manager::m_IsBossStage = false;
Scene                  Manager::m_NextScene = Scene::TITLE;
bool                   Manager::m_SceneTransitionRequested = false;


Camera*                g_Camera = nullptr;

// ─────────────────────────────────────────────
// 初期化
// ─────────────────────────────────────────────
void Manager::Init()
{
    Renderer::Init();
    Input::Init();
    GameRule::Init();
    m_HitStopFrames = 0;
    m_SlowMotionTimer = 0;
    m_SlowMotionDuration = 0;

    // 描画システムの初期化
    bool initRes = m_RenderSystem.Init(Renderer::GetDevice());
    if (!initRes) {
        MessageBoxA(nullptr, "RenderSystem Init Failed.", "Error", MB_OK | MB_ICONERROR);
    }

    // スコアポップアップシステムの初期化
    ScorePopupSystem::Init(Renderer::GetDevice());

    // スコアHUDの初期化
    ScoreHUD::Init(Renderer::GetDevice());

    // 衝撃波システムの初期化
    ShockwaveSystem::Init(Renderer::GetDevice());

    // 光源設定
    LIGHT light;
    ZeroMemory(&light, sizeof(light));
    light.Enable = TRUE;
    light.Direction = XMFLOAT4(0.5f, -1.0f, 0.5f, 0.0f);
    light.Diffuse = XMFLOAT4(1.0f, 0.95f, 0.9f, 1.0f);
    light.Ambient = XMFLOAT4(0.4f, 0.5f, 0.7f, 1.0f);
    light.FogColor = XMFLOAT4(0.6f, 0.8f, 1.0f, 1.0f);
    light.FogStart = 30.0f;
    light.FogEnd = 60.0f;
    Renderer::SetLight(light);

    // 最初はタイトルシーンへ (即時実行)
    m_SceneTransitionRequested = false;
    ExecuteChangeScene(Scene::TITLE);
}

// ─────────────────────────────────────────────
// 終了処理
// ─────────────────────────────────────────────
void Manager::Uninit()
{
    // スコアポップアップシステムの終了処理
    ScorePopupSystem::Uninit();

    // スコアHUDの終了処理
    ScoreHUD::Uninit();

    // 衝撃波システムの終了処理
    ShockwaveSystem::Uninit();

    m_RenderSystem.Uninit();

    // 登録されたすべてのオブジェクトの解放
    for (GameObject* gameObject : m_GameObjects) {
        if (gameObject) {
            gameObject->Uninit();
            delete gameObject;
        }
    }
    m_GameObjects.clear();
    m_UpdateObjects.clear();

    if (g_Camera) {
        g_Camera->Uninit();
        delete g_Camera;
        g_Camera = nullptr;
    }

    Input::Uninit();
    ResourceManager::Uninit();
    Renderer::Uninit();
}

// ─────────────────────────────────────────────
// 更新処理
// ─────────────────────────────────────────────
void Manager::Update()
{
    // ImGui の新規フレームを開始（Update内で各オブジェクトがImGui UIを構築するため、最初に行う必要があります）
    Renderer::BeginNewFrame();

    Input::Update();

    // スコアポップアップはシーンを問わず更新可能にする
    ScorePopupSystem::Update();

    // スコアHUDもシーンを問わず更新
    ScoreHUD::Update();

    // シーンごとの分岐処理
    switch (m_CurrentScene)
    {
    case Scene::TITLE:
        // スペースキーが押されたらゲーム開始
        if (Input::GetKeyTrigger(VK_SPACE)) {
            ChangeScene(Scene::GAMEPLAY);
        }
        // 'T'キーが押されたらシェーダーテストシーンへ
        if (Input::GetKeyTrigger('T')) {
            ChangeScene(Scene::SHADER_TEST);
        }
        break;

    case Scene::GAMEPLAY:
        // 'T'キーが押されたらシェーダーテストシーンへ（デバッグ用）
        if (Input::GetKeyTrigger('T')) {
            ChangeScene(Scene::SHADER_TEST);
        }
        UpdateGameplay();
        break;

    case Scene::SHADER_TEST:
        // Escキーでタイトルに戻る
        if (Input::GetKeyTrigger(VK_ESCAPE)) {
            ChangeScene(Scene::TITLE);
        }
        UpdateGameplay();
        break;

    case Scene::CLEAR:
    case Scene::GAMEOVER:
        // エンターキーが押されたらタイトルに戻る
        if (Input::GetKeyTrigger(VK_RETURN)) {
            ChangeScene(Scene::TITLE);
        }
        break;
    }

    if (g_Camera) g_Camera->Update();

    // 遅延シーン遷移のリクエストが来ている場合は実行する
    if (m_SceneTransitionRequested) {
        m_SceneTransitionRequested = false;
        ExecuteChangeScene(m_NextScene);
    }
}

void Manager::UpdateGameplay()
{
    // スローモーションタイマーの更新
    if (m_SlowMotionTimer > 0) {
        m_SlowMotionTimer--;
    }

    // スローモーション中はプレイヤー以外の更新頻度を1/5にする
    bool updateOthers = true;
    if (m_SlowMotionTimer > 0) {
        updateOthers = (m_SlowMotionTimer % 5 == 0);
    }

    // 衝撃波システムも更新（スロー時は間引く）
    if (updateOthers) {
        ShockwaveSystem::Update();
    }

    // クリア後はゲームオブジェクトの更新を行わない
    if (GameRule::IsGameClear()) return;

    // ヒットストップ中はカメラのみ更新し、他の更新をスキップする
    if (m_HitStopFrames > 0) {
        m_HitStopFrames--;
        return;
    }

    Player* player = GetGameObject<Player>();

    // 動的オブジェクトの更新と破棄（WallとFieldは静的で更新不要なため除外）
    for (size_t i = 0; i < m_UpdateObjects.size(); ) {
        GameObject* obj = m_UpdateObjects[i];
        bool shouldUpdate = true;

        // プレイヤー以外のオブジェクトはスローモーション中に更新を間引く
        if (obj->GetObjectType() != ObjectType::Player) {
            shouldUpdate = updateOthers;
        }

        if (shouldUpdate) {
            obj->Update();
        }

        if (obj->IsDestroy()) {
            if (player) {
                player->NotifyObjectDestroyed(obj);
            }
            obj->Uninit();
            delete obj;

            // O(1)スワップ＆ポップ消去法（swap-erase）を用いて更新リストから削除
            if (i != m_UpdateObjects.size() - 1) {
                m_UpdateObjects[i] = m_UpdateObjects.back();
            }
            m_UpdateObjects.pop_back();

            // 保存しておいたポインタ値を使用して描画用オブジェクトリスト（m_GameObjects）からも削除
            for (size_t j = 0; j < m_GameObjects.size(); j++) {
                if (m_GameObjects[j] == obj) {
                    if (j != m_GameObjects.size() - 1) {
                        m_GameObjects[j] = m_GameObjects.back();
                    }
                    m_GameObjects.pop_back();
                    break;
                }
            }
        } else {
            i++;
        }
    }

    // 2. 衝突判定システムによる判定・物理連鎖の更新（スロー時は間引く）
    if (updateOthers) {
        CollisionSystem::Update();
    }

    // 3. 掴んでいるエネミーの位置確定後処理 (LateUpdate)（スロー時は間引く）
    if (updateOthers && player) {
        if (player->GetState() == PlayerState::GRABBED || player->GetState() == PlayerState::SPINNING) {
            Enemy* grabbedEnemy = player->GetGrabbedEnemy();
            if (grabbedEnemy) {
                Collision::ResolveGrabPhysics(player, grabbedEnemy, 0.8f);
            }
        }
    }

    // 4. ボスステージ遷移およびゲームクリア判定（シェーダーテストシーンでは行わない）
    if (m_CurrentScene == Scene::SHADER_TEST) return;

    if (!m_IsBossStage) {
        // 通常ステージ：攻撃してくる敵が全て全滅したかを監視
        bool attackingEnemyExists = false; 
        for (GameObject* obj : m_GameObjects) {
            if (obj && obj->GetObjectType() == ObjectType::Enemy) {
                Enemy* enemy = static_cast<Enemy*>(obj);
                if (enemy->IsAttackingEnemy() && enemy->GetEnemyState() != EnemyState::DEFEATED) {
                    attackingEnemyExists = true;
                    break;
                }
            }
        }

        if (!attackingEnemyExists) {
            TransitionToBossStage();
        }
    } else {
        // ボスステージ：ボスが倒されたかを監視
        if (!GameRule::IsGameClear()) {
            bool bossExists = false;
            for (GameObject* obj : m_GameObjects) {
                if (obj->GetObjectType() == ObjectType::Boss) {
                    Enemy* boss = static_cast<Enemy*>(obj);
                    if (boss->GetEnemyState() != EnemyState::DEFEATED) {
                        bossExists = true;
                        break;
                    }
                }
            }

            if (!bossExists) {
                GameRule::SetGameClear(true);
                ChangeScene(Scene::CLEAR);
                OutputDebugStringA("[GameRule] *** ボス撃破！ゲームクリア! ***\n");
            }
        }
    }
}

// ─────────────────────────────────────────────
// スローモーション強度の取得（フェードアウト用）
// ─────────────────────────────────────────────
float Manager::GetSlowMotionIntensity()
{
    if (m_SlowMotionTimer <= 0) return 0.0f;
    if (m_SlowMotionTimer > 30) return 1.0f;
    return (float)m_SlowMotionTimer / 30.0f;
}

// ─────────────────────────────────────────────
// 描画処理
// ─────────────────────────────────────────────
void Manager::Draw()
{
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
    for (GameObject* obj : m_GameObjects) {
        ObjectType type = obj->GetObjectType();
        if (type != ObjectType::Field && 
            type != ObjectType::Enemy && 
            type != ObjectType::Player && 
            type != ObjectType::Wall &&
            type != ObjectType::Boss &&
            type != ObjectType::Unknown) {
            obj->Draw();
            Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
    }
    // インスタンスシャドウ描画を実行
    m_RenderSystem.RenderCubeInstances(Renderer::GetDeviceContext(), m_GameObjects, RenderPass::Shadow);
    Renderer::EndShadowPass();

    // === 2. 通常描画パス ===
    Renderer::SetViewMatrix(cameraView);
    Renderer::SetProjectionMatrix(cameraProj);
    Renderer::Begin();

    // まず空 (Skybox) を描画
    for (GameObject* obj : m_GameObjects) {
        if (obj->GetObjectType() == ObjectType::Unknown) { obj->Draw(); }
    }

    // 描画ステートの復元
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 床(Field)を描画
    for (GameObject* obj : m_GameObjects) {
        if (obj->GetObjectType() == ObjectType::Field) { obj->Draw(); }
    }

    // 床描画で変更されたトポロジーをLISTに戻す
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 一括描画以外のオブジェクトを個別描画（プレイヤー・ボスはガイドライン/バリア描画のため呼ぶ）
    Renderer::SetupCubeDraw();
    for (GameObject* obj : m_GameObjects) {
        ObjectType type = obj->GetObjectType();
        if (type == ObjectType::Field || type == ObjectType::Enemy) {
            continue;
        }
        // 通常の壁は一括描画するため個別描画はスキップ。ただし、テスト壁（IsShaderTest()がtrue）は個別描画する。
        if (type == ObjectType::Wall) {
            Wall* wall = static_cast<Wall*>(obj);
            if (!wall->IsShaderTest()) {
                continue;
            }
        }

        obj->Draw();
        // テスト壁（Water/Dissolve/Refract等）がシェーダーを変更した場合、
        // 次のオブジェクト描画前に通常キューブシェーダーへ戻す
        if (type == ObjectType::Wall) {
            Renderer::SetupCubeDraw();
        }
        Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    // キューブオブジェクトの一括インスタンシング描画
    m_RenderSystem.RenderCubeInstances(Renderer::GetDeviceContext(), m_GameObjects, RenderPass::Normal);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // === 3. アウトライン描画 ===
    Renderer::BeginOutlinePass();
    Renderer::SetupCubeDraw();
    for (GameObject* obj : m_GameObjects) {
        ObjectType type = obj->GetObjectType();
        if (type != ObjectType::Field && 
            type != ObjectType::Enemy && 
            type != ObjectType::Player && 
            type != ObjectType::Wall &&
            type != ObjectType::Boss &&
            type != ObjectType::Unknown) {
            obj->Draw();
            Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
    }
    // インスタンスアウトライン描画を実行
    m_RenderSystem.RenderCubeInstances(Renderer::GetDeviceContext(), m_GameObjects, RenderPass::Outline);
    Renderer::EndOutlinePass();

    if (g_Camera) g_Camera->Draw();

    // スコアポップアップをSceneRTVに描画する（End()の前でブルームにも乗る）
    ScorePopupSystem::Draw();

    // 衝撃波を描画（3D空間・ブルーム適用）
    ShockwaveSystem::Draw();

    // スコアHUDを描画（最前面・ブルーム適用）
    ScoreHUD::Draw();

    Renderer::End();
}

// ─────────────────────────────────────────────
// ボスステージへの移行処理
// ─────────────────────────────────────────────
void Manager::TransitionToBossStage()
{
    OutputDebugStringA("[Manager] TransitionToBossStage - 開始\n");
    m_IsBossStage = true;

    // プレイヤーを取得
    Player* player = GetGameObject<Player>();
    if (player) {
        OutputDebugStringA("[Manager] プレイヤー位置をリセット\n");
        player->SetPosition(XMFLOAT3(0.0f, -0.5f, 0.0f));
    }

    // プレイヤー、フィールド以外のすべてのオブジェクトを破棄（リストから削除）
    OutputDebugStringA("[Manager] オブジェクト破棄処理を開始します...\n");
    int destroyedCount = 0;
    std::vector<GameObject*> nextObjects;
    nextObjects.reserve(m_GameObjects.size());
    for (GameObject* obj : m_GameObjects) {
        if (obj->GetObjectType() != ObjectType::Player && obj->GetObjectType() != ObjectType::Field) {
            if (player) {
                player->NotifyObjectDestroyed(obj);
            }
            obj->Uninit();
            delete obj;
            destroyedCount++;
        } else {
            nextObjects.push_back(obj);
        }
    }
    m_GameObjects = std::move(nextObjects);
    // 静的オブジェクトを除外して更新対象リスト（m_UpdateObjects）を再構築
    m_UpdateObjects.clear();
    for (GameObject* obj : m_GameObjects) {
        ::ObjectType t = obj->GetObjectType();
        if (t != ::ObjectType::Wall && t != ::ObjectType::Field) {
            m_UpdateObjects.push_back(obj);
        }
    }
    std::string destroyMsg = "[Manager] オブジェクト破棄が完了しました (個数: " + std::to_string(destroyedCount) + ")\n";
    OutputDebugStringA(destroyMsg.c_str());

    // ボス部屋の壁を生成
    OutputDebugStringA("[Manager] ボス部屋の壁を生成します...\n");
    float roomSize = 18.0f;
    // 北の壁
    Wall* wallN = AddGameObject<Wall>();
    wallN->SetPosition(XMFLOAT3(0.0f, 1.5f, roomSize));
    wallN->SetScale(XMFLOAT3(roomSize * 2.0f, 5.0f, 1.0f));
    // 南の壁
    Wall* wallS = AddGameObject<Wall>();
    wallS->SetPosition(XMFLOAT3(0.0f, 1.5f, -roomSize));
    wallS->SetScale(XMFLOAT3(roomSize * 2.0f, 5.0f, 1.0f));
    // 東の壁
    Wall* wallE = AddGameObject<Wall>();
    wallE->SetPosition(XMFLOAT3(roomSize, 1.5f, 0.0f));
    wallE->SetScale(XMFLOAT3(1.0f, 5.0f, roomSize * 2.0f));
    // 西の壁
    Wall* wallW = AddGameObject<Wall>();
    wallW->SetPosition(XMFLOAT3(-roomSize, 1.5f, 0.0f));
    wallW->SetScale(XMFLOAT3(1.0f, 5.0f, roomSize * 2.0f));
    OutputDebugStringA("[Manager] 壁生成完了\n");

    // ボスエネミーの生成
    OutputDebugStringA("[Manager] ボスエネミーの生成を開始します...\n");
    BossEnemy* boss = AddGameObject<BossEnemy>();
    boss->SetPosition(XMFLOAT3(0.0f, 1.5f, 10.0f)); // プレイヤーの少し前方に配置
    OutputDebugStringA("[Manager] ボスエネミー生成完了\n");

    // ボス部屋用のアイテム生成（ボスから一番遠い南の壁 Z=-18.0f 付近に配置）
    OutputDebugStringA("[Manager] ボス部屋用のアイテムを生成します...\n");
    Item* itemVacuum = AddGameObject<Item>();
    itemVacuum->SetPosition(XMFLOAT3(0.0f, 0.5f, -15.0f));
    itemVacuum->SetItemType(ItemType::VACUUM);

    Item* itemGigant = AddGameObject<Item>();
    itemGigant->SetPosition(XMFLOAT3(-4.0f, 0.5f, -15.0f));
    itemGigant->SetItemType(ItemType::GIGANT);

    Item* itemLightning = AddGameObject<Item>();
    itemLightning->SetPosition(XMFLOAT3(4.0f, 0.5f, -15.0f));
    itemLightning->SetItemType(ItemType::LIGHTNING);
    
    // ボス戦の演出：巨大な衝撃波をプレイヤーとボスの間に走らせる
    ShockwaveSystem::AddShockwave(XMFLOAT3(0.0f, -0.95f, 5.0f), 15.0f, 0.0f, 2.0f, 4.0f, 40, 0.0f, 0);

    // カメラをボスに向けて強めにシェイク
    if (g_Camera) g_Camera->Shake(0.6f, 20);
    OutputDebugStringA("[Manager] TransitionToBossStage - 正常終了\n");
}

// ─────────────────────────────────────────────
// シーン遷移処理
// ─────────────────────────────────────────────
// ─────────────────────────────────────────────
// シーン遷移予約 (遅延遷移)
// ─────────────────────────────────────────────
void Manager::ChangeScene(Scene nextScene)
{
    m_NextScene = nextScene;
    m_SceneTransitionRequested = true;
}

// ─────────────────────────────────────────────
// シーン遷移の実際の実行
// ─────────────────────────────────────────────
void Manager::ExecuteChangeScene(Scene nextScene)
{
    OutputDebugStringA(("[Manager] ExecuteChangeScene: " + std::to_string((int)m_CurrentScene) + " -> " + std::to_string((int)nextScene) + "\n").c_str());

    // 既存オブジェクトのクリーンアップ（Uninitとdelete）
    for (GameObject* gameObject : m_GameObjects) {
        if (gameObject) {
            gameObject->Uninit();
            delete gameObject;
        }
    }
    m_GameObjects.clear();
    m_UpdateObjects.clear();

    if (g_Camera) {
        g_Camera->Uninit();
        delete g_Camera;
        g_Camera = nullptr;
    }

    // 現在のシーン状態を更新
    m_CurrentScene = nextScene;

    // カメラはどのシーンでも必要（UIの描画や投影変換行列の初期化に使用）なので、共通で生成する
    g_Camera = new Camera();
    g_Camera->Init();

    // 各シーンの初期化
    if (nextScene == Scene::TITLE) {
        // タイトル画面用の初期化（カメラのみでHUDが描画する）
    }
    else if (nextScene == Scene::GAMEPLAY) {
        // ゲームプレイ本編の初期化
        GameRule::Init(); // スコアや敵数、ゲームクリア状態などのリセット
        
        m_HitStopFrames = 0;
        m_SlowMotionTimer = 0;
        m_SlowMotionDuration = 0;

        // 地面オブジェクトの生成
        AddGameObject<Field>();

        // プレイヤーオブジェクトの生成
        Player* player = AddGameObject<Player>();
        player->SetPosition(XMFLOAT3(0.0f, -0.5f, 0.0f));

#if START_FROM_BOSS
        m_IsBossStage = true;
        
        // ボス部屋の壁
        float roomSize = 18.0f;
        Wall* wallN = AddGameObject<Wall>();
        wallN->SetPosition(XMFLOAT3(0.0f, 1.5f, roomSize));
        wallN->SetScale(XMFLOAT3(roomSize * 2.0f, 5.0f, 1.0f));
        Wall* wallS = AddGameObject<Wall>();
        wallS->SetPosition(XMFLOAT3(0.0f, 1.5f, -roomSize));
        wallS->SetScale(XMFLOAT3(roomSize * 2.0f, 5.0f, 1.0f));
        Wall* wallE = AddGameObject<Wall>();
        wallE->SetPosition(XMFLOAT3(roomSize, 1.5f, 0.0f));
        wallE->SetScale(XMFLOAT3(1.0f, 5.0f, roomSize * 2.0f));
        Wall* wallW = AddGameObject<Wall>();
        wallW->SetPosition(XMFLOAT3(-roomSize, 1.5f, 0.0f));
        wallW->SetScale(XMFLOAT3(1.0f, 5.0f, roomSize * 2.0f));

        // ボスエネミーの生成
        BossEnemy* boss = AddGameObject<BossEnemy>();
        boss->SetPosition(XMFLOAT3(0.0f, 1.5f, 10.0f));

        GameRule::SetTotalEnemies(1);
#else
        m_IsBossStage = false;
        
        // 通常ステージ：壁を敵エリアの外側に配置（プレイヤーと干渉しない位置）
        Wall* wall = AddGameObject<Wall>();
        wall->SetPosition(XMFLOAT3(-8.0f, 1.5f, -7.0f));
        wall->SetScale(XMFLOAT3(5.0f, 5.0f, 5.0f));

        // プレイヤーを敵・壁から安全な距離（Z+8付近）にスポーン
        player->SetPosition(XMFLOAT3(0.0f, -0.5f, 8.0f));

        int totalEnemies = 0;
        for (int x = -2; x <= 1; x++) {
            for (int z = -2; z <= 1; z++) {
                Enemy* enemy = nullptr;
                if ((x + z) % 2 == 0) {
                    enemy = AddGameObject<AttackingEnemy>();
                } else {
                    enemy = AddGameObject<Enemy>();
                }
                float posX = (float)x * 3.2f + 1.0f;
                float posZ = (float)z * 3.2f - 7.0f;
                enemy->SetPosition(XMFLOAT3(posX, -0.5f, posZ));
                totalEnemies++;
            }
        }
        GameRule::SetTotalEnemies(totalEnemies);
#endif

        // アイテム生成
        Item* itemVacuum = AddGameObject<Item>();
        itemVacuum->SetItemType(ItemType::VACUUM);

        Item* itemGigant = AddGameObject<Item>();
        itemGigant->SetItemType(ItemType::GIGANT);

        Item* itemLightning = AddGameObject<Item>();
        itemLightning->SetItemType(ItemType::LIGHTNING);

        if (m_IsBossStage) {
            // ボスステージ：ボス(Z=10.0f)から一番遠い南の壁（Z=-18.0f）の手前に並べて配置
            itemVacuum->SetPosition(XMFLOAT3(0.0f, 0.5f, -15.0f));
            itemGigant->SetPosition(XMFLOAT3(-4.0f, 0.5f, -15.0f));
            itemLightning->SetPosition(XMFLOAT3(4.0f, 0.5f, -15.0f));
        } else {
            // 通常ステージ：これまでの通常位置に配置
            itemVacuum->SetPosition(XMFLOAT3(0.0f, 0.5f, 4.0f));
            itemGigant->SetPosition(XMFLOAT3(-4.0f, 0.5f, 4.0f));
            itemLightning->SetPosition(XMFLOAT3(2.0f, 0.5f, 6.0f));
        }
    }
    else if (nextScene == Scene::CLEAR) {
        // ゲームクリア画面用の初期化
    }
    else if (nextScene == Scene::GAMEOVER) {
        // ゲームオーバー画面用の初期化
    }
    else if (nextScene == Scene::SHADER_TEST) {
        // シェーダーテストシーンの初期化
        GameRule::Init(); // スコアや敵数、ゲームクリア状態などのリセット
        
        m_HitStopFrames = 0;
        m_SlowMotionTimer = 0;
        m_SlowMotionDuration = 0;

        // 1. スカイボックス（空）の生成
        AddGameObject<Skybox>();

        // 2. 鏡面反射（リフレクション）確認用のオブジェクト（アイテムなど）を生成
        // 鏡ブロック（Z=5.0f）の手前（Z=3.0f）に配置することで、鏡の中にアイテムが映り込むようにする
        Item* item1 = AddGameObject<Item>();
        item1->SetPosition(XMFLOAT3(-0.6f, 0.0f, 3.0f));
        item1->SetItemType(ItemType::VACUUM);

        Item* item2 = AddGameObject<Item>();
        item2->SetPosition(XMFLOAT3(0.0f, 0.0f, 3.0f));
        item2->SetItemType(ItemType::GIGANT);

        Item* item3 = AddGameObject<Item>();
        item3->SetPosition(XMFLOAT3(0.6f, 0.0f, 3.0f));
        item3->SetItemType(ItemType::LIGHTNING);

        // 3. 地面オブジェクトの生成（反射が他のオブジェクト描画後にコピーされるように順序を調整）
        AddGameObject<Field>();

        // 4. 壁オブジェクトの生成（プレイヤーより少し大きく、目の前）
        Wall* wall = AddGameObject<Wall>();
        wall->SetPosition(XMFLOAT3(0.0f, 0.0f, 5.0f)); // Y=0.0fで底面が地面(Y=-1.0f)に接する
        wall->SetScale(XMFLOAT3(2.0f, 2.0f, 2.0f));    // プレイヤー(1.0f, 1.0f, 1.0f)より大きめ
        wall->SetShaderTest(true);                     // シェーダーテスト用フラグをONにする
        m_UpdateObjects.push_back(wall);               // アップデート対象リストに追加

        GameRule::SetTotalEnemies(0); // 敵は存在しない
    }
}
