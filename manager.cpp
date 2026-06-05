#include "manager.h"
#include "camera.h"
#include "input.h"
#include "main.h"
#include "player.h"
#include "renderer.h"
#include "resource_manager.h"
#include "collision.h"
#include "enemy.h"
#include "field.h"
#include "wall.h"
#include "item.h"
#include "game_rule.h"
#include "collision_system.h"
#include "score_popup.h"
#include "score_hud.h"
#include "shockwave.h"

// 静的メンバ変数の実体定義
std::list<GameObject*> Manager::m_GameObjects;
RenderSystem           Manager::m_RenderSystem;
int                    Manager::m_HitStopFrames = 0;
Camera*                g_Camera = nullptr;

// ─────────────────────────────────────────────
// 初期化
// ─────────────────────────────────────────────
void Manager::Init()
{
    Renderer::Init();
    Input::Init();
    GameRule::Init();

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

    // カメラオブジェクトの生成
    g_Camera = new Camera();
    g_Camera->Init();

    // 地面オブジェクトの生成
    AddGameObject<Field>();

    // プレイヤーオブジェクトの生成
    Player* player = AddGameObject<Player>();
    player->SetPosition(XMFLOAT3(0.0f, -0.5f, 0.0f));

    // 壁オブジェクトの生成
    Wall* wall = AddGameObject<Wall>();
    wall->SetPosition(XMFLOAT3(3.0f, 1.5f, 3.0f));
    wall->SetScale(XMFLOAT3(5.0f, 5.0f, 5.0f));

    // 敵を多め（4x4の計16体）に生成
    int totalEnemies = 0;
    for (int x = -2; x <= 1; x++) {
        for (int z = -2; z <= 1; z++) {
            Enemy* enemy = AddGameObject<Enemy>();
            float posX = (float)x * 3.2f + 1.0f;
            float posZ = (float)z * 3.2f - 7.0f;
            enemy->SetPosition(XMFLOAT3(posX, -0.5f, posZ));
            totalEnemies++;
        }
    }
    GameRule::SetTotalEnemies(totalEnemies);

    // パワーアップアイテムを生成（吸引、巨大化、雷電をそれぞれ配置）
    Item* itemVacuum = AddGameObject<Item>();
    itemVacuum->SetPosition(XMFLOAT3(0.0f, 0.5f, 4.0f));
    itemVacuum->SetItemType(ItemType::VACUUM);

    Item* itemGigant = AddGameObject<Item>();
    itemGigant->SetPosition(XMFLOAT3(-4.0f, 0.5f, 4.0f));
    itemGigant->SetItemType(ItemType::GIGANT);

    Item* itemLightning = AddGameObject<Item>();
    itemLightning->SetPosition(XMFLOAT3(2.0f, 0.5f, 6.0f));
    itemLightning->SetItemType(ItemType::LIGHTNING);

    // デバッグ出力
    OutputDebugStringA("[Manager] Init完了 - 敵の合計数: ");
    OutputDebugStringA(std::to_string(totalEnemies).c_str());
    OutputDebugStringA("\n");
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
    Input::Update();

    // スコアポップアップはヒットストップ中も含めて常に更新する
    ScorePopupSystem::Update();

    // スコアHUDも常に更新する
    ScoreHUD::Update();

    // 衝撃波システムも常に更新する
    ShockwaveSystem::Update();

    // クリア後はゲームオブジェクトの更新を行わない
    if (GameRule::IsGameClear()) return;

    // ヒットストップ中はカメラのみ更新し、他の更新をスキップする
    if (m_HitStopFrames > 0) {
        m_HitStopFrames--;
        if (g_Camera) g_Camera->Update();
        return;
    }

    // 1. 各オブジェクトの更新と破棄処理
    for (auto it = m_GameObjects.begin(); it != m_GameObjects.end(); ) {
        (*it)->Update();

        if ((*it)->IsDestroy()) {
            (*it)->Uninit();
            delete *it;
            it = m_GameObjects.erase(it);
        } else {
            it++;
        }
    }

    // 2. 衝突判定システムによる判定・物理連鎖の更新
    CollisionSystem::Update();

    // 3. 掴んでいるエネミーの位置確定後処理 (LateUpdate)
    Player* player = GetGameObject<Player>();
    if (player) {
        if (player->GetState() == PlayerState::GRABBED || player->GetState() == PlayerState::SPINNING) {
            Enemy* grabbedEnemy = player->GetGrabbedEnemy();
            if (grabbedEnemy) {
                Collision::ResolveGrabPhysics(player, grabbedEnemy, 0.8f);
            }
        }
    }

    // 4. 遅延ゲームクリア判定
    if (!GameRule::IsGameClear() && GameRule::GetDefeatedCount() >= GameRule::GetTotalEnemies()) {
        bool enemyExists = false;
        for (GameObject* obj : m_GameObjects) {
            if (dynamic_cast<Enemy*>(obj)) {
                enemyExists = true;
                break;
            }
        }

        if (!enemyExists) {
            GameRule::SetGameClear(true);
            OutputDebugStringA("[GameRule] *** ゲームクリア! ***\n");
        }
    }

    if (g_Camera) g_Camera->Update();
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
        if (!dynamic_cast<Field*>(obj) && 
            !dynamic_cast<Enemy*>(obj) && 
            !dynamic_cast<Player*>(obj) && 
            !dynamic_cast<Wall*>(obj)) {
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

    // 床(Field)を描画
    for (GameObject* obj : m_GameObjects) {
        if (dynamic_cast<Field*>(obj)) { obj->Draw(); }
    }

    // 床描画で変更されたトポロジーをLISTに戻す
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 一括描画以外のオブジェクトを個別描画（プレイヤーはガイドライン描画のため呼ぶ）
    Renderer::SetupCubeDraw();
    for (GameObject* obj : m_GameObjects) {
        if (!dynamic_cast<Field*>(obj) && 
            !dynamic_cast<Enemy*>(obj) && 
            !dynamic_cast<Wall*>(obj)) {
            obj->Draw();
            Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
    }

    // キューブオブジェクトの一括インスタンシング描画
    m_RenderSystem.RenderCubeInstances(Renderer::GetDeviceContext(), m_GameObjects, RenderPass::Normal);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // === 3. アウトライン描画 ===
    Renderer::BeginOutlinePass();
    Renderer::SetupCubeDraw();
    for (GameObject* obj : m_GameObjects) {
        if (!dynamic_cast<Field*>(obj) && 
            !dynamic_cast<Enemy*>(obj) && 
            !dynamic_cast<Player*>(obj) && 
            !dynamic_cast<Wall*>(obj)) {
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
