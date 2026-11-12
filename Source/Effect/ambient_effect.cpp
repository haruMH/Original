#include "ambient_effect.h"
#include "renderer.h"
#include "world_look.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace {
    struct AmbientParticle {
        XMFLOAT3 Pos;
        XMFLOAT3 Vel;
        float    Size;
        float    Phase;
    };

    const int   MAX_AMBIENT = 90;
    const float RANGE_XZ    = 16.0f;   // カメラ周囲の半径(箱)
    const float Y_MIN       = -0.6f;
    const float Y_MAX       = 5.0f;

    AmbientParticle g_P[MAX_AMBIENT];
    bool  g_Enabled = false;
    bool  g_Seeded  = false;
    int   g_Time    = 0;

    ID3D11Buffer*             g_QuadVB = nullptr;
    ID3D11VertexShader*       g_VS = nullptr;
    ID3D11PixelShader*        g_PS = nullptr;
    ID3D11InputLayout*        g_IL = nullptr;
    ID3D11DepthStencilState*  g_Depth = nullptr;
    ID3D11BlendState*         g_Add = nullptr;
    ID3D11ShaderResourceView* g_Tex = nullptr;

    float Rand01() { return (float)rand() / (float)RAND_MAX; }

    XMFLOAT3 GetCameraPos()
    {
        XMMATRIX inv = XMMatrixInverse(nullptr, Renderer::GetViewMatrix());
        XMFLOAT4X4 m;
        XMStoreFloat4x4(&m, inv);
        return XMFLOAT3(m._41, m._42, m._43);
    }

    // 夜の度合い 0(昼)〜1(夜)
    float NightFactor()
    {
        float sun[3] = { 0, 1, 0 };
        WorldLook::GetSunDirection(sun);
        float t = std::clamp((sun[1] + 0.05f) / 0.20f, 0.0f, 1.0f);   // 太陽が地平線付近で切替
        return 1.0f - t * t * (3.0f - 2.0f * t);
    }

    ID3D11ShaderResourceView* CreateSoftTexture(ID3D11Device* device)
    {
        const int N = 32;
        std::vector<BYTE> rgba(N * N * 4);
        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                float dx = ((float)x + 0.5f) / N * 2.0f - 1.0f;
                float dy = ((float)y + 0.5f) / N * 2.0f - 1.0f;
                float a = std::clamp(1.0f - sqrtf(dx * dx + dy * dy), 0.0f, 1.0f);
                a = a * a * (3.0f - 2.0f * a);
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

    // 有効化直後にカメラ周囲へ一度だけ散らす
    void Seed(const XMFLOAT3& cam)
    {
        for (int i = 0; i < MAX_AMBIENT; ++i) {
            AmbientParticle& p = g_P[i];
            p.Pos = XMFLOAT3(cam.x + (Rand01() * 2.0f - 1.0f) * RANGE_XZ,
                             Y_MIN + Rand01() * (Y_MAX - Y_MIN),
                             cam.z + (Rand01() * 2.0f - 1.0f) * RANGE_XZ);
            p.Vel = XMFLOAT3((Rand01() - 0.5f) * 0.006f, (Rand01() - 0.5f) * 0.003f, (Rand01() - 0.5f) * 0.006f);
            p.Size = 0.05f + Rand01() * 0.06f;
            p.Phase = Rand01() * XM_2PI;
        }
        g_Seeded = true;
    }
}

