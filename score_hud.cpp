#include "score_hud.h"
#include "game_constants.h"
#include "renderer.h"
#include "game_rule.h"
#include "player.h"
#include "manager.h"
#include "boss_enemy.h"
#include <vector>

// =================================================================
// 静的メンバ変数の実体定義
// =================================================================
ID3D11Buffer*            ScoreHUD::m_QuadVB     = nullptr;
ID3D11VertexShader*      ScoreHUD::m_VS         = nullptr;
ID3D11PixelShader*       ScoreHUD::m_PS         = nullptr;
ID3D11InputLayout*       ScoreHUD::m_IL         = nullptr;
ID3D11DepthStencilState* ScoreHUD::m_DepthState = nullptr;
ID3D11ShaderResourceView* ScoreHUD::m_Texture    = nullptr;
ID3D11ShaderResourceView* ScoreHUD::m_TitleTexture    = nullptr;
ID3D11ShaderResourceView* ScoreHUD::m_ClearTexture    = nullptr;
ID3D11ShaderResourceView* ScoreHUD::m_GameOverTexture = nullptr;
ID3D11ShaderResourceView* ScoreHUD::m_BossHPTexture   = nullptr;
ID3D11ShaderResourceView* ScoreHUD::m_PlayerHPTexture = nullptr;

int   ScoreHUD::m_LastScore   = 0;
int   ScoreHUD::m_LastHP      = 5;
int   ScoreHUD::m_LastMaxHP   = 5;
int   ScoreHUD::m_LastBossHP    = -1;
int   ScoreHUD::m_LastBossMaxHP = -1;
float ScoreHUD::m_ScaleEffect = 1.0f;

// ─────────────────────────────────────────────────────────────────
// GDI を使用してトータルスコアHUD用のテクスチャを生成する
// テキスト部分がアルファ（輝度）として抽出され、ui_ps.hlsl で発光描画される
// ─────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* ScoreHUD::CreateHUDTexture(
    ID3D11Device* device,
    int score
)
{
    const int W = 512, H = 64;

    // ─── GDI DIB セクションの作成 ───
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;  // トップダウン形式（上から下）
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;  // BGRA 32bit
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* bits = nullptr;
    HDC hdc     = CreateCompatibleDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);

    // 背景を黒（アルファ=0用）でクリア
    memset(bits, 0, W * H * 4);

    // ─── フォントの作成 ───
    HFONT hFont = CreateFontW(
        46, 0, 0, 0, FW_HEAVY,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Impact"
    );
    HGDIOBJ oldFont = SelectObject(hdc, hFont);

    // ─── テキストの描画 ───
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255)); // 白色で描画

    wchar_t buf[64];
    swprintf_s(buf, L"SCORE: %06d", score);

    RECT rc = { 0, 0, W, H };
    DrawTextW(hdc, buf, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    GdiFlush();

    // ─── アルファ値への変換処理 ───
    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];
        BYTE a = (BYTE)(((int)r + (int)g + (int)b) / 3); // 輝度をアルファにする
        rgba[i * 4 + 0] = 255; // R: 白（Emission で色を変えるため白）
        rgba[i * 4 + 1] = 255; // G
        rgba[i * 4 + 2] = 255; // B
        rgba[i * 4 + 3] = a;   // A: テキストの形
    }

    // ─── D3D11 テクスチャの生成 ───
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

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device->CreateTexture2D(&td, &initData, &tex);

    ID3D11ShaderResourceView* srv = nullptr;
    if (SUCCEEDED(hr) && tex) {
        device->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();
    }

    // GDI リソースの解放
    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBmp);
    DeleteObject(hFont);
    DeleteObject(hBmp);
    DeleteDC(hdc);

    return srv;
}

