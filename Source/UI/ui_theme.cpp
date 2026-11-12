#include "ui_theme.h"
#include "imgui.h"
#include "world_look.h"
#include <cmath>

namespace UITheme {

static void SetPalette(float day);

void Apply()
{
    ImGuiStyle& s = ImGui::GetStyle();

    // ── 形: 丸みと余白（ゲームの丸い石畳・小物に合わせる） ──
    s.WindowRounding    = 14.0f;
    s.ChildRounding     = 10.0f;
    s.FrameRounding     = 8.0f;
    s.PopupRounding     = 10.0f;
    s.ScrollbarRounding = 10.0f;
    s.GrabRounding      = 8.0f;
    s.TabRounding       = 8.0f;

    s.WindowBorderSize  = 1.5f;
    s.ChildBorderSize   = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.TabBorderSize     = 0.0f;

    s.WindowPadding     = ImVec2(18.0f, 16.0f);
    s.FramePadding      = ImVec2(10.0f, 6.0f);
    s.ItemSpacing       = ImVec2(10.0f, 9.0f);
    s.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);
    s.ScrollbarSize     = 14.0f;
    s.GrabMinSize       = 18.0f;
    s.WindowTitleAlign  = ImVec2(0.5f, 0.5f);

    SetPalette(0.0f);
}

// day: 0=夜の深い青 / 1=昼の明るい空色（空の時間に連動）
static void SetPalette(float day)
{
    ImGuiStyle& s = ImGui::GetStyle();
    auto L = [&](ImVec4 n, ImVec4 d) {
        return ImVec4(n.x + (d.x - n.x) * day, n.y + (d.y - n.y) * day,
                      n.z + (d.z - n.z) * day, n.w + (d.w - n.w) * day);
    };
    const ImVec4 bg        = L(ImVec4(0.050f, 0.070f, 0.135f, 0.94f), ImVec4(0.150f, 0.250f, 0.440f, 0.90f));
    const ImVec4 panel     = L(ImVec4(0.090f, 0.125f, 0.225f, 0.90f), ImVec4(0.230f, 0.340f, 0.560f, 0.92f));
    const ImVec4 panelHi   = L(ImVec4(0.140f, 0.190f, 0.330f, 0.95f), ImVec4(0.310f, 0.440f, 0.700f, 0.95f));
    const ImVec4 accent    (1.000f, 0.560f, 0.180f, 1.00f);   // 焚き火の橙
    const ImVec4 accentHi  (1.000f, 0.680f, 0.320f, 1.00f);
    const ImVec4 accentLo  (0.850f, 0.440f, 0.120f, 1.00f);
    const ImVec4 glow      (0.450f, 0.780f, 1.000f, 1.00f);   // 発光の水色
    const ImVec4 text      (0.930f, 0.940f, 0.970f, 1.00f);
    const ImVec4 textDim   (0.560f, 0.610f, 0.720f, 1.00f);
    const ImVec4 border    = L(ImVec4(0.900f, 0.560f, 0.260f, 0.50f), ImVec4(0.800f, 0.920f, 1.000f, 0.75f));   // 夜: 橙 / 昼: 水色
    const ImVec4 sep       = L(ImVec4(0.900f, 0.560f, 0.260f, 0.30f), ImVec4(0.800f, 0.920f, 1.000f, 0.40f));
    const ImVec4 frameBg   = L(ImVec4(0.035f, 0.050f, 0.100f, 0.95f), ImVec4(0.090f, 0.150f, 0.300f, 0.95f));
    const ImVec4 frameHov  = L(ImVec4(0.070f, 0.100f, 0.190f, 1.00f), ImVec4(0.130f, 0.210f, 0.390f, 1.00f));
    const ImVec4 frameAct  = L(ImVec4(0.090f, 0.125f, 0.230f, 1.00f), ImVec4(0.170f, 0.260f, 0.460f, 1.00f));
    const ImVec4 titleBg   = L(ImVec4(0.040f, 0.055f, 0.110f, 1.00f), ImVec4(0.120f, 0.200f, 0.370f, 1.00f));
    const ImVec4 titleAct  = L(ImVec4(0.075f, 0.105f, 0.200f, 1.00f), ImVec4(0.170f, 0.270f, 0.470f, 1.00f));
    const ImVec4 popupBg   = L(ImVec4(0.060f, 0.085f, 0.160f, 0.97f), ImVec4(0.140f, 0.230f, 0.410f, 0.97f));
    const ImVec4 btnHov    = L(ImVec4(0.230f, 0.310f, 0.520f, 1.00f), ImVec4(0.400f, 0.540f, 0.800f, 1.00f));

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                 = text;
    c[ImGuiCol_TextDisabled]         = textDim;
    c[ImGuiCol_WindowBg]             = bg;
    c[ImGuiCol_ChildBg]              = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_PopupBg]              = popupBg;
    c[ImGuiCol_Border]               = border;
    c[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    c[ImGuiCol_FrameBg]              = frameBg;
    c[ImGuiCol_FrameBgHovered]       = frameHov;
    c[ImGuiCol_FrameBgActive]        = frameAct;

    c[ImGuiCol_TitleBg]              = titleBg;
    c[ImGuiCol_TitleBgActive]        = titleAct;
    c[ImGuiCol_TitleBgCollapsed]     = titleBg;
    c[ImGuiCol_MenuBarBg]            = panel;

    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.0f, 0.0f, 0.0f, 0.25f);
    c[ImGuiCol_ScrollbarGrab]        = panelHi;
    c[ImGuiCol_ScrollbarGrabHovered] = accentLo;
    c[ImGuiCol_ScrollbarGrabActive]  = accent;

    c[ImGuiCol_CheckMark]            = accent;
    c[ImGuiCol_SliderGrab]           = accent;
    c[ImGuiCol_SliderGrabActive]     = accentHi;

    c[ImGuiCol_Button]               = panelHi;
    c[ImGuiCol_ButtonHovered]        = btnHov;
    c[ImGuiCol_ButtonActive]         = accent;

    c[ImGuiCol_Header]               = ImVec4(accent.x, accent.y, accent.z, 0.30f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(accent.x, accent.y, accent.z, 0.50f);
    c[ImGuiCol_HeaderActive]         = ImVec4(accent.x, accent.y, accent.z, 0.75f);

    c[ImGuiCol_Separator]            = sep;
    c[ImGuiCol_SeparatorHovered]     = accentHi;
    c[ImGuiCol_SeparatorActive]      = accent;
    c[ImGuiCol_ResizeGrip]           = ImVec4(accent.x, accent.y, accent.z, 0.25f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(accent.x, accent.y, accent.z, 0.60f);
    c[ImGuiCol_ResizeGripActive]     = accent;

    c[ImGuiCol_Tab]                  = panel;
    c[ImGuiCol_TabHovered]           = accentHi;
    c[ImGuiCol_TabActive]            = accent;
    c[ImGuiCol_TabUnfocused]         = panel;
    c[ImGuiCol_TabUnfocusedActive]   = accentLo;

    c[ImGuiCol_PlotLines]            = glow;
    c[ImGuiCol_PlotLinesHovered]     = accentHi;
    c[ImGuiCol_PlotHistogram]        = accent;
    c[ImGuiCol_PlotHistogramHovered] = accentHi;
    c[ImGuiCol_TextSelectedBg]       = ImVec4(glow.x, glow.y, glow.z, 0.35f);
    c[ImGuiCol_NavHighlight]         = glow;
}

void PushPrimaryButton()
{
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.00f, 0.56f, 0.18f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 0.68f, 0.32f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.85f, 0.44f, 0.12f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.14f, 0.08f, 0.03f, 1.00f));
}

