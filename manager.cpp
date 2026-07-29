#include "manager.h"
#include <string>
#include "camera.h"
#include "input.h"
#include "main.h"
#include "game_constants.h"
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
#include "enemy_bullet.h"
#include <algorithm>
#include "game_rule.h"
#include "collision_system.h"
#include "score_popup.h"
#include "score_hud.h"
#include "shockwave.h"
#include "event_system.h"
#include "event_types.h"

// DirectX 名前空間の使用
using namespace DirectX;

// 各シーンクラスのインクルード
#include "title_scene.h"
#include "gameplay_scene.h"
#include "clear_scene.h"
#include "gameover_scene.h"

// 静的メンバ変数の実体定義
std::vector<std::unique_ptr<GameObject>> Manager::m_ManagedObjects;
std::vector<GameObject*> Manager::m_GameObjects;
std::vector<GameObject*> Manager::m_UpdateObjects;
Player*                Manager::m_CachedPlayer = nullptr;
BossEnemy*             Manager::m_CachedBoss = nullptr;

std::unordered_map<ObjectType, std::vector<GameObject*>> Manager::m_CategoryMap;
RenderSystem           Manager::m_RenderSystem;
int                    Manager::m_HitStopFrames = 0;
int                    Manager::m_SlowMotionTimer = 0;
int                    Manager::m_SlowMotionDuration = 0;
Scene                  Manager::m_CurrentScene = Scene::TITLE;

bool                   Manager::m_IsBossStage = true;
Scene                  Manager::m_NextScene = Scene::TITLE;
bool                   Manager::m_SceneTransitionRequested = false;

IScene*                Manager::m_ActiveScene = nullptr;

XMFLOAT4               Manager::m_FlashColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
float                  Manager::m_FlashFadeSpeed = 0.05f;
bool                   Manager::m_IsLowHPWarning = false;
float                  Manager::m_LowHPPulseTime = 0.0f;

bool                   Manager::m_IsCutsceneActive = false;
int                    Manager::m_CutsceneTimer = 0;

Camera*                g_Camera = nullptr;

// ─────────────────────────────────────────────
// 初期化
// ─────────────────────────────────────────────
void Manager::Init()
{
    Renderer::Init();
    Input::Init();
    GameRule::Init();
    EnemyBullet::InitPool(); // 弾薬メモリプールの初期化
    m_IsCutsceneActive = false;
    m_CutsceneTimer = 0;
    m_HitStopFrames = 0;
    m_SlowMotionTimer = 0;
    m_SlowMotionDuration = 0;
    m_FlashColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    m_FlashFadeSpeed = 0.05f;
    m_IsLowHPWarning = false;
    m_LowHPPulseTime = 0.0f;
    m_CachedPlayer = nullptr;
    m_CachedBoss = nullptr;
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
    // アクティブなシーンオブジェクトの終了処理と破棄
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

    m_RenderSystem.ClearCache();
    m_RenderSystem.Uninit();

    // 登録されているゲームオブジェクトの破棄
    for (auto& gameObject : m_ManagedObjects) {
        if (gameObject) {
            gameObject->Uninit();
        }
    }
    m_ManagedObjects.clear();
    m_GameObjects.clear();
    m_UpdateObjects.clear();
    m_CachedPlayer = nullptr;
    m_CachedBoss = nullptr;
    ClearCategoryLists();

    if (g_Camera) {
        g_Camera->Uninit();
        delete g_Camera;
        g_Camera = nullptr;
    }

    Input::Uninit();
    ResourceManager::Uninit();
    EnemyBullet::UninitPool(); // 弾薬メモリプールの解放
    Renderer::Uninit();
}