// ─────────────────────────────────────────────────────────────────
// GDI を使用してボスHPバー用のテクスチャを生成する
// 画面下部に魔王名 + カラーゲージを描画
// ─────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* ScoreHUD::CreateBossHPTexture(
    ID3D11Device* device,
    int hp,
    int maxHp
)
{
    const int W = static_cast<int>(Constants::UI::BossHP::BAR_WIDTH);
    const int H = static_cast<int>(Constants::UI::BossHP::BAR_HEIGHT);

    // ─── GDI DIB セクションの作成 ───
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;   // トップダウン
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* bits = nullptr;
    HDC hdc     = CreateCompatibleDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);
    memset(bits, 0, W * H * 4);

    // ─── 輝度バックグラウンド（半透明の深灰） ───
    HBRUSH bgBrush = CreateSolidBrush(RGB(Constants::UI::BossHP::PANEL_BG.R, Constants::UI::BossHP::PANEL_BG.G, Constants::UI::BossHP::PANEL_BG.B));
    RECT bgRect = { 0, 0, W, H };
    FillRect(hdc, &bgRect, bgBrush);
    DeleteObject(bgBrush);

    // ─── "BOSS" ラベルテキスト ───
    HFONT hFontLabel = CreateFontW(
        28, 0, 0, 0, FW_HEAVY,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Impact"
    );
    HGDIOBJ oldFont = SelectObject(hdc, hFontLabel);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(Constants::UI::BossHP::TEXT_LABEL.R, Constants::UI::BossHP::TEXT_LABEL.G, Constants::UI::BossHP::TEXT_LABEL.B));
    RECT rcLabel = { 10, 8, 100, 38 };
    DrawTextW(hdc, L"BOSS", -1, &rcLabel, DT_LEFT | DT_SINGLELINE);

    // ─── HP 数値テキスト ───
    HFONT hFontHP = CreateFontW(
        22, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial"
    );
    SelectObject(hdc, hFontHP);
    SetTextColor(hdc, RGB(Constants::UI::BossHP::TEXT_HP.R, Constants::UI::BossHP::TEXT_HP.G, Constants::UI::BossHP::TEXT_HP.B));
    wchar_t hpBuf[32];
    swprintf_s(hpBuf, L"%d / %d", hp, maxHp);
    RECT rcHP = { W - 120, 8, W - 10, 38 };
    DrawTextW(hdc, hpBuf, -1, &rcHP, DT_RIGHT | DT_SINGLELINE);

    // ─── HPゲージ暀外枕（ダークグレー） ───
    const int barLeft   = 10;
    const int barTop    = 44;
    const int barRight  = W - 10;
    const int barBottom = 70;
    HBRUSH barBgBrush = CreateSolidBrush(RGB(Constants::UI::BossHP::BAR_BG.R, Constants::UI::BossHP::BAR_BG.G, Constants::UI::BossHP::BAR_BG.B));
    RECT barBgRect = { barLeft, barTop, barRight, barBottom };
    FillRect(hdc, &barBgRect, barBgBrush);
    DeleteObject(barBgBrush);

    // ─── HPゲージ内側（赤から黄のグラデーション） ───
    if (maxHp > 0 && hp > 0) {
        float ratio = (float)hp / (float)maxHp;
        int fillRight = barLeft + (int)((barRight - barLeft) * ratio);

        // HP比率に応じて色を変化（高HP: 緑、中間: 黄、低HP: 赤）
        int rCol = 0;
        int gCol = 0;
        int bCol = 0;
        if (ratio > 0.5f) {
            // 緑から黄への変化
            float t = (ratio - 0.5f) * 2.0f; // 0.0(黄) から 1.0(緑)
            rCol = (int)(Constants::UI::BossHP::HP_MID.R * (1.0f - t) + Constants::UI::BossHP::HP_HIGH.R * t);
            gCol = (int)(Constants::UI::BossHP::HP_MID.G * (1.0f - t) + Constants::UI::BossHP::HP_HIGH.G * t);
            bCol = (int)(Constants::UI::BossHP::HP_MID.B * (1.0f - t) + Constants::UI::BossHP::HP_HIGH.B * t);
        } else {
            // 黄から赤への変化
            float t = ratio * 2.0f; // 0.0(赤) から 1.0(黄)
            rCol = (int)(Constants::UI::BossHP::HP_LOW.R * (1.0f - t) + Constants::UI::BossHP::HP_MID.R * t);
            gCol = (int)(Constants::UI::BossHP::HP_LOW.G * (1.0f - t) + Constants::UI::BossHP::HP_MID.G * t);
            bCol = (int)(Constants::UI::BossHP::HP_LOW.B * (1.0f - t) + Constants::UI::BossHP::HP_MID.B * t);
        }
        rCol = (rCol < 0) ? 0 : (rCol > 255 ? 255 : rCol);
        gCol = (gCol < 0) ? 0 : (gCol > 255 ? 255 : gCol);
        bCol = (bCol < 0) ? 0 : (bCol > 255 ? 255 : bCol);

        HBRUSH hpBrush = CreateSolidBrush(RGB(rCol, gCol, bCol));
        RECT fillRect = { barLeft, barTop, fillRight, barBottom };
        FillRect(hdc, &fillRect, hpBrush);
        DeleteObject(hpBrush);

        // ハイライト（ゲージ上部の白い親線）
        HBRUSH hlBrush = CreateSolidBrush(RGB(255, 220, 220));
        RECT hlRect = { barLeft, barTop, fillRight, barTop + 4 };
        FillRect(hdc, &hlRect, hlBrush);
        DeleteObject(hlBrush);
    }

    // ─── ゲージ枚線 ───
    HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(Constants::UI::BossHP::BAR_BORDER.R, Constants::UI::BossHP::BAR_BORDER.G, Constants::UI::BossHP::BAR_BORDER.B));
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HGDIOBJ oldBrush = SelectObject(hdc, nullBrush);
    Rectangle(hdc, barLeft, barTop, barRight, barBottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);

    GdiFlush();

    // ─── アルファを踏まえた RGBA 変換 ───
    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];
        
        // 色が完全に黒 (RGB: 0, 0, 0) の部分のみ透過させ、それ以外は完全に不透明にする
        // これにより、GDIで塗った背景パネルやゲージの色が透過して消えてしまうのを防ぎます
        BYTE alpha = (r == 0 && g == 0 && b == 0) ? 0 : 255;
        
        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b;
        rgba[i * 4 + 3] = alpha;
    }

    // ─── D3D11 テクスチャの生成 ───
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

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device->CreateTexture2D(&td, &initData, &tex);

    ID3D11ShaderResourceView* srv = nullptr;
    if (SUCCEEDED(hr) && tex) {
        device->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();
    }

    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBmp);
    DeleteObject(hFontLabel);
    DeleteObject(hFontHP);
    DeleteObject(hBmp);
    DeleteDC(hdc);

    return srv;
}

