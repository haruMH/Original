#include "option_ui.h"
#include "imgui/imgui.h"
#include "input.h"
#include "sound_manager.h"
#include "camera.h"
#include "main.h"

bool OptionUI::m_IsOpen = false;

void OptionUI::Init()
{
    m_IsOpen = false;
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

    // 画面中央にウィンドウを配置
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 380.0f), ImGuiCond_Appearing);

    // ウィンドウの装飾スタイル設定
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

    // 半透明の背景スタイル
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.12f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.18f, 0.22f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.30f, 0.50f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.45f, 0.70f, 1.0f));

    if (ImGui::Begin("OPTIONS (SETTINGS)", &m_IsOpen, windowFlags))
    {
        // 閉じるボタンが押された場合の同期
        if (!m_IsOpen) {
            SetOpen(false);
        }

        if (ImGui::BeginTabBar("OptionTabs"))
        {
            // ─────────────────────────────────────────────
            // 1. サウンド設定タブ
            // ─────────────────────────────────────────────
            if (ImGui::BeginTabItem("Audio"))
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[ Volume Settings ]");
                ImGui::Separator();
                ImGui::Spacing();

                // BGM 音量スライダー
                float bgmVol = SoundManager::GetBGMVolume() * 100.0f;
                if (ImGui::SliderFloat("BGM Volume", &bgmVol, 0.0f, 100.0f, "%.0f %%")) {
                    SoundManager::SetBGMVolume(bgmVol / 100.0f);
                }

                // SE 音量スライダー
                float seVol = SoundManager::GetSEVolume() * 100.0f;
                if (ImGui::SliderFloat("SE Volume", &seVol, 0.0f, 100.0f, "%.0f %%")) {
                    SoundManager::SetSEVolume(seVol / 100.0f);
                }

                ImGui::Spacing();
                // SE テスト再生ボタン
                if (ImGui::Button("Test SE (Grab)")) {
                    SoundManager::PlaySE(SoundID::SE_GRAB);
                }
                ImGui::SameLine();
                if (ImGui::Button("Test SE (Throw)")) {
                    SoundManager::PlaySE(SoundID::SE_THROW);
                }
                ImGui::SameLine();
                if (ImGui::Button("Test SE (Parry)")) {
                    SoundManager::PlaySE(SoundID::SE_PARRY);
                }

                ImGui::EndTabItem();
            }

            // ─────────────────────────────────────────────
            // 2. コントロール・カメラ設定タブ
            // ─────────────────────────────────────────────
            if (ImGui::BeginTabItem("Controls"))
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[ Camera & Mouse ]");
                ImGui::Separator();
                ImGui::Spacing();

                // マウス感度（ユーザーにわかりやすい 1 ~ 100 のスケールで表示）
                float currentSens = Camera::GetSensitivity();
                float displaySens = currentSens / 0.0001f; // 0.002f なら 20.0f
                if (ImGui::SliderFloat("Mouse Sensitivity", &displaySens, 5.0f, 100.0f, "%.1f")) {
                    Camera::SetSensitivity(displaySens * 0.0001f);
                }

                // 上下反転（Invert Y）
                bool invertY = Camera::GetInvertY();
                if (ImGui::Checkbox("Invert Vertical Look (Invert Y)", &invertY)) {
                    Camera::SetInvertY(invertY);
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Controls Guide:");
                ImGui::BulletText("Move: W, A, S, D");
                ImGui::BulletText("Jump: Space (Double Jump Available)");
                ImGui::BulletText("Dash / Dodge: Left Shift");
                ImGui::BulletText("Grab / Throw: Left Click or J Key");
                ImGui::BulletText("Spin / Sweep: Right Click or K Key");
                ImGui::BulletText("Toggle Option: Tab Key");

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 閉じるボタン
        if (ImGui::Button("Close [Tab]", ImVec2(120, 30))) {
            SetOpen(false);
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(4);
}
