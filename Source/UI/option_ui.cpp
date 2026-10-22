#include "option_ui.h"
#include "imgui/imgui.h"
#include "input.h"
#include "sound_manager.h"
#include "camera.h"
#include "main.h"
#include "renderer.h"
#include "manager.h"
#include "save_system.h"
#include "progression_manager.h"
#include "graphics_config.h"
#include "localization.h"

bool  OptionUI::m_IsOpen = false;
float OptionUI::m_SaveFeedbackTimer = 0.0f;

void OptionUI::Init()
{
    m_IsOpen = false;
    m_SaveFeedbackTimer = 0.0f;
}

void OptionUI::SetOpen(bool open)
{
    m_IsOpen = open;
    if (m_IsOpen) {
        // オプション表示時はマウスロックを解除し、カーソルを表示する
        Input::SetMouseLocked(false);
    } else {
        // オプションを閉じた時はマウスをロック状態に戻す
        Input::SetMouseLocked(true);
    }
}

void OptionUI::Update()
{
    if (m_SaveFeedbackTimer > 0.0f) {
        m_SaveFeedbackTimer -= 0.016f;
    }

    // Tab キーで開閉をトグル
    if (Input::GetKeyTrigger(VK_TAB)) {
        SetOpen(!m_IsOpen);
    }
    // オプションが開いている場合、Esc キーでも閉じられるようにする
    else if (m_IsOpen && Input::GetKeyTrigger(VK_ESCAPE)) {
        SetOpen(false);
    }
}

