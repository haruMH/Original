#include "score_hud.h"
#include "game_constants.h"
#include "renderer.h"
#include "game_rule.h"
#include "player.h"
#include "manager.h"
#include "boss_enemy.h"
#include <vector>
#include <algorithm>

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

int   ScoreHUD::m_LastTimeFrames = -1;
int   ScoreHUD::m_LastHP      = 5;
int   ScoreHUD::m_LastMaxHP   = 5;
int   ScoreHUD::m_LastBossHP    = -1;
int   ScoreHUD::m_LastBossMaxHP = -1;
float ScoreHUD::m_ScaleEffect = 1.0f;
float ScoreHUD::m_ShakeIntensity = 0.0f;
int   ScoreHUD::m_ShakeTimer = 0;
DirectX::XMFLOAT2 ScoreHUD::m_ShakeOffset = DirectX::XMFLOAT2(0.0f, 0.0f);

// ─────────────────────────────────────────────────────────────────
// GDI を使用してタイムHUD用のテクスチャを生成する
// センタートップ・スラッシュプレート（逆台形ダークグラス + ゴールドネオン）
// ─────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* ScoreHUD::CreateHUDTexture(
    ID3D11Device* device,
    int frames
)
{
    const int W = 512, H = 64;

    // ─── GDI DIB セクションの作成 ───
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;  // トップダウン
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* bits = nullptr;
    HDC hdc     = CreateCompatibleDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);
    memset(bits, 0, W * H * 4);

    // ─── 1. センタートップ・スラッシュベースパネル（逆台形ヘックス） ───
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

    HBRUSH plateBrush = CreateSolidBrush(RGB(15, 14, 25)); // 漆黒パープル
    HPEN   goldPen = CreatePen(PS_SOLID, 2, RGB(255, 190, 40)); // ゴールドネオン外枠
    HGDIOBJ oldPen = SelectObject(hdc, goldPen);
    HGDIOBJ oldBrush = SelectObject(hdc, plateBrush);
    Polygon(hdc, platePts, 6);

    // パネル底面のダブルアクセントライン
    HPEN thinGoldPen = CreatePen(PS_SOLID, 1, RGB(255, 230, 120));
    SelectObject(hdc, thinGoldPen);
    MoveToEx(hdc, padX + insetBot + 12, H - 9, nullptr);
    LineTo(hdc, W - padX - insetBot - 12, H - 9);

    // ─── 2. "TIME" ミニバッジ（中央上部） ───
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

    HFONT hFontBadge = CreateFontW(
        13, 0, 0, 0, FW_BLACK,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
    );
    HGDIOBJ oldFont = SelectObject(hdc, hFontBadge);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(20, 15, 5)); // 濃黒文字
    RECT rcBadge = { W / 2 - 40, 5, W / 2 + 40, 18 };
    DrawTextW(hdc, L"TIME", -1, &rcBadge, DT_CENTER | DT_SINGLELINE);

    // ─── 3. タイム数値の描画（大きく洗練されたイタリック体） ───
    HFONT hFontScore = CreateFontW(
        34, 0, 0, 0, FW_BLACK,
        TRUE, FALSE, FALSE, // 斜体
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
    );
    SelectObject(hdc, hFontScore);

    std::string timeStr = GameRule::FormatTime(frames);
    wchar_t buf[32];
    swprintf_s(buf, L"%hs", timeStr.c_str());

    // ドロップシャドウ
    SetTextColor(hdc, RGB(60, 45, 10));
    RECT rcShadow = { 0, 21, W + 2, H - 3 };
    DrawTextW(hdc, buf, -1, &rcShadow, DT_CENTER | DT_SINGLELINE);

    // メイン数値（ゴールドホワイト）
    SetTextColor(hdc, RGB(255, 245, 210));
    RECT rcScore = { 0, 19, W, H - 5 };
    DrawTextW(hdc, buf, -1, &rcScore, DT_CENTER | DT_SINGLELINE);

    // GDI 描画完了
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(plateBrush);
    DeleteObject(goldPen);
    DeleteObject(thinGoldPen);

    GdiFlush();

    // ─── 4. アルファ変換（背景半透明 + 枠線・文字不透明） ───
    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];

        if (r == 0 && g == 0 && b == 0) {
            rgba[i * 4 + 3] = 0; // 完全透過
        } else {
            int lum = (r * 299 + g * 587 + b * 114) / 1000;
            if (lum < 35) {
                rgba[i * 4 + 3] = (BYTE)(190 + lum); // 背景プレートはダークグラス半透明
            } else {
                rgba[i * 4 + 3] = 255; // 枠線・バッジ・数字は鮮明不透明
            }
        }
        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b;
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
    DeleteObject(hFontBadge);
    DeleteObject(hFontScore);
    DeleteObject(hBmp);
    DeleteDC(hdc);

    return srv;
}

