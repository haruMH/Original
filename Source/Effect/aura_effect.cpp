#include "aura_effect.h"
#include "renderer.h"
#include "manager.h"
#include "player.h"
#include "player_combat.h"
#include "game_constants.h"
#include <cmath>
#include <algorithm>
#include <vector>

using Microsoft::WRL::ComPtr;

ComPtr<ID3D11Buffer>             AuraEffect::m_WallVB;
UINT                             AuraEffect::m_WallVertexCount = 0;
ComPtr<ID3D11Buffer>             AuraEffect::m_QuadVB;
ComPtr<ID3D11VertexShader>       AuraEffect::m_VS;
ComPtr<ID3D11PixelShader>        AuraEffect::m_PS;
ComPtr<ID3D11InputLayout>        AuraEffect::m_IL;
ComPtr<ID3D11DepthStencilState>  AuraEffect::m_DepthState;
int                              AuraEffect::m_Time = 0;
int                              AuraEffect::m_LastStage = 0;
int                              AuraEffect::m_PulseTime = 9999;
float                            AuraEffect::m_Fade = 0.0f;
int                              AuraEffect::m_HoldStage = 1;

namespace {
    const int   PULSE_FRAMES    = 30;   // 段階到達パルスの継続フレーム
    const float FADE_IN_FRAMES  = 18.0f;
    const float FADE_OUT_FRAMES = 12.0f;
}

// =================================================================
// 段階ごとの発光色
// =================================================================
XMFLOAT3 AuraEffect::StageColor(int stage)
{
    switch (stage) {
    case 1:  return XMFLOAT3(0.5f, 1.8f, 2.5f);
    case 2:  return XMFLOAT3(1.2f, 2.5f, 0.5f);
    case 3:  return XMFLOAT3(2.8f, 1.6f, 0.2f);
    case 4:  return XMFLOAT3(3.0f, 0.3f, 0.8f);
    default: return XMFLOAT3(2.5f, 2.0f, 0.2f);
    }
}

// =================================================================
// 初期化 / 終了
// =================================================================
bool AuraEffect::Init(ID3D11Device* device)
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

    // 光の壁/光柱用の円筒（半径1・高さ1・y=0〜1）。裏表どちらからも見えるよう両巻きで持つ
    {
        const int N = 48;
        std::vector<VERTEX_3D_LOCAL> wv;
        wv.reserve(N * 12);
        auto mk = [](float a, float y, float u) {
            VERTEX_3D_LOCAL p;
            p.Position = XMFLOAT3(cosf(a), y, sinf(a));
            p.Normal   = XMFLOAT3(cosf(a), 0.0f, sinf(a));
            p.Diffuse  = XMFLOAT4(1, 1, 1, 1);
            p.TexCoord = XMFLOAT2(u, 1.0f - y);
            p.Tangent  = XMFLOAT3(1, 0, 0);
            return p;
        };
        for (int i = 0; i < N; ++i) {
            float a0 = XM_2PI * i / N, a1 = XM_2PI * (i + 1) / N;
            float u0 = (float)i / N,   u1 = (float)(i + 1) / N;
            VERTEX_3D_LOCAL p0 = mk(a0, 0.0f, u0), p1 = mk(a1, 0.0f, u1);
            VERTEX_3D_LOCAL p2 = mk(a0, 1.0f, u0), p3 = mk(a1, 1.0f, u1);
            const VERTEX_3D_LOCAL quad[12] = { p0, p2, p1, p1, p2, p3, p0, p1, p2, p1, p3, p2 };
            wv.insert(wv.end(), quad, quad + 12);
        }
        D3D11_BUFFER_DESC wbd = {};
        wbd.Usage     = D3D11_USAGE_IMMUTABLE;
        wbd.ByteWidth = (UINT)(wv.size() * sizeof(VERTEX_3D_LOCAL));
        wbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA wsd = {};
        wsd.pSysMem = wv.data();
        if (FAILED(device->CreateBuffer(&wbd, &wsd, &m_WallVB))) return false;
        m_WallVertexCount = (UINT)wv.size();
    }

    Renderer::CreateVertexShader(&m_VS, &m_IL, Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(&m_PS, Renderer::ResolveShaderPath("aura_ring_ps.cso").c_str());
    if (!m_VS || !m_PS || !m_IL) return false;

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(device->CreateDepthStencilState(&dsd, &m_DepthState))) return false;

    return true;
}

void AuraEffect::Uninit()
{
    m_DepthState.Reset();
    m_IL.Reset();
    m_PS.Reset();
    m_VS.Reset();
    m_QuadVB.Reset();
    m_WallVB.Reset();
}

