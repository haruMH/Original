#include "clear_scene.h"
#include "input.h"
#include "manager.h"
#include "sound_manager.h"
#include "field.h"
#include "town_backdrop.h"
#include "wall.h"
#include "player.h"
#include "camera.h"
#include "game_constants.h"
#include "shockwave.h"
#include "dust_system.h"
#include "score_popup.h"
#include "imgui.h"
#include "progression_manager.h"
#include <cmath>

namespace {
    // 複合モニュメント用の壁ブロック生成ヘルパー
    Wall* CreateClearBlock(
        const DirectX::XMFLOAT3& pos,
        const DirectX::XMFLOAT3& scale,
        std::string_view tex,
        const DirectX::XMFLOAT3& rot = { 0.0f, 0.0f, 0.0f },
        const DirectX::XMFLOAT3& emissive = { 0.0f, 0.0f, 0.0f })
    {
        Wall* w = Manager::AddGameObject<Wall>();
        if (w) {
            w->SetPosition(pos);
            w->SetScale(scale);
            w->SetRotation(rot);
            w->SetTexture(tex);
            w->SetEmissive(emissive);
        }
        return w;
    }

    // 勝利の黄金・天空回廊の構築
    void BuildVictoryPalace()
    {
        const std::string_view goldTex = "wall_comic_gold.png";
        const std::string_view cyanTex = "wall_comic_cyan.png";

        // 1. 正面奥（北側）: 黄金の凱旋タワーオベリスク群（高さ18m）
        CreateClearBlock({ 0.0f, 9.0f, 16.0f }, { 3.2f, 18.0f, 3.2f }, goldTex);
        // 主塔頂上の黄金発光クラウン
        CreateClearBlock({ 0.0f, 18.2f, 16.0f }, { 2.0f, 2.0f, 2.0f }, goldTex, { 0.0f, 0.0f, 0.0f }, { 1.5f, 1.2f, 0.3f });

        // 左右の凱旋サブピラー（高さ13m）
        CreateClearBlock({ -4.5f, 6.5f, 15.0f }, { 2.2f, 13.0f, 2.2f }, goldTex);
        CreateClearBlock({  4.5f, 6.5f, 15.0f }, { 2.2f, 13.0f, 2.2f }, goldTex);
        CreateClearBlock({ -4.5f, 13.2f, 15.0f }, { 1.4f, 1.4f, 1.4f }, cyanTex, { 0.0f, 0.0f, 0.0f }, { 0.3f, 1.2f, 1.5f });
        CreateClearBlock({  4.5f, 13.2f, 15.0f }, { 1.4f, 1.4f, 1.4f }, cyanTex, { 0.0f, 0.0f, 0.0f }, { 0.3f, 1.2f, 1.5f });

        // 2. 参道両脇（東西）の整然と並ぶ多層列柱回廊（Colonnade）
        const float xDist = 9.0f;
        for (int i = 0; i < 4; ++i) {
            float z = -6.0f + i * 5.5f;
            float pillarH = 7.0f + i * 1.5f;
            // 西列柱
            CreateClearBlock({ -xDist, pillarH * 0.5f, z }, { 1.8f, pillarH, 1.8f }, goldTex);
            CreateClearBlock({ -xDist, pillarH + 0.5f, z }, { 1.2f, 1.0f, 1.2f }, cyanTex, { 0.0f, 0.0f, 0.0f }, { 0.2f, 0.9f, 1.2f });
            // 東列柱
            CreateClearBlock({  xDist, pillarH * 0.5f, z }, { 1.8f, pillarH, 1.8f }, goldTex);
            CreateClearBlock({  xDist, pillarH + 0.5f, z }, { 1.2f, 1.0f, 1.2f }, cyanTex, { 0.0f, 0.0f, 0.0f }, { 0.2f, 0.9f, 1.2f });
        }

        // 3. 背後（南側）: 祝祭のルーンゲート
        CreateClearBlock({ -6.0f, 5.0f, -12.0f }, { 2.2f, 10.0f, 2.2f }, cyanTex);
        CreateClearBlock({  6.0f, 5.0f, -12.0f }, { 2.2f, 10.0f, 2.2f }, cyanTex);
        CreateClearBlock({  0.0f, 10.0f, -12.0f }, { 14.2f, 1.8f, 2.4f }, goldTex);
        // ゲート中央の祝光コア
        CreateClearBlock({  0.0f, 11.2f, -12.0f }, { 2.0f, 1.2f, 2.0f }, goldTex, { 0.0f, 0.0f, 0.0f }, { 1.6f, 1.3f, 0.4f });

        // 4. 4隅の栄光の台座（3段ピラミッド基壇）
        float cornerPos[4][2] = { { -16.0f, 16.0f }, { 16.0f, 16.0f }, { -16.0f, -16.0f }, { 16.0f, -16.0f } };
        for (int c = 0; c < 4; ++c) {
            float cx = cornerPos[c][0];
            float cz = cornerPos[c][1];
            CreateClearBlock({ cx, 0.5f, cz }, { 6.0f, 1.0f, 6.0f }, goldTex);
            CreateClearBlock({ cx, 1.5f, cz }, { 4.2f, 1.0f, 4.2f }, goldTex);
            CreateClearBlock({ cx, 4.5f, cz }, { 2.4f, 5.0f, 2.4f }, cyanTex);
            CreateClearBlock({ cx, 7.2f, cz }, { 1.2f, 1.2f, 1.2f }, goldTex, { 0.0f, 0.0f, 0.0f }, { 1.4f, 1.1f, 0.3f });
        }
    }
}

