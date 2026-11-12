#include "score_hud.h"
#include "game_constants.h"
#include "renderer.h"
#include "game_rule.h"
#include "player.h"
#include "manager.h"
#include "boss_enemy.h"
#include "player_combat.h"
#include "aura_effect.h"
#include <vector>
#include <algorithm>

// =================================================================
// 静的メンバ変数の実体定義
// =================================================================
Microsoft::WRL::ComPtr<ID3D11Buffer>             ScoreHUD::m_QuadVB;
Microsoft::WRL::ComPtr<ID3D11VertexShader>       ScoreHUD::m_VS;
Microsoft::WRL::ComPtr<ID3D11PixelShader>        ScoreHUD::m_PS;
Microsoft::WRL::ComPtr<ID3D11InputLayout>        ScoreHUD::m_IL;
Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  ScoreHUD::m_DepthState;
Microsoft::WRL::ComPtr<ID3D11Texture2D>          ScoreHUD::m_HUDTex2D;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ScoreHUD::m_Texture;
HDC                                              ScoreHUD::m_HudDC        = nullptr;
HBITMAP                                          ScoreHUD::m_HudBmp       = nullptr;
HGDIOBJ                                          ScoreHUD::m_HudOldBmp    = nullptr;
BYTE*                                            ScoreHUD::m_HudBits      = nullptr;
HFONT                                            ScoreHUD::m_HudFontBadge = nullptr;
HFONT                                            ScoreHUD::m_HudFontScore = nullptr;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ScoreHUD::m_TitleTexture;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ScoreHUD::m_ClearTexture;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ScoreHUD::m_GameOverTexture;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ScoreHUD::m_BossHPTexture;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ScoreHUD::m_PlayerHPTexture;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ScoreHUD::m_WhiteTexture;

int   ScoreHUD::m_LastTimeFrames = -1;
int   ScoreHUD::m_LastHP      = 5;
int   ScoreHUD::m_LastMaxHP   = 5;
int   ScoreHUD::m_LastBossHP    = -1;
int   ScoreHUD::m_LastBossMaxHP = -1;
float ScoreHUD::m_ScaleEffect = 1.0f;
float ScoreHUD::m_ShakeIntensity = 0.0f;
int   ScoreHUD::m_ShakeTimer = 0;
DirectX::XMFLOAT2 ScoreHUD::m_ShakeOffset = DirectX::XMFLOAT2(0.0f, 0.0f);

namespace {
    // BGRA DIBバッファをダークグラス半透明付きのRGBAバッファへ変換する共通処理
    void ConvertBgraToGlassRgba(const BYTE* bits, BYTE* dstRgba, int dstPitch, int W, int H, int lumThreshold, int baseAlpha)
    {
        for (int y = 0; y < H; ++y) {
            const BYTE* srcRow = bits + (y * W * 4);
            BYTE* dstRow = dstRgba + (y * dstPitch);
            for (int x = 0; x < W; ++x) {
                BYTE b = srcRow[x * 4 + 0];
                BYTE g = srcRow[x * 4 + 1];
                BYTE r = srcRow[x * 4 + 2];
                BYTE a = 0;
                if (r != 0 || g != 0 || b != 0) {
                    int lum = (r * 299 + g * 587 + b * 114) / 1000;
                    a = (lum < lumThreshold) ? static_cast<BYTE>(baseAlpha + lum) : 255;
                }
                dstRow[x * 4 + 0] = r;
                dstRow[x * 4 + 1] = g;
                dstRow[x * 4 + 2] = b;
                dstRow[x * 4 + 3] = a;
            }
        }
    }

    // タイムHUDのGDI描画処理（キャッシュ済みDC・フォントを使用）
    void RenderTimeHudGdi(HDC hdc, HFONT hFontBadge, HFONT hFontScore, int W, int H, int frames)
    {
        const int padX = 70;
        const int insetBot = 36;
        POINT platePts[6] = {
            { padX + 20, 2 },
            { W - padX - 20, 2 },
            { W - padX, 16 },
            { W - padX - insetBot, H - 4 },
            { padX + insetBot, H - 4 },
            { padX, 16 }
        };

        HBRUSH plateBrush = CreateSolidBrush(RGB(15, 14, 25));
        HPEN   goldPen = CreatePen(PS_SOLID, 2, RGB(255, 190, 40));
        HGDIOBJ oldPen = SelectObject(hdc, goldPen);
        HGDIOBJ oldBrush = SelectObject(hdc, plateBrush);
        Polygon(hdc, platePts, 6);

        HPEN thinGoldPen = CreatePen(PS_SOLID, 1, RGB(255, 230, 120));
        SelectObject(hdc, thinGoldPen);
        MoveToEx(hdc, padX + insetBot + 12, H - 9, nullptr);
        LineTo(hdc, W - padX - insetBot - 12, H - 9);

        POINT badgePts[4] = {
            { W / 2 - 42, 4 },
            { W / 2 + 42, 4 },
            { W / 2 + 35, 18 },
            { W / 2 - 35, 18 }
        };
        HBRUSH badgeBrush = CreateSolidBrush(RGB(255, 180, 20));
        SelectObject(hdc, badgeBrush);
        Polygon(hdc, badgePts, 4);
        DeleteObject(badgeBrush);

        HGDIOBJ oldFont = SelectObject(hdc, hFontBadge);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(20, 15, 5));
        RECT rcBadge = { W / 2 - 40, 5, W / 2 + 40, 18 };
        DrawTextW(hdc, L"TIME", -1, &rcBadge, DT_CENTER | DT_SINGLELINE);

        SelectObject(hdc, hFontScore);
        std::string timeStr = GameRule::FormatTime(frames);
        wchar_t buf[32];
        swprintf_s(buf, L"%hs", timeStr.c_str());

        SetTextColor(hdc, RGB(60, 45, 10));
        RECT rcShadow = { 0, 21, W + 2, H - 3 };
        DrawTextW(hdc, buf, -1, &rcShadow, DT_CENTER | DT_SINGLELINE);

        SetTextColor(hdc, RGB(255, 245, 210));
        RECT rcScore = { 0, 19, W, H - 5 };
        DrawTextW(hdc, buf, -1, &rcScore, DT_CENTER | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(plateBrush);
        DeleteObject(goldPen);
        DeleteObject(thinGoldPen);
        GdiFlush();
    }
}