// ─────────────────────────────────────────────────────────────────
// GDI を使用してプレイヤーHPバー用のテクスチャを生成する
// 画面上部にプレイヤー名/ステータス + カラーゲージを描画
// ─────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* ScoreHUD::CreatePlayerHPTexture(
    ID3D11Device* device,
    int hp,
    int maxHp
)
{
    const int W = static_cast<int>(Constants::UI::PlayerHP::BAR_WIDTH);
    const int H = static_cast<int>(Constants::UI::PlayerHP::BAR_HEIGHT);

    // ─── GDI DIB セクションの作成 ───
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;   // トップダウン
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* bits = nullptr;
    HDC hdc     = CreateCompatibleDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);
    memset(bits, 0, W * H * 4);

    // ─── 輝度バックグラウンド（深青色の半透明パネル） ───
    HBRUSH bgBrush = CreateSolidBrush(RGB(Constants::UI::PlayerHP::PANEL_BG.R, Constants::UI::PlayerHP::PANEL_BG.G, Constants::UI::PlayerHP::PANEL_BG.B));
    RECT bgRect = { 0, 0, W, H };
    FillRect(hdc, &bgRect, bgBrush);
    DeleteObject(bgBrush);

    // ─── "PLAYER" ラベルテキスト ───
    HFONT hFontLabel = CreateFontW(
        22, 0, 0, 0, FW_HEAVY,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Impact"
    );
    HGDIOBJ oldFont = SelectObject(hdc, hFontLabel);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(Constants::UI::PlayerHP::TEXT_LABEL.R, Constants::UI::PlayerHP::TEXT_LABEL.G, Constants::UI::PlayerHP::TEXT_LABEL.B));
    RECT rcLabel = { 8, 4, 120, 26 };
    DrawTextW(hdc, L"PLAYER", -1, &rcLabel, DT_LEFT | DT_SINGLELINE);

    // ─── HP 数値テキスト ───
    HFONT hFontHP = CreateFontW(
        18, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial"
    );
    SelectObject(hdc, hFontHP);
    SetTextColor(hdc, RGB(Constants::UI::PlayerHP::TEXT_HP.R, Constants::UI::PlayerHP::TEXT_HP.G, Constants::UI::PlayerHP::TEXT_HP.B));
    wchar_t hpBuf[32];
    swprintf_s(hpBuf, L"%d / %d", hp, maxHp);
    RECT rcHP = { W - 110, 4, W - 8, 26 };
    DrawTextW(hdc, hpBuf, -1, &rcHP, DT_RIGHT | DT_SINGLELINE);

    // ─── HPゲージ背景（ダークブルー） ───
    const int barLeft   = 8;
    const int barTop    = 28;
    const int barRight  = W - 8;
    const int barBottom = H - 6;
    HBRUSH barBgBrush = CreateSolidBrush(RGB(Constants::UI::PlayerHP::BAR_BG.R, Constants::UI::PlayerHP::BAR_BG.G, Constants::UI::PlayerHP::BAR_BG.B));
    RECT barBgRect = { barLeft, barTop, barRight, barBottom };
    FillRect(hdc, &barBgRect, barBgBrush);
    DeleteObject(barBgBrush);

    // ─── HPゲージ内側（高HP: シアン、中HP: 黄、低HP: 赤） ───
    if (maxHp > 0 && hp > 0) {
        float ratio = (float)hp / (float)maxHp;
        if (ratio > 1.0f) ratio = 1.0f;
        int fillRight = barLeft + (int)((barRight - barLeft) * ratio);

        int rCol = 0, gCol = 0, bCol = 0;
        if (ratio > 0.5f) {
            float t = (ratio - 0.5f) * 2.0f;
            rCol = (int)(Constants::UI::PlayerHP::HP_MID.R * (1.0f - t) + Constants::UI::PlayerHP::HP_HIGH.R * t);
            gCol = (int)(Constants::UI::PlayerHP::HP_MID.G * (1.0f - t) + Constants::UI::PlayerHP::HP_HIGH.G * t);
            bCol = (int)(Constants::UI::PlayerHP::HP_MID.B * (1.0f - t) + Constants::UI::PlayerHP::HP_HIGH.B * t);
        } else {
            float t = ratio * 2.0f;
            rCol = (int)(Constants::UI::PlayerHP::HP_LOW.R * (1.0f - t) + Constants::UI::PlayerHP::HP_MID.R * t);
            gCol = (int)(Constants::UI::PlayerHP::HP_LOW.G * (1.0f - t) + Constants::UI::PlayerHP::HP_MID.G * t);
            bCol = (int)(Constants::UI::PlayerHP::HP_LOW.B * (1.0f - t) + Constants::UI::PlayerHP::HP_MID.B * t);
        }
        rCol = (rCol < 0) ? 0 : (rCol > 255 ? 255 : rCol);
        gCol = (gCol < 0) ? 0 : (gCol > 255 ? 255 : gCol);
        bCol = (bCol < 0) ? 0 : (bCol > 255 ? 255 : bCol);

        HBRUSH hpBrush = CreateSolidBrush(RGB(rCol, gCol, bCol));
        RECT fillRect = { barLeft, barTop, fillRight, barBottom };
        FillRect(hdc, &fillRect, hpBrush);
        DeleteObject(hpBrush);

        // 上部のホワイト/ライトシアンハイライト線
        HBRUSH hlBrush = CreateSolidBrush(RGB(220, 245, 255));
        RECT hlRect = { barLeft, barTop, fillRight, barTop + 3 };
        FillRect(hdc, &hlRect, hlBrush);
        DeleteObject(hlBrush);
    }

    // ─── ゲージ枠線 ───
    HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(Constants::UI::PlayerHP::BAR_BORDER.R, Constants::UI::PlayerHP::BAR_BORDER.G, Constants::UI::PlayerHP::BAR_BORDER.B));
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HGDIOBJ oldBrush = SelectObject(hdc, nullBrush);
    Rectangle(hdc, barLeft, barTop, barRight, barBottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);

    GdiFlush();

    // ─── アルファ変換 ───
    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];
        BYTE alpha = (r == 0 && g == 0 && b == 0) ? 0 : 255;
        
        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b;
        rgba[i * 4 + 3] = alpha;
    }

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

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device->CreateTexture2D(&td, &initData, &tex);

    ID3D11ShaderResourceView* srv = nullptr;
    if (SUCCEEDED(hr) && tex) {
        device->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();
    }

    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBmp);
    DeleteObject(hFontLabel);
    DeleteObject(hFontHP);
    DeleteObject(hBmp);
    DeleteDC(hdc);

    return srv;
}

