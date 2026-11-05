#include "manager.h"
#include <string>
#include <unordered_set>
#include "camera.h"
#include "input.h"
#include "main.h"
#include "game_constants.h"
#include "render_constants.h"
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
#include "aura_effect.h"
#include "hit_impact_effect.h"
#include "dust_system.h"
#include "shadow_system.h"
#include "event_system.h"
#include "event_types.h"
#include "sound_manager.h"
#include "option_ui.h"
#include "progression_manager.h"
#include "item_notification_hud.h"
#include "graphics_config.h"

// DirectX 名前空間の使用
using namespace DirectX;

// 各シーンクラスのインクルード
#include "title_scene.h"
#include "tutorial_scene.h"
#include "hub_scene.h"
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
bool                   Manager::m_StartFromBoss = false;
Scene                  Manager::m_NextScene = Scene::TITLE;
bool                   Manager::m_SceneTransitionRequested = false;
bool                   Manager::m_PendingFadeIn = false;
float                  Manager::m_FadeInDuration = 0.5f;
XMFLOAT4               Manager::m_FadeColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

IScene*                Manager::m_ActiveScene = nullptr;

XMFLOAT4               Manager::m_FlashColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
float                  Manager::m_FlashFadeSpeed = 0.05f;
bool                   Manager::m_IsLowHPWarning = false;
float                  Manager::m_LowHPPulseTime = 0.0f;
float                  Manager::m_LowHPAlpha = 0.0f;

bool                   Manager::m_IsCutsceneActive = false;
int                    Manager::m_CutsceneTimer = 0;

int                    Manager::m_TargetFPS = 60;
float                  Manager::m_CurrentFPS = 60.0f;
int                    Manager::m_FpsFrameCount = 0;
float                  Manager::m_FpsTimeAccumulator = 0.0f;

float                  Manager::m_RadialBlurIntensity = 0.0f;
float                  Manager::m_ChromaticAberration = 0.0f;
DirectX::XMFLOAT2      Manager::m_RadialBlurCenter = DirectX::XMFLOAT2(0.5f, 0.5f);

Camera*                g_Camera = nullptr;

