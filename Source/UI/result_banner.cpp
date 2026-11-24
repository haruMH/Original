#include "result_banner.h"
#include "renderer.h"
#include "imgui/imgui.h"
#include <windows.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
    const int TEX_W = 1024;
    const int TEX_H = 256;

    bool  g_Active = false;
    bool  g_Triumph = false;
    float g_Time = 0.0f;                       // 経過秒
    std::string g_Text;
    std::vector<int> g_Pre;                    // 各文字の左端(テキスト内のpx)。最後は全体幅
    int   g_TextW = 0, g_TextH = 0;            // 文字の幅・高さ(テクスチャ内のpx)
    int   g_X0 = 0, g_Y0 = 0;                  // テクスチャ内で文字を置いた左上
    ComPtr<ID3D11ShaderResourceView> g_Srv;

    float Saturate(float x) { return (std::min)(1.0f, (std::max)(0.0f, x)); }
    float Smooth(float x) { x = Saturate(x); return x * x * (3.0f - 2.0f * x); }
    float Lerp(float a, float b, float t) { return a + (b - a) * t; }

    // 0→1 に向かって落ち、地面で2回ほど跳ねる
    float EaseOutBounce(float t)
    {
        t = Saturate(t);
        const float n1 = 7.5625f, d1 = 2.75f;
        if (t < 1.0f / d1) return n1 * t * t;
        if (t < 2.0f / d1) { t -= 1.5f / d1;   return n1 * t * t + 0.75f; }
        if (t < 2.5f / d1) { t -= 2.25f / d1;  return n1 * t * t + 0.9375f; }
        t -= 2.625f / d1;                       return n1 * t * t + 0.984375f;
    }

    // 文字を白のマスク(アルファ)画像にする。成功したら true
    bool BuildTexture(const char* text)
    {
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = TEX_W;
        bmi.bmiHeader.biHeight      = -TEX_H;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HDC dc = CreateCompatibleDC(nullptr);
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bmp || !bits) { if (bmp) DeleteObject(bmp); DeleteDC(dc); return false; }
        HGDIOBJ oldBmp = SelectObject(dc, bmp);
        memset(bits, 0, (size_t)TEX_W * TEX_H * 4);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));

        // 太い書体を順に試し、テクスチャに収まる最大の大きさを選ぶ
        const char* faces[] = { "Rockwell Extra Bold", "Arial Black", "Impact" };
        HFONT font = nullptr;
        SIZE sz = {};
        const int len = (int)strlen(text);
        for (const char* face : faces) {
            for (int px = 220; px >= 60; px -= 6) {
                HFONT f = CreateFontA(-px, 0, 0, 0, FW_HEAVY, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                      OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                      DEFAULT_PITCH | FF_DONTCARE, face);
                if (!f) continue;
                HGDIOBJ old = SelectObject(dc, f);
                char got[LF_FACESIZE] = {};
                GetTextFaceA(dc, LF_FACESIZE, got);
                GetTextExtentPoint32A(dc, text, len, &sz);
                SelectObject(dc, old);
                if (_stricmp(got, face) != 0) { DeleteObject(f); break; }
                if (sz.cx <= TEX_W - 40 && sz.cy <= TEX_H - 20) { font = f; break; }
                DeleteObject(f);
            }
            if (font) break;
        }
        if (!font) {
            font = CreateFontA(-120, 0, 0, 0, FW_HEAVY, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, "Arial");
        }
        HGDIOBJ oldFont = SelectObject(dc, font);
        GetTextExtentPoint32A(dc, text, len, &sz);

        g_TextW = sz.cx; g_TextH = sz.cy;
        g_X0 = (TEX_W - g_TextW) / 2;
        g_Y0 = (TEX_H - g_TextH) / 2;
        g_Pre.clear();
        for (int i = 0; i <= len; ++i) {
            SIZE p = {};
            GetTextExtentPoint32A(dc, text, i, &p);
            g_Pre.push_back(p.cx);
        }
        TextOutA(dc, g_X0, g_Y0, text, len);
        GdiFlush();

        // 明るさ(赤チャンネル)をアルファにした白画像へ
        std::vector<unsigned char> rgba((size_t)TEX_W * TEX_H * 4);
        const unsigned char* src = static_cast<const unsigned char*>(bits);
        for (int i = 0; i < TEX_W * TEX_H; ++i) {
            rgba[i * 4 + 0] = 255; rgba[i * 4 + 1] = 255; rgba[i * 4 + 2] = 255;
            rgba[i * 4 + 3] = src[i * 4 + 2];
        }

        SelectObject(dc, oldFont);
        DeleteObject(font);
        SelectObject(dc, oldBmp);
        DeleteObject(bmp);
        DeleteDC(dc);

        ID3D11Device* device = Renderer::GetDevice();
        if (!device) return false;
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = TEX_W; td.Height = TEX_H; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem = rgba.data();
        sd.SysMemPitch = TEX_W * 4;
        ComPtr<ID3D11Texture2D> tex;
        if (FAILED(device->CreateTexture2D(&td, &sd, tex.GetAddressOf()))) return false;
        return SUCCEEDED(device->CreateShaderResourceView(tex.Get(), nullptr, g_Srv.ReleaseAndGetAddressOf()));
    }

    // 1文字を、中心 c・倍率 sc・回転 rot で描く(影を4方向に重ねて縁取りする)
    void DrawLetter(ImDrawList* dl, ImTextureRef tex, ImVec2 uv0, ImVec2 uv1, ImVec2 c, float w, float h,
                    float sc, float rot, ImU32 col, ImU32 outline, float outlinePx, float shadowPx)
    {
        auto quad = [&](ImVec2 off, ImU32 color) {
            const float cs = cosf(rot), sn = sinf(rot);
            const float hw = w * 0.5f * sc, hh = h * 0.5f * sc;
            const ImVec2 corner[4] = { ImVec2(-hw, -hh), ImVec2(hw, -hh), ImVec2(hw, hh), ImVec2(-hw, hh) };
            ImVec2 p[4];
            for (int k = 0; k < 4; ++k) {
                p[k] = ImVec2(c.x + off.x + corner[k].x * cs - corner[k].y * sn,
                              c.y + off.y + corner[k].x * sn + corner[k].y * cs);
            }
            dl->AddImageQuad(tex, p[0], p[1], p[2], p[3],
                             ImVec2(uv0.x, uv0.y), ImVec2(uv1.x, uv0.y), ImVec2(uv1.x, uv1.y), ImVec2(uv0.x, uv1.y), color);
        };
        quad(ImVec2(0.0f, shadowPx), (outline & 0x00FFFFFF) | (((outline >> 24) / 2) << 24));   // 落ち影
        const float o = outlinePx;
        quad(ImVec2(-o, 0.0f), outline); quad(ImVec2(o, 0.0f), outline);
        quad(ImVec2(0.0f, -o), outline); quad(ImVec2(0.0f, o), outline);
        quad(ImVec2(0.0f, 0.0f), col);
    }
}

