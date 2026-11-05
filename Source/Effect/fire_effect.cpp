#include "fire_effect.h"
#include "renderer.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace {
    struct FireParticle {
        XMFLOAT3 Pos;
        XMFLOAT3 Vel;
        float    Size;       // 基準サイズ
        float    Phase;      // 揺らぎの位相
        float    Rot;
        float    RotSpeed;
        int      Life;
        int      MaxLife;
        int      Kind;       // 0: 炎  1: 火の粉
        bool     Active;
    };

    const int MAX_FIRE = 140;
    FireParticle g_P[MAX_FIRE];

    bool     g_Lit = false;
    XMFLOAT3 g_Base = XMFLOAT3(0, 0, 0);
    int      g_Time = 0;

    ID3D11Buffer*             g_QuadVB = nullptr;
    ID3D11VertexShader*       g_VS = nullptr;
    ID3D11PixelShader*        g_PS = nullptr;
    ID3D11InputLayout*        g_IL = nullptr;
    ID3D11DepthStencilState*  g_Depth = nullptr;
    ID3D11BlendState*         g_Add = nullptr;
    ID3D11ShaderResourceView* g_Tex = nullptr;

    float Rand01() { return (float)rand() / (float)RAND_MAX; }

    // 中心が明るく滑らかに減衰する円形テクスチャ
    ID3D11ShaderResourceView* CreateSoftTexture(ID3D11Device* device)
    {
        const int N = 64;
        std::vector<BYTE> rgba(N * N * 4);
        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                float dx = ((float)x + 0.5f) / N * 2.0f - 1.0f;
                float dy = ((float)y + 0.5f) / N * 2.0f - 1.0f;
                float d = sqrtf(dx * dx + dy * dy);
                float a = std::clamp(1.0f - d, 0.0f, 1.0f);
                a = a * a * (3.0f - 2.0f * a);
                a = powf(a, 1.4f);
                int i = (y * N + x) * 4;
                rgba[i + 0] = 255; rgba[i + 1] = 255; rgba[i + 2] = 255;
                rgba[i + 3] = (BYTE)(a * 255.0f);
            }
        }
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = N; td.Height = N; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem = rgba.data();
        sd.SysMemPitch = N * 4;
        ID3D11Texture2D* tex = nullptr;
        if (FAILED(device->CreateTexture2D(&td, &sd, &tex))) return nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        device->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();
        return srv;
    }

    void Spawn(int kind)
    {
        for (int i = 0; i < MAX_FIRE; ++i) {
            FireParticle& p = g_P[i];
            if (p.Active) continue;
            p.Active = true;
            p.Kind = kind;
            p.Phase = Rand01() * XM_2PI;
            p.Rot = Rand01() * XM_2PI;
            p.RotSpeed = (Rand01() - 0.5f) * 0.06f;
            float a = Rand01() * XM_2PI;
            float r = Rand01() * 0.18f;
            p.Pos = XMFLOAT3(g_Base.x + cosf(a) * r, g_Base.y, g_Base.z + sinf(a) * r);
            if (kind == 0) {
                p.Vel = XMFLOAT3((Rand01() - 0.5f) * 0.004f, 0.014f + Rand01() * 0.010f, (Rand01() - 0.5f) * 0.004f);
                p.Size = 0.55f + Rand01() * 0.30f;
                p.MaxLife = 26 + rand() % 14;
            } else {
                p.Vel = XMFLOAT3((Rand01() - 0.5f) * 0.012f, 0.018f + Rand01() * 0.020f, (Rand01() - 0.5f) * 0.012f);
                p.Size = 0.07f + Rand01() * 0.05f;
                p.MaxLife = 50 + rand() % 40;
            }
            p.Life = p.MaxLife;
            return;
        }
    }
}