// ─────────────────────────────────────────────────────────────────
// GDI DIB描画から D3D11 SRV を生成する共通ヘルパー
// ─────────────────────────────────────────────────────────────────
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ScoreHUD::CreateGdiTexture(
    ID3D11Device* device,
    int W,
    int H,
    int lumThreshold,
    int baseAlpha,
    const std::function<void(HDC)>& drawCallback
)
{
    if (!device) return nullptr;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* bits = nullptr;
    HDC hdc     = CreateCompatibleDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);
    memset(bits, 0, W * H * 4);

    if (drawCallback) {
        drawCallback(hdc);
    }
    GdiFlush();

    std::vector<BYTE> rgba(W * H * 4);
    ConvertBgraToGlassRgba(bits, rgba.data(), W * 4, W, H, lumThreshold, baseAlpha);

    SelectObject(hdc, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdc);

    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = W;
    td.Height           = H;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_IMMUTABLE;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem     = rgba.data();
    initData.SysMemPitch = W * 4;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    if (SUCCEEDED(device->CreateTexture2D(&td, &initData, tex.GetAddressOf())) && tex) {
        device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
    }
    return srv;
}

// ─────────────────────────────────────────────────────────────────
// タイムHUD用の動的テクスチャ生成とインプレース高速更新（Map/Unmap）
// ─────────────────────────────────────────────────────────────────
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ScoreHUD::CreateHUDTexture(
    ID3D11Device* device,
    int frames
)
{
    const int W = 512, H = 64;

    if (!m_HudDC) {
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = W;
        bmi.bmiHeader.biHeight      = -H;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        m_HudDC = CreateCompatibleDC(nullptr);
        m_HudBmp = CreateDIBSection(m_HudDC, &bmi, DIB_RGB_COLORS, (void**)&m_HudBits, nullptr, 0);
        m_HudOldBmp = SelectObject(m_HudDC, m_HudBmp);

        m_HudFontBadge = CreateFontW(
            13, 0, 0, 0, FW_BLACK, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
        );
        m_HudFontScore = CreateFontW(
            34, 0, 0, 0, FW_BLACK, TRUE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
        );
    }

    if (!m_HUDTex2D && device) {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width            = W;
        td.Height           = H;
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_DYNAMIC;
        td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;

        if (SUCCEEDED(device->CreateTexture2D(&td, nullptr, m_HUDTex2D.ReleaseAndGetAddressOf())) && m_HUDTex2D) {
            device->CreateShaderResourceView(m_HUDTex2D.Get(), nullptr, m_Texture.ReleaseAndGetAddressOf());
        }
    }

    UpdateHUDTexture(frames);
    return m_Texture;
}

void ScoreHUD::UpdateHUDTexture(int frames)
{
    if (!m_HudDC || !m_HudBits || !m_HUDTex2D) return;
    const int W = 512, H = 64;

    memset(m_HudBits, 0, W * H * 4);
    RenderTimeHudGdi(m_HudDC, m_HudFontBadge, m_HudFontScore, W, H, frames);

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    if (!ctx) return;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(ctx->Map(m_HUDTex2D.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        ConvertBgraToGlassRgba(m_HudBits, static_cast<BYTE*>(mapped.pData), mapped.RowPitch, W, H, 35, 190);
        ctx->Unmap(m_HUDTex2D.Get(), 0);
    }
}

// ─────────────────────────────────────────────────────────────────
// GDI を使用してボスHPバー用のテクスチャを生成する
// ─────────────────────────────────────────────────────────────────
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ScoreHUD::CreateBossHPTexture(
    ID3D11Device* device,
    int hp,
    int maxHp
)
{
    const int W = static_cast<int>(Constants::UI::BossHP::BAR_WIDTH);
    const int H = static_cast<int>(Constants::UI::BossHP::BAR_HEIGHT);

    return CreateGdiTexture(device, W, H, 35, 205, [&](HDC hdc) {
        const int wingCut = 22;
        POINT wingPts[6] = {
            { wingCut, 2 },
            { W - wingCut, 2 },
            { W - 4, H / 2 },
            { W - wingCut, H - 2 },
            { wingCut, H - 2 },
            { 4, H / 2 }
        };

        HBRUSH bgBrush = CreateSolidBrush(RGB(18, 10, 20));
        HPEN   borderPen = CreatePen(PS_SOLID, 2, RGB(255, 45, 75));
        HGDIOBJ oldPen = SelectObject(hdc, borderPen);
        HGDIOBJ oldBrush = SelectObject(hdc, bgBrush);
        Polygon(hdc, wingPts, 6);

        HPEN thinPen = CreatePen(PS_SOLID, 1, RGB(255, 140, 160));
        SelectObject(hdc, thinPen);
        MoveToEx(hdc, 12, H / 2, nullptr);
        LineTo(hdc, wingCut + 4, 6);
        MoveToEx(hdc, 12, H / 2, nullptr);
        LineTo(hdc, wingCut + 4, H - 6);

        MoveToEx(hdc, W - 12, H / 2, nullptr);
        LineTo(hdc, W - wingCut - 4, 6);
        MoveToEx(hdc, W - 12, H / 2, nullptr);
        LineTo(hdc, W - wingCut - 4, H - 6);
        DeleteObject(thinPen);

        HFONT hFontLabel = CreateFontW(
            24, 0, 0, 0, FW_BLACK, TRUE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
        );
        HGDIOBJ oldFont = SelectObject(hdc, hFontLabel);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 60, 90));
        RECT rcLabel = { 28, 8, 300, 36 };
        DrawTextW(hdc, L"WARNING // BOSS", -1, &rcLabel, DT_LEFT | DT_SINGLELINE);

        HFONT hFontHP = CreateFontW(
            20, 0, 0, 0, FW_HEAVY, TRUE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
        );
        SelectObject(hdc, hFontHP);
        SetTextColor(hdc, RGB(255, 235, 235));
        wchar_t hpBuf[32];
        swprintf_s(hpBuf, L"%d / %d", hp, maxHp);
        RECT rcHP = { W - 220, 10, W - 28, 36 };
        DrawTextW(hdc, hpBuf, -1, &rcHP, DT_RIGHT | DT_SINGLELINE);

        const int barLeft   = 28;
        const int barTop    = 42;
        const int barRight  = W - 28;
        const int barBottom = H - 12;

        HBRUSH barBgBrush = CreateSolidBrush(RGB(38, 16, 26));
        RECT barBgRect = { barLeft, barTop, barRight, barBottom };
        FillRect(hdc, &barBgRect, barBgBrush);
        DeleteObject(barBgBrush);

        if (maxHp > 0 && hp > 0) {
            float ratio = (float)hp / (float)maxHp;
            if (ratio > 1.0f) ratio = 1.0f;
            int fillRight = barLeft + (int)((barRight - barLeft) * ratio);

            int rCol = 255;
            int gCol = (int)(30 + ratio * 150);
            int bCol = (int)(60 * (1.0f - ratio));
            rCol = std::min(255, std::max(0, rCol));
            gCol = std::min(255, std::max(0, gCol));
            bCol = std::min(255, std::max(0, bCol));

            HBRUSH hpBrush = CreateSolidBrush(RGB(rCol, gCol, bCol));
            RECT fillRect = { barLeft, barTop, fillRight, barBottom };
            FillRect(hdc, &fillRect, hpBrush);
            DeleteObject(hpBrush);

            HBRUSH hlBrush = CreateSolidBrush(RGB(255, 230, 235));
            RECT hlRect = { barLeft, barTop, fillRight, barTop + 3 };
            FillRect(hdc, &hlRect, hlBrush);
            DeleteObject(hlBrush);
        }

        HPEN tickPen = CreatePen(PS_SOLID, 1, RGB(18, 10, 20));
        SelectObject(hdc, tickPen);
        for (int t = 1; t < 10; ++t) {
            int tx = barLeft + (int)((barRight - barLeft) * (t * 0.1f));
            MoveToEx(hdc, tx, barTop, nullptr);
            LineTo(hdc, tx, barBottom);
        }
        DeleteObject(tickPen);

        HPEN gaugeBorderPen = CreatePen(PS_SOLID, 1, RGB(255, 90, 120));
        SelectObject(hdc, gaugeBorderPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, barLeft, barTop, barRight, barBottom);
        DeleteObject(gaugeBorderPen);

        SelectObject(hdc, oldFont);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hFontLabel);
        DeleteObject(hFontHP);
        DeleteObject(bgBrush);
        DeleteObject(borderPen);
    });
}

