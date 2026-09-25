#include "title_scene.h"
#include "input.h"
#include "manager.h"
#include "sound_manager.h"
#include "field.h"
#include "wall.h"
#include "player.h"
#include "camera.h"
#include "game_constants.h"
#include "shockwave.h"
#include "dust_system.h"
#include "score_popup.h"
#include "save_system.h"
#include "player_controller.h"
#include "imgui/imgui.h"
#include <cmath>

// =================================================================
// コンストラクタ / デストラクタ
// =================================================================
TitleScene::TitleScene()
    : m_OrbitAngle(0.0f)
{
}

TitleScene::~TitleScene()
{
}

namespace {
    // 複合モニュメント用の壁ブロック生成ヘルパー（自発光対応）
    Wall* CreateBlock(
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

    // 1. 東側: ゴールド・古代神殿・天守回廊（高さ16m・オベリスク天守・6連列柱・3段ピラミッド基壇）★主役級
    void BuildGoldShrine(float dist)
    {
        const std::string_view tex = "wall_comic_gold.png";

        // 天を突く中央オベリスク主塔（高さ16m）
        CreateBlock({ dist, 7.5f, 0.0f }, { 2.8f, 16.0f, 2.8f }, tex);
        // 主塔頂上の黄金発光コアクラウン
        CreateBlock({ dist, 15.9f, 0.0f }, { 1.8f, 1.6f, 1.8f }, tex, { 0.0f, 0.0f, 0.0f }, { 1.2f, 0.95f, 0.25f });

        // 左右の多重ステップ回廊ピラー群
        // 内側高層ピラー（高さ11.5m）
        CreateBlock({ dist, 5.3f, -3.8f }, { 2.2f, 11.5f, 2.2f }, tex);
        CreateBlock({ dist, 5.3f,  3.8f }, { 2.2f, 11.5f, 2.2f }, tex);
        // 中間中層ピラー（高さ8.5m）
        CreateBlock({ dist - 1.0f, 3.8f, -7.4f }, { 2.0f, 8.5f, 2.0f }, tex);
        CreateBlock({ dist - 1.0f, 3.8f,  7.4f }, { 2.0f, 8.5f, 2.0f }, tex);
        // 外側低層ピラー（高さ6.0m）
        CreateBlock({ dist - 2.2f, 2.6f, -10.5f }, { 1.8f, 6.2f, 1.8f }, tex);
        CreateBlock({ dist - 2.2f, 2.6f,  10.5f }, { 1.8f, 6.2f, 1.8f }, tex);

        // 回廊を繋ぐ多重アーチ水平梁（リンテル）
        CreateBlock({ dist, 11.3f, 0.0f }, { 2.4f, 1.8f, 10.5f }, tex);
        CreateBlock({ dist - 1.5f, 7.6f, -8.8f }, { 2.2f, 1.5f, 7.2f }, tex);
        CreateBlock({ dist - 1.5f, 7.6f,  8.8f }, { 2.2f, 1.5f, 7.2f }, tex);

        // 重厚3段ピラミッド基壇（神殿の土台）
        CreateBlock({ dist - 0.5f, 0.3f, 0.0f }, { 5.2f, 1.4f, 24.5f }, tex); // 1段目 (大)
        CreateBlock({ dist,        0.9f, 0.0f }, { 4.2f, 1.2f, 18.5f }, tex); // 2段目 (中)
        CreateBlock({ dist,        1.5f, 0.0f }, { 3.4f, 1.2f, 12.5f }, tex); // 3段目 (小)
    }

    // 2. 北側: シアン・ルーンゲート（高さ14.5m・二重魔導門・多層スカイブリッジ）
    void BuildCyanRuneGate(float dist)
    {
        const std::string_view tex = "wall_comic_cyan.png";

        // 外側巨大巨石ピラー2本（高さ14.5m）
        CreateBlock({ -6.5f, 6.8f, dist }, { 2.5f, 14.5f, 2.5f }, tex);
        CreateBlock({  6.5f, 6.8f, dist }, { 2.5f, 14.5f, 2.5f }, tex);
        // 内側ピラー2本（高さ10.5m）
        CreateBlock({ -2.8f, 4.8f, dist }, { 2.0f, 10.5f, 2.0f }, tex);
        CreateBlock({  2.8f, 4.8f, dist }, { 2.0f, 10.5f, 2.0f }, tex);

        // 最上層スカイブリッジ（超大型リンテル 幅16.5m）
        CreateBlock({ 0.0f, 14.3f, dist }, { 16.5f, 2.2f, 3.0f }, tex);
        // 中層アーチ梁
        CreateBlock({ 0.0f, 9.5f, dist }, { 9.2f, 1.8f, 2.4f }, tex);
        // ゲート中央の魔導シアン発光コア
        CreateBlock({ 0.0f, 10.7f, dist }, { 1.8f, 1.6f, 2.6f }, tex, { 0.0f, 0.0f, 0.0f }, { 0.2f, 1.1f, 1.3f });

        // 祭壇階段ステップブロック群
        CreateBlock({ 0.0f, 0.4f, dist - 3.0f }, { 8.5f, 1.0f, 3.5f }, tex);
        CreateBlock({ 0.0f, 1.0f, dist - 1.5f }, { 6.5f, 1.0f, 2.5f }, tex);
        // 門奥の重厚バックウォール
        CreateBlock({ 0.0f, 4.5f, dist + 3.8f }, { 15.0f, 9.0f, 1.6f }, tex);
    }

    // 3. 南側: クリムゾン・多層要塞城塞天守（高さ15m・天守城壁・左右16.5mバットレス塔・二重胸壁）
    void BuildCrimsonFortress(float dist)
    {
        const std::string_view tex = "wall_comic_crimson.png";

        // メイン城塞天守（高さ15m、幅20m）
        CreateBlock({ 0.0f, 7.0f, -dist - 2.5f }, { 20.0f, 15.0f, 2.4f }, tex);
        // 両翼バットレス要塞タワー（高さ16.5m）
        CreateBlock({ -9.8f, 7.8f, -dist - 2.0f }, { 3.2f, 16.5f, 3.2f }, tex);
        CreateBlock({  9.8f, 7.8f, -dist - 2.0f }, { 3.2f, 16.5f, 3.2f }, tex);
        // 要塞中央の紅蓮発光エンブレムコア
        CreateBlock({ 0.0f, 11.8f, -dist - 1.1f }, { 2.2f, 2.2f, 1.2f }, tex, { 0.0f, 0.0f, 0.0f }, { 1.4f, 0.2f, 0.35f });

        // 手前前衛の低層スリット胸壁（二重構造 高さ4.5m）
        CreateBlock({ 0.0f, 1.8f, -dist + 1.5f }, { 15.5f, 4.5f, 1.8f }, tex);
        // 前衛監視ピラー
        CreateBlock({ -5.8f, 2.8f, -dist + 1.8f }, { 2.0f, 6.5f, 2.0f }, tex);
        CreateBlock({  5.8f, 2.8f, -dist + 1.8f }, { 2.0f, 6.5f, 2.0f }, tex);
    }

    // 4. 西側: アメジスト・超巨石メガリスヘンジ（高さ14m・斜行主柱・上空交差冠石・風化崩落群）
    void BuildAmethystStonehenge(float dist)
    {
        const std::string_view tex = "wall_comic_block.png";

        // 傾きを持たせた巨大左石柱（高さ13.5m）
        CreateBlock({ -dist, 6.3f, -4.5f }, { 2.8f, 13.5f, 4.0f }, tex, { 0.0f, 0.12f, 0.06f });
        // 傾きを持たせた巨大右石柱（高さ14.0m）
        CreateBlock({ -dist, 6.5f,  4.5f }, { 2.8f, 14.0f, 3.8f }, tex, { 0.0f, -0.15f, -0.05f });
        // 上空で渡された超大型アーチ冠石（高さ14m、長さ14.5m）
        CreateBlock({ -dist, 13.8f, 0.0f }, { 3.2f, 2.4f, 14.5f }, tex, { 0.04f, 0.0f, 0.0f });
        // 冠石上の神秘のアメジスト発光コア
        CreateBlock({ -dist, 15.4f, 0.0f }, { 1.6f, 1.6f, 1.6f }, tex, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.3f, 1.5f });

        // 手前に崩落・横倒しになった巨石群
        CreateBlock({ -dist + 3.8f, 0.8f, -1.8f }, { 2.4f, 2.0f, 9.0f }, tex, { 0.0f, 0.52f, 0.0f });
        CreateBlock({ -dist + 2.2f, 0.6f,  4.2f }, { 2.0f, 1.8f, 5.5f }, tex, { 0.08f, -0.28f, 0.0f });
        // 背後を支える巨大立石
        CreateBlock({ -dist - 3.2f, 4.8f, 0.0f }, { 2.2f, 10.5f, 6.5f }, tex);
    }

