#include "beach.h"
#include "renderer.h"
#include "world_look.h"
#include <vector>
#include <cmath>
#include <algorithm>

using namespace DirectX;

namespace {
    const float SHORE_Z   = 12.0f;    // 岸線の基準Z（sea_ps.hlsl の SHORE_Z と揃える）
    const float SEA_SLOPE = 0.16f;    // 海底の傾き（sea_ps.hlsl の SEA_SLOPE と揃える）
    const float WATER_Y   = -0.985f;  // 水面の高さ（砂との重なりちらつき防止に少し上げる）

    float Smooth(float a, float b, float x) {
        float t = (std::min)(1.0f, (std::max)(0.0f, (x - a) / (b - a)));
        return t * t * (3.0f - 2.0f * t);
    }
    // 整数格子のハッシュ(0〜1)。period で折り返してタイル可能にする
    float Hash(int x, int y, int period) {
        x = ((x % period) + period) % period;
        y = ((y % period) + period) % period;
        unsigned int h = (unsigned int)(x * 374761393 + y * 668265263);
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= (h >> 16);
        return (float)(h & 0xffffff) / (float)0x1000000;
    }
    float ValueNoise(float x, float y, int period) {
        int xi = (int)floorf(x), yi = (int)floorf(y);
        float fx = x - xi, fy = y - yi;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);
        float a = Hash(xi, yi, period),     b = Hash(xi + 1, yi, period);
        float c = Hash(xi, yi + 1, period), d = Hash(xi + 1, yi + 1, period);
        float top = a + (b - a) * fx;
        float bot = c + (d - c) * fx;
        return top + (bot - top) * fy;
    }
}

float Beach::ShoreZ(float x)
{
    return SHORE_Z + 1.6f * sinf(x * 0.13f) + 0.8f * sinf(x * 0.31f + 1.3f);
}

float Beach::GroundY(float x, float z)
{
    const float t = ShoreZ(x) - z;   // 岸からの陸側の距離（負なら海の中）
    if (t < 0.0f) {
        return -1.0f + SEA_SLOPE * t;   // 海底（沖ほど深い）
    }
    float h = -1.0f + 0.8f * (1.0f - expf(-t * 0.12f));   // 岸から緩やかに上がる
    float dune = sinf(x * 0.11f + 1.0f) * sinf(z * 0.09f) + 0.5f * sinf(x * 0.23f + z * 0.17f);
    return h + 0.22f * dune * Smooth(4.0f, 14.0f, t);        // 奥は砂丘のうねり
}

// 砂の絵: タイル可能な粒ノイズ + ほのかな風紋 + きらめく粒
void Beach::CreateSandTexture()
{
    const int N = 256;
    std::vector<unsigned char> px(N * N * 4);
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            float u = (float)x / N, v = (float)y / N;
            float n = 0.0f;
            n += 0.50f * ValueNoise(u * 8.0f,  v * 8.0f,  8);
            n += 0.30f * ValueNoise(u * 32.0f, v * 32.0f, 32);
            n += 0.20f * ValueNoise(u * 128.0f, v * 128.0f, 128);
            float ripple = 0.5f + 0.5f * sinf((v * 6.0f + 0.35f * ValueNoise(u * 4.0f, v * 4.0f, 4)) * XM_2PI);
            float grain = Hash(x, y, N);
            float k = 0.80f + 0.22f * n + 0.06f * ripple;
            float spark = (grain > 0.985f) ? 0.10f : 0.0f;
            float r = (std::min)(1.0f, 0.93f * k + spark);
            float g = (std::min)(1.0f, 0.84f * k + spark);
            float b = (std::min)(1.0f, 0.66f * k + spark);
            unsigned char* p = &px[(y * N + x) * 4];
            p[0] = (unsigned char)(r * 255.0f); p[1] = (unsigned char)(g * 255.0f); p[2] = (unsigned char)(b * 255.0f); p[3] = 255;
        }
    }

    ID3D11Device* dev = Renderer::GetDevice();
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    D3D11_TEXTURE2D_DESC td{};
    td.Width = N; td.Height = N; td.MipLevels = 0; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &tex))) return;
    ctx->UpdateSubresource(tex.Get(), 0, nullptr, px.data(), N * 4, 0);
    if (FAILED(dev->CreateShaderResourceView(tex.Get(), nullptr, &m_SandTex))) return;
    ctx->GenerateMips(m_SandTex.Get());
}