// ─────────────────────────────────────────────────────────────────
// GDI を使用してプレイヤーHPバー用のテクスチャを生成する
// ─────────────────────────────────────────────────────────────────
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ScoreHUD::CreatePlayerHPTexture(
    ID3D11Device* device,
    int hp,
    int maxHp
)
{
    const int W = static_cast<int>(Constants::UI::PlayerHP::BAR_WIDTH);
    const int H = static_cast<int>(Constants::UI::PlayerHP::BAR_HEIGHT);
    const int SLANT = 14;

    return CreateGdiTexture(device, W, H, 40, 195, [&](HDC hdc) {
        POINT panelPts[4] = {
            { SLANT + 2, 2 },
            { W - 4, 2 },
            { W - 4 - SLANT, H - 2 },
            { 2, H - 2 }
        };
        HBRUSH panelBgBrush = CreateSolidBrush(RGB(14, 20, 32));
        HPEN   panelBorderPen = CreatePen(PS_SOLID, 2, RGB(0, 210, 255));
        HGDIOBJ oldPen = SelectObject(hdc, panelBorderPen);
        HGDIOBJ oldBrush = SelectObject(hdc, panelBgBrush);
        Polygon(hdc, panelPts, 4);

        POINT decoPts[3] = {
            { W - 4, 2 },
            { W - 18, 2 },
            { W - 4, 16 }
        };
        HBRUSH decoBrush = CreateSolidBrush(RGB(0, 240, 255));
        SelectObject(hdc, decoBrush);
        Polygon(hdc, decoPts, 3);
        DeleteObject(decoBrush);

        POINT badgePts[4] = {
            { SLANT + 6, 6 },
            { SLANT + 68, 6 },
            { 68, 24 },
            { 6, 24 }
        };
        HBRUSH badgeBrush = CreateSolidBrush(RGB(0, 210, 255));
        SelectObject(hdc, badgeBrush);
        Polygon(hdc, badgePts, 4);
        DeleteObject(badgeBrush);

        HFONT hFontBadge = CreateFontW(
            15, 0, 0, 0, FW_BLACK, TRUE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
        );
        HGDIOBJ oldFont = SelectObject(hdc, hFontBadge);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(10, 20, 35));
        RECT rcBadge = { 10, 7, 72, 25 };
        DrawTextW(hdc, L"HERO", -1, &rcBadge, DT_CENTER | DT_SINGLELINE);

        HFONT hFontHP = CreateFontW(
            19, 0, 0, 0, FW_HEAVY, TRUE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
        );
        SelectObject(hdc, hFontHP);
        SetTextColor(hdc, RGB(225, 245, 255));
        wchar_t hpBuf[32];
        swprintf_s(hpBuf, L"HP  %d / %d", hp, maxHp);
        RECT rcHP = { W - 150, 5, W - 24, 25 };
        DrawTextW(hdc, hpBuf, -1, &rcHP, DT_RIGHT | DT_SINGLELINE);

        const int barTop    = 30;
        const int barBottom = H - 8;
        const int barLeft   = 16;
        const int barRight  = W - 22;
        const int totalBarW = barRight - barLeft;
        const int segCount  = (maxHp > 0) ? maxHp : 5;
        const int segGap    = 4;
        const float segW    = (float)(totalBarW - segGap * (segCount - 1)) / (float)segCount;

        float ratio = (maxHp > 0) ? ((float)hp / (float)maxHp) : 1.0f;
        if (ratio > 1.0f) ratio = 1.0f;

        int rCol = 0, gCol = 230, bCol = 255;
        if (ratio > 0.5f) {
            float t = (ratio - 0.5f) * 2.0f;
            rCol = (int)(0 * (1.0f - t) + 0 * t);
            gCol = (int)(220 * (1.0f - t) + 255 * t);
            bCol = (int)(255 * (1.0f - t) + 140 * t);
        } else {
            float t = ratio * 2.0f;
            rCol = (int)(255 * (1.0f - t) + 255 * t);
            gCol = (int)(40 * (1.0f - t) + 210 * t);
            bCol = (int)(40 * (1.0f - t) + 0 * t);
        }
        rCol = std::min(255, std::max(0, rCol));
        gCol = std::min(255, std::max(0, gCol));
        bCol = std::min(255, std::max(0, bCol));

        HBRUSH fillBrush = CreateSolidBrush(RGB(rCol, gCol, bCol));
        HBRUSH emptyBrush = CreateSolidBrush(RGB(24, 32, 48));
        HBRUSH hlBrush = CreateSolidBrush(RGB(220, 250, 255));
        HPEN   segPen = CreatePen(PS_SOLID, 1, RGB(0, 160, 210));

        for (int i = 0; i < segCount; ++i) {
            float x0 = barLeft + i * (segW + segGap);
            float x1 = x0 + segW;
            bool isFilled = (i < hp);

            POINT segPts[4] = {
                { (int)(x0 + 6), barTop },
                { (int)(x1 + 6), barTop },
                { (int)(x1), barBottom },
                { (int)(x0), barBottom }
            };

            SelectObject(hdc, isFilled ? fillBrush : emptyBrush);
            SelectObject(hdc, isFilled ? segPen : (HGDIOBJ)GetStockObject(NULL_PEN));
            Polygon(hdc, segPts, 4);

            if (isFilled) {
                POINT hlPts[4] = {
                    { (int)(x0 + 6), barTop },
                    { (int)(x1 + 6), barTop },
                    { (int)(x1 + 5), barTop + 3 },
                    { (int)(x0 + 5), barTop + 3 }
                };
                SelectObject(hdc, hlBrush);
                SelectObject(hdc, GetStockObject(NULL_PEN));
                Polygon(hdc, hlPts, 4);
            }
        }

        DeleteObject(fillBrush);
        DeleteObject(emptyBrush);
        DeleteObject(hlBrush);
        DeleteObject(segPen);

        SelectObject(hdc, oldFont);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hFontBadge);
        DeleteObject(hFontHP);
        DeleteObject(panelBgBrush);
        DeleteObject(panelBorderPen);
    });
}