    // 5. 四隅: 多段キャピタル・タワーピラー（高さ11m・3段冠装飾・頂上発光コア）
    void BuildCornerPillars(float dist)
    {
        const float cd = dist * 0.72f;
        auto makeCorner = [](float x, float z, std::string_view tex, const DirectX::XMFLOAT3& glow) {
            // 主柱（高さ10.5m）
            CreateBlock({ x, 4.8f, z }, { 2.6f, 10.5f, 2.6f }, tex);
            // 3段式キャピタル頭部装飾
            CreateBlock({ x, 10.3f, z }, { 3.4f, 0.8f, 3.4f }, tex);
            CreateBlock({ x, 11.0f, z }, { 4.0f, 0.8f, 4.0f }, tex);
            // 頂上発光コアブロック
            CreateBlock({ x, 11.8f, z }, { 1.8f, 1.2f, 1.8f }, tex, { 0.0f, 0.0f, 0.0f }, glow);
        };

        makeCorner( cd,  cd, "wall_comic_gold.png",    { 0.95f, 0.75f, 0.2f });  // 北東: 黄金
        makeCorner(-cd,  cd, "wall_comic_cyan.png",    { 0.2f,  0.85f, 1.1f });  // 北西: シアン
        makeCorner( cd, -cd, "wall_comic_crimson.png", { 1.1f,  0.25f, 0.35f }); // 南東: クリムゾン
        makeCorner(-cd, -cd, "wall_comic_block.png",   { 0.85f, 0.25f, 1.2f });  // 南西: アメジスト
    }
}

