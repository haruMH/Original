#include "title_logo.h"
#include "renderer.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {
    const int   MASK_W = 1536;
    const int   MASK_H = 384;
    const float SPREAD = 28.0f;            // 距離場に記録する最大距離（マスク画素）

    const float MENU_READY_TIME = 2.9f;    // この秒数でメニュー操作を許可
    const float SKIP_TIME       = 3.3f;    // 飛ばした時にジャンプする秒数

    struct LogoCB {
        float p0[4];   // 経過秒, 全体アルファ, -, -
        float p1[4];   // 矩形 x, y, w, h
        float p2[4];   // マスク幅, マスク高さ, SPREAD, -
    };

    bool   g_Active = false;
    float  g_Time   = 0.0f;

    ComPtr<ID3D11VertexShader>       g_VS;
    ComPtr<ID3D11PixelShader>        g_PS;
    ComPtr<ID3D11Buffer>             g_CB;
    ComPtr<ID3D11ShaderResourceView> g_Srv;
    ComPtr<ID3D11SamplerState>       g_Sampler;
    ComPtr<ID3D11DepthStencilState>  g_DepthOff;

    // Felzenszwalb の 1次元距離変換（二乗距離）
    void Edt1D(const float* f, float* d, int n, std::vector<int>& v, std::vector<float>& z)
    {
        int k = 0;
        v[0] = 0; z[0] = -1e20f; z[1] = 1e20f;
        for (int q = 1; q < n; ++q) {
            float s;
            while (true) {
                int p = v[k];
                s = ((f[q] + (float)q * q) - (f[p] + (float)p * p)) / (2.0f * (float)(q - p));
                if (s <= z[k]) { --k; } else { break; }
            }
            ++k;
            v[k] = q; z[k] = s; z[k + 1] = 1e20f;
        }
        k = 0;
        for (int q = 0; q < n; ++q) {
            while (z[k + 1] < (float)q) ++k;
            float dq = (float)(q - v[k]);
            d[q] = dq * dq + f[v[k]];
        }
    }

    // 2次元: 値が 0 の画素までの二乗距離
    void Edt2D(std::vector<float>& grid, int w, int h)
    {
        const int n = (std::max)(w, h);
        std::vector<float> f(n), d(n), z(n + 1);
        std::vector<int> v(n);
        for (int x = 0; x < w; ++x) {
            for (int y = 0; y < h; ++y) f[y] = grid[y * w + x];
            Edt1D(f.data(), d.data(), h, v, z);
            for (int y = 0; y < h; ++y) grid[y * w + x] = d[y];
        }
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) f[x] = grid[y * w + x];
            Edt1D(f.data(), d.data(), w, v, z);
            for (int x = 0; x < w; ++x) grid[y * w + x] = d[x];
        }
    }

    // GDI で文字を描いて、符号付き距離場（R8）の SRV を作る
    bool BuildSdfTexture(const char* text)
    {
        // ---- 1. 文字を白でビットマップへ描く ----
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = MASK_W;
        bmi.bmiHeader.biHeight      = -MASK_H;   // 上から下
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HDC dc = CreateCompatibleDC(nullptr);
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bmp || !bits) { if (bmp) DeleteObject(bmp); DeleteDC(dc); return false; }
        HGDIOBJ oldBmp = SelectObject(dc, bmp);
        memset(bits, 0, (size_t)MASK_W * MASK_H * 4);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));

        // このゲームの雰囲気（ずんぐりした太い書体）に合う順にフォントを試す
        const char* faces[] = { "Rockwell Extra Bold", "Arial Black", "Impact" };
        const int textAreaH = 300;               // 文字を置く領域の高さ（下は下線用）
        const int maxTextW  = MASK_W - 2 * ((int)SPREAD + 24);
        HFONT font = nullptr;
        for (const char* face : faces) {
            for (int px = 230; px >= 60; px -= 6) {
                HFONT f = CreateFontA(-px, 0, 0, 0, FW_HEAVY, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                      OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                      DEFAULT_PITCH | FF_DONTCARE, face);
                if (!f) continue;
                HGDIOBJ old = SelectObject(dc, f);
                char got[LF_FACESIZE] = {};
                GetTextFaceA(dc, LF_FACESIZE, got);
                SIZE sz = {};
                GetTextExtentPoint32A(dc, text, (int)strlen(text), &sz);
                SelectObject(dc, old);
                if (_stricmp(got, face) != 0) { DeleteObject(f); break; }   // この書体は無い→次へ
                if (sz.cx <= maxTextW && sz.cy <= textAreaH - 20) { font = f; break; }
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
        RECT rc = { 0, 0, MASK_W, textAreaH };
        DrawTextA(dc, text, -1, &rc, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        GdiFlush();

        // ---- 2. 文字の内外を 2値化 ----
        std::vector<float> toInside((size_t)MASK_W * MASK_H);    // 文字内の画素までの距離
        std::vector<float> toOutside((size_t)MASK_W * MASK_H);   // 文字外の画素までの距離
        const unsigned char* px = static_cast<const unsigned char*>(bits);
        const float INF = 1e12f;
        for (int i = 0; i < MASK_W * MASK_H; ++i) {
            bool in = px[i * 4 + 1] > 127;
            toInside[i]  = in ? 0.0f : INF;
            toOutside[i] = in ? INF : 0.0f;
        }
        SelectObject(dc, oldFont);
        DeleteObject(font);
        SelectObject(dc, oldBmp);
        DeleteObject(bmp);
        DeleteDC(dc);

        // ---- 3. 距離変換 → 符号付き距離（外側が正）→ R8 ----
        Edt2D(toInside, MASK_W, MASK_H);
        Edt2D(toOutside, MASK_W, MASK_H);
        std::vector<unsigned char> out((size_t)MASK_W * MASK_H);
        for (int i = 0; i < MASK_W * MASK_H; ++i) {
            float dIn  = sqrtf(toInside[i]);    // 外側なら >0
            float dOut = sqrtf(toOutside[i]);   // 内側なら >0
            float sd = (dIn > 0.0f) ? (dIn - 0.5f) : -(dOut - 0.5f);
            sd = std::clamp(sd, -SPREAD, SPREAD);
            out[i] = (unsigned char)std::clamp((0.5f + sd / (2.0f * SPREAD)) * 255.0f + 0.5f, 0.0f, 255.0f);
        }

        // ---- 4. テクスチャ化 ----
        ID3D11Device* device = Renderer::GetDevice();
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = MASK_W; td.Height = MASK_H; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem = out.data();
        sd.SysMemPitch = MASK_W;
        ComPtr<ID3D11Texture2D> tex;
        if (FAILED(device->CreateTexture2D(&td, &sd, tex.GetAddressOf()))) return false;
        return SUCCEEDED(device->CreateShaderResourceView(tex.Get(), nullptr, g_Srv.ReleaseAndGetAddressOf()));
    }
}

void TitleLogo::Start(const char* text)
{
    Stop();
    ID3D11Device* device = Renderer::GetDevice();
    if (!device) return;

    if (!BuildSdfTexture(text)) { g_Srv.Reset(); return; }

    Renderer::CreateVertexShader(g_VS.ReleaseAndGetAddressOf(), nullptr, Renderer::ResolveShaderPath("postEffect_vs.cso").c_str());
    Renderer::CreatePixelShader(g_PS.ReleaseAndGetAddressOf(), Renderer::ResolveShaderPath("title_logo_ps.cso").c_str());

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(LogoCB);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device->CreateBuffer(&bd, nullptr, g_CB.ReleaseAndGetAddressOf());

    D3D11_SAMPLER_DESC sm = {};
    sm.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sm.AddressU = sm.AddressV = sm.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sm.MaxLOD = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&sm, g_Sampler.ReleaseAndGetAddressOf());

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = FALSE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    device->CreateDepthStencilState(&dsd, g_DepthOff.ReleaseAndGetAddressOf());

    g_Time = -0.5f;    // 起動直後は少し間を置く
    g_Active = true;
}