// ─────────────────────────────────────────────────────────────────
// GDI を使用してタイトル画面用のテクスチャを生成する
// ─────────────────────────────────────────────────────────────────
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ScoreHUD::CreateTitleTexture(ID3D11Device* device)
{
    const int W = 512, H = 256;

    return CreateGdiTexture(device, W, H, 35, 205, [&](HDC hdc) {
        POINT bgPts[6] = {
            { 24, 6 },
            { W - 24, 6 },
            { W - 6, 36 },
            { W - 24, H - 6 },
            { 24, H - 6 },
            { 6, H - 36 }
        };
        HBRUSH bgBrush = CreateSolidBrush(RGB(14, 16, 26));
        HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(0, 210, 255));
        HGDIOBJ oldPen = SelectObject(hdc, borderPen);
        HGDIOBJ oldBrush = SelectObject(hdc, bgBrush);
        Polygon(hdc, bgPts, 6);

        HFONT hFontSub = CreateFontW(
            13, 0, 0, 0, FW_BOLD, TRUE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
        );
        HGDIOBJ oldFont = SelectObject(hdc, hFontSub);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 190, 40));
        RECT rcSub = { 0, 18, W, 34 };
        DrawTextW(hdc, L"◆ HIGH SPEED SLASH ACTION ◆", -1, &rcSub, DT_CENTER | DT_SINGLELINE);

        HFONT hFontTitle = CreateFontW(
            54, 0, 0, 0, FW_BLACK, TRUE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
        );
        SelectObject(hdc, hFontTitle);

        SetTextColor(hdc, RGB(6, 8, 14));
        RECT rcShadow = { 0, 44, W + 4, 114 };
        DrawTextW(hdc, L"TOON SLASHER", -1, &rcShadow, DT_CENTER | DT_SINGLELINE);

        SetTextColor(hdc, RGB(245, 255, 255));
        RECT rcTitle = { 0, 40, W, 110 };
        DrawTextW(hdc, L"TOON SLASHER", -1, &rcTitle, DT_CENTER | DT_SINGLELINE);

        HPEN sepPen = CreatePen(PS_SOLID, 1, RGB(0, 220, 255));
        SelectObject(hdc, sepPen);
        MoveToEx(hdc, 40, 114, nullptr);
        LineTo(hdc, W - 40, 114);
        DeleteObject(sepPen);

        POINT enterBadgePts[4] = {
            { W / 2 - 130, 126 },
            { W / 2 + 130, 126 },
            { W / 2 + 120, 154 },
            { W / 2 - 120, 154 }
        };
        HBRUSH enterBadgeBrush = CreateSolidBrush(RGB(255, 190, 30));
        SelectObject(hdc, enterBadgeBrush);
        Polygon(hdc, enterBadgePts, 4);
        DeleteObject(enterBadgeBrush);

        HFONT hFontEnter = CreateFontW(
            17, 0, 0, 0, FW_BLACK, TRUE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
        );
        SelectObject(hdc, hFontEnter);
        SetTextColor(hdc, RGB(20, 15, 5));
        RECT rcEnter = { W / 2 - 120, 131, W / 2 + 120, 152 };
        DrawTextW(hdc, L"PRESS ENTER TO START", -1, &rcEnter, DT_CENTER | DT_SINGLELINE);

        HFONT hFontGuide = CreateFontW(
            14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
        );
        SelectObject(hdc, hFontGuide);
        SetTextColor(hdc, RGB(180, 210, 235));
        RECT rcGuide1 = { 0, 172, W, 194 };
        DrawTextW(hdc, L"[WASD] MOVE    |    [SHIFT] DASH / DODGE", -1, &rcGuide1, DT_CENTER | DT_SINGLELINE);
        RECT rcGuide2 = { 0, 198, W, 220 };
        DrawTextW(hdc, L"[L-CLICK] GRAB / THROW    |    [R-CLICK] SPIN ATTACK", -1, &rcGuide2, DT_CENTER | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hFontTitle);
        DeleteObject(hFontSub);
        DeleteObject(hFontEnter);
        DeleteObject(hFontGuide);
        DeleteObject(bgBrush);
        DeleteObject(borderPen);
    });
}