// ─────────────────────────────────────────────────────────────────
// GDI を使用してボスHPバー用のテクスチャを生成する
// ヘックス・ウィングフレーム（左右対称六角形ダークグラス + クリムゾンネオン）
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

    // ─── 1. 左右対称ヘックス・ウィング背景パネル ───
    const int wingCut = 22;
    POINT wingPts[6] = {
        { wingCut, 2 },
        { W - wingCut, 2 },
        { W - 4, H / 2 },
        { W - wingCut, H - 2 },
        { wingCut, H - 2 },
        { 4, H / 2 }
    };

    HBRUSH bgBrush = CreateSolidBrush(RGB(18, 10, 20)); // 深紅ブラック
    HPEN   borderPen = CreatePen(PS_SOLID, 2, RGB(255, 45, 75)); // クリムゾンネオン枠線
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, bgBrush);
    Polygon(hdc, wingPts, 6);

    // 左右先端のネオンアクセントシェブロン
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

    // ─── 2. "BOSS // OVERLORD" ラベルテキスト ───
    HFONT hFontLabel = CreateFontW(
        24, 0, 0, 0, FW_BLACK,
        TRUE, FALSE, FALSE, // 斜体
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
    );
    HGDIOBJ oldFont = SelectObject(hdc, hFontLabel);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 60, 90)); // クリムゾンピンク
    RECT rcLabel = { 28, 8, 300, 36 };
    DrawTextW(hdc, L"WARNING // BOSS", -1, &rcLabel, DT_LEFT | DT_SINGLELINE);

    // ─── 3. HP 数値テキスト ───
    HFONT hFontHP = CreateFontW(
        20, 0, 0, 0, FW_HEAVY,
        TRUE, FALSE, FALSE, // 斜体
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
    );
    SelectObject(hdc, hFontHP);
    SetTextColor(hdc, RGB(255, 235, 235));
    wchar_t hpBuf[32];
    swprintf_s(hpBuf, L"%d / %d", hp, maxHp);
    RECT rcHP = { W - 220, 10, W - 28, 36 };
    DrawTextW(hdc, hpBuf, -1, &rcHP, DT_RIGHT | DT_SINGLELINE);

    // ─── 4. ボスHPゲージ ───
    const int barLeft   = 28;
    const int barTop    = 42;
    const int barRight  = W - 28;
    const int barBottom = H - 12;

    // ゲージスロット背景（暗いバーガンディ）
    HBRUSH barBgBrush = CreateSolidBrush(RGB(38, 16, 26));
    RECT barBgRect = { barLeft, barTop, barRight, barBottom };
    FillRect(hdc, &barBgRect, barBgBrush);
    DeleteObject(barBgBrush);

    if (maxHp > 0 && hp > 0) {
        float ratio = (float)hp / (float)maxHp;
        if (ratio > 1.0f) ratio = 1.0f;
        int fillRight = barLeft + (int)((barRight - barLeft) * ratio);

        // クリムゾンレッドからネオンアンバーへの変化
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

        // 天面ホワイトハイライト
        HBRUSH hlBrush = CreateSolidBrush(RGB(255, 230, 235));
        RECT hlRect = { barLeft, barTop, fillRight, barTop + 3 };
        FillRect(hdc, &hlRect, hlBrush);
        DeleteObject(hlBrush);
    }

    // 10%刻みの目盛りスリット線
    HPEN tickPen = CreatePen(PS_SOLID, 1, RGB(18, 10, 20));
    SelectObject(hdc, tickPen);
    for (int t = 1; t < 10; ++t) {
        int tx = barLeft + (int)((barRight - barLeft) * (t * 0.1f));
        MoveToEx(hdc, tx, barTop, nullptr);
        LineTo(hdc, tx, barBottom);
    }
    DeleteObject(tickPen);

    // ゲージ枠線
    HPEN gaugeBorderPen = CreatePen(PS_SOLID, 1, RGB(255, 90, 120));
    SelectObject(hdc, gaugeBorderPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, barLeft, barTop, barRight, barBottom);
    DeleteObject(gaugeBorderPen);

    // GDI 描画完了
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(bgBrush);
    DeleteObject(borderPen);

    GdiFlush();

    // ─── 5. アルファ変換（背景半透明 + 枠線・文字不透明） ───
    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];

        if (r == 0 && g == 0 && b == 0) {
            rgba[i * 4 + 3] = 0; // 完全透過
        } else {
            int lum = (r * 299 + g * 587 + b * 114) / 1000;
            if (lum < 35) {
                rgba[i * 4 + 3] = (BYTE)(205 + lum); // 背景プレートはダークグラス半透明
            } else {
                rgba[i * 4 + 3] = 255; // ゲージ、枠線、テキストは鮮明不透明
            }
        }
        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b;
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
// ─────────────────────────────────────────────────────────────────
// GDI を使用してプレイヤーHPバー用のテクスチャを生成する
// スラッシュ＆ネオン・トゥーン調のスラント（傾斜）フレーム + セグメントゲージ
// ─────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* ScoreHUD::CreatePlayerHPTexture(
    ID3D11Device* device,
    int hp,
    int maxHp
)
{
    const int W = static_cast<int>(Constants::UI::PlayerHP::BAR_WIDTH);
    const int H = static_cast<int>(Constants::UI::PlayerHP::BAR_HEIGHT);
    const int SLANT = 14; // スラント（平行四辺形の傾斜オフセット）

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

    // ─── 1. スラント背景パネル（ダークグラス調・平行四辺形） ───
    POINT panelPts[4] = {
        { SLANT + 2, 2 },
        { W - 4, 2 },
        { W - 4 - SLANT, H - 2 },
        { 2, H - 2 }
    };
    HBRUSH panelBgBrush = CreateSolidBrush(RGB(14, 20, 32)); // 深いサイバーネイビー
    HPEN   panelBorderPen = CreatePen(PS_SOLID, 2, RGB(0, 210, 255)); // ネオンシアン枠線
    HGDIOBJ oldPen = SelectObject(hdc, panelBorderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, panelBgBrush);
    Polygon(hdc, panelPts, 4);

    // パネル右上のアクセント小三角
    POINT decoPts[3] = {
        { W - 4, 2 },
        { W - 18, 2 },
        { W - 4, 16 }
    };
    HBRUSH decoBrush = CreateSolidBrush(RGB(0, 240, 255));
    SelectObject(hdc, decoBrush);
    Polygon(hdc, decoPts, 3);
    DeleteObject(decoBrush);

    // ─── 2. "HERO / P1" スタイリッシュバッジ（左上） ───
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

    // バッジ内テキスト "HERO"
    HFONT hFontBadge = CreateFontW(
        15, 0, 0, 0, FW_BLACK,
        TRUE, FALSE, FALSE, // 斜体
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
    );
    HGDIOBJ oldFont = SelectObject(hdc, hFontBadge);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(10, 20, 35)); // 濃紺文字
    RECT rcBadge = { 10, 7, 72, 25 };
    DrawTextW(hdc, L"HERO", -1, &rcBadge, DT_CENTER | DT_SINGLELINE);

    // ─── 3. HP 数値テキスト（右側） ───
    HFONT hFontHP = CreateFontW(
        19, 0, 0, 0, FW_HEAVY,
        TRUE, FALSE, FALSE, // 斜体
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
    );
    SelectObject(hdc, hFontHP);
    SetTextColor(hdc, RGB(225, 245, 255));
    wchar_t hpBuf[32];
    swprintf_s(hpBuf, L"HP  %d / %d", hp, maxHp);
    RECT rcHP = { W - 150, 5, W - 24, 25 };
    DrawTextW(hdc, hpBuf, -1, &rcHP, DT_RIGHT | DT_SINGLELINE);

    // ─── 4. スラント・セグメントHPゲージ ───
    const int barTop    = 30;
    const int barBottom = H - 8;
    const int barLeft   = 16;
    const int barRight  = W - 22;
    const int totalBarW = barRight - barLeft;
    const int segCount  = (maxHp > 0) ? maxHp : 5;
    const int segGap    = 4; // セグメント間のスリット隙間
    const float segW    = (float)(totalBarW - segGap * (segCount - 1)) / (float)segCount;

    // HP残量比率に応じたカラー算出
    float ratio = (maxHp > 0) ? ((float)hp / (float)maxHp) : 1.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    int rCol = 0, gCol = 230, bCol = 255;
    if (ratio > 0.5f) {
        // シアンからエメラルドグリーン
        float t = (ratio - 0.5f) * 2.0f;
        rCol = (int)(0 * (1.0f - t) + 0 * t);
        gCol = (int)(220 * (1.0f - t) + 255 * t);
        bCol = (int)(255 * (1.0f - t) + 140 * t);
    } else {
        // イエローからクリムゾンレッド（ピンチ）
        float t = ratio * 2.0f;
        rCol = (int)(255 * (1.0f - t) + 255 * t);
        gCol = (int)(40 * (1.0f - t) + 210 * t);
        bCol = (int)(40 * (1.0f - t) + 0 * t);
    }
    rCol = std::min(255, std::max(0, rCol));
    gCol = std::min(255, std::max(0, gCol));
    bCol = std::min(255, std::max(0, bCol));

    HBRUSH fillBrush = CreateSolidBrush(RGB(rCol, gCol, bCol));
    HBRUSH emptyBrush = CreateSolidBrush(RGB(24, 32, 48)); // 未充填スロット（ダークスレート）
    HBRUSH hlBrush = CreateSolidBrush(RGB(220, 250, 255));   // 上部ハイライト光彩
    HPEN   segPen = CreatePen(PS_SOLID, 1, RGB(0, 160, 210)); // セグメント枠線

    for (int i = 0; i < segCount; ++i) {
        float x0 = barLeft + i * (segW + segGap);
        float x1 = x0 + segW;
        bool isFilled = (i < hp);

        // 各セグメントの平行四辺形頂点
        POINT segPts[4] = {
            { (int)(x0 + 6), barTop },
            { (int)(x1 + 6), barTop },
            { (int)(x1), barBottom },
            { (int)(x0), barBottom }
        };

        SelectObject(hdc, isFilled ? fillBrush : emptyBrush);
        SelectObject(hdc, isFilled ? segPen : (HGDIOBJ)GetStockObject(NULL_PEN));
        Polygon(hdc, segPts, 4);

        // 充填ブロックの上部ハイライトライン
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

    // GDI 描画完了
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(panelBgBrush);
    DeleteObject(panelBorderPen);

    GdiFlush();

    // ─── 5. 高品質アルファチャンネル変換（ダークグラス半透明 ＆ 鮮明発光） ───
    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];

        if (r == 0 && g == 0 && b == 0) {
            rgba[i * 4 + 3] = 0; // 完全透過
        } else {
            int lum = (r * 299 + g * 587 + b * 114) / 1000;
            if (lum < 40) {
                rgba[i * 4 + 3] = (BYTE)(195 + lum); // 背景は美しいダークグラス半透明
            } else {
                rgba[i * 4 + 3] = 255; // ゲージ、枠線、テキストは完全不透明
            }
        }
        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b;
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
    DeleteObject(hFontBadge);
    DeleteObject(hFontHP);
    DeleteObject(hBmp);
    DeleteDC(hdc);

    return srv;
}