// ─────────────────────────────────────────────
// 更新処理
// ─────────────────────────────────────────────
void Manager::Update()
{
    // ImGui の新規フレーム開始
    Renderer::BeginNewFrame();

    Input::Update();

    // スコアポップアップ表示の更新
    ScorePopupSystem::Update();

    // スコアHUDの更新
    ScoreHUD::Update();

    // 現在アクティブなシーンオブジェクトの更新を実行
    if (m_ActiveScene) {
        m_ActiveScene->Update();
    }

    if (g_Camera) g_Camera->Update();

    // 画面フラッシュと低HP警告赤パルスの更新
    if (m_FlashColor.w > 0.0f) {
        m_FlashColor.w -= m_FlashFadeSpeed;
        if (m_FlashColor.w < 0.0f) m_FlashColor.w = 0.0f;
    }

    // ボス登場カットシーンのタイムライン更新
    if (m_IsCutsceneActive) {
        m_CutsceneTimer--;

        // 1. スローモーション（ウィッチタイム）の持続
        // カットシーン中は、ボスとプレイヤー以外の動きを0.3倍速のスローモーションにする
        if (m_CutsceneTimer > 30) {
            m_SlowMotionTimer = 2; // 毎フレーム 2 を代入してスロー状態を維持
            m_SlowMotionDuration = 10;
        }

        // 2. タイムラインごとのイベントトリガー
        if (m_CutsceneTimer == 180) {
            // 開始時: 白フラッシュ（フェード速度はゆっくり 0.02f）
            TriggerFlash(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.8f), 0.02f);
        }
        else if (m_CutsceneTimer == 120) {
            // ボス咆哮・着地時: 足元に巨大でゆっくり広がる赤い衝撃波を発生（ダメージなし、force=0）
            BossEnemy* boss = GetGameObject<BossEnemy>();
            if (boss) {
                XMFLOAT3 bossPos = boss->GetPosition();
                // 衝撃波の発生（半径 15.0f, 赤, 継続 60フレーム, 吹き飛ばし力 0.0f, ディレイ 0, 収縮なし）
                ShockwaveSystem::AddShockwave(bossPos, 15.0f, 2.5f, 0.2f, 0.0f, 60, 0.0f, 0, false);
            }
            // 咆哮に合わせて、画面全体を「ゆったりとした警告赤パルス」で明滅させるために警告状態をON
            m_IsLowHPWarning = true;
            m_LowHPPulseTime = 0.0f; // パルス角度をリセット
        }
        else if (m_CutsceneTimer == 40) {
            // 咆哮終了: 赤パルス明滅をOFFにする
            m_IsLowHPWarning = false;
            // 通常画面に戻るフェード
            TriggerFlash(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f), 0.05f);
        }
        else if (m_CutsceneTimer <= 0) {
            // カットシーン終了
            m_IsCutsceneActive = false;
            m_IsLowHPWarning = false;
            m_SlowMotionTimer = 0; // スローモーション解除
            
            // 戦闘開始イベントを発行
            BossBattleStartEvent startEvent;
            EventSystem::Publish<BossBattleStartEvent>(startEvent);
        }
    }

    if (m_IsLowHPWarning) {
        // 低HP時は、画面を赤くパルス（明滅）させる（カットシーン時はゆっくり、通常プレイのHP1時は高速）
        m_LowHPPulseTime += m_IsCutsceneActive ? 0.06f : 0.15f; 
        // サイン波を用いてアルファ値を 0.04 ~ 0.28 の間で脈動させる
        float pulseAlpha = 0.16f + 0.12f * sinf(m_LowHPPulseTime);
        
        // 通常のフラッシュが走っていない場合は警告赤をセット
        if (m_FlashColor.w <= 0.0f) {
            m_FlashColor = XMFLOAT4(1.0f, 0.0f, 0.0f, pulseAlpha);
        } else {
            // 通常フラッシュが優先され、警告赤をブレンドしてマージ
            m_FlashColor.x = m_FlashColor.x + (1.0f - m_FlashColor.x) * pulseAlpha;
            m_FlashColor.w = (std::max)(m_FlashColor.w, pulseAlpha);
        }
    } else {
        m_LowHPPulseTime = 0.0f;
    }

    // 遅延シーン遷移リクエストがある場合は実行する
    if (m_SceneTransitionRequested) {
        m_SceneTransitionRequested = false;
        ExecuteChangeScene(m_NextScene);
    }
}