// =================================================================
// 初期化
// =================================================================
void TitleScene::Init()
{
    m_OrbitAngle = 0.0f;
    m_HasSaveData = SaveSystem::HasSaveFile();
    m_SelectedIndex = 0; // デフォルトで「つづきから」（セーブデータ有時）
    m_ShowNewGameConfirm = false;

    // タイトル画面ではUI操作のためマウスロックを解除
    Input::SetMouseLocked(false);

    // 前シーンのエフェクト残存を防止
    ShockwaveSystem::Clear();
    DustSystem::Clear();
    ScorePopupSystem::Clear();

    // 1. 3D床面の生成（波打つグリッドメッシュ＆フレネル光沢）
    Manager::AddGameObject<Field>();
    Manager::AddGameObject<Skybox>();

    // 2. 4属性コミック調石積みの多層複合モニュメント配置
    const float baseDist = 22.0f;
    BuildCyanRuneGate(baseDist);        // 北: シアン・ルーンゲート
    BuildGoldShrine(baseDist);          // 東: ゴールド・古代神殿モノリス群
    BuildCrimsonFortress(baseDist);     // 南: クリムゾン・多層要塞城壁
    BuildAmethystStonehenge(baseDist);  // 西: アメジスト・崩落巨石ヘンジ
    BuildCornerPillars(baseDist);       // 四隅: コーナータワーピラー

    // 3. 中央に佇むプレイヤーキャラクター
    Player* player = Manager::AddGameObject<Player>();
    if (player) {
        player->SetPosition(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
        player->SetDamageTimer(99999); // タイトル中のキー入力による勝手な移動を抑止
    }

    // 4. カメラの初期旋回位置を設定（巨大建造物を見上げるシネマティック構図）
    if (g_Camera) {
        DirectX::XMFLOAT3 eye(0.0f, 4.0f, 12.5f);
        DirectX::XMFLOAT3 target(0.0f, 2.2f, 0.0f);
        g_Camera->SetCutsceneMode(true, eye, target);
    }
}

// =================================================================
// 終了処理
// =================================================================
void TitleScene::Uninit()
{
    // タイトル終了時にカメラのカットシーン固定を解除
    if (g_Camera) {
        g_Camera->SetCutsceneMode(false);
    }
}

// =================================================================
// 更新処理
// =================================================================
void TitleScene::Update()
{
    // 1. カメラのゆっくりとした360度オービット旋回
    m_OrbitAngle += 0.006f; // 約60fpsで1周約17秒の滑らかな旋回
    if (m_OrbitAngle > DirectX::XM_2PI) {
        m_OrbitAngle -= DirectX::XM_2PI;
    }

    if (g_Camera) {
        const float radius = 12.5f;
        const float height = 4.0f;
        DirectX::XMFLOAT3 eye(
            sinf(m_OrbitAngle) * radius,
            height,
            cosf(m_OrbitAngle) * radius
        );
        DirectX::XMFLOAT3 target(0.0f, 2.2f, 0.0f);
        g_Camera->SetCutsceneMode(true, eye, target);
    }

    // 2. プレイヤーを定位置に固定
    Player* player = Manager::GetGameObject<Player>();
    if (player) {
        player->SetPosition(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
        player->SetDamageTimer(99999);
    }

    // 3. メニューキーボード操作
    if (m_ShowNewGameConfirm) {
        // 上書き確認中: ESCキーまたはBボタンでキャンセル
        if (Input::GetKeyTrigger(VK_ESCAPE) || PlayerController::IsGuardAction()) {
            m_ShowNewGameConfirm = false;
        }
    } else {
        if (m_HasSaveData) {
            // 上下選択（W/S または 上下矢印キー）
            if (Input::GetKeyTrigger('W') || Input::GetKeyTrigger(VK_UP)) {
                m_SelectedIndex = (m_SelectedIndex - 1 + 2) % 2;
            }
            if (Input::GetKeyTrigger('S') || Input::GetKeyTrigger(VK_DOWN)) {
                m_SelectedIndex = (m_SelectedIndex + 1) % 2;
            }

            // 決定入力
            if (Input::GetKeyTrigger(VK_RETURN) || Input::GetKeyTrigger(VK_SPACE) || PlayerController::IsGrabOrThrowAction()) {
                SoundManager::PlaySE(SoundID::SE_DECIDE);
                if (m_SelectedIndex == 0) {
                    // つづきから: データをロードして拠点へ
                    SaveSystem::Load();
                    Manager::ChangeScene(Scene::HUB);
                } else {
                    // はじめから: 上書き確認を表示
                    m_ShowNewGameConfirm = true;
                }
            }
        } else {
            // セーブデータなし: ゲーム開始（チュートリアルへ）
            if (Input::GetKeyTrigger(VK_RETURN) || Input::GetKeyTrigger(VK_SPACE) || PlayerController::IsGrabOrThrowAction()) {
                SoundManager::PlaySE(SoundID::SE_DECIDE);
                Manager::ChangeScene(Scene::TUTORIAL);
            }
        }
    }
}

// =================================================================
// 描画処理 (ImGui によるタイトル画面メニューUI)
// =================================================================
void TitleScene::Draw()
{
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    // 画面下部中央にメニューパネルを配置
    ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.72f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                           | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.06f, 0.12f, 0.88f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.50f, 0.90f, 0.70f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);

    if (ImGui::Begin("TitleMenuOverlay", nullptr, flags)) {
        // タイトル見出し
        ImGui::SetWindowFontScale(1.4f);
        const char* titleText = (const char*)u8"TOON SLASHER";
        float titleW = ImGui::CalcTextSize(titleText).x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - titleW) * 0.5f);
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "%s", titleText);
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (m_ShowNewGameConfirm) {
            // 上書き確認ダイアログ
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), (const char*)u8"※既存のセーブデータが上書きされます！");
            ImGui::Text((const char*)u8"本当にはじめから開始しますか？");
            ImGui::Spacing();

            if (ImGui::Button((const char*)u8"はい (開始)", ImVec2(200, 42))) {
                SoundManager::PlaySE(SoundID::SE_DECIDE);
                SaveSystem::ResetData();
                SaveSystem::Save();
                Manager::ChangeScene(Scene::TUTORIAL);
            }
            ImGui::SameLine();
            if (ImGui::Button((const char*)u8"いいえ (戻る)", ImVec2(200, 42))) {
                m_ShowNewGameConfirm = false;
            }
        } else {
            if (m_HasSaveData) {
                // 1. つづきから (CONTINUE)
                bool isContinueSelected = (m_SelectedIndex == 0);
                if (isContinueSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.52f, 0.95f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.62f, 1.0f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.18f, 0.28f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.30f, 0.45f, 1.0f));
                }

                std::string continueLabel = std::string((const char*)u8"つづきから (拠点キャンプ)") + (isContinueSelected ? "  <" : "");
                if (ImGui::Button(continueLabel.c_str(), ImVec2(-1, 46))) {
                    SoundManager::PlaySE(SoundID::SE_DECIDE);
                    SaveSystem::Load();
                    Manager::ChangeScene(Scene::HUB);
                }
                ImGui::PopStyleColor(2);

                // セーブデータプレビュー
                const SaveData& data = SaveSystem::GetData();
                ImGui::TextColored(ImVec4(0.65f, 0.75f, 0.85f, 1.0f),
                    (const char*)u8"  討伐: %d体 | クリア: %d回 | 最大コンボ: %d",
                    data.totalKills, data.totalClears, data.highestCombo);

                ImGui::Spacing();
                ImGui::Spacing();

                // 2. はじめから (NEW GAME)
                bool isNewGameSelected = (m_SelectedIndex == 1);
                if (isNewGameSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.52f, 0.95f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.62f, 1.0f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.18f, 0.28f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.30f, 0.45f, 1.0f));
                }

                std::string newGameLabel = std::string((const char*)u8"はじめから (チュートリアル)") + (isNewGameSelected ? "  <" : "");
                if (ImGui::Button(newGameLabel.c_str(), ImVec2(-1, 40))) {
                    SoundManager::PlaySE(SoundID::SE_DECIDE);
                    m_ShowNewGameConfirm = true;
                }
                ImGui::PopStyleColor(2);
            } else {
                // 初回起動（セーブデータなし）: ゲーム開始
                if (ImGui::Button((const char*)u8"ゲーム開始 (START)", ImVec2(-1, 50))) {
                    SoundManager::PlaySE(SoundID::SE_DECIDE);
                    Manager::ChangeScene(Scene::TUTORIAL);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // 操作ガイド
            ImGui::TextColored(ImVec4(0.50f, 0.55f, 0.65f, 1.0f),
                (const char*)u8"操作: W/Sキー (上下選択) | Space/Enter (決定)");
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}