void OptionUI::Draw()
{
    if (!m_IsOpen) return;

    // 画面中央にウィンドウを配置（フルスクリーン切替時は自動で追従再センタリング）
    static bool s_PrevFs = false;
    bool isFs = IsFullscreen();
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    if (s_PrevFs != isFs) {
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        s_PrevFs = isFs;
    } else {
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }
    ImGui::SetNextWindowSize(ImVec2(580.0f, 560.0f), ImGuiCond_Appearing);

    // ウィンドウの装飾スタイル設定
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

    // 半透明の背景スタイル
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.12f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.18f, 0.22f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.30f, 0.50f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.45f, 0.70f, 1.0f));

    if (ImGui::Begin(Loc::Tr("OPTIONS (SETTINGS)", "オプション設定 (OPTIONS)"), &m_IsOpen, windowFlags))
    {
        // 閉じるボタンが押された場合の同期
        if (!m_IsOpen) {
            SetOpen(false);
        }

        // ─── 言語切り替えトグル（Language: EN / JP） ───
        Language curLang = Loc::GetLanguage();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 170.0f);
        ImGui::Text(Loc::Tr("Language:", "言語:"));
        ImGui::SameLine();
        if (curLang == Language::English) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.85f, 1.0f));
        }
        if (ImGui::Button("EN", ImVec2(32, 20))) {
            Loc::SetLanguage(Language::English);
        }
        if (curLang == Language::English) {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        if (curLang == Language::Japanese) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.85f, 1.0f));
        }
        if (ImGui::Button(Loc::ToUTF8("日本語"), ImVec2(48, 20))) {
            Loc::SetLanguage(Language::Japanese);
        }
        if (curLang == Language::Japanese) {
            ImGui::PopStyleColor();
        }

        if (ImGui::BeginTabBar("OptionTabs"))
        {
            // ─────────────────────────────────────────────
            // 1. サウンド設定タブ
            // ─────────────────────────────────────────────
            if (ImGui::BeginTabItem(Loc::Tr("Audio", "サウンド")))
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Loc::Tr("[ Volume Settings ]", "[ 音量設定 ]"));
                ImGui::Separator();
                ImGui::Spacing();

                // BGM 音量スライダー
                float bgmVol = SoundManager::GetBGMVolume() * 100.0f;
                if (ImGui::SliderFloat(Loc::Tr("BGM Volume", "BGM 音量"), &bgmVol, 0.0f, 100.0f, "%.0f %%")) {
                    SoundManager::SetBGMVolume(bgmVol / 100.0f);
                }

                // SE 音量スライダー
                float seVol = SoundManager::GetSEVolume() * 100.0f;
                if (ImGui::SliderFloat(Loc::Tr("SE Volume", "SE 音量"), &seVol, 0.0f, 100.0f, "%.0f %%")) {
                    SoundManager::SetSEVolume(seVol / 100.0f);
                }

                ImGui::Spacing();
                // SE テスト再生ボタン
                if (ImGui::Button(Loc::Tr("Test SE (Grab)", "SEテスト (掴み)"))) {
                    SoundManager::PlaySE(SoundID::SE_GRAB);
                }
                ImGui::SameLine();
                if (ImGui::Button(Loc::Tr("Test SE (Throw)", "SEテスト (投げ)"))) {
                    SoundManager::PlaySE(SoundID::SE_THROW);
                }
                ImGui::SameLine();
                if (ImGui::Button(Loc::Tr("Test SE (Parry)", "SEテスト (パリィ)"))) {
                    SoundManager::PlaySE(SoundID::SE_PARRY);
                }

                ImGui::EndTabItem();
            }

            // ─────────────────────────────────────────────
            // 2. コントロール・カメラ設定タブ
            // ─────────────────────────────────────────────
            if (ImGui::BeginTabItem(Loc::Tr("Controls", "操作・視点")))
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Loc::Tr("[ Camera & Mouse ]", "[ カメラ＆マウス設定 ]"));
                ImGui::Separator();
                ImGui::Spacing();

                // マウス感度
                float currentSens = Camera::GetSensitivity();
                float displaySens = currentSens / 0.0001f;
                if (ImGui::SliderFloat(Loc::Tr("Mouse Sensitivity", "マウス感度"), &displaySens, 5.0f, 100.0f, "%.1f")) {
                    Camera::SetSensitivity(displaySens * 0.0001f);
                }

                // 上下反転
                bool invertY = Camera::GetInvertY();
                if (ImGui::Checkbox(Loc::Tr("Invert Vertical Look (Invert Y)", "上下視点反転 (Invert Y)"), &invertY)) {
                    Camera::SetInvertY(invertY);
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), Loc::Tr("Controls Guide:", "操作ガイド:"));
                ImGui::BulletText(Loc::Tr("Move: W, A, S, D", "移動: W, A, S, D"));
                ImGui::BulletText(Loc::Tr("Jump: Space (Double Jump Available)", "ジャンプ: Space (2段ジャンプ可)"));
                ImGui::BulletText(Loc::Tr("Dash / Dodge: Left Shift", "ダッシュ/回避: 左Shift"));
                ImGui::BulletText(Loc::Tr("Grab / Throw: Left Click or J Key", "掴み/投擲: 左クリック または Jキー"));
                ImGui::BulletText(Loc::Tr("Spin / Sweep: Right Click or K Key", "スピン薙ぎ払い: 右クリック または Kキー"));
                ImGui::BulletText(Loc::Tr("Toggle Option: Tab Key", "設定画面開閉: Tabキー"));

                ImGui::EndTabItem();
            }

            // ─────────────────────────────────────────────
            // 3. 画面・ディスプレイ設定タブ
            // ─────────────────────────────────────────────
            if (ImGui::BeginTabItem(Loc::Tr("Display", "画面表示")))
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Loc::Tr("[ Screen & Display Settings ]", "[ 画面・ディスプレイ設定 ]"));
                ImGui::Separator();
                ImGui::Spacing();

                bool currentFs = IsFullscreen();
                if (ImGui::Checkbox(Loc::Tr("Fullscreen Mode (Hotkey: F11 / Alt+Enter)", "フルスクリーン表示 (F11 / Alt+Enter)"), &currentFs)) {
                    SetFullscreen(currentFs);
                }

                ImGui::Spacing();
                const char* fsBtnLabel = currentFs 
                    ? Loc::Tr("Switch to Windowed (1280x720) [F11]", "ウィンドウモードに切替 (1280x720) [F11]")
                    : Loc::Tr("Switch to Fullscreen (Native Resolution) [F11]", "フルスクリーンに切替 (ネイティブ解像度) [F11]");
                if (ImGui::Button(fsBtnLabel, ImVec2(380, 34))) {
                    ToggleFullscreen();
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), Loc::Tr("Current Resolution: %u x %u (%s)", "現在の解像度: %u x %u (%s)"), 
                    Renderer::GetScreenWidth(), Renderer::GetScreenHeight(),
                    currentFs ? Loc::Tr("Native Fullscreen", "フルスクリーン") : Loc::Tr("Windowed", "ウィンドウ"));
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), Loc::Tr("* Press F11 or Alt+Enter at any time to toggle Fullscreen", "* F11 または Alt+Enter でいつでも切替可能"));

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // ─── フレームレート（FPS）設定 ───
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Loc::Tr("[ Frame Rate (FPS) ]", "[ フレームレート (FPS) ]"));
                ImGui::Separator();
                ImGui::Spacing();

                // 実測FPS表示
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), Loc::Tr("Current FPS: %.1f", "実測FPS: %.1f"), Manager::GetCurrentFPS());

                const char* fpsLabelsEn[] = { "30 FPS", "60 FPS (Default)", "120 FPS", "180 FPS", "240 FPS", "Unlimited (No Limit)" };
                const char* fpsLabelsJa[] = { "30 FPS", "60 FPS (標準)", "120 FPS", "180 FPS", "240 FPS", "無制限 (Unlimited)" };
                const char* fpsLabels[6];
                for (int i = 0; i < 6; ++i) {
                    fpsLabels[i] = Loc::Tr(fpsLabelsEn[i], fpsLabelsJa[i]);
                }
                const int fpsValues[] = { 30, 60, 120, 180, 240, 0 };
                const int fpsCount = 6;

                int currentFPS = Manager::GetTargetFPS();
                int currentIdx = 1; // 60 FPS
                for (int i = 0; i < fpsCount; ++i) {
                    if (fpsValues[i] == currentFPS) {
                        currentIdx = i;
                        break;
                    }
                }

                if (ImGui::Combo(Loc::Tr("Target FPS", "目標FPS"), &currentIdx, fpsLabels, fpsCount)) {
                    Manager::SetTargetFPS(fpsValues[currentIdx]);
                }

                ImGui::Spacing();
                ImGui::Text(Loc::Tr("Quick Select:", "クイック選択:"));
                for (int i = 0; i < fpsCount; ++i) {
                    if (i > 0) ImGui::SameLine();
                    bool isSelected = (currentFPS == fpsValues[i]);
                    if (isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.35f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.7f, 0.4f, 1.0f));
                    }
                    const char* btnLabel = (fpsValues[i] == 0) ? "Max" : (
                        fpsValues[i] == 30  ? "30" :
                        fpsValues[i] == 60  ? "60" :
                        fpsValues[i] == 120 ? "120" :
                        fpsValues[i] == 180 ? "180" : "240"
                    );
                    if (ImGui::Button(btnLabel, ImVec2(52, 28))) {
                        Manager::SetTargetFPS(fpsValues[i]);
                    }
                    if (isSelected) {
                        ImGui::PopStyleColor(2);
                    }
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), Loc::Tr("* Game physics and timers remain fixed at 60Hz.", "* 物理演算・タイマーは60Hz基準で一定に保たれます。"));

                ImGui::EndTabItem();
            }

            // ─────────────────────────────────────────────
            // 4. グラフィックス設定タブ (Monster Hunter 風)
            // ─────────────────────────────────────────────
            if (ImGui::BeginTabItem(Loc::Tr("Graphics", "グラフィックス")))
            {
                GraphicsSettings& settings = GraphicsConfig::Get();

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Loc::Tr("[ Graphics Preset ]", "[ 総合グラフィックプリセット ]"));
                ImGui::Separator();
                ImGui::Spacing();

                // 1. 総合プリセット（Ultra / High / Medium / Low / Custom）
                const char* presetLabelsEn[] = { "Ultra (Max Quality)", "High (Standard)", "Medium (Balanced)", "Low (Max FPS)", "Custom" };
                const char* presetLabelsJa[] = { "Ultra (最高品質)", "High (高画質・標準)", "Medium (中画質・FPS向上)", "Low (低画質・最高FPS)", "Custom (手動カスタム)" };
                const char* presetLabels[5];
                for (int i = 0; i < 5; ++i) {
                    presetLabels[i] = Loc::Tr(presetLabelsEn[i], presetLabelsJa[i]);
                }

                int curPresetIdx = static_cast<int>(settings.preset);
                if (ImGui::Combo(Loc::Tr("Preset", "プリセット"), &curPresetIdx, presetLabels, 5)) {
                    if (curPresetIdx != 4) {
                        GraphicsConfig::SetPreset(static_cast<GraphicsPreset>(curPresetIdx));
                    } else {
                        GraphicsConfig::MarkCustom();
                    }
                }

                // プリセット クイック選択ボタン列
                ImGui::Spacing();
                ImGui::Text(Loc::Tr("Quick Select:", "クイック選択:"));
                const char* quickLabels[] = { "Ultra", "High", "Medium", "Low" };
                for (int i = 0; i < 4; ++i) {
                    if (i > 0) ImGui::SameLine();
                    bool isCurrent = (static_cast<int>(settings.preset) == i);
                    if (isCurrent) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.75f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.65f, 0.85f, 1.0f));
                    }
                    if (ImGui::Button(quickLabels[i], ImVec2(75, 26))) {
                        GraphicsConfig::SetPreset(static_cast<GraphicsPreset>(i));
                    }
                    if (isCurrent) {
                        ImGui::PopStyleColor(2);
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // 2. 詳細設定（スクロール領域）
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Loc::Tr("[ Detailed Settings ]", "[ 詳細カスタム設定 ]"));
                ImGui::BeginChild("GraphicsDetails", ImVec2(0, 220), true);

                // (A) 解像度 & スケーリング
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), Loc::Tr("-- Scaling & Sharpening --", "-- 解像度＆シャープネス --"));
                int scalePercent = static_cast<int>(settings.renderScale * 100.0f + 0.5f);
                if (ImGui::SliderInt(Loc::Tr("Render Scale", "レンダリング解像度"), &scalePercent, 50, 100, "%d %%")) {
                    settings.renderScale = scalePercent / 100.0f;
                    GraphicsConfig::MarkCustom();
                }
                if (ImGui::SliderFloat(Loc::Tr("CAS Sharpness", "CAS輪郭強調"), &settings.casSharpness, 0.0f, 1.0f, "%.2f")) {
                    GraphicsConfig::MarkCustom();
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), Loc::Tr("-- Geometry & Outlines --", "-- ジオメトリ＆輪郭線 --"));
                const char* outlineLabelsEn[] = { "All (All Objects)", "Characters Only", "Off (Huge FPS Boost)" };
                const char* outlineLabelsJa[] = { "All (全オブジェクト)", "Characters Only (キャラのみ)", "Off (無効・大幅軽量化)" };
                const char* outlineLabels[3];
                for (int i = 0; i < 3; ++i) {
                    outlineLabels[i] = Loc::Tr(outlineLabelsEn[i], outlineLabelsJa[i]);
                }
                int curOutline = static_cast<int>(settings.outline);
                if (ImGui::Combo(Loc::Tr("Mesh Outline", "アウトライン描画"), &curOutline, outlineLabels, 3)) {
                    settings.outline = static_cast<OutlineQuality>(curOutline);
                    GraphicsConfig::MarkCustom();
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), Loc::Tr("-- Shadows & Lighting --", "-- 影＆ライティング --"));
                if (ImGui::Checkbox(Loc::Tr("Ground Drop Shadow (Blob)", "足元丸影 (ドロップシャドウ)"), &settings.blobShadow)) {
                    GraphicsConfig::MarkCustom();
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), Loc::Tr("-- Textures & Effects --", "-- テクスチャ＆エフェクト --"));
                const char* anisoLabelsEn[] = { "16x (Ultra)", "8x (High)", "4x (Medium)", "2x (Low)" };
                const char* anisoLabelsJa[] = { "16x (最高)", "8x (高)", "4x (中)", "2x (低)" };
                const char* anisoLabels[4];
                for (int i = 0; i < 4; ++i) {
                    anisoLabels[i] = Loc::Tr(anisoLabelsEn[i], anisoLabelsJa[i]);
                }
                int curAniso = static_cast<int>(settings.anisotropy);
                if (ImGui::Combo(Loc::Tr("Anisotropic Filter", "異方性フィルタ"), &curAniso, anisoLabels, 4)) {
                    settings.anisotropy = static_cast<AnisoQuality>(curAniso);
                    GraphicsConfig::MarkCustom();
                }
                if (ImGui::Checkbox(Loc::Tr("Realtime Refraction Copy (Water/Glass)", "屈折用リアルタイム背景コピー (水面・ガラス)"), &settings.refractionCopy)) {
                    GraphicsConfig::MarkCustom();
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), Loc::Tr("-- Post Processing --", "-- ポストプロセス --"));
                const char* postLabelsEn[] = { "High (Bloom + CA + CAS)", "Medium (Bloom Simple + CAS)", "Low (Tonemap Only)" };
                const char* postLabelsJa[] = { "High (ブルーム+色収差+CAS)", "Medium (ブルーム簡略+CAS)", "Low (トーンマップのみ・最軽量)" };
                const char* postLabels[3];
                for (int i = 0; i < 3; ++i) {
                    postLabels[i] = Loc::Tr(postLabelsEn[i], postLabelsJa[i]);
                }
                int curPost = static_cast<int>(settings.postProcess);
                if (ImGui::Combo(Loc::Tr("Post Processing", "ポストプロセス品質"), &curPost, postLabels, 3)) {
                    settings.postProcess = static_cast<PostProcessQuality>(curPost);
                    GraphicsConfig::MarkCustom();
                }
                if (ImGui::Checkbox(Loc::Tr("Radial Blur (Action Boost)", "ラジアルブラー演出 (アクション強調)"), &settings.radialBlur)) {
                    GraphicsConfig::MarkCustom();
                }

                ImGui::EndChild();

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), Loc::Tr("* Settings are applied immediately.", "* 変更は即座に反映されます。"));

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ─── 4. セーブ＆システム操作 ───
        // 手動セーブボタン
        if (ImGui::Button(Loc::Tr("Save Game", "セーブする"), ImVec2(140, 28))) {
            SaveSystem::Save();
            SoundManager::PlaySE(SoundID::SE_DECIDE);
            m_SaveFeedbackTimer = 2.5f;
        }
        ImGui::SameLine();
        if (m_SaveFeedbackTimer > 0.0f) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), Loc::Tr("  [ Save Completed! ]", "  [ セーブ完了! ]"));
        } else {
            ImGui::TextDisabled(Loc::Tr("  Auto-save is active", "  オートセーブ有効"));
        }

        ImGui::Spacing();

        // シーンに応じた帰還ボタン
        Scene curScene = Manager::GetCurrentScene();
        if (curScene == Scene::GAMEPLAY) {
            if (ImGui::Button(Loc::Tr("Abandon Quest & Return to Camp", "クエスト離脱・拠点へ帰還"), ImVec2(280, 28))) {
                ProgressionManager::CommitSession(false); // 獲得素材を保存
                SoundManager::PlaySE(SoundID::SE_DECIDE);
                Manager::ChangeScene(Scene::HUB);
                SetOpen(false);
            }
        } else if (curScene == Scene::HUB) {
            if (ImGui::Button(Loc::Tr("Return to Title", "タイトルへ戻る"), ImVec2(160, 28))) {
                SaveSystem::Save();
                SoundManager::PlaySE(SoundID::SE_DECIDE);
                Manager::ChangeScene(Scene::TITLE);
                SetOpen(false);
            }
        } else if (curScene == Scene::TUTORIAL) {
            if (ImGui::Button(Loc::Tr("Finish Tutorial & Go to Camp", "チュートリアル終了・拠点へ"), ImVec2(240, 28))) {
                SoundManager::PlaySE(SoundID::SE_DECIDE);
                Manager::ChangeScene(Scene::HUB);
                SetOpen(false);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 閉じるボタンとゲーム終了ボタン
        if (ImGui::Button(Loc::Tr("Close [Tab]", "閉じる [Tab]"), ImVec2(120, 32))) {
            SetOpen(false);
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 160.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.15f, 0.15f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button(Loc::Tr("Quit Game", "ゲーム終了"), ImVec2(140, 32))) {
            SaveSystem::Save(); // 終了前に確実にセーブ
            PostQuitMessage(0);
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::End();

    ImGui::PopStyleColor(4);
}
