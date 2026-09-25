#include "shadow_system.h"
#include "renderer.h"
#include "gameobject.h"
#include "player.h"
#include "enemy.h"
#include "boss_enemy.h"
#include "item.h"
#include "game_constants.h"
#include "fade_system.h"
#include <vector>
#include <cmath>
#include <algorithm>

// Static member definitions
ID3D11Buffer*            ShadowSystem::m_QuadVB     = nullptr;
ID3D11VertexShader*      ShadowSystem::m_VS         = nullptr;
ID3D11PixelShader*       ShadowSystem::m_PS         = nullptr;
ID3D11InputLayout*       ShadowSystem::m_IL         = nullptr;
ID3D11DepthStencilState* ShadowSystem::m_DepthState = nullptr;

// ----------------------------------------------------------------
// Init
// ----------------------------------------------------------------
void ShadowSystem::Init()
{
    ID3D11Device* device = Renderer::GetDevice();
    if (!device) return;

    // 1. Horizontal quad VB (XZ plane, centered at origin)
    VERTEX_3D quadVertices[6] = {
        { XMFLOAT3(-0.5f, 0.0f,  0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0, 0, 0) },
        { XMFLOAT3( 0.5f, 0.0f, -0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0, 0, 0) },
        { XMFLOAT3(-0.5f, 0.0f, -0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0, 0, 0) },

        { XMFLOAT3(-0.5f, 0.0f,  0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0, 0, 0) },
        { XMFLOAT3( 0.5f, 0.0f,  0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0, 0, 0) },
        { XMFLOAT3( 0.5f, 0.0f, -0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0, 0, 0) },
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth = sizeof(quadVertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = quadVertices;
    device->CreateBuffer(&bd, &sd, &m_QuadVB);

    // 2. Load shaders (shadow_ps.cso clips to circle on GPU)
    Renderer::CreateVertexShader(&m_VS, &m_IL, Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(&m_PS, Renderer::ResolveShaderPath("shadow_ps.cso").c_str());

    // 3. Depth state: test ON, write OFF
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    device->CreateDepthStencilState(&dsd, &m_DepthState);
}

// ----------------------------------------------------------------
// Uninit
// ----------------------------------------------------------------
void ShadowSystem::Uninit()
{
    if (m_DepthState) { m_DepthState->Release(); m_DepthState = nullptr; }
    if (m_IL)         { m_IL->Release();         m_IL         = nullptr; }
    if (m_PS)         { m_PS->Release();         m_PS         = nullptr; }
    if (m_VS)         { m_VS->Release();         m_VS         = nullptr; }
    if (m_QuadVB)     { m_QuadVB->Release();     m_QuadVB     = nullptr; }
}

// ----------------------------------------------------------------
// Draw - blob shadow for each active object
// ----------------------------------------------------------------
void ShadowSystem::Draw(const std::vector<GameObject*>& objects)
{
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    if (!ctx || !m_QuadVB || !m_VS || !m_PS) return;

    // --- Scene-transition flicker guard ---
    // FadeHold: screen fully black -> skip
    FadeState fadeState = FadeSystem::GetState();
    if (fadeState == FadeState::FadeHold) return;

    float fadeAlpha = FadeSystem::GetAlpha();

    // FadeIn: suppress shadow until screen is >=70% visible (alpha < 0.3)
    // Prevents "shadow appears before scene" flash on first frames of fade-in
    if (fadeState == FadeState::FadeIn && fadeAlpha > 0.3f) return;

    // Scale shadow opacity with fade progress
    float fadeMultiplier = (std::max)(0.0f, 1.0f - fadeAlpha);
    if (fadeMultiplier <= 0.02f) return;

    // --- Pipeline setup ---
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &stride, &offset);
    ctx->IASetInputLayout(m_IL);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(m_VS, nullptr, 0);
    ctx->PSSetShader(m_PS, nullptr, 0);
    ctx->OMSetDepthStencilState(m_DepthState, 0);

    // Ensure alpha blending
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ID3D11BlendState* blendState = Renderer::GetBlendState();
    if (blendState)
    {
        ctx->OMSetBlendState(blendState, blendFactor, 0xffffffff);
    }

    const float floorY = -0.99f; // slightly above ground Y=-1.0 to avoid Z-fighting

    for (GameObject* obj : objects)
    {
        if (!obj || obj->IsDestroy()) continue;

        ObjectType type   = obj->GetObjectType();
        float baseRadius  = 0.0f;
        float baseAlpha   = 0.60f;
        float groundBaseY = -0.5f;

        if (type == ObjectType::Player)
        {
            Player* p = static_cast<Player*>(obj);
            if (!p->GetRenderComponent().visible) continue;
            baseRadius  = Constants::Shadow::PLAYER_RADIUS_SCALE * p->GetScale().x;
            baseAlpha   = Constants::Shadow::PLAYER_ALPHA;
            groundBaseY = -0.48f;
        }
        else if (type == ObjectType::Enemy)
        {
            Enemy* e = static_cast<Enemy*>(obj);
            if (e->GetEnemyState() == EnemyState::DEFEATED) continue;
            baseRadius  = Constants::Shadow::ENEMY_RADIUS_SCALE * e->GetScale().x;
            baseAlpha   = Constants::Shadow::ENEMY_ALPHA;
            groundBaseY = -0.5f;
        }
        else if (type == ObjectType::Boss)
        {
            BossEnemy* b = static_cast<BossEnemy*>(obj);
            baseRadius  = Constants::Shadow::BOSS_RADIUS_SCALE * b->GetScale().x;
            baseAlpha   = Constants::Shadow::BOSS_ALPHA;
            groundBaseY = Constants::Boss::GROUND_Y;
        }
        else if (type == ObjectType::Item)
        {
            baseRadius  = Constants::Shadow::ITEM_RADIUS;
            baseAlpha   = Constants::Shadow::ITEM_ALPHA;
            groundBaseY = -0.5f;
        }
        else if (type == ObjectType::MaterialDrop)
        {
            baseRadius  = 0.35f;
            baseAlpha   = 0.45f;
            groundBaseY = -0.5f;
        }
        else
        {
            continue; // Wall / Field -> no blob shadow
        }

        XMFLOAT3 pos = obj->GetPosition();

        // Height above ground (0 when grounded)
        float height = (std::max)(0.0f, pos.y - groundBaseY);

        // Attenuate + expand shadow as object rises
        float alpha  = (baseAlpha / (1.0f + height * 0.45f)) * fadeMultiplier;
        float radius = baseRadius * (1.0f + height * 0.12f);

        if (alpha <= 0.02f) continue;

        // World matrix: scale on XZ, translate to floor level
        XMMATRIX world = XMMatrixScaling(radius, 1.0f, radius) *
                         XMMatrixTranslation(pos.x, floorY, pos.z);
        Renderer::SetWorldMatrix(world);

        // Pass shadow alpha via material Diffuse.a
        MATERIAL mat;
        ZeroMemory(&mat, sizeof(mat));
        mat.Diffuse       = XMFLOAT4(0.0f, 0.0f, 0.0f, alpha);
        mat.Ambient       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        mat.TextureEnable = FALSE;
        Renderer::SetMaterial(mat);

        ctx->Draw(6, 0);
    }

    // Restore depth state
    ctx->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
}