// ─────────────────────────────────────────────────────────────────
// GDI を使用してリザルト画面用のテクスチャを生成する
// ─────────────────────────────────────────────────────────────────
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ScoreHUD::CreateResultTexture(ID3D11Device* device, bool isClear, int frames)
{
    const int W = 512, H = 256;

    return CreateGdiTexture(device, W, H, 35, 210, [&](HDC hdc) {
        POINT bgPts[6] = {
            { 24, 6 },
            { W - 24, 6 },
            { W - 6, 36 },
            { W - 24, H - 6 },
            { 24, H - 6 },
            { 6, H - 36 }
        };
        HBRUSH bgBrush = CreateSolidBrush(RGB(15, 14, 24));
        HPEN borderPen = CreatePen(PS_SOLID, 2, isClear ? RGB(255, 190, 40) : RGB(255, 45, 75));
        HGDIOBJ oldPen = SelectObject(hdc, borderPen);
        HGDIOBJ oldBrush = SelectObject(hdc, bgBrush);
        Polygon(hdc, bgPts, 6);

        HFONT hFontTitle = CreateFontW(
            50, 0, 0, 0, FW_BLACK, TRUE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
        );
        HGDIOBJ oldFont = SelectObject(hdc, hFontTitle);
        SetBkMode(hdc, TRANSPARENT);

        SetTextColor(hdc, RGB(6, 6, 12));
        RECT rcShadow = { 0, 26, W + 4, 86 };
        DrawTextW(hdc, isClear ? L"MISSION CLEAR" : L"GAME OVER", -1, &rcShadow, DT_CENTER | DT_SINGLELINE);

        SetTextColor(hdc, isClear ? RGB(255, 220, 60) : RGB(255, 60, 80));
        RECT rcTitle = { 0, 24, W, 84 };
        DrawTextW(hdc, isClear ? L"MISSION CLEAR" : L"GAME OVER", -1, &rcTitle, DT_CENTER | DT_SINGLELINE);

        HFONT hFontLabel = CreateFontW(
            16, 0, 0, 0, FW_BLACK, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
        );
        SelectObject(hdc, hFontLabel);

        if (isClear) {
            if (GameRule::IsNewRecord()) {
                SetTextColor(hdc, RGB(255, 210, 30));
                RECT rcLabel = { 0, 96, W, 118 };
                DrawTextW(hdc, L"★ NEW RECORD! ★", -1, &rcLabel, DT_CENTER | DT_SINGLELINE);
            } else {
                SetTextColor(hdc, RGB(0, 220, 255));
                RECT rcLabel = { 0, 96, W, 118 };
                DrawTextW(hdc, L"★ CLEAR TIME ★", -1, &rcLabel, DT_CENTER | DT_SINGLELINE);
            }
        } else {
            SetTextColor(hdc, RGB(255, 120, 140));
            RECT rcLabel = { 0, 96, W, 118 };
            DrawTextW(hdc, L"- SURVIVAL TIME -", -1, &rcLabel, DT_CENTER | DT_SINGLELINE);
        }
        DeleteObject(hFontLabel);

        HFONT hFontTimeNum = CreateFontW(
            42, 0, 0, 0, FW_BLACK, TRUE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
        );
        SelectObject(hdc, hFontTimeNum);

        std::string timeStr = GameRule::FormatTime(frames);
        wchar_t timeBuf[32];
        swprintf_s(timeBuf, L"%hs", timeStr.c_str());

        SetTextColor(hdc, isClear ? RGB(255, 245, 210) : RGB(230, 200, 210));
        RECT rcTimeNum = { 0, 116, W, 168 };
        DrawTextW(hdc, timeBuf, -1, &rcTimeNum, DT_CENTER | DT_SINGLELINE);
        DeleteObject(hFontTimeNum);

        if (isClear) {
            HFONT hFontBest = CreateFontW(
                13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
            );
            SelectObject(hdc, hFontBest);
            SetTextColor(hdc, RGB(160, 190, 210));
            std::string bestStr = "BEST: " + GameRule::GetBestFormattedTime();
            wchar_t bestBuf[32];
            swprintf_s(bestBuf, L"%hs", bestStr.c_str());
            RECT rcBest = { 0, 166, W, 186 };
            DrawTextW(hdc, bestBuf, -1, &rcBest, DT_CENTER | DT_SINGLELINE);
            DeleteObject(hFontBest);
        }

        HFONT hFontDesc = CreateFontW(
            16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
        );
        SelectObject(hdc, hFontDesc);
        SetTextColor(hdc, RGB(200, 215, 230));
        RECT rcDesc = { 0, 194, W, 226 };
        DrawTextW(hdc, L"PRESS ENTER TO RETURN TITLE", -1, &rcDesc, DT_CENTER | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hFontTitle);
        DeleteObject(hFontDesc);
        DeleteObject(bgBrush);
        DeleteObject(borderPen);
    });
}