// ─────────────────────────────────────────────
// 初期化
// ─────────────────────────────────────────────
void Manager::Init()
{
    Renderer::Init();
    Input::Init();
    GameRule::Init();
    ProgressionManager::Init(); // セーブデータ読み込みと進行管理の初期化
    EnemyBullet::InitPool(); // 弾薬メモリプールの初期化
    m_IsCutsceneActive = false;
    m_CutsceneTimer = 0;
    m_RadialBlurIntensity = 0.0f;
    m_ChromaticAberration = 0.0f;
    m_RadialBlurCenter = XMFLOAT2(0.5f, 0.5f);
    m_HitStopFrames = 0;
    m_SlowMotionTimer = 0;
    m_SlowMotionDuration = 0;
    m_FlashColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    m_FlashFadeSpeed = 0.05f;
    m_IsLowHPWarning = false;
    m_LowHPPulseTime = 0.0f;
    m_LowHPAlpha = 0.0f;
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
    AuraEffect::Init(Renderer::GetDevice());
    HitImpactEffect::Init(Renderer::GetDevice());

    // 砂埃パーティクルシステムの初期化
    DustSystem::Init();

    // 接地丸影（ドロップシャドウ）システムの初期化
    ShadowSystem::Init();

    // アイテム入手通知HUDの初期化
    ItemNotificationHUD::Init();

    // フェードシステムの初期化
    FadeSystem::Init(Renderer::GetDevice());

    // サウンドシステムの初期化
    SoundManager::Init();

    // オプションUIの初期化
    OptionUI::Init();

    // 光源設定
    LIGHT light;
    ZeroMemory(&light, sizeof(light));
    light.Enable = TRUE;
    light.Direction = RenderConstants::DefaultLight::LIGHT_DIRECTION;
    light.Diffuse = RenderConstants::DefaultLight::LIGHT_DIFFUSE;
    light.Ambient = RenderConstants::DefaultLight::LIGHT_AMBIENT;
    light.FogColor = RenderConstants::DefaultLight::LIGHT_FOG_COLOR;
    light.FogStart = RenderConstants::DefaultLight::FOG_START;
    light.FogEnd = RenderConstants::DefaultLight::FOG_END;
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
    HitImpactEffect::Uninit();
    AuraEffect::Uninit();

    // 砂埃パーティクルシステムの終了処理
    DustSystem::Uninit();

    // 接地丸影（ドロップシャドウ）システムの終了処理
    ShadowSystem::Uninit();

    // アイテム入手通知HUDの終了処理
    ItemNotificationHUD::Uninit();

    // フェードシステムの終了処理
    FadeSystem::Uninit();

    m_RenderSystem.ClearCache();
    m_RenderSystem.Uninit();

    // 登録されているゲームオブジェクトの破棄（DestroyObjectsIf に一元化）
    DestroyObjectsIf([](GameObject*) { return true; });

    if (g_Camera) {
        g_Camera->Uninit();
        delete g_Camera;
        g_Camera = nullptr;
    }

    SoundManager::Uninit();
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
    Input::Update();
    SoundManager::Update();

    // オプションUIの更新（Tabキー監視）
    OptionUI::Update();

    // グラフィック設定変更の即時反映（ポーズ中も反映）
    if (GraphicsConfig::ConsumeDirty()) {
        const GraphicsSettings& s = GraphicsConfig::GetConst();
        int aniso = (s.anisotropy == AnisoQuality::Aniso16x) ? 16 :
                    (s.anisotropy == AnisoQuality::Aniso8x)  ? 8 :
                    (s.anisotropy == AnisoQuality::Aniso4x)  ? 4 : 2;
        Renderer::SetAnisotropy(aniso);
    }

    // オプション画面が開いている間は、背後のゲームプレイ更新を一時停止（ポーズ）する
    if (OptionUI::IsOpen()) {
        return;
    }

    // フェードシステムのタイマー更新
    FadeSystem::Update(1.0f / 60.0f);

    // スコアポップアップ表示の更新
    ScorePopupSystem::Update();

    // スコアHUDの更新
    ScoreHUD::Update();

    // 砂埃パーティクルシステムの更新
    DustSystem::Update();

    // アイテム入手通知HUDのタイマー更新
    ItemNotificationHUD::Update(1.0f / 60.0f);

    // 現在アクティブなシーンオブジェクトの更新を実行
    if (m_ActiveScene) {
        m_ActiveScene->Update();
    }

    if (g_Camera) g_Camera->Update();

    // 画面フラッシュの減衰（フェードアウト完了時は色も完全リセット）
    if (m_FlashColor.w > 0.0f) {
        m_FlashColor.w -= m_FlashFadeSpeed;
        if (m_FlashColor.w <= 0.0f) {
            m_FlashColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        }
    }

    // ラジアルブラーと色収差の自然減衰
    if (m_RadialBlurIntensity > 0.001f) {
        m_RadialBlurIntensity *= 0.85f;
        if (m_RadialBlurIntensity < 0.001f) m_RadialBlurIntensity = 0.0f;
    }
    if (m_ChromaticAberration > 0.001f) {
        m_ChromaticAberration *= 0.85f;
        if (m_ChromaticAberration < 0.001f) m_ChromaticAberration = 0.0f;
    }

    // ボス登場カットシーンのタイムライン更新
    if (m_IsCutsceneActive) {
        UpdateBossCutscene();
    }

    if (m_IsLowHPWarning) {
        // 低HP時は、画面を赤くパルス（明滅）させる（カットシーン時はゆっくり、通常プレイのHP1時は高速）
        m_LowHPPulseTime += m_IsCutsceneActive ? 0.06f : 0.15f; 
        // サイン波を用いてアルファ値を 0.04 ~ 0.28 の間で脈動させる
        m_LowHPAlpha = 0.16f + 0.12f * sinf(m_LowHPPulseTime);
    } else {
        m_LowHPPulseTime = 0.0f;
        m_LowHPAlpha = 0.0f;
    }

    // 遅延シーン遷移のリクエストがあり、かつ暗転（FadeHold）が完了した場合に切り替える
    if (m_SceneTransitionRequested) {
        if (!FadeSystem::IsFading() || FadeSystem::IsFadeOutComplete()) {
            m_SceneTransitionRequested = false;

            // 1. 新しいシーンへ切り替えと初期化を実行
            ExecuteChangeScene(m_NextScene);

            // 2. 描画キャッシュの完全クリア
            m_RenderSystem.ClearCache();

            // 3. このフレームは FadeHold（完全暗転 100%遮断）のまま新シーンの初期化・初描画・テクスチャバインドを完了させ、
            //    次フレームで明転（FadeIn）を開始する
            m_PendingFadeIn = true;
        }
    } else if (m_PendingFadeIn) {
        // 暗転の裏で新シーンの1フレーム目の描画・テクスチャバインドが正常に完了したため、明転を開始
        m_PendingFadeIn = false;
        FadeSystem::StartFadeIn(m_FadeInDuration, m_FadeColor);
    }
}