// ─────────────────────────────────────────────────────────────────
// GDI を使用してタイトル画面用のテクスチャを生成する
// ─────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* ScoreHUD::CreateTitleTexture(ID3D11Device* device)
{
    const int W = 512, H = 256;

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

    HFONT hFontTitle = CreateFontW(
        64, 0, 0, 0, FW_HEAVY,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Impact"
    );
    HFONT hFontDesc = CreateFontW(
        22, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial"
    );

    SetBkMode(hdc, TRANSPARENT);

    // 1. タイトルを描画
    HGDIOBJ oldFont = SelectObject(hdc, hFontTitle);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT rcTitle = { 0, 20, W, 100 };
    DrawTextW(hdc, L"TOON SLASHER", -1, &rcTitle, DT_CENTER | DT_SINGLELINE);

    // 2. 説明を描画
    SelectObject(hdc, hFontDesc);
    SetTextColor(hdc, RGB(200, 200, 200));
    RECT rcDesc = { 0, 120, W, 250 };
    DrawTextW(hdc, L"PRESS SPACE TO START\n\n[Controls]\nMove: WASD / Jump: Space\nGrab/Throw: Left Click / Spin: Right Click", -1, &rcDesc, DT_CENTER);

    GdiFlush();

    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];
        BYTE a = (BYTE)(((int)r + (int)g + (int)b) / 3);
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = a;
    }

    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBmp);
    DeleteObject(hFontTitle);
    DeleteObject(hFontDesc);
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

    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    if (SUCCEEDED(device->CreateTexture2D(&td, &initData, &tex))) {
        device->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();
    }
    return srv;
}

