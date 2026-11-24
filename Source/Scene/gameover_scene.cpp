#include "gameover_scene.h"
#include "input.h"
#include "manager.h"
#include "sound_manager.h"
#include "field.h"
#include "beach.h"
#include "result_banner.h"
#include "ui_theme.h"
#include "save_system.h"
#include <algorithm>
#include "world_look.h"
#include "wall.h"
#include "player.h"
#include "camera.h"
#include "game_constants.h"
#include "shockwave.h"
#include "dust_system.h"
#include "score_popup.h"
#include "renderer.h"
#include "imgui.h"
#include "progression_manager.h"
#include <cmath>

namespace {
    // プレイヤーを囲む4つの巨石ブロック（顔付きオブジェクト）の完全データ
    struct RuinBlockData {
        DirectX::XMFLOAT3 pos;       // ブロック中心座標
        DirectX::XMFLOAT3 scale;     // ブロックスケール
        DirectX::XMFLOAT3 rot;       // ブロック回転 (Pitch, Yaw, Roll)
        std::string_view tex;        // テクスチャ
        DirectX::XMFLOAT4 color;     // 瞳・口の発光色 (Emission)
        float faceScale;             // 顔パーツのスケール倍率
    };

    // プレイヤー(0, 0.35f, 0)を正確に直視・見下ろす4ブロックの定義
    const RuinBlockData g_SurroundBlocks[4] = {
        // 1. 北西ブロック (ダークブロック): プレイヤーを見下ろす
        {
            { -3.4f, 0.60f,  2.8f }, { 1.8f, 1.2f, 1.4f },
            { 0.06f, 2.28f, 0.0f }, "wall_comic_block.png",
            { 2.8f, 0.2f, 0.15f, 1.0f }, 1.0f
        },
        // 2. 北東ブロック (クリムゾンブロック): プレイヤーを見下ろす
        {
            {  3.2f, 0.55f,  2.4f }, { 1.6f, 1.1f, 1.5f },
            { 0.05f, -2.25f, 0.0f }, "wall_comic_crimson.png",
            { 2.6f, 0.2f, 2.5f, 1.0f }, 0.95f
        },
        // 3. 南西ブロック (ダークブロック): プレイヤーを見下ろす
        {
            { -2.4f, 0.50f, -2.7f }, { 1.5f, 1.0f, 1.3f },
            { 0.04f, 0.73f, 0.0f }, "wall_comic_block.png",
            { 0.2f, 2.2f, 2.8f, 1.0f }, 0.90f
        },
        // 4. 南東ブロック (クリムゾンブロック): プレイヤーを見下ろす
        {
            {  2.7f, 0.60f, -2.5f }, { 1.7f, 1.2f, 1.4f },
            { 0.07f, -0.82f, 0.0f }, "wall_comic_crimson.png",
            { 2.8f, 1.4f, 0.1f, 1.0f }, 1.0f
        },
    };