void AmbientEffect::Init()
{
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

void AmbientEffect::Uninit()
{
    if (g_Tex)    { g_Tex->Release();    g_Tex = nullptr; }
    if (g_Add)    { g_Add->Release();    g_Add = nullptr; }
    if (g_Depth)  { g_Depth->Release();  g_Depth = nullptr; }
    if (g_IL)     { g_IL->Release();     g_IL = nullptr; }
    if (g_PS)     { g_PS->Release();     g_PS = nullptr; }
    if (g_VS)     { g_VS->Release();     g_VS = nullptr; }
    if (g_QuadVB) { g_QuadVB->Release(); g_QuadVB = nullptr; }
}

void AmbientEffect::Clear()
{
    g_Enabled = false;
    g_Seeded = false;
}

void AmbientEffect::SetEnabled(bool enable)
{
    g_Enabled = enable;
    if (!enable) g_Seeded = false;
}

void AmbientEffect::Update()
{
    if (!g_Enabled) return;
    g_Time++;

    XMFLOAT3 cam = GetCameraPos();
    if (!g_Seeded) Seed(cam);

    const float R2 = RANGE_XZ * 2.0f;
    for (int i = 0; i < MAX_AMBIENT; ++i) {
        AmbientParticle& p = g_P[i];
        // ふわふわ漂う（ゆっくりした揺れ）
        p.Pos.x += p.Vel.x + sinf(g_Time * 0.013f + p.Phase) * 0.0035f;
        p.Pos.y += p.Vel.y + sinf(g_Time * 0.021f + p.Phase * 1.7f) * 0.0025f;
        p.Pos.z += p.Vel.z + cosf(g_Time * 0.011f + p.Phase) * 0.0035f;

        // カメラ周囲の箱から出たら反対側へ回り込ませる（常に周囲に粒子がある）
        if (p.Pos.x - cam.x >  RANGE_XZ) p.Pos.x -= R2;
        if (p.Pos.x - cam.x < -RANGE_XZ) p.Pos.x += R2;
        if (p.Pos.z - cam.z >  RANGE_XZ) p.Pos.z -= R2;
        if (p.Pos.z - cam.z < -RANGE_XZ) p.Pos.z += R2;
        if (p.Pos.y > Y_MAX) p.Pos.y = Y_MIN;
        if (p.Pos.y < Y_MIN) p.Pos.y = Y_MAX;
    }
}

void AmbientEffect::Draw()
{
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    if (!g_Enabled || !g_Seeded || !ctx || !g_QuadVB || !g_VS || !g_PS || !g_Tex || !g_Add) return;

    XMFLOAT3 cam = GetCameraPos();
    const float night = NightFactor();
    if (night < 0.01f) return;   // 昼は蛍がいないので何も描かない

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

    // 昼: 淡い暖色の埃 / 夜: 黄緑に明滅する蛍
    const XMFLOAT3 dayCol(1.00f, 0.92f, 0.75f);
    const XMFLOAT3 nightCol(0.70f, 1.00f, 0.35f);
    const XMFLOAT3 col(dayCol.x + (nightCol.x - dayCol.x) * night,
                       dayCol.y + (nightCol.y - dayCol.y) * night,
                       dayCol.z + (nightCol.z - dayCol.z) * night);

    MATERIAL m;
    ZeroMemory(&m, sizeof(m));
    m.Ambient = XMFLOAT4(1, 1, 1, 1);
    m.TextureEnable = FALSE;

    for (int i = 0; i < MAX_AMBIENT; ++i) {
        const AmbientParticle& p = g_P[i];
        // 箱の端では薄くして、粒子が突然現れたり消えたりしないようにする
        float dx = (p.Pos.x - cam.x) / RANGE_XZ;
        float dz = (p.Pos.z - cam.z) / RANGE_XZ;
        float edge = std::clamp(1.0f - (std::max)(fabsf(dx), fabsf(dz)), 0.0f, 1.0f);
        edge = (std::min)(1.0f, edge * 4.0f);

        float tw = 0.5f + 0.5f * sinf(g_Time * 0.06f + p.Phase * 3.0f);
        float alpha = (0.30f + 0.70f * tw) * night * edge;   // 昼は描かない（蛍は夜のみ）
        float size  = p.Size * (1.0f + night * 0.8f);

        m.Diffuse  = XMFLOAT4(1, 1, 1, alpha);
        m.Emission = XMFLOAT4(col.x, col.y, col.z, 0.0f);   // ui_ps はテクスチャ無効時に Emission を色として使う
        Renderer::SetMaterial(m);
        XMMATRIX w = XMMatrixScaling(size, size, size) * bb *
                     XMMatrixTranslation(p.Pos.x, p.Pos.y, p.Pos.z);
        Renderer::SetWorldMatrix(w);
        ctx->Draw(6, 0);
    }

    ctx->OMSetBlendState(Renderer::GetBlendState(), bf, 0xffffffff);
    ctx->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
}