void Beach::Init()
{
    m_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);

    // --- 砂浜メッシュ（x:±80, z:-60〜45 を 1.25m 格子で）---
    const float X0 = -80.0f, X1 = 80.0f, Z0 = -60.0f, Z1 = 45.0f, STEP = 1.25f;
    const int cols = (int)((X1 - X0) / STEP), rows = (int)((Z1 - Z0) / STEP);
    std::vector<VERTEX_3D> v;
    v.reserve((size_t)cols * rows * 6);

    auto mk = [](float x, float z) {
        VERTEX_3D vt{};
        const float y = Beach::GroundY(x, z);
        const float e = 0.25f;
        const float dx = Beach::GroundY(x + e, z) - Beach::GroundY(x - e, z);
        const float dz = Beach::GroundY(x, z + e) - Beach::GroundY(x, z - e);
        XMVECTOR n = XMVector3Normalize(XMVectorSet(-dx / (2 * e), 1.0f, -dz / (2 * e), 0.0f));
        XMStoreFloat3(&vt.Normal, n);
        vt.Position = XMFLOAT3(x, y, z);
        // 波打ち際ほど濡れて暗い砂。海の中は水越しに見える海底の色
        const float t = Beach::ShoreZ(x) - z;
        const float wet = (t < 0.0f) ? 1.0f : 1.0f - Smooth(0.0f, 3.5f, t);
        vt.Diffuse = XMFLOAT4(1.0f - 0.40f * wet, 1.0f - 0.44f * wet, 1.0f - 0.46f * wet, 1.0f);
        vt.TexCoord = XMFLOAT2(x * 0.5f, z * 0.5f);
        vt.Tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);
        return vt;
    };
    // Field と同じ巻き方（左上→右上→左下 / 右上→右下→左下。上は +z）
    for (int zi = 0; zi < rows; ++zi) {
        for (int xi = 0; xi < cols; ++xi) {
            const float x0 = X0 + STEP * xi, x1 = x0 + STEP;
            const float z0 = Z1 - STEP * zi, z1 = z0 - STEP;
            v.push_back(mk(x0, z0)); v.push_back(mk(x1, z0)); v.push_back(mk(x0, z1));
            v.push_back(mk(x1, z0)); v.push_back(mk(x1, z1)); v.push_back(mk(x0, z1));
        }
    }
    (void)Z0;
    m_SandVertexCount = (int)v.size();
    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = (UINT)(sizeof(VERTEX_3D) * v.size());
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = v.data();
        Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_SandVB);
    }

    // --- 海面（手前ほど細かい格子。頂点は sea_vs が波の形に動かし、色は sea_ps が描く）---
    {
        const int N = 160;
        auto gx = [](float u) { return 400.0f * (0.10f * u + 0.90f * u * u * u); };   // u:-1〜1 → x（中央が細かい）
        auto gz = [](float w) { return 8.0f + 392.0f * (0.15f * w + 0.85f * w * w); }; // w:0〜1 → z（岸側が細かい）
        auto sv = [](float x, float z) {
            VERTEX_3D vt{};
            vt.Position = XMFLOAT3(x, WATER_Y, z);
            vt.Normal = XMFLOAT3(0, 1, 0);
            vt.Diffuse = XMFLOAT4(1, 1, 1, 1);
            vt.TexCoord = XMFLOAT2(0, 0);
            vt.Tangent = XMFLOAT3(1, 0, 0);
            return vt;
        };
        std::vector<VERTEX_3D> q;
        q.reserve((size_t)N * N * 6);
        for (int zi = 0; zi < N; ++zi) {
            const float zLo = gz((float)zi / N), zHi = gz((float)(zi + 1) / N);
            for (int xi = 0; xi < N; ++xi) {
                const float x0 = gx(-1.0f + 2.0f * xi / N), x1 = gx(-1.0f + 2.0f * (xi + 1) / N);
                q.push_back(sv(x0, zHi)); q.push_back(sv(x1, zHi)); q.push_back(sv(x0, zLo));
                q.push_back(sv(x1, zHi)); q.push_back(sv(x1, zLo)); q.push_back(sv(x0, zLo));
            }
        }
        m_SeaVertexCount = (int)q.size();
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = (UINT)(sizeof(VERTEX_3D) * q.size());
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = q.data();
        Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_SeaVB);

        D3D11_BUFFER_DESC cbd{};
        cbd.ByteWidth = sizeof(float) * 4 * 5;
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Renderer::GetDevice()->CreateBuffer(&cbd, nullptr, &m_SeaCB);
    }

    CreateSandTexture();
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(&m_SandPS, Renderer::ResolveShaderPath("pixelShader.cso").c_str());
    Renderer::CreatePixelShader(&m_SeaPS, Renderer::ResolveShaderPath("sea_ps.cso").c_str());
    Renderer::CreateVertexShader(&m_SeaVS, &m_SeaLayout, Renderer::ResolveShaderPath("sea_vs.cso").c_str());
}

