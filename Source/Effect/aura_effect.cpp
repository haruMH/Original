#include "aura_effect.h"
#include "renderer.h"
#include "manager.h"
#include "player.h"
#include "player_combat.h"
#include <vector>
#include <cmath>

using Microsoft::WRL::ComPtr;

ComPtr<ID3D11Buffer>             AuraEffect::m_QuadVB;
ComPtr<ID3D11VertexShader>       AuraEffect::m_VS;
ComPtr<ID3D11PixelShader>        AuraEffect::m_PS;
ComPtr<ID3D11InputLayout>        AuraEffect::m_IL;
ComPtr<ID3D11DepthStencilState>  AuraEffect::m_DepthState;
ComPtr<ID3D11ShaderResourceView> AuraEffect::m_Texture;
int                              AuraEffect::m_Time = 0;

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
// ソフトグロー（中心が明るく縁で消える、下側が濃いグラデーション）
// =================================================================
bool AuraEffect::CreateGlowTexture(ID3D11Device* device)
{
    const int W = 128, H = 128;
    std::vector<BYTE> rgba(W * H * 4);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float u = ((float)x + 0.5f) / W * 2.0f - 1.0f;
            float v = ((float)y + 0.5f) / H;            // 0=上 1=下
            float d = sqrtf(u * u + (v * 2.0f - 1.0f) * (v * 2.0f - 1.0f));
            float a = 1.0f - d;
            if (a < 0.0f) a = 0.0f;
            a = a * a * (0.35f + 0.65f * v);            // 下側ほど濃い

            BYTE* p = &rgba[(y * W + x) * 4];
            p[0] = 255; p[1] = 255; p[2] = 255;
            p[3] = (BYTE)(a * 255.0f);
        }
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

    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(device->CreateTexture2D(&td, &initData, &tex))) return false;
    if (FAILED(device->CreateShaderResourceView(tex.Get(), nullptr, &m_Texture))) return false;
    return true;
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
    } v[4];

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
    Renderer::CreatePixelShader(&m_PS, Renderer::ResolveShaderPath("ui_ps.cso").c_str());
    if (!m_VS || !m_PS || !m_IL) return false;

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(device->CreateDepthStencilState(&dsd, &m_DepthState))) return false;

    return CreateGlowTexture(device);
}

void AuraEffect::Uninit()
{
    m_Texture.Reset();
    m_DepthState.Reset();
    m_IL.Reset();
    m_PS.Reset();
    m_VS.Reset();
    m_QuadVB.Reset();
}

// =================================================================
// 描画
// =================================================================
void AuraEffect::Draw()
{
    if (!m_VS || !m_PS || !m_QuadVB || !m_IL || !m_DepthState || !m_Texture) return;

    Player* player = Manager::GetGameObject<Player>();
    if (!player || player->IsDestroy() || !player->GetCombatModule()) return;

    int stage = player->GetCombatModule()->GetChargeStage();
    if (stage <= 0) return;

    m_Time++;

    XMFLOAT3 col = StageColor(stage);
    XMFLOAT3 pos = player->GetPosition();

    float radius = 1.0f + 0.3f * stage;      // オーラの半径
    float height = radius * 2.2f;            // オーラの高さ
    float baseA  = 0.30f + 0.10f * stage;    // 基本の濃さ
    int   planes = 2 + stage;                // 交差させる縦面の枚数

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
    Renderer::SetTexture(m_Texture.Get());

    MATERIAL mat = {};
    mat.TextureEnable = FALSE;

    // 縦面: 揺らぎながらゆっくり回転する（両面見えるよう180度ずらして2回描く）
    for (int i = 0; i < planes; ++i) {
        float pulse = 1.0f + 0.10f * sinf(m_Time * 0.25f + i * 1.7f);
        float w = radius * 2.0f * pulse;
        float h = height * (1.0f + 0.06f * sinf(m_Time * 0.31f + i));
        float baseAngle = XM_PI * i / planes + m_Time * 0.03f * (1.0f + 0.3f * i);
        float a = baseA * (0.8f + 0.2f * sinf(m_Time * 0.4f + i * 2.1f));

        mat.Diffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, a);
        mat.Emission = XMFLOAT4(col.x, col.y, col.z, 0.0f);
        Renderer::SetMaterial(mat);

        for (int side = 0; side < 2; ++side) {
            XMMATRIX world = XMMatrixScaling(w, 1.0f, h)
                           * XMMatrixRotationX(-XM_PIDIV2)
                           * XMMatrixRotationY(baseAngle + side * XM_PI)
                           * XMMatrixTranslation(pos.x, pos.y + height * 0.5f - 0.5f, pos.z);
            Renderer::SetWorldMatrix(world);
            ctx->Draw(4, 0);
        }
    }

    // 足元の光だまり
    {
        float pulse = 1.0f + 0.08f * sinf(m_Time * 0.35f);
        mat.Diffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, baseA + 0.2f);
        mat.Emission = XMFLOAT4(col.x, col.y, col.z, 0.0f);
        Renderer::SetMaterial(mat);

        XMMATRIX world = XMMatrixScaling(radius * 2.4f * pulse, 1.0f, radius * 2.4f * pulse)
                       * XMMatrixRotationY(m_Time * 0.05f)
                       * XMMatrixTranslation(pos.x, pos.y - 0.45f, pos.z);
        Renderer::SetWorldMatrix(world);
        ctx->Draw(4, 0);
    }

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
}