// ─────────────────────────────────────────────────────────────────
// GDI を使用してリザルト画面用のテクスチャを生成する
// ─────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* ScoreHUD::CreateResultTexture(ID3D11Device* device, bool isClear, int score)
{
    const int W = 512, H = 256;

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

    HFONT hFontResult = CreateFontW(
        64, 0, 0, 0, FW_HEAVY,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Impact"
    );
    HFONT hFontDesc = CreateFontW(
        24, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial"
    );

    SetBkMode(hdc, TRANSPARENT);

    // 1. タイトルを描画
    HGDIOBJ oldFont = SelectObject(hdc, hFontResult);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT rcTitle = { 0, 20, W, 100 };
    if (isClear) {
        DrawTextW(hdc, L"GAME CLEAR", -1, &rcTitle, DT_CENTER | DT_SINGLELINE);
    } else {
        DrawTextW(hdc, L"GAME OVER", -1, &rcTitle, DT_CENTER | DT_SINGLELINE);
    }

    // 2. スコアとタイトル戻る案内を描画
    SelectObject(hdc, hFontDesc);
    SetTextColor(hdc, RGB(200, 200, 200));
    wchar_t buf[128];
    if (isClear) {
        swprintf_s(buf, L"FINAL SCORE: %06d\n\nPRESS ENTER TO RETURN TITLE", score);
    } else {
        swprintf_s(buf, L"PRESS ENTER TO RETURN TITLE");
    }
    RECT rcDesc = { 0, 130, W, 250 };
    DrawTextW(hdc, buf, -1, &rcDesc, DT_CENTER);

    GdiFlush();

    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];
        BYTE a = (BYTE)(((int)r + (int)g + (int)b) / 3);
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = a;
    }

    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBmp);
    DeleteObject(hFontResult);
    DeleteObject(hFontDesc);
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

    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    if (SUCCEEDED(device->CreateTexture2D(&td, &initData, &tex))) {
        device->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();
    }
    return srv;
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
    if (FAILED(device->CreateBuffer(&bd, &sd, &m_QuadVB))) return false;

    // ─── シェーダーファイルの読み込み（2-1 対応: Renderer::ResolveShaderPath でパス解決を一元化）───
    Renderer::CreateVertexShader(&m_VS, &m_IL, Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(&m_PS, Renderer::ResolveShaderPath("ui_ps.cso").c_str());

    if (!m_VS || !m_PS || !m_IL) {
        OutputDebugStringA("[ScoreHUD] シェーダーの読み込みに失敗しました\n");
        return false;
    }

    // ─── 深度ステートの作成（テストOFF / 書き込みOFF）───
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = FALSE; // 常に最前面に描画
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc      = D3D11_COMPARISON_ALWAYS;
    if (FAILED(device->CreateDepthStencilState(&dsd, &m_DepthState))) return false;

    // ─── 初期テクスチャ（スコア0、HP5）の生成 ───
    m_LastScore = GameRule::GetScore();
    m_LastHP = 5;
    m_LastMaxHP = 5;
    Player* player = Manager::GetGameObject<Player>();
    if (player) {
        m_LastHP    = player->GetHP();
        m_LastMaxHP = player->GetMaxHP();
    }
    m_Texture         = CreateHUDTexture(device, m_LastScore);
    m_PlayerHPTexture = CreatePlayerHPTexture(device, m_LastHP, m_LastMaxHP);
    m_ScaleEffect = 1.0f;

    OutputDebugStringA("[ScoreHUD] 初期化が正常に完了しました\n");
    return true;
}