// ─────────────────────────────────────────────
// 描画処理
// ─────────────────────────────────────────────
void Manager::Draw()
{
    m_RenderSystem.ClearCache(); // キャッシュクリア

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
    Renderer::SetProjectionMatrix(cameraProj); // カメラのプロジェクション行列を再設定

    // インスタンシングまたは別パスで描画されるオブジェクトを除外する判定
    auto ShouldSkipShadowOutline = [](ObjectType type) -> bool {
        return type == ObjectType::Field
            || type == ObjectType::Enemy
            || type == ObjectType::Player
            || type == ObjectType::Wall
            || type == ObjectType::Boss
            || type == ObjectType::Unknown;
    };

    // === 1. シャドウマップ描画パス ===
    Renderer::BeginShadowPass();
    Renderer::SetupCubeDraw();
    for (GameObject* obj : m_GameObjects) {
        if (!ShouldSkipShadowOutline(obj->GetObjectType())) {
            obj->Draw();
            Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
    }
    m_RenderSystem.RenderCubeInstances(Renderer::GetDeviceContext(), m_GameObjects, RenderPass::Shadow);
    Renderer::EndShadowPass();

    // === 2. 通常描画パス ===
    Renderer::SetViewMatrix(cameraView);
    Renderer::SetProjectionMatrix(cameraProj);
    Renderer::Begin();

    for (GameObject* obj : m_GameObjects) {
        if (obj->GetObjectType() == ObjectType::Unknown) { obj->Draw(); }
    }
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (GameObject* obj : m_GameObjects) {
        if (obj->GetObjectType() == ObjectType::Field) { obj->Draw(); }
    }
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Renderer::SetupCubeDraw();
    for (GameObject* obj : m_GameObjects) {
        ObjectType type = obj->GetObjectType();
        if (type == ObjectType::Field || type == ObjectType::Enemy || type == ObjectType::Wall) {
            continue;
        }
        obj->Draw();
        Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    m_RenderSystem.RenderCubeInstances(Renderer::GetDeviceContext(), m_GameObjects, RenderPass::Normal);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // === 3. アウトライン描画パス (シャドウと同様の除外判定) ===
    Renderer::BeginOutlinePass();
    Renderer::SetupCubeDraw();
    for (GameObject* obj : m_GameObjects) {
        if (!ShouldSkipShadowOutline(obj->GetObjectType())) {
            obj->Draw();
            Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
    }
    m_RenderSystem.RenderCubeInstances(Renderer::GetDeviceContext(), m_GameObjects, RenderPass::Outline);
    Renderer::EndOutlinePass();

    if (g_Camera) g_Camera->Draw();

    ScorePopupSystem::Draw();
    ShockwaveSystem::Draw();
    ScoreHUD::Draw();

    if (m_ActiveScene) {
        m_ActiveScene->Draw();
    }

    Renderer::End();
}

// ─────────────────────────────────────────────
// ボスステージへの遷移と構築
// ─────────────────────────────────────────────
void Manager::TransitionToBossStage()
{
    LOG_INFO("[Manager] TransitionToBossStage - 開始\n");
    m_IsBossStage = true;

    // プレイヤーの位置を中央にリセット
    Player* player = GetGameObject<Player>();
    if (player) {
        LOG_INFO("[Manager] プレイヤー位置をリセット\n");
        player->SetPosition(XMFLOAT3(0.0f, -0.5f, 0.0f));
    }

    // プレイヤーとフィールド以外のオブジェクトをすべて破棄
    LOG_INFO("[Manager] オブジェクト破棄を開始します...\n");
    int destroyedCount = 0;
    m_ManagedObjects.erase(
        std::remove_if(m_ManagedObjects.begin(), m_ManagedObjects.end(),
            [player, &destroyedCount](const std::unique_ptr<GameObject>& obj) {
                ObjectType t = obj->GetObjectType();
                if (t != ObjectType::Player && t != ObjectType::Field) {
                    if (player) {
                        player->NotifyObjectDestroyed(obj.get());
                    }
                    obj->Uninit();
                    destroyedCount++;
                    return true;
                }
                return false;
            }),
        m_ManagedObjects.end());

    // 描画・走査用リストからも削除
    m_GameObjects.erase(
        std::remove_if(m_GameObjects.begin(), m_GameObjects.end(),
            [](GameObject* obj) {
                ObjectType t = obj->GetObjectType();
                return t != ObjectType::Player && t != ObjectType::Field;
            }),
        m_GameObjects.end());

    // カテゴリ別キャッシュリストのクリアと再登録
    ClearCategoryLists();
    m_UpdateObjects.clear();
    for (GameObject* obj : m_GameObjects) {
        ::ObjectType t = obj->GetObjectType();
        if (t != ::ObjectType::Wall && t != ::ObjectType::Field) {
            m_UpdateObjects.push_back(obj);
        }
        RegisterCategory(obj);
    }
    LOG_INFO("[Manager] オブジェクト破棄クリーンアップ完了 (破棄数: %d)\n", destroyedCount);
    m_RenderSystem.ClearCache();

    // ボス部屋の壁を生成
    LOG_INFO("[Manager] ボス部屋の壁を生成します...\n");
    float roomSize = Constants::Stage::BOSS_ROOM_SIZE;
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
    LOG_INFO("[Manager] 壁の配置完了\n");

    // ボスエネミーの生成
    LOG_INFO("[Manager] ボスエネミーの生成を開始します...\n");
    BossEnemy* boss = AddGameObject<BossEnemy>();
    boss->SetPosition(XMFLOAT3(0.0f, 25.0f, 10.0f)); // 上空から登場させるため初期Y座標を25.0fに変更
    LOG_INFO("[Manager] ボスエネミー生成完了\n");

    // ボス戦用のアイテム生成
    LOG_INFO("[Manager] ボス部屋用のアイテムを生成します...\n");
    Item* itemVacuum = AddGameObject<Item>();
    itemVacuum->SetPosition(XMFLOAT3(0.0f, 0.5f, -15.0f));
    itemVacuum->SetItemType(ItemType::VACUUM);

    Item* itemGigant = AddGameObject<Item>();
    itemGigant->SetPosition(XMFLOAT3(-4.0f, 0.5f, -15.0f));
    itemGigant->SetItemType(ItemType::GIGANT);

    Item* itemLightning = AddGameObject<Item>();
    itemLightning->SetPosition(XMFLOAT3(4.0f, 0.5f, -15.0f));
    itemLightning->SetItemType(ItemType::LIGHTNING);
    
    // ボス登場カットシーンのトリガー
    TriggerBossSpawnCutscene();
    LOG_INFO("[Manager] TransitionToBossStage - 正常終了\n");
}

// ─────────────────────────────────────────────
// ボス登場カットシーンのトリガー
// ─────────────────────────────────────────────
void Manager::TriggerBossSpawnCutscene()
{
    m_IsCutsceneActive = true;
    m_CutsceneTimer = 180; // 3秒間 (60fps)

    // イベント発行
    BossSpawnEvent spawnEvent;
    BossEnemy* boss = GetGameObject<BossEnemy>();
    if (boss) {
        spawnEvent.bossPosition = boss->GetPosition();
    } else {
        spawnEvent.bossPosition = XMFLOAT3(0.0f, 1.5f, 10.0f);
    }
    EventSystem::Publish<BossSpawnEvent>(spawnEvent);
}

// ─────────────────────────────────────────────
// シーン遷移予約
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

    // 現在アクティブなシーンオブジェクトの Uninit と破棄
    if (m_ActiveScene) {
        m_ActiveScene->Uninit();
        delete m_ActiveScene;
        m_ActiveScene = nullptr;
    }

    // 既存登録オブジェクトのクリーンアップ
    for (auto& gameObject : m_ManagedObjects) {
        if (gameObject) {
            gameObject->Uninit();
        }
    }
    m_ManagedObjects.clear();
    m_GameObjects.clear();
    m_UpdateObjects.clear();
    m_CachedPlayer = nullptr;
    m_CachedBoss = nullptr;
    ClearCategoryLists(); // カテゴリ別キャッシュリストも必ずクリアする（ダングリングポインタ防止）
    m_RenderSystem.ClearCache();

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
// スローモーション強度の取得（フェードアウト演出等用）
// ─────────────────────────────────────────────
float Manager::GetSlowMotionIntensity()
{
    if (m_SlowMotionTimer <= 0) return 0.0f;
    if (m_SlowMotionTimer > 30) return 1.0f;
    return (float)m_SlowMotionTimer / 30.0f;
}

// ─────────────────────────────────────────────
// 画面フラッシュの発動
// ─────────────────────────────────────────────
void Manager::TriggerFlash(XMFLOAT4 color, float fadeSpeed)
{
    m_FlashColor = color;
    m_FlashFadeSpeed = fadeSpeed;
}

// ─────────────────────────────────────────────
// カテゴリ別オブジェクトキャッシュ登録・解除
// ─────────────────────────────────────────────
void Manager::RegisterCategory(GameObject* obj)
{
    if (!obj) return;
    ObjectType type = obj->GetObjectType();
    m_CategoryMap[type].push_back(obj);
}

void Manager::UnregisterCategory(GameObject* obj)
{
    if (!obj) return;

    if (obj == m_CachedBoss) {
        m_CachedBoss = nullptr;
    }

    ObjectType type = obj->GetObjectType();
    auto it = m_CategoryMap.find(type);
    if (it != m_CategoryMap.end()) {
        auto& vec = it->second;
        auto vit = std::find(vec.begin(), vec.end(), obj);
        if (vit != vec.end()) {
            std::iter_swap(vit, vec.end() - 1);
            vec.pop_back();
        }
    }
}

void Manager::ClearCategoryLists()
{
    m_CategoryMap.clear();
}

// ─────────────────────────────────────────────
// 不要になった（IsDestroy == true）オブジェクトの自動クリーンアップ
// ─────────────────────────────────────────────
void Manager::DestroyObjectsIf()
{
    Player* player = GetGameObject<Player>();

    // 1. 管理オブジェクトリストから IsDestroy() == true を安全に破棄
    m_ManagedObjects.erase(
        std::remove_if(m_ManagedObjects.begin(), m_ManagedObjects.end(),
            [player](const std::unique_ptr<GameObject>& obj) {
                if (obj && obj->IsDestroy()) {
                    if (player) {
                        player->NotifyObjectDestroyed(obj.get());
                    }
                    if (obj.get() == m_CachedPlayer) {
                        m_CachedPlayer = nullptr;
                    }
                    if (obj.get() == m_CachedBoss) {
                        m_CachedBoss = nullptr;
                    }
                    UnregisterCategory(obj.get());
                    obj->Uninit();
                    return true;
                }
                return false;
            }),
        m_ManagedObjects.end());

    // 2. 描画・更新用生ポインタリストからも破棄
    m_GameObjects.erase(
        std::remove_if(m_GameObjects.begin(), m_GameObjects.end(),
            [](GameObject* obj) {
                return obj == nullptr || obj->IsDestroy();
            }),
        m_GameObjects.end());

    m_UpdateObjects.erase(
        std::remove_if(m_UpdateObjects.begin(), m_UpdateObjects.end(),
            [](GameObject* obj) {
                return obj == nullptr || obj->IsDestroy();
            }),
        m_UpdateObjects.end());

    m_RenderSystem.ClearCache();
}
