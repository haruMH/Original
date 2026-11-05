#include "hit_impact_effect.h"
#include "aura_effect.h"
#include "renderer.h"
#include "game_constants.h"
#include <cmath>
#include <algorithm>

using Microsoft::WRL::ComPtr;

ComPtr<ID3D11Buffer>             HitImpactEffect::m_QuadVB;
ComPtr<ID3D11VertexShader>       HitImpactEffect::m_VS;
ComPtr<ID3D11PixelShader>        HitImpactEffect::m_PS;
ComPtr<ID3D11InputLayout>        HitImpactEffect::m_IL;
ComPtr<ID3D11DepthStencilState>  HitImpactEffect::m_DepthOff;

namespace {
    const int MAX_IMPACTS = 8;

    struct Impact {
        bool     active = false;
        XMFLOAT3 pos   = XMFLOAT3(0, 0, 0);
        XMFLOAT3 dir   = XMFLOAT3(0, 0, 1);
        float    power = 0.0f;
        int      tier  = 0;
        int      age   = 0;
        int      life  = 30;
        float    seed  = 0.0f;
    };
    Impact g_Impacts[MAX_IMPACTS];
    unsigned g_SeedCounter = 1;
    ComPtr<ID3D11RasterizerState> g_RasterNoCull;   // 両面描画（ビルボードの向きに依存しない）

    // 決定的な擬似乱数（0〜1）
    float Rnd(unsigned s, unsigned k)
    {
        unsigned x = s * 747796405u + k * 2891336453u;
        x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
        x = (x >> 22u) ^ x;
        return (float)(x & 0xFFFFFFu) / (float)0x1000000;
    }

    float EaseOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
}

int HitImpactEffect::TierFromPower(float power)
{
    for (int i = 4; i >= 1; --i) {
        if (power >= Constants::Player::SPIN_STAGE_GUARANTEE[i]) return i;
    }
    return 0;
}

bool HitImpactEffect::Init(ID3D11Device* device)
{
    if (!device) return false;

    struct VERTEX_3D_LOCAL {
        XMFLOAT3 Position;
        XMFLOAT3 Normal;
        XMFLOAT4 Diffuse;
        XMFLOAT2 TexCoord;
        XMFLOAT3 Tangent;
    };
    VERTEX_3D_LOCAL v[4];
    const XMFLOAT3 pos[4] = { {-0.5f, 0.0f, 0.5f}, {0.5f, 0.0f, 0.5f}, {-0.5f, 0.0f, -0.5f}, {0.5f, 0.0f, -0.5f} };
    const XMFLOAT2 uv[4]  = { {0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f} };
    for (int i = 0; i < 4; ++i) {
        v[i].Position = pos[i];
        v[i].Normal   = XMFLOAT3(0.0f, 1.0f, 0.0f);
        v[i].Diffuse  = XMFLOAT4(1, 1, 1, 1);
        v[i].TexCoord = uv[i];
        v[i].Tangent  = XMFLOAT3(1, 0, 0);
    }

    D3D11_BUFFER_DESC bd = {};
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth = sizeof(v);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = v;
    if (FAILED(device->CreateBuffer(&bd, &sd, &m_QuadVB))) return false;

    Renderer::CreateVertexShader(&m_VS, &m_IL, Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(&m_PS, Renderer::ResolveShaderPath("hit_impact_ps.cso").c_str());
    if (!m_VS || !m_PS || !m_IL) return false;

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = FALSE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc      = D3D11_COMPARISON_ALWAYS;
    if (FAILED(device->CreateDepthStencilState(&dsd, &m_DepthOff))) return false;

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode        = D3D11_FILL_SOLID;
    rd.CullMode        = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(device->CreateRasterizerState(&rd, &g_RasterNoCull))) return false;

    for (Impact& im : g_Impacts) im.active = false;
    return true;
}

void HitImpactEffect::Uninit()
{
    g_RasterNoCull.Reset();
    m_DepthOff.Reset();
    m_IL.Reset();
    m_PS.Reset();
    m_VS.Reset();
    m_QuadVB.Reset();
    for (Impact& im : g_Impacts) im.active = false;
}

void HitImpactEffect::Spawn(const XMFLOAT3& pos, const XMFLOAT3& dir, float power)
{
    // 空きスロット、無ければ最も古いものを上書きする
    Impact* slot = nullptr;
    for (Impact& im : g_Impacts) {
        if (!im.active) { slot = &im; break; }
    }
    if (!slot) {
        slot = &g_Impacts[0];
        for (Impact& im : g_Impacts) {
            if (im.age > slot->age) slot = &im;
        }
    }

    if (power < 0.0f) power = 0.0f;
    if (power > 1.0f) power = 1.0f;

    slot->active = true;
    slot->pos    = pos;
    slot->dir    = dir;
    slot->power  = power;
    slot->tier   = TierFromPower(power);
    slot->age    = 0;
    slot->life   = 26 + slot->tier * 4;
    slot->seed   = (float)(g_SeedCounter++ % 997);
}