    // 崩壊オブジェクト生成ヘルパー
    Wall* CreateRuinBlock(
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

    // 崩落と廃墟の荒野の構築
    void BuildDesolationRuins()
    {
        const std::string_view crimsonTex = "wall_comic_crimson.png";
        const std::string_view blockTex   = "wall_comic_block.png";

        // 1. 斜めにへし折れて地面に突き刺さった巨大ピラー群
        // 北西: 斜め30度傾いた崩壊主塔
        CreateRuinBlock({ -9.0f, 4.5f, 10.0f }, { 2.4f, 12.0f, 2.4f }, crimsonTex, { 0.35f, 0.2f, -0.45f });
        CreateRuinBlock({ -8.0f, 8.5f, 8.5f }, { 1.6f, 1.6f, 1.6f }, crimsonTex, { 0.35f, 0.2f, -0.45f }, { 1.6f, 0.15f, 0.05f });

        // 北東: 2つにへし折れて転がるピラー
        CreateRuinBlock({ 10.0f, 2.8f, 9.0f }, { 2.2f, 7.0f, 2.2f }, blockTex, { -0.2f, 0.1f, 0.55f });
        CreateRuinBlock({ 13.0f, 0.8f, 7.5f }, { 2.0f, 5.0f, 2.0f }, crimsonTex, { 0.0f, 0.8f, 1.45f }); // 横倒し

        // 南西: 地面に突き刺さった尖塔
        CreateRuinBlock({ -11.0f, 3.5f, -8.0f }, { 2.0f, 9.0f, 2.0f }, blockTex, { -0.4f, 0.3f, 0.3f });

        // 南東: 崩落して積み上がった多層要塞の残骸
        CreateRuinBlock({ 9.5f, 1.5f, -9.0f }, { 5.5f, 3.0f, 5.0f }, crimsonTex, { 0.1f, -0.15f, 0.08f });
        CreateRuinBlock({ 8.5f, 3.8f, -8.0f }, { 3.5f, 2.2f, 3.2f }, blockTex, { -0.15f, 0.25f, -0.1f });
        CreateRuinBlock({ 8.0f, 5.2f, -7.5f }, { 1.4f, 1.4f, 1.4f }, crimsonTex, { 0.0f, 0.0f, 0.0f }, { 1.4f, 0.1f, 0.05f });

        // 2. プレイヤー周辺に鎮座する4体の意思を持つ監視巨石（プレイヤーを見下ろす）
        for (int i = 0; i < 4; ++i) {
            const auto& b = g_SurroundBlocks[i];
            CreateRuinBlock(b.pos, b.scale, b.tex, b.rot);
        }

        // 3. 奥にそびえる荒野の巨大防壁の亀裂跡（北側背景）
        CreateRuinBlock({ -7.0f, 3.0f, 18.0f }, { 8.0f, 6.0f, 2.5f }, blockTex);
        CreateRuinBlock({  7.0f, 3.0f, 18.0f }, { 8.0f, 6.0f, 2.5f }, blockTex);
        // 防壁の間から不気味に覗く暗赤色モノリス
        CreateRuinBlock({  0.0f, 7.0f, 20.0f }, { 3.0f, 14.0f, 3.0f }, crimsonTex, { 0.0f, 0.0f, 0.05f });
        CreateRuinBlock({  0.0f, 14.2f, 20.0f }, { 1.8f, 1.8f, 1.8f }, crimsonTex, { 0.0f, 0.0f, 0.0f }, { 1.8f, 0.1f, 0.05f });
    }
}

GameOverScene::GameOverScene()
    : m_OrbitAngle(0.0f)
    , m_CameraHeight(3.6f)
    , m_Timer(0.0f)
{
}

GameOverScene::~GameOverScene()
{
}

void GameOverScene::Init()
{
    m_OrbitAngle = 0.0f;
    m_CameraHeight = 3.6f;
    m_Timer = 0.0f;

    // 前シーンのエフェクト残存をクリア
    ShockwaveSystem::Clear();
    DustSystem::Clear();
    ScorePopupSystem::Clear();

    // 1. 夜の砂浜と海（タイトルと同じ舞台。静かで暗い雰囲気）
    Manager::AddGameObject<Beach>();
    Manager::AddGameObject<Skybox>()->SetSkyType(SkyType::Night);

    // 3. 中央で倒れ伏すプレイヤー（横倒し姿勢・さっきの状態に復元）
    Player* player = Manager::AddGameObject<Player>();
    if (player) {
        player->SetPosition(DirectX::XMFLOAT3(0.0f, Beach::GroundY(0.0f, 0.0f) + 0.85f, 0.0f));
        // 横倒し（Z軸回転90度）で地面に倒れ込んでいる姿
        player->SetRotation(DirectX::XMFLOAT3(0.0f, 0.6f, DirectX::XM_PIDIV2));
        player->SetDamageTimer(0);
        player->GetLightningEffects().clear();
        player->SetLockOnTarget(nullptr);
    }

    // 4. カメラの初期化（倒れたプレイヤーとそれを見つめる4つの巨石ブロックを収めるシネマティック構図）
    if (g_Camera) {
        DirectX::XMFLOAT3 eye(0.0f, m_CameraHeight, -7.5f);
        DirectX::XMFLOAT3 target(0.0f, 0.6f, 0.0f);
        g_Camera->SetCutsceneMode(true, eye, target);
    }

    Input::SetMouseLocked(false);

    // 見出しの演出（1文字ずつ動く）
    ResultBanner::Start("GAME OVER", false);
}

void GameOverScene::Uninit()
{
    ResultBanner::Stop();
    WorldLook::SetTarget(0, true);   // 夜を他のシーンへ持ち越さない（昼へ戻す）
}

void GameOverScene::Update()
{
    ResultBanner::Update();
    // 重厚で低速な旋回カメラワーク（寂寥感と悲壮感の演出）
    m_Timer += 0.016f;
    m_OrbitAngle += 0.0025f; // ゆっくり旋回
    m_CameraHeight = 3.6f + 0.6f * sinf(m_Timer * 0.25f);

    if (g_Camera) {
        const float radius = 7.5f + 0.5f * cosf(m_Timer * 0.2f);
        // 道の南側から左右にゆっくり振る(家並みに入らない範囲)
        const float swing = 0.4f * sinf(m_OrbitAngle);
        float eyeX = radius * sinf(swing);
        float eyeZ = -radius * cosf(swing);

        DirectX::XMFLOAT3 eye(eyeX, m_CameraHeight, eyeZ);
        DirectX::XMFLOAT3 target(0.0f, 0.6f, 0.0f);
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

void GameOverScene::Draw()
{
    ResultBanner::Draw();
    DrawRewardUI();

    // 旧背景の4つの巨石(顔つき)は砂浜背景へ置き換えたため描かない（以降は旧巨石の顔の描画のみ）
    return;

    Renderer::GetDeviceContext()->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
    Renderer::SetupCubeDraw();

    // プレイヤーの位置 (0.0f, 0.35f, 0.0f)
    const DirectX::XMFLOAT3 playerPos(0.0f, 0.35f, 0.0f);

    // 漆黒のアイソケットフレーム用マテリアル
    MATERIAL baseMat;
    ZeroMemory(&baseMat, sizeof(baseMat));
    baseMat.Diffuse       = DirectX::XMFLOAT4(0.02f, 0.02f, 0.03f, 1.0f);
    baseMat.Ambient       = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    baseMat.Specular      = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    baseMat.Emission      = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    baseMat.Shininess     = 0.0f;
    baseMat.TextureEnable = FALSE;

    // 怒り眉・バイザーフレーム用マテリアル
    MATERIAL browMat;
    ZeroMemory(&browMat, sizeof(browMat));
    browMat.Diffuse       = DirectX::XMFLOAT4(0.10f, 0.10f, 0.14f, 1.0f);
    browMat.Ambient       = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    browMat.Specular      = DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
    browMat.Emission      = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    browMat.Shininess     = 10.0f;
    browMat.TextureEnable = FALSE;

    for (int i = 0; i < 4; ++i) {
        const auto& b = g_SurroundBlocks[i];

        // ブロック本体の姿勢（回転・位置）を行列化
        DirectX::XMMATRIX blockMatrix = DirectX::XMMatrixRotationRollPitchYaw(b.rot.x, b.rot.y, b.rot.z) *
                                        DirectX::XMMatrixTranslation(b.pos.x, b.pos.y, b.pos.z);

        // ブロックの前面（+Z面）のローカルZ位置
        const float frontZ = b.scale.z * 0.5f;
        const float baseZ  = frontZ + 0.015f; // 黒外枠の前面オフセット
        const float glowZ  = frontZ + 0.025f; // 発光パーツの前面オフセット
        const float s      = b.faceScale;

        // 光る瞳・口マテリアル（脈動パルス付き）
        float pulse = 1.0f + 0.18f * sinf(m_Timer * 2.8f + (float)i * 1.57f);
        MATERIAL pupilMat;
        ZeroMemory(&pupilMat, sizeof(pupilMat));
        pupilMat.Diffuse       = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        pupilMat.Ambient       = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        pupilMat.Specular      = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        pupilMat.Shininess     = 0.0f;
        pupilMat.TextureEnable = FALSE;
        pupilMat.Emission      = DirectX::XMFLOAT4(
            b.color.x * pulse,
            b.color.y * pulse,
            b.color.z * pulse,
            1.0f
        );

        // 1. 左右の斜めの目（ツリ目の黒枠外枠）
        const float eyeSlant = 0.22f; // 外上がり傾斜
        DirectX::XMMATRIX leftBaseLocal = DirectX::XMMatrixScaling(0.18f * s, 0.24f * s, 0.04f) *
                                          DirectX::XMMatrixRotationZ(eyeSlant) *
                                          DirectX::XMMatrixTranslation(-0.20f * s, 0.08f, baseZ);
        Renderer::SetWorldMatrix(leftBaseLocal * blockMatrix);
        Renderer::SetMaterial(baseMat);
        Renderer::GetDeviceContext()->Draw(36, 0);

        DirectX::XMMATRIX rightBaseLocal = DirectX::XMMatrixScaling(0.18f * s, 0.24f * s, 0.04f) *
                                           DirectX::XMMatrixRotationZ(-eyeSlant) *
                                           DirectX::XMMatrixTranslation(0.20f * s, 0.08f, baseZ);
        Renderer::SetWorldMatrix(rightBaseLocal * blockMatrix);
        Renderer::SetMaterial(baseMat);
        Renderer::GetDeviceContext()->Draw(36, 0);

        // 2. 左右の斜めの目（光るネオン瞳）
        DirectX::XMMATRIX leftPupilLocal = DirectX::XMMatrixScaling(0.12f * s, 0.16f * s, 0.05f) *
                                           DirectX::XMMatrixRotationZ(eyeSlant) *
                                           DirectX::XMMatrixTranslation(-0.20f * s, 0.08f, glowZ);
        Renderer::SetWorldMatrix(leftPupilLocal * blockMatrix);
        Renderer::SetMaterial(pupilMat);
        Renderer::GetDeviceContext()->Draw(36, 0);

        DirectX::XMMATRIX rightPupilLocal = DirectX::XMMatrixScaling(0.12f * s, 0.16f * s, 0.05f) *
                                            DirectX::XMMatrixRotationZ(-eyeSlant) *
                                            DirectX::XMMatrixTranslation(0.20f * s, 0.08f, glowZ);
        Renderer::SetWorldMatrix(rightPupilLocal * blockMatrix);
        Renderer::SetMaterial(pupilMat);
        Renderer::GetDeviceContext()->Draw(36, 0);

        // 3. 「ロ」の形の口（黒枠外枠）
        DirectX::XMMATRIX mouthBaseLocal = DirectX::XMMatrixScaling(0.16f * s, 0.11f * s, 0.04f) *
                                           DirectX::XMMatrixTranslation(0.0f, -0.11f, baseZ);
        Renderer::SetWorldMatrix(mouthBaseLocal * blockMatrix);
        Renderer::SetMaterial(baseMat);
        Renderer::GetDeviceContext()->Draw(36, 0);

        // 4. 「ロ」の形の口（光るネオン内側・開いた口）
        DirectX::XMMATRIX mouthPupilLocal = DirectX::XMMatrixScaling(0.10f * s, 0.06f * s, 0.05f) *
                                            DirectX::XMMatrixTranslation(0.0f, -0.11f, glowZ);
        Renderer::SetWorldMatrix(mouthPupilLocal * blockMatrix);
        Renderer::SetMaterial(pupilMat);
        Renderer::GetDeviceContext()->Draw(36, 0);
    }
}

void GameOverScene::DrawRewardUI()
{
    ImGuiIO& io = ImGui::GetIO();
    const float s = (std::max)(0.7f, (std::min)(1.0f, io.DisplaySize.y / 900.0f));
    const bool shortMode = SaveSystem::IsShortMode();

    // 画面右側のリザルト枠。高さは内容に合わせ、色とボタンはタイトルと同じ UITheme に揃える
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 30.0f, io.DisplaySize.y * 0.56f), ImGuiCond_Always, ImVec2(1.0f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(380.0f * s, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize
                           | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f * s, 18.0f * s));
    if (ImGui::Begin("##GameOverResult", nullptr, flags)) {
        ImGui::SetWindowFontScale((std::max)(0.8f, s));

        ImGui::TextColored(ImVec4(1.0f, 0.40f, 0.40f, 1.0f), (const char*)u8"クエスト失敗");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text((const char*)u8"撃破数: %d体", ProgressionManager::GetSessionKills());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), (const char*)u8"回収した素材");
        ImGui::Spacing();

        const float valueX = ImGui::GetWindowWidth() * 0.50f;
        bool anyAcquired = false;
        for (int i = 0; i < static_cast<int>(MaterialType::COUNT); ++i) {
            MaterialType mType = static_cast<MaterialType>(i);
            int sessionCount = ProgressionManager::GetSessionMaterial(mType);
            int totalCount = SaveSystem::GetMaterialCount(mType);
            DirectX::XMFLOAT3 col = ProgressionData::GetMaterialColor(mType);
            const char* name = ProgressionData::GetMaterialNameUtf8(mType);

            if (sessionCount > 0) {
                anyAcquired = true;
                ImGui::TextColored(ImVec4(col.x, col.y, col.z, 1.0f), "%s", name);
                ImGui::SameLine(valueX);
                ImGui::TextColored(ImVec4(col.x, col.y, col.z, 1.0f), (const char*)u8"+%d  (所持 %d)", sessionCount, totalCount);
            } else {
                ImGui::TextDisabled("%s", name);
                ImGui::SameLine(valueX);
                ImGui::TextDisabled((const char*)u8"0  (所持 %d)", totalCount);
            }
        }
        if (!anyAcquired) {
            ImGui::TextDisabled((const char*)u8"回収した素材はありません");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 戻るボタン（ストーリーは拠点へ、ショートモードはタイトルへ）
        UITheme::PushPrimaryButton();
        const char* label = shortMode ? (const char*)u8"タイトルへ戻る  (Space / Enter)" : (const char*)u8"拠点へ戻る  (Space / Enter)";
        if (ImGui::Button(label, ImVec2(-1.0f, 42.0f * s))) {
            SoundManager::PlaySE(SoundID::SE_DECIDE);
            Manager::ChangeScene(shortMode ? Scene::TITLE : Scene::HUB);
        }
        UITheme::PopButton();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