void ResultBanner::Start(const char* text, bool triumphant)
{
    Stop();
    g_Text = text;
    g_Triumph = triumphant;
    g_Time = 0.0f;
    if (!BuildTexture(text)) { g_Srv.Reset(); return; }
    g_Active = true;
}

void ResultBanner::Stop()
{
    g_Active = false;
    g_Srv.Reset();
    g_Pre.clear();
}

void ResultBanner::Update()
{
    if (!g_Active) return;
    g_Time += 1.0f / 60.0f;
}

void ResultBanner::Draw()
{
    if (!g_Active || !g_Srv || g_Pre.size() < 2) return;

    ImGuiIO& io = ImGui::GetIO();
    const float sw = io.DisplaySize.x, sh = io.DisplaySize.y;
    const float k = (std::min)(sw * 0.55f / (float)g_TextW, sh * 0.20f / (float)g_TextH);   // 1px あたりの画面px
    const float totalW = g_TextW * k;
    const float letterH = g_TextH * k;
    const float left = (sw - totalW) * 0.5f;
    const float baseY = sh * 0.07f + letterH * 0.5f;   // 見出しの中心の高さ

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImTextureRef tex((ImTextureID)(uintptr_t)g_Srv.Get());
    const int n = (int)g_Text.size();
    const float t = g_Time;

    // 光が走る位置(達成): 全文字が登場し終えてから左から右へ
    const float landDone = 0.5f + 0.13f * (n - 1) + 0.6f;
    const float sweepT = (t - landDone) / 0.9f;
    const float sweepX = left + (totalW + letterH * 2.0f) * sweepT - letterH;

    for (int i = 0; i < n; ++i) {
        if (g_Text[i] == ' ') continue;
        const float x0 = (float)g_Pre[i], x1 = (float)g_Pre[i + 1];
        const float w = (x1 - x0) * k;
        const ImVec2 uv0((g_X0 + x0) / TEX_W, g_Y0 / (float)TEX_H);
        const ImVec2 uv1((g_X0 + x1) / TEX_W, (g_Y0 + g_TextH) / (float)TEX_H);
        const float cx = left + (x0 + (x1 - x0) * 0.5f) * k;
        float cy = baseY, sc = 1.0f, rot = 0.0f, alpha = 1.0f;
        float r, g, b;
        ImU32 outline;

        if (g_Triumph) {
            // 1文字ずつ上から落ちて弾み、着地で少し潰れる
            const float u = (t - (0.5f + 0.13f * i)) / 0.6f;
            if (u <= 0.0f) continue;
            const float fall = 1.0f - EaseOutBounce(u);              // 1→0
            cy = baseY - fall * sh * 0.30f;
            alpha = Saturate(u * 5.0f);
            sc = 1.0f + 0.22f * (1.0f - Smooth(u * 1.4f));
            rot = (1.0f - Smooth(u)) * (i % 2 ? 0.12f : -0.12f);
            if (u > 1.0f) cy += sinf(t * 2.2f + i * 0.7f) * letterH * 0.015f;   // 着地後はほんの少し揺れる
            // 金色。光が通る位置では白く光る
            const float d = (cx - sweepX) / (letterH * 1.1f);
            const float glow = (sweepT > 0.0f && sweepT < 1.2f) ? expf(-d * d) : 0.0f;
            r = Lerp(1.00f, 1.00f, glow); g = Lerp(0.78f, 0.98f, glow); b = Lerp(0.18f, 0.85f, glow);
            outline = IM_COL32(60, 30, 5, (int)(255 * alpha));
        } else {
            // ゆっくり浮かび上がり(0.4秒から約2秒)、そのあと少しずつ沈んで暗くなる
            const float u = Smooth((t - (0.4f + 0.10f * i)) / 2.0f);
            if (u <= 0.0f) continue;
            alpha = u;
            const float sink = Smooth((t - 1.6f) / 3.5f);
            cy = baseY - letterH * 0.10f * u + letterH * 0.28f * sink;
            const float dim = Lerp(1.0f, 0.55f, sink);
            r = 1.00f * dim; g = 0.26f * dim; b = 0.28f * dim;
            outline = IM_COL32(20, 4, 8, (int)(255 * alpha));
        }

        const ImU32 col = IM_COL32((int)(Saturate(r) * 255), (int)(Saturate(g) * 255), (int)(Saturate(b) * 255), (int)(255 * Saturate(alpha)));
        DrawLetter(dl, tex, uv0, uv1, ImVec2(cx, cy), w, letterH, sc, rot, col, outline, (std::max)(2.0f, letterH * 0.035f), letterH * 0.07f);
    }
}