void Beach::Uninit()
{
    if (m_VertexShader) { m_VertexShader->Release(); m_VertexShader = nullptr; }
    if (m_SandPS)       { m_SandPS->Release();       m_SandPS = nullptr; }
    if (m_SeaPS)        { m_SeaPS->Release();        m_SeaPS = nullptr; }
    if (m_VertexLayout) { m_VertexLayout->Release(); m_VertexLayout = nullptr; }
    if (m_SeaVS)        { m_SeaVS->Release();        m_SeaVS = nullptr; }
    if (m_SeaLayout)    { m_SeaLayout->Release();    m_SeaLayout = nullptr; }
    m_SandVB.Reset();
    m_SeaVB.Reset();
    m_SeaCB.Reset();
    m_SandTex.Reset();
}

void Beach::Draw()
{
    if (!m_SandVB || !m_SeaVB) return;

    Renderer::SetWorldMatrix(XMMatrixIdentity());
    ID3D11DeviceContext* dc = Renderer::GetDeviceContext();
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc->IASetInputLayout(m_VertexLayout);
    dc->VSSetShader(m_VertexShader, nullptr, 0);

    // 砂浜
    {
        dc->IASetVertexBuffers(0, 1, m_SandVB.GetAddressOf(), &stride, &offset);
        dc->PSSetShader(m_SandPS, nullptr, 0);
        Renderer::SetTexture(m_SandTex.Get());
        MATERIAL m{};
        m.Diffuse = XMFLOAT4(1, 1, 1, 1);
        m.Ambient = XMFLOAT4(1, 1, 1, 1);
        m.Specular = XMFLOAT4(0.05f, 0.05f, 0.05f, 1.0f);
        m.Shininess = 8.0f;
        m.TextureEnable = TRUE;
        Renderer::SetMaterial(m);
        dc->Draw(m_SandVertexCount, 0);
    }

    // 海面（半透明。砂の上に重ねる）
    if (m_SeaPS && m_SeaVS && m_SeaCB) {
        float cb[20] = {};
        cb[0] = WorldLook::GetElapsedSeconds();
        float sun[3];
        WorldLook::GetSunDirection(sun);
        cb[4] = sun[0]; cb[5] = sun[1]; cb[6] = sun[2];
        const float* sc = WorldLook::GetSunColor();
        cb[8] = sc[0]; cb[9] = sc[1]; cb[10] = sc[2];
        const float* hz = WorldLook::GetHorizonColor();
        cb[12] = hz[0]; cb[13] = hz[1]; cb[14] = hz[2];
        const float* zn = WorldLook::GetZenithColor();
        cb[16] = zn[0]; cb[17] = zn[1]; cb[18] = zn[2];
        dc->UpdateSubresource(m_SeaCB.Get(), 0, nullptr, cb, 0, 0);
        dc->PSSetConstantBuffers(6, 1, m_SeaCB.GetAddressOf());
        dc->VSSetConstantBuffers(6, 1, m_SeaCB.GetAddressOf());

        dc->IASetInputLayout(m_SeaLayout);
        dc->VSSetShader(m_SeaVS, nullptr, 0);
        dc->IASetVertexBuffers(0, 1, m_SeaVB.GetAddressOf(), &stride, &offset);
        dc->PSSetShader(m_SeaPS, nullptr, 0);
        dc->Draw(m_SeaVertexCount, 0);
    }
}