ClearScene::ClearScene()
    : m_OrbitAngle(0.0f)
    , m_CameraHeight(2.8f)
    , m_Timer(0.0f)
{
}

ClearScene::~ClearScene()
{
}

void ClearScene::Init()
{
    m_OrbitAngle = 0.0f;
    m_CameraHeight = 2.8f;
    m_Timer = 0.0f;

    // 前シーンのエフェクト残存をクリア
    ShockwaveSystem::Clear();
    DustSystem::Clear();
    ScorePopupSystem::Clear();

    // 1. 拠点と同じ街並みの背景
    TownBackdrop::Build(false);

    // 3. 中央で堂々と佇むプレイヤーキャラクター
    Player* player = Manager::AddGameObject<Player>();
    if (player) {
        player->SetPosition(DirectX::XMFLOAT3(0.0f, -0.5f, 0.0f));
        player->SetRotation(DirectX::XMFLOAT3(0.0f, DirectX::XM_PI, 0.0f)); // カメラ側（手前）を向く
        player->SetDamageTimer(0);
        player->GetLightningEffects().clear();
        player->SetLockOnTarget(nullptr);
    }

    // 4. カメラの初期化（プレイヤーの足元付近から見上げるシネマティック構図）
    if (g_Camera) {
        DirectX::XMFLOAT3 eye(0.0f, m_CameraHeight, -8.5f);
        DirectX::XMFLOAT3 target(0.0f, 1.8f, 0.0f);
        g_Camera->SetCutsceneMode(true, eye, target);
    }

    Input::SetMouseLocked(false);
}

void ClearScene::Uninit()
{
}

void ClearScene::Update()
{
    // タイマーとカメラ旋回角の更新（ゆっくりと優雅に旋回しながら、少しずつ高度を上げていく）
    m_Timer += 0.016f;
    m_OrbitAngle += 0.0035f; // ゆっくり旋回
    m_CameraHeight = 2.5f + 1.2f * sinf(m_Timer * 0.4f); // 2.5m〜3.7mを優雅に上下動

    if (g_Camera) {
        const float radius = 8.5f + 0.8f * cosf(m_Timer * 0.3f);
        // 道の南側から左右にゆっくり振る(家並みに入らない範囲)
        const float swing = 0.4f * sinf(m_OrbitAngle);
        float eyeX = radius * sinf(swing);
        float eyeZ = -radius * cosf(swing);

        DirectX::XMFLOAT3 eye(eyeX, m_CameraHeight, eyeZ);
        DirectX::XMFLOAT3 target(0.0f, 1.6f, 0.0f);
        g_Camera->SetCutsceneMode(true, eye, target);
    }

    // UI操作中でなければ、Enter/Space/クリックで拠点ハブに戻る
    ImGuiIO& io = ImGui::GetIO();
    bool canClick = !io.WantCaptureMouse && Input::GetKeyTrigger(VK_LBUTTON);
    if (Input::GetKeyTrigger(VK_RETURN) || Input::GetKeyTrigger(VK_SPACE) || canClick) {
        SoundManager::PlaySE(SoundID::SE_DECIDE);
        Manager::ChangeScene(SaveSystem::IsShortMode() ? Scene::TITLE : Scene::HUB);
    }
}

void ClearScene::Draw()
{
    DrawRewardUI();
}

void ClearScene::DrawRewardUI()
{
    ImGuiIO& io = ImGui::GetIO();

    // 画面右側に半透明のリザルト・報酬ウィンドウを配置
    float panelWidth = 360.0f;
    float panelHeight = 440.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelWidth - 30.0f, (io.DisplaySize.y - panelHeight) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin("QUEST REWARDS##ClearResult", nullptr, flags)) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.88f, 0.2f, 1.0f), "  === MISSION COMPLETE ===");
        ImGui::Separator();
        ImGui::Spacing();

        // 戦闘スタッツ
        ImGui::Text("Defeated Enemies: %d", ProgressionManager::GetSessionKills());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "ACQUIRED MATERIALS:");
        ImGui::Spacing();

        const char* matNames[] = {
            "Scrap Metal",
            "Energy Cell",
            "Gunpowder",
            "Charged Plug",
            "Titan Plating",
            "Kinetic Core"
        };

        bool anyAcquired = false;
        for (int i = 0; i < static_cast<int>(MaterialType::COUNT); ++i) {
            MaterialType mType = static_cast<MaterialType>(i);
            int sessionCount = ProgressionManager::GetSessionMaterial(mType);
            int totalCount = SaveSystem::GetMaterialCount(mType);
            DirectX::XMFLOAT3 col = ProgressionData::GetMaterialColor(mType);

            if (sessionCount > 0) {
                anyAcquired = true;
                ImGui::TextColored(ImVec4(col.x, col.y, col.z, 1.0f), " * %-14s : +%d  (Total: %d)", matNames[i], sessionCount, totalCount);
            } else {
                ImGui::TextDisabled("   %-14s :  0  (Total: %d)", matNames[i], totalCount);
            }
        }

        if (!anyAcquired) {
            ImGui::TextDisabled("   (No materials recovered)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 帰還ボタン
        if (ImGui::Button("Return to Base Camp [SPACE / ENTER]", ImVec2(-1.0f, 40.0f))) {
            SoundManager::PlaySE(SoundID::SE_DECIDE);
            Manager::ChangeScene(SaveSystem::IsShortMode() ? Scene::TITLE : Scene::HUB);
        }

        ImGui::End();
    }
}