// =================================================================
// 描画
// =================================================================
void AuraEffect::Draw()
{
    if (!m_VS || !m_PS || !m_QuadVB || !m_IL || !m_DepthState) return;

    Player* player = Manager::GetGameObject<Player>();
    if (!player || player->IsDestroy() || !player->GetCombatModule()) return;

    // 出現/消滅フェード: チャージ中は上昇、解除後は直前の段階の見た目のまま縮んで消える
    int cur = player->GetCombatModule()->GetChargeStage();
    if (cur > 0) {
        if (cur > m_LastStage) {
            m_PulseTime = 0;
            if (cur >= 4) {   // 最大チャージ到達: 画面フラッシュ
                Manager::TriggerFlash(XMFLOAT4(1.0f, 0.55f, 0.95f, 0.5f), 0.08f);
            }
        }
        m_HoldStage = cur;
        m_Fade = (std::min)(1.0f, m_Fade + 1.0f / FADE_IN_FRAMES);
    } else {
        m_Fade = (std::max)(0.0f, m_Fade - 1.0f / FADE_OUT_FRAMES);
        if (m_Fade <= 0.0f) { m_LastStage = 0; return; }
    }
    m_LastStage = cur;
    int stage = m_HoldStage;
    m_PulseTime++;
    m_Time++;

    float spinN = player->GetCurrentSpinSpeed() / Constants::Player::MAX_SPIN_SPEED;
    if (spinN > 1.0f) spinN = 1.0f;
    if (spinN < 0.0f) spinN = 0.0f;

    XMFLOAT3 col = StageColor(stage);
    XMFLOAT3 pos = player->GetPosition();

    // クアッド半幅 = radius*3（シェーダー側の定常リング半径 0.4 → 実半径 radius*1.2）
    float radius = 1.0f + 0.3f * stage;
    float half   = radius * 3.0f;

    // パラメータはマテリアルの空きフィールドに載せてシェーダーへ渡す
    MATERIAL mat = {};
    mat.TextureEnable = FALSE;
    mat.Diffuse   = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.75f + 0.05f * stage);
    mat.Specular  = XMFLOAT4((float)stage, m_Fade, 0.0f, 0.0f);
    mat.Emission  = XMFLOAT4(col.x, col.y, col.z, 0.0f);
    mat.Shininess = m_Time / 60.0f;
    mat.RimPower  = spinN;
    mat.Dummy     = (m_PulseTime < PULSE_FRAMES) ? (float)m_PulseTime / PULSE_FRAMES : 1.0f;

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    float bf[4] = { 0, 0, 0, 0 };
    ctx->OMSetDepthStencilState(m_DepthState.Get(), 0);
    ctx->OMSetBlendState(Renderer::GetBlendState(), bf, 0xffffffff);
    ctx->VSSetShader(m_VS.Get(), nullptr, 0);
    ctx->PSSetShader(m_PS.Get(), nullptr, 0);
    ctx->IASetInputLayout(m_IL.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    UINT stride = 60u;
    UINT offset = 0;
    ID3D11Buffer* vb = m_QuadVB.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);

    Renderer::SetMaterial(mat);
    Renderer::SetWorldMatrix(XMMatrixScaling(half * 2.0f, 1.0f, half * 2.0f)
                           * XMMatrixTranslation(pos.x, pos.y - 0.4f, pos.z));
    ctx->Draw(4, 0);

    // --- 光の壁（リングの縁から立ち上る円筒）と、段階4到達時の光柱 ---
    if (m_WallVB) {
        float e  = 1.0f - (1.0f - m_Fade) * (1.0f - m_Fade);
        float s  = 0.35f + 0.65f * e;
        float rw = radius * 1.2f * s;

        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->IASetVertexBuffers(0, 1, m_WallVB.GetAddressOf(), &stride, &offset);

        mat.Specular = XMFLOAT4((float)stage, m_Fade, 1.0f, 0.0f);
        Renderer::SetMaterial(mat);
        Renderer::SetWorldMatrix(XMMatrixScaling(rw, radius * 1.5f * (0.3f + 0.7f * e), rw)
                               * XMMatrixTranslation(pos.x, pos.y - 0.4f, pos.z));
        ctx->Draw(m_WallVertexCount, 0);

        const int PILLAR_FRAMES = 45;
        if (stage >= 4 && m_PulseTime < PILLAR_FRAMES) {
            float pt = (float)m_PulseTime / PILLAR_FRAMES;
            float rise = 1.0f - (1.0f - pt) * (1.0f - pt) * (1.0f - pt);
            mat.Specular = XMFLOAT4((float)stage, 1.0f, 2.0f, 0.0f);
            mat.Dummy    = pt;
            Renderer::SetMaterial(mat);
            Renderer::SetWorldMatrix(XMMatrixScaling(radius * 0.45f, 3.0f + 12.0f * rise, radius * 0.45f)
                                   * XMMatrixTranslation(pos.x, pos.y - 0.4f, pos.z));
            ctx->Draw(m_WallVertexCount, 0);
        }
    }

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->OMSetBlendState(Renderer::GetBlendState(), bf, 0xffffffff);
    ctx->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
}