// ─────────────────────────────────────────────
// 描画処理
// ─────────────────────────────────────────────
void Manager::Draw(float alpha)
{
    // 描画補間: カメラのビュー行列を alpha に応じて補間適用
    if (g_Camera) {
        g_Camera->ApplyViewMatrix(alpha);
    }

    // ImGui の新規フレーム開始（Drawと1:1でペアにする）
    Renderer::BeginNewFrame();

    m_RenderSystem.ClearCache(); // キャッシュクリア
    ID3D11DeviceContext* context = Renderer::GetDeviceContext();

    XMMATRIX cameraView = Renderer::GetViewMatrix();
    XMMATRIX cameraProj = Renderer::GetProjectionMatrix();

    LIGHT currentLight = Renderer::GetLight();
    XMVECTOR lightDir = XMLoadFloat4(&currentLight.Direction);
    XMVECTOR lightPos = XMVector3Normalize(lightDir) * -40.0f;
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos,
        XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    XMMATRIX lightProj = XMMatrixOrthographicLH(
        RenderConstants::Shadow::PROJECTION_SIZE,
        RenderConstants::Shadow::PROJECTION_SIZE,
        1.0f, 90.0f);
    Renderer::SetShadowVPMatrix(lightView * lightProj);
    Renderer::SetViewMatrix(lightView);
    Renderer::SetProjectionMatrix(cameraProj); // カメラのプロジェクション行列を再設定

    // === 1. シャドウマップ描画パス ===
    Renderer::BeginShadowPass();
    Renderer::SetupCubeDraw();
    DrawShadowAndOutlineObjects();
    m_RenderSystem.RenderCubeInstances(context, m_GameObjects, RenderPass::Shadow);
    Renderer::EndShadowPass();

    // === 2. 通常描画パス ===
    Renderer::SetViewMatrix(cameraView);
    Renderer::SetProjectionMatrix(cameraProj);
    Renderer::Begin();

    for (GameObject* obj : GetCategoryList(ObjectType::Unknown)) {
        if (obj && !obj->IsDestroy()) { obj->Draw(); }
    }
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (GameObject* obj : GetCategoryList(ObjectType::Field)) {
        if (obj && !obj->IsDestroy()) { obj->Draw(); }
    }
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const GraphicsSettings& gs = GraphicsConfig::GetConst();

    // 全キャラクター・敵・アイテムの接地丸影（ドロップシャドウ）を描画
    if (gs.blobShadow) {
        ShadowSystem::Draw(m_GameObjects);
    }

    // 1. 本体キューブのインスタンシング描画（敵、プレイヤー、壁など）
    m_RenderSystem.RenderCubeInstances(context, m_GameObjects, RenderPass::Normal);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 2. 各オブジェクトの個別装飾・目・個別エフェクトの描画（常時有効）
    Renderer::SetupCubeDraw();
    for (GameObject* obj : m_GameObjects) {
        if (!obj || obj->IsDestroy()) continue;
        ObjectType type = obj->GetObjectType();
        if (type == ObjectType::Field || type == ObjectType::Wall || type == ObjectType::Unknown) {
            continue;
        }
        obj->Draw();
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    // === 3. アウトライン描画パス (シャドウと同様の除外判定) ===
    if (gs.outline != OutlineQuality::Outline_Off) {
        Renderer::BeginOutlinePass();
        Renderer::SetupCubeDraw();
        if (gs.outline == OutlineQuality::Outline_All) {
            DrawShadowAndOutlineObjects();
        }
        m_RenderSystem.RenderCubeInstances(context, m_GameObjects, RenderPass::Outline);
        Renderer::EndOutlinePass();
    }

    if (g_Camera) g_Camera->Draw();

    ScorePopupSystem::Draw();
    ShockwaveSystem::Draw();
    // AuraEffect は Renderer::End のポスト合成後(ブラーの後)に描画する
    DustSystem::Draw();
    ScoreHUD::Draw();

    // シーン固有の3D描画（リザルト画面の巨石など）のため、深度ステートを確実に有効状態にセット
    Renderer::GetDeviceContext()->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
    if (m_ActiveScene) {
        m_ActiveScene->Draw();
    }

    // オプション画面UIの描画（ImGuiコマンドを発行）
    OptionUI::Draw();

    // アイテム入手通知HUDの描画（ImGuiオーバーレイ）
    ItemNotificationHUD::Draw();

    Renderer::End();
}

// ─────────────────────────────────────────────
// シャドウ／アウトラインパス共通のオブジェクト描画
// ─────────────────────────────────────────────
void Manager::DrawShadowAndOutlineObjects()
{
    // インスタンシングまたは別パスで描画されるオブジェクトを除外する判定
    auto ShouldSkipShadowOutline = [](ObjectType type) -> bool {
        return type == ObjectType::Field
            || type == ObjectType::Enemy
            || type == ObjectType::Player
            || type == ObjectType::Wall
            || type == ObjectType::Boss
            || type == ObjectType::Bullet
            || type == ObjectType::Prop
            || type == ObjectType::Unknown;
    };

    for (GameObject* obj : m_GameObjects) {
        if (obj && !ShouldSkipShadowOutline(obj->GetObjectType())) {
            obj->Draw();
            Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
    }
}

// ─────────────────────────────────────────────
// ボスステージへの遷移と構築
// ─────────────────────────────────────────────
void Manager::TransitionToBossStage()
{
    LOG_INFO("[Manager] TransitionToBossStage - 開始\n");
    m_IsBossStage = true;

    // スローモーション・ヒットストップ・エフェクトの完全リセット
    m_HitStopFrames = 0;
    m_SlowMotionTimer = 0;
    m_SlowMotionDuration = 0;
    ShockwaveSystem::Clear();
    DustSystem::Clear();
    ScorePopupSystem::Clear();

    // ボス戦専用BGMの再生開始
    SoundManager::PlayBGM(SoundID::BGM_BOSS_STAGE, true);

    // プレイヤーの位置とエフェクトをリセット
    Player* player = GetGameObject<Player>();
    if (player) {
        LOG_INFO("[Manager] プレイヤー位置をリセット\n");
        player->SetPosition(XMFLOAT3(0.0f, -0.5f, 0.0f));
        player->GetLightningEffects().clear();
        player->SetLockOnTarget(nullptr);
        player->SetLockOnFrame(0);
        player->SetTackleTimer(0);
        player->SetDamageTimer(0);
        player->SetGuardTimer(0);
        player->SetWarpSlashCount(0);
        player->SetCanWarpSlash(true);
    }

    // プレイヤーとフィールド以外のオブジェクトをすべて破棄（破棄処理の一元化）
    LOG_INFO("[Manager] オブジェクト破棄を開始します...\n");
    DestroyObjectsIf([](GameObject* obj) {
        if (!obj) return false;
        ObjectType t = obj->GetObjectType();
        return t != ObjectType::Player && t != ObjectType::Field;
    });
    LOG_INFO("[Manager] オブジェクト破棄クリーンアップ完了\n");

    // ボス部屋の四方壁を生成（高く・厚くしてプレイヤーの場外飛び出しを防止）
    Wall::CreateBoundaryWalls(
        Constants::Stage::BOSS_ROOM_SIZE,
        Constants::Stage::BOSS_WALL_HEIGHT,
        Constants::Stage::BOSS_WALL_Y,
        Constants::Stage::BOSS_WALL_THICKNESS);

    // ボスエネミーの生成
    BossEnemy* boss = AddGameObject<BossEnemy>();
    boss->SetPosition(XMFLOAT3(0.0f, Constants::Boss::GROUND_Y, Constants::Stage::BOSS_SPAWN_OFFSET_Z));
    boss->SetRotation(XMFLOAT3(0.0f, XM_PI, 0.0f));

    // ボス戦用のアイテム生成
    Item* itemVacuum = AddGameObject<Item>();
    itemVacuum->SetPosition(XMFLOAT3(0.0f, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_BOSS_VACUUM_Z));
    itemVacuum->SetItemType(ItemType::VACUUM);

    Item* itemGigant = AddGameObject<Item>();
    itemGigant->SetPosition(XMFLOAT3(Constants::Stage::ITEM_BOSS_GIGANT_X, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_BOSS_GIGANT_Z));
    itemGigant->SetItemType(ItemType::GIGANT);

    Item* itemLightning = AddGameObject<Item>();
    itemLightning->SetPosition(XMFLOAT3(Constants::Stage::ITEM_BOSS_LIGHTNING_X, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_BOSS_LIGHTNING_Z));
    itemLightning->SetItemType(ItemType::LIGHTNING);
    
    // ボス登場カットシーンのトリガー
    TriggerBossSpawnCutscene();

    if (g_Camera) {
        g_Camera->Update();
    }
    LOG_INFO("[Manager] TransitionToBossStage - 正常終了\n");
}

// ─────────────────────────────────────────────
// ボス登場カットシーンのトリガー
// ─────────────────────────────────────────────
void Manager::UpdateBossCutscene()
{
    m_CutsceneTimer--;

    // 1. スローモーション（ウィッチタイム）の持続
    if (m_CutsceneTimer > 30) {
        m_SlowMotionTimer = 2;
        m_SlowMotionDuration = 10;
    }

    // 2. タイムラインごとのイベントトリガー
    if (m_CutsceneTimer == 180) {
        TriggerFlash(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.8f), 0.02f);
    }
    else if (m_CutsceneTimer == 120) {
        BossEnemy* boss = GetGameObject<BossEnemy>();
        if (boss) {
            ShockwaveSystem::AddShockwave(boss->GetPosition(), 15.0f, 2.5f, 0.2f, 0.0f, 60, 0.0f, 0, false);
        }
        m_IsLowHPWarning = true;
        m_LowHPPulseTime = 0.0f;
    }
    else if (m_CutsceneTimer == 40) {
        m_IsLowHPWarning = false;
        TriggerFlash(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f), 0.05f);
    }
    else if (m_CutsceneTimer <= 0) {
        m_IsCutsceneActive = false;
        m_IsLowHPWarning = false;
        m_SlowMotionTimer = 0;

        Player* player = GetGameObject<Player>();
        if (player) {
            player->SetLockOnTarget(nullptr);
            player->SetLockOnFrame(0);
        }

        BossBattleStartEvent startEvent;
        EventSystem::Publish<BossBattleStartEvent>(startEvent);
    }
}

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
void Manager::ChangeScene(Scene nextScene, float fadeOutDuration, float fadeInDuration, XMFLOAT4 color)
{
    // 既に遷移中（フェードアウト中や切り替え・明転待ち中）なら多重リクエストを防止（フェードリセットによるスタック防止）
    if (m_SceneTransitionRequested || m_PendingFadeIn) {
        return;
    }

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
    LOG_INFO("[Manager] ExecuteChangeScene: %d -> %d\n", (int)m_CurrentScene, (int)nextScene);

    // 現在アクティブなシーンオブジェクトの Uninit と破棄
    if (m_ActiveScene) {
        m_ActiveScene->Uninit();
        delete m_ActiveScene;
        m_ActiveScene = nullptr;
    }

    // 既存登録オブジェクトのクリーンアップ（DestroyObjectsIf に一元化）
    DestroyObjectsIf([](GameObject*) { return true; });

    if (g_Camera) {
        g_Camera->Uninit();
        delete g_Camera;
        g_Camera = nullptr;
    }

    // 現在のシーン状態を更新
    m_CurrentScene = nextScene;

    // シーン遷移時に低HP警告パルスや画面フラッシュ演出を完全リセット
    m_IsLowHPWarning = false;
    m_LowHPPulseTime = 0.0f;
    m_LowHPAlpha = 0.0f;
    m_FlashColor = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

    // スローモーション・ヒットストップを完全リセット（前シーンの演出残存を防止）
    m_HitStopFrames = 0;
    m_SlowMotionTimer = 0;
    m_SlowMotionDuration = 0;

    // シーン遷移時に衝撃波・砂埃・ポップアップを完全クリア（前シーンのエフェクト残存防止）
    ShockwaveSystem::Clear();
    DustSystem::Clear();
    ScorePopupSystem::Clear();
    ItemNotificationHUD::Clear();

    // カメラはどのシーンでも必要なので、共通で生成する
    g_Camera = new Camera();
    g_Camera->Init();

    // 新しいシーンに対応するシーンオブジェクトを生成
    switch (nextScene)
    {
    case Scene::TITLE:
        m_ActiveScene = new TitleScene();
        SoundManager::PlayBGM(SoundID::BGM_TITLE);
        break;
    case Scene::TUTORIAL:
        m_ActiveScene = new TutorialScene();
        SoundManager::PlayBGM(SoundID::BGM_NORMAL_STAGE);
        break;
    case Scene::HUB:
        m_ActiveScene = new HubScene();
        SoundManager::PlayBGM(SoundID::BGM_TITLE);
        break;
    case Scene::GAMEPLAY:
        m_ActiveScene = new GameplayScene();
        if (Constants::Debug::START_FROM_BOSS) {
            SoundManager::PlayBGM(SoundID::BGM_BOSS_STAGE);
        } else {
            SoundManager::PlayBGM(SoundID::BGM_NORMAL_STAGE);
        }
        break;
    case Scene::CLEAR:
        m_ActiveScene = new ClearScene();
        SoundManager::PlayBGM(SoundID::BGM_CLEAR, false); // クリアはループなし
        break;
    case Scene::GAMEOVER:
        m_ActiveScene = new GameOverScene();
        SoundManager::PlayBGM(SoundID::BGM_LOSE);
        break;
    }

    // 新しいシーンオブジェクトの初期化を実行
    if (m_ActiveScene) {
        m_ActiveScene->Init();
    }

    // 新シーン構築直後にカメラの初期姿勢・ビュー行列を即座に確定させる
    // （初描画フレームで古いビュー行列が使われてアウトラインや影が画面全体に四角く歪む現象を完全防止）
    if (g_Camera) {
        g_Camera->Update();
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
    DestroyObjectsIf([](GameObject* obj) {
        return obj && obj->IsDestroy();
    });
}

// ─────────────────────────────────────────────
// ─────────────────────────────────────────────
// 条件指定によるオブジェクト破棄の一元化
// ─────────────────────────────────────────────
void Manager::DestroyObjectsIf(std::function<bool(GameObject*)> pred)
{
    if (!pred || m_ManagedObjects.empty()) {
        return;
    }

    Player* player = GetGameObject<Player>();

    // 1. 破棄対象ポインタの収集（predの評価を一度だけ安全に行う）
    std::vector<GameObject*> targets;
    for (const auto& uptr : m_ManagedObjects) {
        if (uptr && pred(uptr.get())) {
            targets.push_back(uptr.get());
        }
    }

    if (targets.empty()) {
        return;
    }

    // 高速検索用に対象セットを構築
    std::unordered_set<GameObject*> targetSet(targets.begin(), targets.end());

    // 2. 外部参照通知と終了処理（Uninit）
    for (GameObject* obj : targets) {
        if (!obj) continue;
        if (player) {
            player->NotifyObjectDestroyed(obj);
        }
        if (obj == m_CachedPlayer) {
            m_CachedPlayer = nullptr;
        }
        if (obj == m_CachedBoss) {
            m_CachedBoss = nullptr;
        }
        UnregisterCategory(obj);
        obj->Uninit();
    }

    // 3. 生ポインタキャッシュ（m_GameObjects, m_UpdateObjects）から swap-and-pop で除外
    auto swapAndPopRawList = [&targetSet](std::vector<GameObject*>& vec) {
        for (size_t i = vec.size(); i-- > 0; ) {
            if (!vec[i] || targetSet.count(vec[i]) > 0) {
                vec[i] = vec.back();
                vec.pop_back();
            }
        }
    };
    swapAndPopRawList(m_GameObjects);
    swapAndPopRawList(m_UpdateObjects);

    // 4. 所有権リスト（unique_ptr）から swap-and-pop で安全に解放・破棄
    for (size_t i = m_ManagedObjects.size(); i-- > 0; ) {
        if (!m_ManagedObjects[i] || targetSet.count(m_ManagedObjects[i].get()) > 0) {
            m_ManagedObjects[i] = std::move(m_ManagedObjects.back());
            m_ManagedObjects.pop_back();
        }
    }

    m_RenderSystem.ClearCache();
}

void Manager::UpdateFPSCounter(float deltaDrawTime)
{
    m_FpsFrameCount++;
    m_FpsTimeAccumulator += deltaDrawTime;
    if (m_FpsTimeAccumulator >= 0.5f) {
        m_CurrentFPS = static_cast<float>(m_FpsFrameCount) / m_FpsTimeAccumulator;
        m_FpsFrameCount = 0;
        m_FpsTimeAccumulator = 0.0f;
    }
}