void FireEffect::Init()
{
    for (int i = 0; i < MAX_FIRE; ++i) g_P[i].Active = false;
    ID3D11Device* device = Renderer::GetDevice();
    if (!device) return;

    VERTEX_3D q[6] = {
        { XMFLOAT3(-0.5f,  0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT4(1, 1, 1, 1), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT4(1, 1, 1, 1), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT4(1, 1, 1, 1), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-0.5f,  0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT4(1, 1, 1, 1), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 0.5f,  0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT4(1, 1, 1, 1), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3( 0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT4(1, 1, 1, 1), XMFLOAT2(1.0f, 1.0f) },
    };
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth = sizeof(q);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = q;
    device->CreateBuffer(&bd, &sd, &g_QuadVB);

    Renderer::CreateVertexShader(&g_VS, &g_IL, Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(&g_PS, Renderer::ResolveShaderPath("ui_ps.cso").c_str());

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    device->CreateDepthStencilState(&dsd, &g_Depth);

    // 加算合成（炎が光って見える）
    D3D11_BLEND_DESC bld = {};
    bld.RenderTarget[0].BlendEnable = TRUE;
    bld.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bld.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    bld.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bld.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bld.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bld.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&bld, &g_Add);

    g_Tex = CreateSoftTexture(device);
}

void FireEffect::Uninit()
{
    if (g_Tex)    { g_Tex->Release();    g_Tex = nullptr; }
    if (g_Add)    { g_Add->Release();    g_Add = nullptr; }
    if (g_Depth)  { g_Depth->Release();  g_Depth = nullptr; }
    if (g_IL)     { g_IL->Release();     g_IL = nullptr; }
    if (g_PS)     { g_PS->Release();     g_PS = nullptr; }
    if (g_VS)     { g_VS->Release();     g_VS = nullptr; }
    if (g_QuadVB) { g_QuadVB->Release(); g_QuadVB = nullptr; }
}

void FireEffect::Clear()
{
    g_Lit = false;
    for (int i = 0; i < MAX_FIRE; ++i) g_P[i].Active = false;
}

void FireEffect::SetEmitter(const XMFLOAT3& basePos)
{
    g_Base = basePos;
    g_Lit = true;
}

void FireEffect::Update()
{
    g_Time++;

    if (g_Lit) {
        // 炎の粒子は毎フレーム2個、火の粉は時々
        Spawn(0);
        Spawn(0);
        if (rand() % 6 == 0) Spawn(1);
    }

    for (int i = 0; i < MAX_FIRE; ++i) {
        FireParticle& p = g_P[i];
        if (!p.Active) continue;
        if (--p.Life <= 0) { p.Active = false; continue; }

        float t = 1.0f - (float)p.Life / (float)p.MaxLife;
        // 横揺れ（中心へ寄せつつ揺らす）
        float sway = sinf(g_Time * 0.12f + p.Phase) * 0.0018f;
        p.Vel.x += sway - (p.Pos.x - g_Base.x) * 0.004f;
        p.Vel.z += sway * 0.7f - (p.Pos.z - g_Base.z) * 0.004f;
        p.Vel.x *= 0.96f;
        p.Vel.z *= 0.96f;
        if (p.Kind == 0) p.Vel.y *= 0.99f;
        p.Pos.x += p.Vel.x;
        p.Pos.y += p.Vel.y;
        p.Pos.z += p.Vel.z;
        p.Rot += p.RotSpeed;
        (void)t;
    }
}

void FireEffect::Draw()
{
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    if (!ctx || !g_QuadVB || !g_VS || !g_PS || !g_Tex || !g_Add) return;

    bool any = g_Lit;
    for (int i = 0; i < MAX_FIRE && !any; ++i) any = g_P[i].Active;
    if (!any) return;

    XMFLOAT4X4 v;
    XMStoreFloat4x4(&v, Renderer::GetViewMatrix());
    XMMATRIX bb = XMMatrixSet(
        v._11, v._21, v._31, 0,
        v._12, v._22, v._32, 0,
        v._13, v._23, v._33, 0,
        0, 0, 0, 1);

    UINT stride = sizeof(VERTEX_3D), offset = 0;
    ctx->IASetVertexBuffers(0, 1, &g_QuadVB, &stride, &offset);
    ctx->IASetInputLayout(g_IL);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(g_VS, nullptr, 0);
    ctx->PSSetShader(g_PS, nullptr, 0);
    ctx->PSSetShaderResources(0, 1, &g_Tex);
    ctx->OMSetDepthStencilState(g_Depth, 0);
    float bf[4] = { 0, 0, 0, 0 };
    ctx->OMSetBlendState(g_Add, bf, 0xffffffff);

    auto drawSprite = [&](const XMFLOAT3& pos, float sx, float sy, float rot, const XMFLOAT4& col) {
        XMMATRIX w = XMMatrixScaling(sx, sy, sx) * XMMatrixRotationZ(rot) * bb *
                     XMMatrixTranslation(pos.x, pos.y, pos.z);
        Renderer::SetWorldMatrix(w);
        MATERIAL m;
        ZeroMemory(&m, sizeof(m));
        m.Diffuse = XMFLOAT4(1, 1, 1, col.w);
        m.Emission = XMFLOAT4(col.x, col.y, col.z, 0.0f);   // ui_ps はテクスチャ無効時に Emission を色として使う
        m.Ambient = XMFLOAT4(1, 1, 1, 1);
        m.TextureEnable = FALSE;
        Renderer::SetMaterial(m);
        ctx->Draw(6, 0);
    };

    // 周囲を照らす淡い光（ゆらぎ付き）
    if (g_Lit) {
        float fl = 0.85f + 0.15f * sinf(g_Time * 0.31f) * sinf(g_Time * 0.17f + 1.3f);
        drawSprite(XMFLOAT3(g_Base.x, g_Base.y + 0.55f, g_Base.z), 3.6f * fl, 3.0f * fl, 0.0f,
                   XMFLOAT4(1.0f, 0.45f, 0.12f, 0.12f * fl));
    }

    for (int i = 0; i < MAX_FIRE; ++i) {
        const FireParticle& p = g_P[i];
        if (!p.Active) continue;
        float t = 1.0f - (float)p.Life / (float)p.MaxLife;   // 0:発生 → 1:消滅

        if (p.Kind == 0) {
            // 根元は白黄 → 橙 → 赤、上へ行くほど細く暗く消える
            XMFLOAT3 c0(1.00f, 0.55f, 0.10f), c1(0.95f, 0.28f, 0.04f), c2(0.50f, 0.06f, 0.01f);
            XMFLOAT3 c;
            if (t < 0.4f) { float k = t / 0.4f; c = XMFLOAT3(c0.x + (c1.x - c0.x) * k, c0.y + (c1.y - c0.y) * k, c0.z + (c1.z - c0.z) * k); }
            else          { float k = (t - 0.4f) / 0.6f; c = XMFLOAT3(c1.x + (c2.x - c1.x) * k, c1.y + (c2.y - c1.y) * k, c1.z + (c2.z - c1.z) * k); }
            float alpha = (t < 0.15f ? t / 0.15f : 1.0f - (t - 0.15f) / 0.85f) * 0.30f;
            float s = p.Size * (1.0f - 0.65f * t);
            drawSprite(p.Pos, s * 0.8f, s * 1.25f, p.Rot * 0.3f, XMFLOAT4(c.x, c.y, c.z, alpha));
        } else {
            float tw = 0.6f + 0.4f * sinf(g_Time * 0.5f + p.Phase * 3.0f);
            float alpha = (1.0f - t) * tw;
            drawSprite(p.Pos, p.Size, p.Size, 0.0f, XMFLOAT4(1.0f, 0.70f, 0.25f, alpha));
        }
    }

    ctx->OMSetBlendState(Renderer::GetBlendState(), bf, 0xffffffff);
    ctx->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
}