// ─────────────────────────────────────────────────────────────────
// GDI を使用してタイトル画面用のテクスチャを生成する
// スタイリッシュ・トゥーンタイトルパネル（立体デュアルトーンロゴ + キーバッジ）
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

    // ─── 1. 背景のスラント・ダークグラスパネル ───
    POINT bgPts[6] = {
        { 24, 6 },
        { W - 24, 6 },
        { W - 6, 36 },
        { W - 24, H - 6 },
        { 24, H - 6 },
        { 6, H - 36 }
    };
    HBRUSH bgBrush = CreateSolidBrush(RGB(14, 16, 26)); // 深いダークパープルネイビー
    HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(0, 210, 255)); // ネオンシアン枠線
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, bgBrush);
    Polygon(hdc, bgPts, 6);

    // ─── 2. サブタイトル・リボン ───
    HFONT hFontSub = CreateFontW(
        13, 0, 0, 0, FW_BOLD,
        TRUE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
    );
    HGDIOBJ oldFont = SelectObject(hdc, hFontSub);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 190, 40)); // ゴールド
    RECT rcSub = { 0, 18, W, 34 };
    DrawTextW(hdc, L"◆ HIGH SPEED SLASH ACTION ◆", -1, &rcSub, DT_CENTER | DT_SINGLELINE);

    // ─── 3. メインタイトルロゴ "TOON SLASHER" ───
    HFONT hFontTitle = CreateFontW(
        54, 0, 0, 0, FW_BLACK,
        TRUE, FALSE, FALSE, // 斜体
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
    );
    SelectObject(hdc, hFontTitle);

    // 立体黒シャドウ
    SetTextColor(hdc, RGB(6, 8, 14));
    RECT rcShadow = { 0, 44, W + 4, 114 };
    DrawTextW(hdc, L"TOON SLASHER", -1, &rcShadow, DT_CENTER | DT_SINGLELINE);

    // メインロゴ（ホワイト〜シアン発光）
    SetTextColor(hdc, RGB(245, 255, 255));
    RECT rcTitle = { 0, 40, W, 110 };
    DrawTextW(hdc, L"TOON SLASHER", -1, &rcTitle, DT_CENTER | DT_SINGLELINE);

    // セパレーターライン
    HPEN sepPen = CreatePen(PS_SOLID, 1, RGB(0, 220, 255));
    SelectObject(hdc, sepPen);
    MoveToEx(hdc, 40, 114, nullptr);
    LineTo(hdc, W - 40, 114);
    DeleteObject(sepPen);

    // ─── 4. "PRESS ENTER TO START" バッジ ───
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
        17, 0, 0, 0, FW_BLACK,
        TRUE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
    );
    SelectObject(hdc, hFontEnter);
    SetTextColor(hdc, RGB(20, 15, 5));
    RECT rcEnter = { W / 2 - 120, 131, W / 2 + 120, 152 };
    DrawTextW(hdc, L"PRESS ENTER TO START", -1, &rcEnter, DT_CENTER | DT_SINGLELINE);

    // ─── 5. 操作キーガイド ───
    HFONT hFontGuide = CreateFontW(
        14, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    SelectObject(hdc, hFontGuide);
    SetTextColor(hdc, RGB(180, 210, 235));
    RECT rcGuide1 = { 0, 172, W, 194 };
    DrawTextW(hdc, L"[WASD] MOVE    |    [SHIFT] DASH / DODGE", -1, &rcGuide1, DT_CENTER | DT_SINGLELINE);
    RECT rcGuide2 = { 0, 198, W, 220 };
    DrawTextW(hdc, L"[L-CLICK] GRAB / THROW    |    [R-CLICK] SPIN ATTACK", -1, &rcGuide2, DT_CENTER | DT_SINGLELINE);

    // GDI 描画完了
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(bgBrush);
    DeleteObject(borderPen);

    GdiFlush();

    // ─── 6. アルファ変換 ───
    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];

        if (r == 0 && g == 0 && b == 0) {
            rgba[i * 4 + 3] = 0;
        } else {
            int lum = (r * 299 + g * 587 + b * 114) / 1000;
            if (lum < 35) {
                rgba[i * 4 + 3] = (BYTE)(205 + lum); // 背景はダークグラス半透明
            } else {
                rgba[i * 4 + 3] = 255;
            }
        }
        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b;
    }

    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBmp);
    DeleteObject(hFontTitle);
    DeleteObject(hFontSub);
    DeleteObject(hFontEnter);
    DeleteObject(hFontGuide);
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
ID3D11ShaderResourceView* ScoreHUD::CreateResultTexture(ID3D11Device* device, bool isClear, int frames)
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

    // ─── 1. 背景パネル ───
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

    // ─── 2. リザルトタイトル ───
    HFONT hFontTitle = CreateFontW(
        50, 0, 0, 0, FW_BLACK,
        TRUE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Black"
    );
    HGDIOBJ oldFont = SelectObject(hdc, hFontTitle);
    SetBkMode(hdc, TRANSPARENT);

    // シャドウ
    SetTextColor(hdc, RGB(6, 6, 12));
    RECT rcShadow = { 0, 26, W + 4, 86 };
    DrawTextW(hdc, isClear ? L"MISSION CLEAR" : L"GAME OVER", -1, &rcShadow, DT_CENTER | DT_SINGLELINE);

    // メイン文字
    SetTextColor(hdc, isClear ? RGB(255, 220, 60) : RGB(255, 60, 80));
    RECT rcTitle = { 0, 24, W, 84 };
    DrawTextW(hdc, isClear ? L"MISSION CLEAR" : L"GAME OVER", -1, &rcTitle, DT_CENTER | DT_SINGLELINE);

    // ─── 3. タイムアタック結果表示 ───
    HFONT hFontLabel = CreateFontW(
        16, 0, 0, 0, FW_BLACK,
        FALSE, FALSE, FALSE,
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

    // タイム数値（大きく洗練されたイタリック体）
    HFONT hFontTimeNum = CreateFontW(
        42, 0, 0, 0, FW_BLACK,
        TRUE, FALSE, FALSE,
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

    // ベストタイムサブ表示（クリア時のみ）
    if (isClear) {
        HFONT hFontBest = CreateFontW(
            13, 0, 0, 0, FW_BOLD,
            FALSE, FALSE, FALSE,
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

    // ─── 4. 操作案内 ───
    HFONT hFontDesc = CreateFontW(
        16, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    SelectObject(hdc, hFontDesc);
    SetTextColor(hdc, RGB(200, 215, 230));
    RECT rcDesc = { 0, 194, W, 226 };
    DrawTextW(hdc, L"PRESS ENTER TO RETURN TITLE", -1, &rcDesc, DT_CENTER | DT_SINGLELINE);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(bgBrush);
    DeleteObject(borderPen);

    GdiFlush();

    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];

        if (r == 0 && g == 0 && b == 0) {
            rgba[i * 4 + 3] = 0;
        } else {
            int lum = (r * 299 + g * 587 + b * 114) / 1000;
            if (lum < 35) {
                rgba[i * 4 + 3] = (BYTE)(210 + lum);
            } else {
                rgba[i * 4 + 3] = 255;
            }
        }
        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b;
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
            if (m_PlayerHPTexture) {
                m_PlayerHPTexture->Release();
                m_PlayerHPTexture = nullptr;
            }
            m_PlayerHPTexture = CreatePlayerHPTexture(Renderer::GetDevice(), curPlayerHP, curPlayerMaxHP);
            m_LastHP    = curPlayerHP;
            m_LastMaxHP = curPlayerMaxHP;
        }

        // タイムアタック時間の監視（約2フレーム = 0.033秒ごと、またはタイム短縮ボーナス時にテクスチャ更新）
        int currentFrames = GameRule::GetElapsedFrames();
        bool timeJumped = (m_LastTimeFrames >= 0 && currentFrames < m_LastTimeFrames); // タイム短縮ボーナス発生
        if (!m_Texture || timeJumped || std::abs(currentFrames - m_LastTimeFrames) >= 2) {
            if (m_Texture) {
                m_Texture->Release();
                m_Texture = nullptr;
            }
            m_Texture = CreateHUDTexture(Renderer::GetDevice(), currentFrames);
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
            m_ClearTexture = CreateResultTexture(Renderer::GetDevice(), true, GameRule::GetClearTimeFrames());
        }
    } else if (currentScene == Scene::GAMEOVER) {
        if (!m_GameOverTexture) {
            m_GameOverTexture = CreateResultTexture(Renderer::GetDevice(), false, GameRule::GetElapsedFrames());
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

    const float screenW = (float)Renderer::GetScreenWidth();
    const float screenH = (float)Renderer::GetScreenHeight();

    float w = 512.0f;
    float h = 64.0f;
    float posX = screenW * 0.5f; // 画面中央
    float posY = 45.0f;          // 画面上部から少し下げた位置
    XMFLOAT4 emissionColor = XMFLOAT4(1.5f, 1.2f, 0.0f, 0.0f); // スコアHUDは黄金色の発光

    switch (currentScene) {
    case Scene::TITLE:
        texToDraw = m_TitleTexture;
        w = 512.0f;
        h = 256.0f;
        {
            static float titleFloatTime = 0.0f;
            titleFloatTime += 0.035f;
            posY = screenH * 0.5f + sinf(titleFloatTime) * 7.0f; // 上下にフワフワ浮遊
        }
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
        posY = screenH * 0.5f;
        emissionColor = XMFLOAT4(1.5f, 1.2f, 0.0f, 0.0f); // クリアは黄金発光
        break;
    case Scene::GAMEOVER:
        texToDraw = m_GameOverTexture;
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

        XMMATRIX world = XMMatrixScaling(w, h, 1.0f) * XMMatrixTranslation(posX + m_ShakeOffset.x, posY + m_ShakeOffset.y, 0.0f);
        Renderer::SetWorldMatrix(world);

        MATERIAL mat = {};
        mat.Diffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        mat.Emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        mat.TextureEnable = TRUE;
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

        XMMATRIX worldPlayer = XMMatrixScaling(pBarW, pBarH, 1.0f) * XMMatrixTranslation(pBarX + m_ShakeOffset.x, pBarY + m_ShakeOffset.y, 0.0f);
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
        const float bossBarX = screenW * 0.5f;
        const float bossBarY = screenH - bossBarH * 0.5f - Constants::UI::BossHP::SCREEN_MARGIN_Y;

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

        XMMATRIX worldBoss = XMMatrixScaling(bossBarW, bossBarH, 1.0f) * XMMatrixTranslation(bossBarX + m_ShakeOffset.x, bossBarY + m_ShakeOffset.y, 0.0f);
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

    // 後続の3D描画（シーンやオブジェクト）への影響を防ぐため、深度ステートを有効状態に復元する
    ctx->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
}