// ─────────────────────────────────────────────────────────────────
// 終了処理
// ─────────────────────────────────────────────────────────────────
void ScoreHUD::Uninit()
{
    if (m_Texture)    { m_Texture->Release();    m_Texture    = nullptr; }
    if (m_TitleTexture) { m_TitleTexture->Release(); m_TitleTexture = nullptr; }
    if (m_ClearTexture) { m_ClearTexture->Release(); m_ClearTexture = nullptr; }
    if (m_GameOverTexture) { m_GameOverTexture->Release(); m_GameOverTexture = nullptr; }
    if (m_BossHPTexture)   { m_BossHPTexture->Release();   m_BossHPTexture   = nullptr; }
    if (m_PlayerHPTexture) { m_PlayerHPTexture->Release(); m_PlayerHPTexture = nullptr; }
    if (m_DepthState) { m_DepthState->Release(); m_DepthState = nullptr; }
    if (m_IL)         { m_IL->Release();         m_IL         = nullptr; }
    if (m_PS)         { m_PS->Release();         m_PS         = nullptr; }
    if (m_VS)         { m_VS->Release();         m_VS         = nullptr; }
    if (m_QuadVB)     { m_QuadVB->Release();     m_QuadVB     = nullptr; }
}

// ─────────────────────────────────────────────────────────────────
// 更新処理
// ─────────────────────────────────────────────────────────────────
void ScoreHUD::Update()
{
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
            if (m_PlayerHPTexture) {
                m_PlayerHPTexture->Release();
                m_PlayerHPTexture = nullptr;
            }
            m_PlayerHPTexture = CreatePlayerHPTexture(Renderer::GetDevice(), curPlayerHP, curPlayerMaxHP);
            m_LastHP    = curPlayerHP;
            m_LastMaxHP = curPlayerMaxHP;
        }

        // スコア変化の監視
        int currentScore = GameRule::GetScore();
        if (currentScore != m_LastScore || !m_Texture) {
            if (m_Texture) {
                m_Texture->Release();
                m_Texture = nullptr;
            }
            m_Texture = CreateHUDTexture(Renderer::GetDevice(), currentScore);
            m_LastScore = currentScore;

            // スケールポップ効果を発動
            m_ScaleEffect = 1.25f;
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
                    if (m_BossHPTexture) { m_BossHPTexture->Release(); m_BossHPTexture = nullptr; }
                    m_BossHPTexture   = CreateBossHPTexture(Renderer::GetDevice(), curBossHP, curBossMaxHP);
                    m_LastBossHP      = curBossHP;
                    m_LastBossMaxHP   = curBossMaxHP;
                }
            } else {
                // ボスが倒された・存在しない場合はバーを消す
                if (m_BossHPTexture) { m_BossHPTexture->Release(); m_BossHPTexture = nullptr; }
            }
        } else {
            if (m_BossHPTexture) { m_BossHPTexture->Release(); m_BossHPTexture = nullptr; }
        }
    } else if (currentScene == Scene::CLEAR) {
        if (!m_ClearTexture) {
            m_ClearTexture = CreateResultTexture(Renderer::GetDevice(), true, GameRule::GetScore());
        }
    } else if (currentScene == Scene::GAMEOVER) {
        if (!m_GameOverTexture) {
            m_GameOverTexture = CreateResultTexture(Renderer::GetDevice(), false, GameRule::GetScore());
        }
    }

    // 不要なシーンテクスチャを解放
    if (currentScene != Scene::TITLE && m_TitleTexture) {
        m_TitleTexture->Release();
        m_TitleTexture = nullptr;
    }
    if (currentScene != Scene::CLEAR && m_ClearTexture) {
        m_ClearTexture->Release();
        m_ClearTexture = nullptr;
    }
    if (currentScene != Scene::GAMEOVER && m_GameOverTexture) {
        m_GameOverTexture->Release();
        m_GameOverTexture = nullptr;
    }
    if (currentScene != Scene::GAMEPLAY) {
        if (m_Texture) {
            m_Texture->Release();
            m_Texture = nullptr;
        }
        if (m_PlayerHPTexture) {
            m_PlayerHPTexture->Release();
            m_PlayerHPTexture = nullptr;
        }
    }
}

