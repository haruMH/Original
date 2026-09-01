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

// シーンオブジェクト定義のインクルード
#include "title_scene.h"
#include "gameplay_scene.h"
#include "clear_scene.h"
#include "gameover_scene.h"

// 静的メンバ変数の実体定義
std::vector<GameObject*> Manager::m_GameObjects;
std::vector<GameObject*> Manager::m_UpdateObjects;
Player*                Manager::m_CachedPlayer = nullptr;
RenderSystem           Manager::m_RenderSystem;
int                    Manager::m_HitStopFrames = 0;
int                    Manager::m_SlowMotionTimer = 0;
int                    Manager::m_SlowMotionDuration = 0;
Scene                  Manager::m_CurrentScene = Scene::TITLE;

bool                   Manager::m_IsBossStage = true;
Scene                  Manager::m_NextScene = Scene::TITLE;
bool                   Manager::m_SceneTransitionRequested = false;
float                  Manager::m_FadeInDuration = 0.4f;
XMFLOAT4               Manager::m_FadeColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

IScene*                Manager::m_ActiveScene = nullptr;

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
    m_CachedPlayer = nullptr;
    m_ActiveScene = nullptr;

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

    // フェードシステムの初期化
    FadeSystem::Init(Renderer::GetDevice());

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
    // アクティブなシーンオブジェクトの終了処理と解放
    if (m_ActiveScene) {
        m_ActiveScene->Uninit();
        delete m_ActiveScene;
        m_ActiveScene = nullptr;
    }

    // スコアポップアップシステムの終了処理
    ScorePopupSystem::Uninit();

    // スコアHUDの終了処理
    ScoreHUD::Uninit();

    // 衝撃波システムの終了処理
    ShockwaveSystem::Uninit();

    // フェードシステムの終了処理
    FadeSystem::Uninit();

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
    m_CachedPlayer = nullptr;

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
    // ImGui の新規フレームを開始
    Renderer::BeginNewFrame();

    Input::Update();

    // フェードシステムのタイマー更新
    FadeSystem::Update(1.0f / 60.0f);

    // スコアポップアップはシーンを問わず更新可能にする
    ScorePopupSystem::Update();

    // スコアHUDもシーンを問わず更新
    ScoreHUD::Update();

    // 現在アクティブなシーンオブジェクトの更新処理を実行 (ポリモーフィズムによる委譲)
    if (m_ActiveScene) {
        m_ActiveScene->Update();
    }

    if (g_Camera) g_Camera->Update();

    // 遅延シーン遷移のリクエストがあり、かつ暗転が完了した（またはフェード中でない）場合に切り替える
    if (m_SceneTransitionRequested) {
        if (!FadeSystem::IsFading() || FadeSystem::IsFadeOutComplete()) {
            m_SceneTransitionRequested = false;
            ExecuteChangeScene(m_NextScene);
            // シーンの切り替えが完了したら、新しいシーンでフェードインを開始
            FadeSystem::StartFadeIn(m_FadeInDuration, m_FadeColor);
        }
    }
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

    // === 1. シャドウパス描画 ===
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
        // 床、敵、壁は一括描画するため個別描画はスキップ
        if (type == ObjectType::Field || type == ObjectType::Enemy || type == ObjectType::Wall) {
            continue;
        }

        obj->Draw();
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

    // 各種ポップアップ・HUD・エフェクト描画
    ScorePopupSystem::Draw();
    ShockwaveSystem::Draw();
    ScoreHUD::Draw();

    // 現在アクティブなシーン独自の3D/2D描画があれば追加で実行
    if (m_ActiveScene) {
        m_ActiveScene->Draw();
    }

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
// シーン遷移予約 (遅延遷移)
// ─────────────────────────────────────────────
void Manager::ChangeScene(Scene nextScene, float fadeOutDuration, float fadeInDuration, XMFLOAT4 color)
{
    m_NextScene = nextScene;
    m_SceneTransitionRequested = true;
    m_FadeInDuration = fadeInDuration;
    m_FadeColor = color;

    // フェードアウトを開始
    FadeSystem::StartFadeOut(fadeOutDuration, color);
}

// ─────────────────────────────────────────────
// シーン遷移の実際の実行
// ─────────────────────────────────────────────
void Manager::ExecuteChangeScene(Scene nextScene)
{
    OutputDebugStringA(("[Manager] ExecuteChangeScene: " + std::to_string((int)m_CurrentScene) + " -> " + std::to_string((int)nextScene) + "\n").c_str());

    // 現在アクティブなシーンオブジェクトのUninitと破棄
    if (m_ActiveScene) {
        m_ActiveScene->Uninit();
        delete m_ActiveScene;
        m_ActiveScene = nullptr;
    }

    // 既存登録オブジェクトのクリーンアップ（Uninitとdelete）
    for (GameObject* gameObject : m_GameObjects) {
        if (gameObject) {
            gameObject->Uninit();
            delete gameObject;
        }
    }
    m_GameObjects.clear();
    m_UpdateObjects.clear();
    m_CachedPlayer = nullptr;

    if (g_Camera) {
        g_Camera->Uninit();
        delete g_Camera;
        g_Camera = nullptr;
    }

    // 現在のシーン状態を更新
    m_CurrentScene = nextScene;

    // カメラはどのシーンでも必要なので、共通で生成する
    g_Camera = new Camera();
    g_Camera->Init();

    // 新しいシーンに対応するシーンオブジェクトを生成
    switch (nextScene)
    {
    case Scene::TITLE:
        m_ActiveScene = new TitleScene();
        break;
    case Scene::GAMEPLAY:
        m_ActiveScene = new GameplayScene();
        break;
    case Scene::CLEAR:
        m_ActiveScene = new ClearScene();
        break;
    case Scene::GAMEOVER:
        m_ActiveScene = new GameOverScene();
        break;
    }

    // 新しいシーンオブジェクトの初期化を実行
    if (m_ActiveScene) {
        m_ActiveScene->Init();
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