void HitImpactEffect::Draw()
{
    if (!m_VS || !m_PS || !m_QuadVB || !m_IL || !m_DepthOff) return;

    bool any = false;
    for (const Impact& im : g_Impacts) { if (im.active) { any = true; break; } }
    if (!any) return;

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    float bf[4] = { 0, 0, 0, 0 };
    ctx->OMSetDepthStencilState(m_DepthOff.Get(), 0);
    ctx->OMSetBlendState(Renderer::GetBlendState(), bf, 0xffffffff);
    ctx->RSSetState(g_RasterNoCull.Get());
    ctx->VSSetShader(m_VS.Get(), nullptr, 0);
    ctx->PSSetShader(m_PS.Get(), nullptr, 0);
    ctx->IASetInputLayout(m_IL.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    UINT stride = 60u;
    UINT offset = 0;
    ID3D11Buffer* vb = m_QuadVB.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);

    // 常にカメラへ正対するビルボード回転
    XMMATRIX camRot = XMMatrixInverse(nullptr, Renderer::GetViewMatrix());
    camRot.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    for (Impact& im : g_Impacts) {
        if (!im.active) continue;

        XMFLOAT3 col = AuraEffect::StageColor(im.tier);
        float t = (float)im.age / (float)im.life;
        float pw = im.power;

        MATERIAL mat = {};
        mat.TextureEnable = FALSE;
        mat.Emission  = XMFLOAT4(col.x, col.y, col.z, 0.0f);
        mat.Shininess = im.seed;
        mat.RimPower  = pw;

        // mode: 1=フラッシュ 2=リング 3=火花
        auto drawBillboard = [&](int mode, float size, const XMFLOAT3& c, float alpha, float prog) {
            if (alpha <= 0.001f || size <= 0.001f) return;
            mat.Diffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, alpha);
            mat.Specular = XMFLOAT4((float)im.tier, 0.0f, (float)mode, 0.0f);
            mat.Dummy    = prog;
            Renderer::SetMaterial(mat);
            Renderer::SetWorldMatrix(XMMatrixScaling(size, 1.0f, size)
                                   * XMMatrixRotationX(-XM_PIDIV2)
                                   * camRot
                                   * XMMatrixTranslation(c.x, c.y, c.z));
            ctx->Draw(4, 0);
        };

        // --- 衝撃リング（段階3で2本、段階4で3本。少しずつ遅れて広がる） ---
        int rings = 1 + (im.tier >= 3 ? 1 : 0) + (im.tier >= 4 ? 1 : 0);
        for (int k = 0; k < rings; ++k) {
            int la = im.age - k * 5;
            float rlife = im.life * 0.85f;
            if (la < 0 || la >= rlife) continue;
            float rt = la / rlife;
            float radius = 0.5f + (2.2f + 5.0f * pw + 0.5f * im.tier) * EaseOut(rt);
            drawBillboard(2, radius * 2.0f / 0.82f, im.pos, 0.95f * powf(1.0f - rt, 1.2f), rt);
        }

        // --- ヒットフラッシュ ---
        const int FLASH_FRAMES = 9;
        if (im.age < FLASH_FRAMES) {
            float ft = (float)im.age / FLASH_FRAMES;
            float size = (1.6f + 2.4f * pw + 0.4f * im.tier) * (0.5f + 0.5f * EaseOut(ft));
            drawBillboard(1, size, im.pos, 1.0f - ft * ft, ft);
        }

        // --- 火花（後方へ飛び散る。尾を2つ重ねて速さを出す） ---
        int sparks = 8 + 6 * im.tier + (int)(pw * 8.0f);
        if (sparks > 48) sparks = 48;
        unsigned sd = (unsigned)im.seed + 1u;
        for (int i = 0; i < sparks; ++i) {
            XMFLOAT3 rv(Rnd(sd, i * 4 + 0) * 2.0f - 1.0f, Rnd(sd, i * 4 + 1) * 2.0f - 1.0f, Rnd(sd, i * 4 + 2) * 2.0f - 1.0f);
            XMVECTOR vv = XMVectorSet(rv.x - im.dir.x * 0.8f, rv.y - im.dir.y * 0.8f + 0.2f, rv.z - im.dir.z * 0.8f, 0.0f);
            vv = XMVector3Normalize(vv);
            float speed = (0.08f + 0.20f * Rnd(sd, i * 4 + 3)) * (0.8f + pw);
            float sizeBase = (0.22f + 0.18f * Rnd(sd, i * 4 + 5)) * (0.8f + 0.6f * pw);

            for (int tail = 0; tail < 3; ++tail) {
                float a = (float)im.age - tail * 1.5f;
                if (a < 0.0f) continue;
                float st = a / (float)im.life;
                if (st >= 1.0f) continue;
                float travel = a * (1.0f - 0.5f * st);
                XMFLOAT3 c(im.pos.x + XMVectorGetX(vv) * speed * travel,
                           im.pos.y + XMVectorGetY(vv) * speed * travel - 0.003f * a * a,
                           im.pos.z + XMVectorGetZ(vv) * speed * travel);
                float tailScale = (tail == 0) ? 1.0f : (tail == 1 ? 0.7f : 0.45f);
                float tailAlpha = (tail == 0) ? 1.0f : (tail == 1 ? 0.6f : 0.35f);
                drawBillboard(3, sizeBase * (1.0f - 0.6f * st) * tailScale, c, powf(1.0f - st, 0.8f) * tailAlpha, st);
            }
        }

        im.age++;
        if (im.age >= im.life) im.active = false;
        (void)t;
    }

    ctx->RSSetState(Renderer::GetRasterizerStateCullBack());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
}