void PushNormalButton()
{
    // テーマの既定色と同じ値を 4 色分 push（PopButton と数を合わせるため）
    ImGuiStyle& s = ImGui::GetStyle();
    ImGui::PushStyleColor(ImGuiCol_Button,        s.Colors[ImGuiCol_Button]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, s.Colors[ImGuiCol_ButtonHovered]);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  s.Colors[ImGuiCol_ButtonActive]);
    ImGui::PushStyleColor(ImGuiCol_Text,          s.Colors[ImGuiCol_Text]);
}

void PushDangerButton()
{
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.62f, 0.17f, 0.20f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.82f, 0.26f, 0.28f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.95f, 0.34f, 0.32f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.00f, 0.93f, 0.92f, 1.00f));
}

void PopButton()
{
    ImGui::PopStyleColor(4);
}

void Update()
{
    // 太陽の高さから昼の度合いを求め、パレットを滑らかに切り替える
    float sun[3];
    WorldLook::GetSunDirection(sun);
    float t = (sun[1] + 0.05f) / 0.40f;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    float day = t * t * (3.0f - 2.0f * t);

    static float s_Last = -1.0f;
    if (std::fabs(day - s_Last) > 0.002f) {
        SetPalette(day);
        s_Last = day;
    }
}

} // namespace UITheme