// ─────────────────────────────────────────────────────────────────
// 初期化処理
// ─────────────────────────────────────────────────────────────────
bool ScoreHUD::Init(ID3D11Device* device)
{
    if (!device) return false;

    // ─── 描画用クアッド頂点バッファの作成 ───
    // 中心基準でスケールできるように、ローカル座標範囲 [-0.5, 0.5] で作成
    struct VERTEX_3D_LOCAL {
        XMFLOAT3 Position;
        XMFLOAT3 Normal;
        XMFLOAT4 Diffuse;
        XMFLOAT2 TexCoord;
        XMFLOAT3 Tangent;
    } v[4];

    // TL: 左上
    v[0].Position = XMFLOAT3(-0.5f, -0.5f, 0.0f);
    v[0].Normal   = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[0].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[0].TexCoord = XMFLOAT2(0.0f, 0.0f);
    v[0].Tangent  = XMFLOAT3(1, 0, 0);

    // TR: 右上
    v[1].Position = XMFLOAT3( 0.5f, -0.5f, 0.0f);
    v[1].Normal   = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[1].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[1].TexCoord = XMFLOAT2(1.0f, 0.0f);
    v[1].Tangent  = XMFLOAT3(1, 0, 0);

    // BL: 左下
    v[2].Position = XMFLOAT3(-0.5f,  0.5f, 0.0f);
    v[2].Normal   = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[2].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[2].TexCoord = XMFLOAT2(0.0f, 1.0f);
    v[2].Tangent  = XMFLOAT3(1, 0, 0);

    // BR: 右下
    v[3].Position = XMFLOAT3( 0.5f,  0.5f, 0.0f);
    v[3].Normal   = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[3].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[3].TexCoord = XMFLOAT2(1.0f, 1.0f);
    v[3].Tangent  = XMFLOAT3(1, 0, 0);

    D3D11_BUFFER_DESC bd = {};
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth = sizeof(v);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = v;
    if (FAILED(device->CreateBuffer(&bd, &sd, m_QuadVB.ReleaseAndGetAddressOf()))) return false;

    // ─── シェーダーファイルの読み込み（2-1 対応: Renderer::ResolveShaderPath でパス解決を一元化）───
    Renderer::CreateVertexShader(m_VS.ReleaseAndGetAddressOf(), m_IL.ReleaseAndGetAddressOf(), Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(m_PS.ReleaseAndGetAddressOf(), Renderer::ResolveShaderPath("ui_ps.cso").c_str());

    if (!m_VS || !m_PS || !m_IL) {
        LOG_INFO("[ScoreHUD] シェーダーの読み込みに失敗しました\n");
        return false;
    }

    // ─── 深度ステートの作成（テストOFF / 書き込みOFF）───
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = FALSE; // 常に最前面に描画
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc      = D3D11_COMPARISON_ALWAYS;
    if (FAILED(device->CreateDepthStencilState(&dsd, m_DepthState.ReleaseAndGetAddressOf()))) return false;

    // ─── 初期テクスチャ（タイム0、HP5）の生成 ───
    m_LastTimeFrames = GameRule::GetElapsedFrames();
    m_LastHP = 5;
    m_LastMaxHP = 5;
    Player* player = Manager::GetGameObject<Player>();
    if (player) {
        m_LastHP    = player->GetHP();
        m_LastMaxHP = player->GetMaxHP();
    }
    m_Texture         = CreateHUDTexture(device, m_LastTimeFrames);
    m_PlayerHPTexture = CreatePlayerHPTexture(device, m_LastHP, m_LastMaxHP);
    m_ScaleEffect = 1.0f;

    // 単色ゲージ描画用の1x1白テクスチャ
    {
        const UINT32 whitePixel = 0xFFFFFFFF;
        D3D11_TEXTURE2D_DESC td = {};
        td.Width            = 1;
        td.Height           = 1;
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_IMMUTABLE;
        td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd = { &whitePixel, 4, 0 };
        Microsoft::WRL::ComPtr<ID3D11Texture2D> whiteTex;
        if (SUCCEEDED(device->CreateTexture2D(&td, &sd, &whiteTex))) {
            device->CreateShaderResourceView(whiteTex.Get(), nullptr, &m_WhiteTexture);
        }
    }

    LOG_INFO("[ScoreHUD] 初期化が正常に完了しました\n");
    return true;
}

// ─────────────────────────────────────────────────────────────────
// 終了処理
// ─────────────────────────────────────────────────────────────────
void ScoreHUD::Uninit()
{
    if (m_HudDC) {
        if (m_HudOldBmp) { SelectObject(m_HudDC, m_HudOldBmp); m_HudOldBmp = nullptr; }
        if (m_HudBmp)    { DeleteObject(m_HudBmp); m_HudBmp = nullptr; }
        DeleteDC(m_HudDC);
        m_HudDC = nullptr;
        m_HudBits = nullptr;
    }
    if (m_HudFontBadge) { DeleteObject(m_HudFontBadge); m_HudFontBadge = nullptr; }
    if (m_HudFontScore) { DeleteObject(m_HudFontScore); m_HudFontScore = nullptr; }
    m_HUDTex2D.Reset();
    m_Texture.Reset();
    m_TitleTexture.Reset();
    m_ClearTexture.Reset();
    m_GameOverTexture.Reset();
    m_BossHPTexture.Reset();
    m_PlayerHPTexture.Reset();
    m_WhiteTexture.Reset();
    m_DepthState.Reset();
    m_IL.Reset();
    m_PS.Reset();
    m_VS.Reset();
    m_QuadVB.Reset();
}

void ScoreHUD::Shake(float intensity, int frames)
{
    m_ShakeIntensity = intensity;
    m_ShakeTimer = frames;
}

// ─────────────────────────────────────────────────────────────────
// 更新処理
// ─────────────────────────────────────────────────────────────────
void ScoreHUD::Update()
{
    // UIシェイクの更新
    if (m_ShakeTimer > 0) {
        m_ShakeTimer--;
        float angle = ((float)rand() / RAND_MAX) * XM_2PI;
        float factor = (float)m_ShakeTimer / (float)(std::max)(1, m_ShakeTimer + 1);
        m_ShakeOffset.x = cosf(angle) * m_ShakeIntensity * factor;
        m_ShakeOffset.y = sinf(angle) * m_ShakeIntensity * factor;
    } else {
        m_ShakeOffset = XMFLOAT2(0.0f, 0.0f);
        m_ShakeIntensity = 0.0f;
    }

    Scene currentScene = Manager::GetCurrentScene();

    if (currentScene == Scene::TITLE) {
        if (!m_TitleTexture) {
            m_TitleTexture = CreateTitleTexture(Renderer::GetDevice());
        }
    } else if (currentScene == Scene::GAMEPLAY) {
        // プレイヤーHP変化の監視
        int curPlayerHP = m_LastHP;
        int curPlayerMaxHP = m_LastMaxHP;
        Player* player = Manager::GetGameObject<Player>();
        if (player) {
            curPlayerHP    = player->GetHP();
            curPlayerMaxHP = player->GetMaxHP();
        }

        if (curPlayerHP != m_LastHP || curPlayerMaxHP != m_LastMaxHP || !m_PlayerHPTexture) {
            m_PlayerHPTexture = CreatePlayerHPTexture(Renderer::GetDevice(), curPlayerHP, curPlayerMaxHP);
            m_LastHP    = curPlayerHP;
            m_LastMaxHP = curPlayerMaxHP;
        }

        // タイムアタック時間の監視（約2フレーム = 0.033秒ごと、またはタイム短縮ボーナス時にテクスチャ更新）
        int currentFrames = GameRule::GetElapsedFrames();
        bool timeJumped = (m_LastTimeFrames >= 0 && currentFrames < m_LastTimeFrames); // タイム短縮ボーナス発生
        if (!m_Texture) {
            m_Texture = CreateHUDTexture(Renderer::GetDevice(), currentFrames);
            m_LastTimeFrames = currentFrames;
        } else if (timeJumped || std::abs(currentFrames - m_LastTimeFrames) >= 2) {
            UpdateHUDTexture(currentFrames);
            m_LastTimeFrames = currentFrames;

            if (timeJumped) {
                // タイム短縮時はポップ演出を発動！
                m_ScaleEffect = 1.35f;
            }
        }

        // スケールを徐々に 1.0 に戻す（イージング）
        if (m_ScaleEffect > 1.0f) {
            m_ScaleEffect += (1.0f - m_ScaleEffect) * 0.15f;
            if (m_ScaleEffect - 1.0f < 0.005f) {
                m_ScaleEffect = 1.0f;
            }
        }

        // ボスステージ中のHP監視
        if (Manager::IsBossStage()) {
            BossEnemy* boss = Manager::GetGameObject<BossEnemy>();
            if (boss && boss->GetEnemyState() != EnemyState::DEFEATED) {
                int curBossHP    = boss->GetHP();
                int curBossMaxHP = boss->GetMaxHP();
                if (curBossHP != m_LastBossHP || curBossMaxHP != m_LastBossMaxHP || !m_BossHPTexture) {
                    m_BossHPTexture = CreateBossHPTexture(Renderer::GetDevice(), curBossHP, curBossMaxHP);
                    m_LastBossHP    = curBossHP;
                    m_LastBossMaxHP = curBossMaxHP;
                }
            } else {
                // ボスが倒された・存在しない場合はバーを消す
                m_BossHPTexture.Reset();
            }
        } else {
            m_BossHPTexture.Reset();
        }
    } else if (currentScene == Scene::CLEAR) {
        if (!m_ClearTexture) {
            m_ClearTexture = CreateResultTexture(Renderer::GetDevice(), true, GameRule::GetClearTimeFrames());
        }
    } else if (currentScene == Scene::GAMEOVER) {
        if (!m_GameOverTexture) {
            m_GameOverTexture = CreateResultTexture(Renderer::GetDevice(), false, GameRule::GetElapsedFrames());
        }
    }

    // 不要なシーンテクスチャを解放
    if (currentScene != Scene::TITLE && m_TitleTexture) {
        m_TitleTexture.Reset();
    }
    if (currentScene != Scene::CLEAR && m_ClearTexture) {
        m_ClearTexture.Reset();
    }
    if (currentScene != Scene::GAMEOVER && m_GameOverTexture) {
        m_GameOverTexture.Reset();
    }
    if (currentScene != Scene::GAMEPLAY && m_PlayerHPTexture) {
        m_PlayerHPTexture.Reset();
    }
}

// ─────────────────────────────────────────────────────────────────
// 描画処理（Renderer::End() の直前に呼び出す）
// ─────────────────────────────────────────────────────────────────
void ScoreHUD::Draw()
{
    ID3D11ShaderResourceView* texToDraw = nullptr;
    Scene currentScene = Manager::GetCurrentScene();

    const float screenW = (float)Renderer::GetScreenWidth();
    const float screenH = (float)Renderer::GetScreenHeight();

    float w = 512.0f;
    float h = 64.0f;
    float posX = screenW * 0.5f; // 画面中央
    float posY = 45.0f;          // 画面上部から少し下げた位置
    XMFLOAT4 emissionColor = XMFLOAT4(1.5f, 1.2f, 0.0f, 0.0f); // スコアHUDは黄金色の発光

    switch (currentScene) {
    case Scene::TITLE:
        texToDraw = m_TitleTexture.Get();
        {
            // ウィンドウサイズに応じて縮小し、ロゴとメニューの間に収める
            const float s = (std::max)(0.6f, (std::min)(1.0f, screenH / 900.0f));
            w = 512.0f * s;
            h = 256.0f * s;
            static float titleFloatTime = 0.0f;
            titleFloatTime += 0.035f;
            posY = screenH * 0.42f + sinf(titleFloatTime) * 5.0f * s; // 上下にフワフワ浮遊
        }
        emissionColor = XMFLOAT4(0.0f, 1.2f, 1.8f, 0.0f); // タイトルは青白発光
        break;
    case Scene::GAMEPLAY:
        texToDraw = m_Texture.Get();
        w = 512.0f * m_ScaleEffect;
        h = 64.0f * m_ScaleEffect;
        posY = 45.0f;
        break;
    case Scene::CLEAR:
        texToDraw = m_ClearTexture.Get();
        w = 512.0f;
        h = 256.0f;
        posY = screenH * 0.5f;
        emissionColor = XMFLOAT4(1.5f, 1.2f, 0.0f, 0.0f); // クリアは黄金発光
        break;
    case Scene::GAMEOVER:
        texToDraw = m_GameOverTexture.Get();
        w = 512.0f;
        h = 256.0f;
        posY = screenH * 0.5f;
        emissionColor = XMFLOAT4(1.8f, 0.0f, 0.0f, 0.0f); // ゲームオーバーは赤発光
        break;
    }

    if (!m_QuadVB || !m_VS || !m_PS || !m_IL || !m_DepthState) return;

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();

    // ─── 2D正射影行列の設定 ───
    XMMATRIX oldView = Renderer::GetViewMatrix();
    XMMATRIX oldProj = Renderer::GetProjectionMatrix();

    XMMATRIX view2D = XMMatrixIdentity();
    XMMATRIX proj2D = XMMatrixOrthographicOffCenterLH(0.0f, screenW, screenH, 0.0f, 0.0f, 1.0f);

    // 1. スコア/タイトル/リザルトHUDの描画
    if (texToDraw) {
        DrawQuad2D(texToDraw, posX + m_ShakeOffset.x, posY + m_ShakeOffset.y, w, h, view2D, proj2D);
    }

    // 2. プレイヤーHPバーの描画（ゲームプレイ中のみ、画面左上）
    if (currentScene == Scene::GAMEPLAY && m_PlayerHPTexture) {
        DrawQuad2D(
            m_PlayerHPTexture.Get(),
            Constants::UI::PlayerHP::SCREEN_POS_X + m_ShakeOffset.x,
            Constants::UI::PlayerHP::SCREEN_POS_Y + m_ShakeOffset.y,
            Constants::UI::PlayerHP::BAR_WIDTH,
            Constants::UI::PlayerHP::BAR_HEIGHT,
            view2D, proj2D);
    }

    // 3. ボスHPバーの別途描画（ボスステージ中のみ、画面下部中央）
    if (currentScene == Scene::GAMEPLAY && m_BossHPTexture) {
        const float bossBarW = Constants::UI::BossHP::BAR_WIDTH;
        const float bossBarH = Constants::UI::BossHP::BAR_HEIGHT;
        const float bossBarX = screenW * 0.5f + m_ShakeOffset.x;
        const float bossBarY = screenH - bossBarH * 0.5f - Constants::UI::BossHP::SCREEN_MARGIN_Y + m_ShakeOffset.y;
        DrawQuad2D(m_BossHPTexture.Get(), bossBarX, bossBarY, bossBarW, bossBarH, view2D, proj2D);
    }

    // 4. 投擲チャージ段階ゲージ＋非回転投げクールダウン（画面右下）
    if (currentScene == Scene::GAMEPLAY || currentScene == Scene::TUTORIAL || currentScene == Scene::HUB) {
        DrawChargeGauge(screenW, screenH, view2D, proj2D);
    }

    // 後続の3D描画（シーンやオブジェクト）への影響を防ぐため、深度ステートを有効状態に復元する
    ctx->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
    Renderer::SetViewMatrix(oldView);
    Renderer::SetProjectionMatrix(oldProj);
}

void ScoreHUD::DrawQuad2D(
    ID3D11ShaderResourceView* srv,
    float posX,
    float posY,
    float width,
    float height,
    const DirectX::XMMATRIX& view2D,
    const DirectX::XMMATRIX& proj2D,
    const DirectX::XMFLOAT4& tint
)
{
    if (!srv) return;
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();

    ctx->OMSetDepthStencilState(m_DepthState.Get(), 0);
    ctx->VSSetShader(m_VS.Get(), nullptr, 0);
    ctx->PSSetShader(m_PS.Get(), nullptr, 0);
    ctx->IASetInputLayout(m_IL.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    UINT stride = 60u;
    UINT offset = 0;
    ID3D11Buffer* vb = m_QuadVB.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);

    Renderer::SetViewMatrix(view2D);
    Renderer::SetProjectionMatrix(proj2D);

    XMMATRIX world = XMMatrixScaling(width, height, 1.0f) * XMMatrixTranslation(posX, posY, 0.0f);
    Renderer::SetWorldMatrix(world);

    MATERIAL mat = {};
    mat.Diffuse       = tint;
    mat.Emission      = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    mat.TextureEnable = TRUE;
    Renderer::SetMaterial(mat);

    Renderer::SetTexture(srv);
    ctx->Draw(4, 0);

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// ─────────────────────────────────────────────────────────────────
// 投擲チャージ段階ゲージ（4分割）と非回転投げクールダウンゲージ
// ─────────────────────────────────────────────────────────────────
void ScoreHUD::DrawChargeGauge(
    float screenW,
    float screenH,
    const DirectX::XMMATRIX& view2D,
    const DirectX::XMMATRIX& proj2D
)
{
    Player* player = Manager::GetGameObject<Player>();
    if (!player || !m_WhiteTexture || !player->GetCombatModule()) return;

    PlayerState state = player->GetState();
    int cooldown = player->GetCombatModule()->GetNonSpinThrowCooldown();
    bool holding = (state == PlayerState::GRABBED || state == PlayerState::SPINNING);
    if (!holding && cooldown <= 0) return; // 掴み中かクールダウン中のみ表示

    const float segW = 44.0f, segH = 14.0f, gap = 4.0f;
    const float totalW = segW * 4 + gap * 3;
    const float left = screenW - 40.0f - totalW + m_ShakeOffset.x;
    const float baseY = screenH - 70.0f + m_ShakeOffset.y;

    int stage = player->GetCombatModule()->GetChargeStage();
    float ratio = player->GetCurrentSpinSpeed() / Constants::Player::MAX_SPIN_SPEED;

    if (holding) {
        for (int i = 0; i < 4; ++i) {
            float x = left + i * (segW + gap) + segW * 0.5f;
            // 枠（暗色）
            DrawQuad2D(m_WhiteTexture.Get(), x, baseY, segW, segH, view2D, proj2D, XMFLOAT4(0.08f, 0.09f, 0.12f, 0.85f));

            float fill = 0.0f;
            if (i < stage) {
                fill = 1.0f;
            } else if (i == stage && state == PlayerState::SPINNING) {
                // 次の段階までの進捗
                float lo = (i == 0) ? 0.0f : Constants::Player::SPIN_STAGE_THRESHOLD[i - 1];
                float hi = Constants::Player::SPIN_STAGE_THRESHOLD[i];
                fill = (ratio - lo) / (hi - lo);
                if (fill < 0.0f) fill = 0.0f;
                if (fill > 1.0f) fill = 1.0f;
            }
            if (fill > 0.0f) {
                XMFLOAT3 c = AuraEffect::StageColor(i + 1);
                float m = (std::max)((std::max)(c.x, c.y), c.z);
                float inv = (m > 1.0f) ? 1.0f / m : 1.0f;
                float a = (i < stage) ? 1.0f : 0.55f;
                DrawQuad2D(m_WhiteTexture.Get(), x - segW * 0.5f * (1.0f - fill), baseY, (segW - 4.0f) * fill, segH - 4.0f, view2D, proj2D,
                           XMFLOAT4(c.x * inv, c.y * inv, c.z * inv, a));
            }
        }
    }

    // 非回転投げクールダウン（ゲージ＝再使用可能までの回復量）
    if (cooldown > 0 || holding) {
        float ready = 1.0f - (float)cooldown / (float)Constants::Player::NONSPIN_THROW_COOLDOWN;
        if (ready < 0.0f) ready = 0.0f;
        if (ready > 1.0f) ready = 1.0f;
        const float barH = 8.0f;
        const float y = baseY + segH * 0.5f + barH * 0.5f + 6.0f;
        DrawQuad2D(m_WhiteTexture.Get(), left + totalW * 0.5f, y, totalW, barH, view2D, proj2D, XMFLOAT4(0.08f, 0.09f, 0.12f, 0.85f));
        XMFLOAT4 col = (cooldown > 0) ? XMFLOAT4(1.0f, 0.45f, 0.1f, 1.0f) : XMFLOAT4(0.2f, 0.9f, 1.0f, 1.0f);
        DrawQuad2D(m_WhiteTexture.Get(), left + totalW * 0.5f - totalW * 0.5f * (1.0f - ready), y, (totalW - 4.0f) * ready, barH - 4.0f, view2D, proj2D, col);
    }
}
