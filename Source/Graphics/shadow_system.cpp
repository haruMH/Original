#include "shadow_system.h"
#include "renderer.h"
#include "gameobject.h"
#include "player.h"
#include "enemy.h"
#include "boss_enemy.h"
#include "item.h"
#include "game_constants.h"
#include <vector>
#include <cmath>
#include <algorithm>

// =================================================================
// 静的メンバの実体定義
// =================================================================
ID3D11Buffer*            ShadowSystem::m_QuadVB     = nullptr;
ID3D11VertexShader*      ShadowSystem::m_VS         = nullptr;
ID3D11PixelShader*       ShadowSystem::m_PS         = nullptr;
ID3D11InputLayout*       ShadowSystem::m_IL         = nullptr;
ID3D11DepthStencilState* ShadowSystem::m_DepthState = nullptr;

// =================================================================
// 初期化処理
// =================================================================
void ShadowSystem::Init()
{
    ID3D11Device* device = Renderer::GetDevice();
    if (!device) return;

    // 1. 地面に水平なクアッド頂点バッファの作成（XZ平面、Y=0、中心原点）
    // UV座標は (0,0) 〜 (1,1)
    VERTEX_3D quadVertices[6] = {
        { XMFLOAT3(-0.5f, 0.0f,  0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0, 0, 0) },
        { XMFLOAT3( 0.5f, 0.0f, -0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0, 0, 0) },
        { XMFLOAT3(-0.5f, 0.0f, -0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0, 0, 0) },

        { XMFLOAT3(-0.5f, 0.0f,  0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0, 0, 0) },
        { XMFLOAT3( 0.5f, 0.0f,  0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0, 0, 0) },
        { XMFLOAT3( 0.5f, 0.0f, -0.5f), XMFLOAT3(0, 1, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0, 0, 0) },
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage          = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth      = sizeof(quadVertices);
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = quadVertices;
    device->CreateBuffer(&bd, &sd, &m_QuadVB);

    // 2. シェーダーの読み込み
    // ピクセルシェーダーは専用の shadow_ps.cso を使用
    // （テクスチャやサンプラーに一切依存せず、GPU内で数学的真円を計算し、半径0.48以上をdiscardすることで四角い枠線を100%根絶）
    Renderer::CreateVertexShader(&m_VS, &m_IL, Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(&m_PS, Renderer::ResolveShaderPath("shadow_ps.cso").c_str());

    // 3. 深度ステート（テストON / 書き込みOFF）
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 半透明の丸影は深度バッファに書き込まない
    dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    device->CreateDepthStencilState(&dsd, &m_DepthState);
}

// =================================================================
// 終了処理
// =================================================================
void ShadowSystem::Uninit()
{
    if (m_DepthState) { m_DepthState->Release(); m_DepthState = nullptr; }
    if (m_IL)         { m_IL->Release();         m_IL = nullptr; }
    if (m_PS)         { m_PS->Release();         m_PS = nullptr; }
    if (m_VS)         { m_VS->Release();         m_VS = nullptr; }
    if (m_QuadVB)     { m_QuadVB->Release();     m_QuadVB = nullptr; }
}

// =================================================================
// 丸影の描画処理
// =================================================================
void ShadowSystem::Draw(const std::vector<GameObject*>& objects)
{
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    if (!ctx || !m_QuadVB || !m_VS || !m_PS) return;

    // パイプライン設定
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &stride, &offset);
    ctx->IASetInputLayout(m_IL);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetShader(m_VS, nullptr, 0);
    ctx->PSSetShader(m_PS, nullptr, 0);

    ctx->OMSetDepthStencilState(m_DepthState, 0);

    // ブレンドステート（アルファブレンド）を確実に適用
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ID3D11BlendState* blendState = Renderer::GetBlendState();
    if (blendState) {
        ctx->OMSetBlendState(blendState, blendFactor, 0xffffffff);
    }

    const float floorY = -0.99f; // 地面（Y = -1.0f）のわずか上（Z-fighting防止）

    for (GameObject* obj : objects) {
        if (!obj || obj->IsDestroy()) continue;

        ObjectType type = obj->GetObjectType();
        float baseRadius  = 0.0f;
        float baseAlpha   = 0.60f;
        float groundBaseY = -0.5f;

        if (type == ObjectType::Player) {
            Player* p = static_cast<Player*>(obj);
            if (!p->GetRenderComponent().visible) continue;
            baseRadius  = Constants::Shadow::PLAYER_RADIUS_SCALE * p->GetScale().x;
            baseAlpha   = Constants::Shadow::PLAYER_ALPHA;
            groundBaseY = -0.48f;
        }
        else if (type == ObjectType::Enemy) {
            Enemy* e = static_cast<Enemy*>(obj);
            if (e->GetEnemyState() == EnemyState::DEFEATED) continue;
            baseRadius  = Constants::Shadow::ENEMY_RADIUS_SCALE * e->GetScale().x;
            baseAlpha   = Constants::Shadow::ENEMY_ALPHA;
            groundBaseY = -0.5f;
        }
        else if (type == ObjectType::Boss) {
            BossEnemy* b = static_cast<BossEnemy*>(obj);
            baseRadius  = Constants::Shadow::BOSS_RADIUS_SCALE * b->GetScale().x;
            baseAlpha   = Constants::Shadow::BOSS_ALPHA;
            groundBaseY = Constants::Boss::GROUND_Y; // ボスの接地基準Y（1.8f）
        }
        else if (type == ObjectType::Item) {
            baseRadius  = Constants::Shadow::ITEM_RADIUS;
            baseAlpha   = Constants::Shadow::ITEM_ALPHA;
            groundBaseY = -0.5f;
        }
        else {
            continue; // 壁やフィールドなどは丸影対象外
        }

        XMFLOAT3 pos = obj->GetPosition();

        // 床からの浮遊高度（接地時は 0.0f）
        float height = (std::max)(0.0f, pos.y - groundBaseY);

        // 高さによる影の減衰（高くなるほど薄く、わずかに広がる）
        float alpha = baseAlpha / (1.0f + height * 0.45f);
        float radius = baseRadius * (1.0f + height * 0.12f);

        if (alpha <= 0.02f) continue;

        // 床面上のワールド行列
        XMMATRIX world = XMMatrixScaling(radius, 1.0f, radius) *
                         XMMatrixTranslation(pos.x, floorY, pos.z);

        Renderer::SetWorldMatrix(world);

        // マテリアル設定（Diffuse.a に影のアルファ値を渡す）
        MATERIAL mat;
        ZeroMemory(&mat, sizeof(mat));
        mat.Diffuse       = XMFLOAT4(0.0f, 0.0f, 0.0f, alpha);
        mat.Ambient       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        mat.TextureEnable = FALSE;
        Renderer::SetMaterial(mat);

        ctx->Draw(6, 0);
    }

    // 深度ステートを元に戻す
    ctx->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
}