// ─────────────────────────────────────────────────────────────────
// 描画処理（Renderer::End() の直前に呼び出す）
// ─────────────────────────────────────────────────────────────────
void ScoreHUD::Draw()
{
    ID3D11ShaderResourceView* texToDraw = nullptr;
    Scene currentScene = Manager::GetCurrentScene();

    float w = 512.0f;
    float h = 64.0f;
    float posX = (float)SCREEN_WIDTH * 0.5f; // 画面中央
    float posY = 45.0f;                      // 画面上部から少し下げた位置
    XMFLOAT4 emissionColor = XMFLOAT4(1.5f, 1.2f, 0.0f, 0.0f); // スコアHUDは黄金色の発光

    switch (currentScene) {
    case Scene::TITLE:
        texToDraw = m_TitleTexture;
        w = 512.0f;
        h = 256.0f;
        posY = (float)SCREEN_HEIGHT * 0.5f; // 画面中央
        emissionColor = XMFLOAT4(0.0f, 1.2f, 1.8f, 0.0f); // タイトルは青白発光
        break;
    case Scene::GAMEPLAY:
        texToDraw = m_Texture;
        w = 512.0f * m_ScaleEffect;
        h = 64.0f * m_ScaleEffect;
        posY = 45.0f;
        break;
    case Scene::CLEAR:
        texToDraw = m_ClearTexture;
        w = 512.0f;
        h = 256.0f;
        posY = (float)SCREEN_HEIGHT * 0.5f;
        emissionColor = XMFLOAT4(1.5f, 1.2f, 0.0f, 0.0f); // クリアは黄金発光
        break;
    case Scene::GAMEOVER:
        texToDraw = m_GameOverTexture;
        w = 512.0f;
        h = 256.0f;
        posY = (float)SCREEN_HEIGHT * 0.5f;
        emissionColor = XMFLOAT4(1.8f, 0.0f, 0.0f, 0.0f); // ゲームオーバーは赤発光
        break;
    }

    if (!m_QuadVB || !m_VS || !m_PS || !m_IL || !m_DepthState) return;

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();

    // ─── 2D正射影行列の設定 ───
    XMMATRIX oldView = Renderer::GetViewMatrix();
    XMMATRIX oldProj = Renderer::GetProjectionMatrix();

    XMMATRIX view2D = XMMatrixIdentity();
    XMMATRIX proj2D = XMMatrixOrthographicOffCenterLH(0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);

    // 1. スコア/タイトル/リザルトHUDの描画
    if (texToDraw) {
        ctx->OMSetDepthStencilState(m_DepthState, 0);
        ctx->VSSetShader(m_VS, nullptr, 0);
        ctx->PSSetShader(m_PS, nullptr, 0);
        ctx->IASetInputLayout(m_IL);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        UINT stride = 60u;
        UINT offset = 0;
        ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &stride, &offset);

        Renderer::SetViewMatrix(view2D);
        Renderer::SetProjectionMatrix(proj2D);

        XMMATRIX world = XMMatrixScaling(w, h, 1.0f) * XMMatrixTranslation(posX, posY, 0.0f);
        Renderer::SetWorldMatrix(world);

        MATERIAL mat = {};
        mat.Diffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        mat.Emission = emissionColor;
        mat.TextureEnable = FALSE;
        Renderer::SetMaterial(mat);

        Renderer::SetTexture(texToDraw);
        ctx->Draw(4, 0);

        Renderer::SetViewMatrix(oldView);
        Renderer::SetProjectionMatrix(oldProj);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    // 2. プレイヤーHPバーの描画（ゲームプレイ中のみ、画面左上）
    if (currentScene == Scene::GAMEPLAY && m_PlayerHPTexture) {
        const float pBarW = Constants::UI::PlayerHP::BAR_WIDTH;
        const float pBarH = Constants::UI::PlayerHP::BAR_HEIGHT;
        const float pBarX = Constants::UI::PlayerHP::SCREEN_POS_X;
        const float pBarY = Constants::UI::PlayerHP::SCREEN_POS_Y;

        ctx->OMSetDepthStencilState(m_DepthState, 0);
        ctx->VSSetShader(m_VS, nullptr, 0);
        ctx->PSSetShader(m_PS, nullptr, 0);
        ctx->IASetInputLayout(m_IL);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        UINT strideP = 60u;
        UINT offsetP = 0;
        ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &strideP, &offsetP);

        Renderer::SetViewMatrix(view2D);
        Renderer::SetProjectionMatrix(proj2D);

        XMMATRIX worldPlayer = XMMatrixScaling(pBarW, pBarH, 1.0f) * XMMatrixTranslation(pBarX, pBarY, 0.0f);
        Renderer::SetWorldMatrix(worldPlayer);

        MATERIAL matPlayer = {};
        matPlayer.Diffuse       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        matPlayer.Emission      = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        matPlayer.TextureEnable = TRUE;
        Renderer::SetMaterial(matPlayer);
        Renderer::SetTexture(m_PlayerHPTexture);
        ctx->Draw(4, 0);

        Renderer::SetViewMatrix(oldView);
        Renderer::SetProjectionMatrix(oldProj);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    // 3. ボスHPバーの別途描画（ボスステージ中のみ、画面下部中央）
    if (currentScene == Scene::GAMEPLAY && m_BossHPTexture) {
        const float bossBarW = Constants::UI::BossHP::BAR_WIDTH;
        const float bossBarH = Constants::UI::BossHP::BAR_HEIGHT;
        const float bossBarX = (float)SCREEN_WIDTH * 0.5f;
        const float bossBarY = (float)SCREEN_HEIGHT - bossBarH * 0.5f - Constants::UI::BossHP::SCREEN_MARGIN_Y;

        ctx->OMSetDepthStencilState(m_DepthState, 0);
        ctx->VSSetShader(m_VS, nullptr, 0);
        ctx->PSSetShader(m_PS, nullptr, 0);
        ctx->IASetInputLayout(m_IL);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        UINT stride2 = 60u;
        UINT offset2 = 0;
        ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &stride2, &offset2);

        Renderer::SetViewMatrix(view2D);
        Renderer::SetProjectionMatrix(proj2D);

        XMMATRIX worldBoss = XMMatrixScaling(bossBarW, bossBarH, 1.0f) * XMMatrixTranslation(bossBarX, bossBarY, 0.0f);
        Renderer::SetWorldMatrix(worldBoss);

        MATERIAL matBoss = {};
        matBoss.Diffuse       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        matBoss.Emission      = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        matBoss.TextureEnable = TRUE;
        Renderer::SetMaterial(matBoss);
        Renderer::SetTexture(m_BossHPTexture);
        ctx->Draw(4, 0);

        Renderer::SetViewMatrix(oldView);
        Renderer::SetProjectionMatrix(oldProj);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }
}
