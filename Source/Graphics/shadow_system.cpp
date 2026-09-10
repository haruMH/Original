#include "shadow_system.h"
#include "renderer.h"
#include "gameobject.h"
#include "player.h"
#include "enemy.h"
#include "boss_enemy.h"
#include "item.h"
#include <vector>
#include <cmath>
#include <algorithm>

// =================================================================
// 静的メンバの実体定義
// =================================================================
ID3D11Buffer*            ShadowSystem::m_QuadVB        = nullptr;
ID3D11VertexShader*      ShadowSystem::m_VS            = nullptr;
ID3D11PixelShader*       ShadowSystem::m_PS            = nullptr;
ID3D11InputLayout*       ShadowSystem::m_IL            = nullptr;
ID3D11DepthStencilState* ShadowSystem::m_DepthState    = nullptr;
ID3D11ShaderResourceView* ShadowSystem::m_ShadowTexture = nullptr;

// =================================================================
// 円形グラデーションテクスチャのプロシージャル生成
// =================================================================
ID3D11ShaderResourceView* ShadowSystem::CreateCircleTexture(ID3D11Device* device)
{
    const int W = 128, H = 128;
    const float center = W * 0.5f;
    const float maxRadius = W * 0.48f;
    const float innerRadius = W * 0.15f;

    std::vector<BYTE> rgba(W * H * 4);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float dx = (float)x + 0.5f - center;
            float dy = (float)y + 0.5f - center;
            float dist = sqrtf(dx * dx + dy * dy);

            BYTE alpha = 0;
            if (dist <= innerRadius) {
                alpha = 255;
            }
            else if (dist < maxRadius) {
                float t = (dist - innerRadius) / (maxRadius - innerRadius);
                // 縁に向かって滑らかに減衰（コサインカーブ）
                float smoothT = 0.5f * (1.0f + cosf(t * XM_PI));
                alpha = static_cast<BYTE>(std::clamp(smoothT * 255.0f, 0.0f, 255.0f));
            }

            int idx = (y * W + x) * 4;
            rgba[idx + 0] = 0;   // R (黒)
            rgba[idx + 1] = 0;   // G (黒)
            rgba[idx + 2] = 0;   // B (黒)
            rgba[idx + 3] = alpha; // A (アルファマスク)
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
    initData.pSysMem          = rgba.data();
    initData.SysMemPitch      = W * 4;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device->CreateTexture2D(&td, &initData, &tex);
    if (FAILED(hr)) return nullptr;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format                    = td.Format;
    srvd.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels       = 1;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device->CreateShaderResourceView(tex, &srvd, &srv);
    tex->Release();

    return SUCCEEDED(hr) ? srv : nullptr;
}

// =================================================================
// 初期化処理
// =================================================================
void ShadowSystem::Init()
{
    ID3D11Device* device = Renderer::GetDevice();
    if (!device) return;

    // 1. 地面に水平なクアッド頂点バッファの作成（XZ平面、Y=0、中心原点）
    VERTEX_3D quadVertices[6] = {
        { XMFLOAT3(-0.5f, 0.0f,  0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 0.5f, 0.0f, -0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-0.5f, 0.0f, -0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(0.0f, 1.0f) },

        { XMFLOAT3(-0.5f, 0.0f,  0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 0.5f, 0.0f,  0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3( 0.5f, 0.0f, -0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(1.0f, 1.0f) },
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage          = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth      = sizeof(quadVertices);
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = quadVertices;
    device->CreateBuffer(&bd, &sd, &m_QuadVB);

    // 2. シェーダーの読み込み
    Renderer::CreateVertexShader(&m_VS, &m_IL, Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(&m_PS, Renderer::ResolveShaderPath("ui_ps.cso").c_str());

    // 3. 深度ステート（テストON / 書き込みOFF）
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 半透明の丸影は深度バッファに書き込まない
    dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    device->CreateDepthStencilState(&dsd, &m_DepthState);

    // 4. 円形テクスチャの生成
    m_ShadowTexture = CreateCircleTexture(device);
}

// =================================================================
// 終了処理
// =================================================================
void ShadowSystem::Uninit()
{
    if (m_ShadowTexture) { m_ShadowTexture->Release(); m_ShadowTexture = nullptr; }
    if (m_DepthState)    { m_DepthState->Release();    m_DepthState = nullptr; }
    if (m_IL)            { m_IL->Release();            m_IL = nullptr; }
    if (m_PS)            { m_PS->Release();            m_PS = nullptr; }
    if (m_VS)            { m_VS->Release();            m_VS = nullptr; }
    if (m_QuadVB)        { m_QuadVB->Release();        m_QuadVB = nullptr; }
}

// =================================================================
// 丸影の描画処理
// =================================================================
void ShadowSystem::Draw(const std::vector<GameObject*>& objects)
{
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    if (!ctx || !m_QuadVB || !m_VS || !m_PS || !m_ShadowTexture) return;

    // パイプライン設定
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &stride, &offset);
    ctx->IASetInputLayout(m_IL);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetShader(m_VS, nullptr, 0);
    ctx->PSSetShader(m_PS, nullptr, 0);

    ctx->OMSetDepthStencilState(m_DepthState, 0);
    ctx->PSSetShaderResources(0, 1, &m_ShadowTexture);

    const float floorY = -0.99f; // 地面（Y = -1.0f）のわずか上（Z-fighting防止）

    for (GameObject* obj : objects) {
        if (!obj || obj->IsDestroy()) continue;

        ObjectType type = obj->GetObjectType();
        float baseRadius = 0.0f;
        float baseAlpha = 0.60f;

        if (type == ObjectType::Player) {
            Player* p = static_cast<Player*>(obj);
            if (!p->GetRenderComponent().visible) continue;
            baseRadius = 1.4f * p->GetScale().x;
            baseAlpha = 0.65f;
        }
        else if (type == ObjectType::Enemy) {
            Enemy* e = static_cast<Enemy*>(obj);
            if (e->GetEnemyState() == EnemyState::DEFEATED) continue;
            baseRadius = 1.25f * e->GetScale().x;
            baseAlpha = 0.55f;
        }
        else if (type == ObjectType::Boss) {
            BossEnemy* b = static_cast<BossEnemy*>(obj);
            baseRadius = 3.6f * b->GetScale().x;
            baseAlpha = 0.70f;
        }
        else if (type == ObjectType::Item) {
            baseRadius = 1.0f;
            baseAlpha = 0.45f;
        }
        else {
            continue; // 壁やフィールドなどは丸影対象外
        }

        XMFLOAT3 pos = obj->GetPosition();

        // 床からの高さ
        float height = (std::max)(0.0f, pos.y - (-0.5f));

        // 高さによる影の減衰（高くなるほど薄く、わずかに広がる）
        float alpha = baseAlpha / (1.0f + height * 0.45f);
        float radius = baseRadius * (1.0f + height * 0.12f);

        if (alpha <= 0.02f) continue;

        // 床面上のワールド行列
        XMMATRIX world = XMMatrixScaling(radius, 1.0f, radius) *
                         XMMatrixTranslation(pos.x, floorY, pos.z);

        Renderer::SetWorldMatrix(world);

        MATERIAL mat;
        ZeroMemory(&mat, sizeof(mat));
        mat.Diffuse       = XMFLOAT4(0.0f, 0.0f, 0.0f, alpha);
        mat.Ambient       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        mat.TextureEnable = TRUE;
        Renderer::SetMaterial(mat);

        ctx->Draw(6, 0);
    }

    // 深度ステートを元に戻す
    ctx->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
}