void TitleLogo::Stop()
{
    g_Active = false;
    g_Srv.Reset();
    g_Sampler.Reset();
    g_CB.Reset();
    g_PS.Reset();
    g_VS.Reset();
    g_DepthOff.Reset();
}

void TitleLogo::Update()
{
    if (!g_Active) return;
    g_Time += 1.0f / 60.0f;
}

void TitleLogo::Skip()
{
    if (g_Active && g_Time < SKIP_TIME) g_Time = SKIP_TIME;
}

bool TitleLogo::IsMenuReady()
{
    // ロゴが使えない(失敗)場合もメニューは使えるようにする
    return !g_Active || g_Time >= MENU_READY_TIME;
}

float TitleLogo::GetMenuAlpha()
{
    if (!g_Active) return 1.0f;
    float t = std::clamp((g_Time - 2.3f) / 0.7f, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void TitleLogo::Draw()
{
    if (!g_Active || !g_Srv || !g_VS || !g_PS || !g_CB || g_Time < 0.0f) return;

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    const float sw = (float)Renderer::GetScreenWidth();
    const float sh = (float)Renderer::GetScreenHeight();

    // ロゴの矩形（画面上部寄り・幅は画面の約52%）
    float w = (std::min)(sw * 0.52f, sh * 0.52f * (float)MASK_W / (float)MASK_H * 0.9f);
    float h = w * (float)MASK_H / (float)MASK_W;
    float x = (sw - w) * 0.5f;
    float y = sh * 0.06f;

    LogoCB cb = {};
    cb.p0[0] = g_Time; cb.p0[1] = 1.0f;
    cb.p1[0] = x; cb.p1[1] = y; cb.p1[2] = w; cb.p1[3] = h;
    cb.p2[0] = (float)MASK_W; cb.p2[1] = (float)MASK_H; cb.p2[2] = SPREAD;
    ctx->UpdateSubresource(g_CB.Get(), 0, nullptr, &cb, 0, 0);

    ID3D11SamplerState* oldSampler = nullptr;
    ctx->PSGetSamplers(0, 1, &oldSampler);

    float bf[4] = { 0, 0, 0, 0 };
    ctx->OMSetBlendState(Renderer::GetBlendState(), bf, 0xffffffff);
    ctx->OMSetDepthStencilState(g_DepthOff.Get(), 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->VSSetShader(g_VS.Get(), nullptr, 0);
    ctx->PSSetShader(g_PS.Get(), nullptr, 0);
    ctx->PSSetConstantBuffers(6, 1, g_CB.GetAddressOf());
    ctx->PSSetShaderResources(0, 1, g_Srv.GetAddressOf());
    ctx->PSSetSamplers(0, 1, g_Sampler.GetAddressOf());
    ctx->Draw(3, 0);

    ID3D11ShaderResourceView* nullSrv = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSrv);
    ctx->PSSetSamplers(0, 1, &oldSampler);
    if (oldSampler) oldSampler->Release();
    ctx->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
}
