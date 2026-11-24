#include "grass_system.h"
#include "renderer.h"
#include <cmath>

namespace {
    struct GrassCB {
        XMFLOAT4X4 View;
        XMFLOAT4X4 Proj;
        XMFLOAT4   P0;   // x:時間 y:風の強さ z:風の速さ w:草の高さ
        XMFLOAT4   P1;   // x:風向X y:風向Z z:刃の間隔 w:1辺の本数
        XMFLOAT4   P2;   // x:幅 y:除外半幅X z:除外半幅Z w:外縁
        XMFLOAT4   P3;   // xyz:カメラ位置 w:1本あたりの頂点数
        XMFLOAT4   P4;   // x:内縁 y:高さ倍率 z:幅倍率 w:草を生やす範囲(±)
    };

    // ---- 調整用の定数 ----
    const float GRASS_HEIGHT = 0.62f;
    const float GRASS_WIDTH  = 0.030f;
    const float WIND_POWER   = 0.55f;
    const float WIND_SPEED   = 1.5f;
    const float WIND_DIR     = 0.6f;
    const float CAMP_HALF    = 21.8f;    // この内側（キャンプ）には生やさない
    const float GRASS_LIMIT  = 68.0f;    // 草を生やす範囲（±。丘の手前の平地まで）

    // 近景: 密で細い刃（カメラ周囲 約17m）
    const int   NEAR_SIDE   = 180;       // 1辺の本数（総数 = SIDE^2）
    const float NEAR_CELL   = 0.19f;     // 刃の間隔(m)。小さいほど密
    const float NEAR_OUTER  = 17.0f;
    const int   NEAR_VERTS  = 9;
    // 遠景: 疎で太く高い刃（近景の外側 〜 約40m）。遠いほど刃を大きくして隙間を埋める
    const int   FAR_SIDE    = 230;
    const float FAR_CELL    = 0.36f;
    const float FAR_INNER   = 11.0f;
    const float FAR_OUTER   = 40.0f;
    const int   FAR_VERTS   = 5;
    // 超遠景: さらに疎で太い刃（遠景の外側 〜 約66m）。キャンプ中央から見ても草原が続いて見えるように
    const int   XFAR_SIDE   = 156;
    const float XFAR_CELL   = 0.9f;
    const float XFAR_INNER  = 26.0f;
    const float XFAR_OUTER  = 66.0f;

    bool  g_Enabled = false;
    int   g_Time = 0;

    ID3D11Buffer*             g_CB = nullptr;
    ID3D11VertexShader*       g_VS = nullptr;
    ID3D11PixelShader*        g_PS = nullptr;
    ID3D11DepthStencilState*  g_Depth = nullptr;
    ID3D11RasterizerState*    g_Rs = nullptr;
}

void GrassSystem::Init()
{
    ID3D11Device* device = Renderer::GetDevice();
    if (!device) return;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = sizeof(GrassCB);
    device->CreateBuffer(&bd, nullptr, &g_CB);

    Renderer::CreateVertexShader(&g_VS, nullptr, Renderer::ResolveShaderPath("grass_vs.cso").c_str());
    Renderer::CreatePixelShader(&g_PS, Renderer::ResolveShaderPath("grass_ps.cso").c_str());

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS;
    device->CreateDepthStencilState(&dsd, &g_Depth);

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;       // 草は両面描画
    rd.DepthClipEnable = TRUE;
    device->CreateRasterizerState(&rd, &g_Rs);
}

void GrassSystem::Uninit()
{
    if (g_Rs)    { g_Rs->Release();    g_Rs = nullptr; }
    if (g_Depth) { g_Depth->Release(); g_Depth = nullptr; }
    if (g_PS)    { g_PS->Release();    g_PS = nullptr; }
    if (g_VS)    { g_VS->Release();    g_VS = nullptr; }
    if (g_CB)    { g_CB->Release();    g_CB = nullptr; }
}

void GrassSystem::Clear()
{
    g_Enabled = false;
}

void GrassSystem::SetEnabled(bool enable)
{
    g_Enabled = enable;
}

void GrassSystem::Update()
{
    if (g_Enabled) g_Time++;
}

void GrassSystem::Draw()
{
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    if (!g_Enabled || !ctx || !g_CB || !g_VS || !g_PS || !g_Depth || !g_Rs) return;

    XMMATRIX view = Renderer::GetViewMatrix();
    XMFLOAT4X4 vInv;
    XMStoreFloat4x4(&vInv, XMMatrixInverse(nullptr, view));

    GrassCB cb;
    XMStoreFloat4x4(&cb.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&cb.Proj, XMMatrixTranspose(Renderer::GetProjectionMatrix()));
    cb.P0 = XMFLOAT4((float)g_Time / 60.0f, WIND_POWER, WIND_SPEED, GRASS_HEIGHT);

    // 現在のラスタライザ状態を退避して、描画後に戻す
    ID3D11RasterizerState* oldRs = nullptr;
    ctx->RSGetState(&oldRs);

    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->VSSetShader(g_VS, nullptr, 0);
    ctx->PSSetShader(g_PS, nullptr, 0);
    ctx->VSSetConstantBuffers(8, 1, &g_CB);
    ctx->OMSetDepthStencilState(g_Depth, 0);
    ctx->RSSetState(g_Rs);

    auto drawLayer = [&](int side, float cell, int verts, float inner, float outer, float heightMul, float widthMul) {
        cb.P1 = XMFLOAT4(cosf(WIND_DIR), sinf(WIND_DIR), cell, (float)side);
        cb.P2 = XMFLOAT4(GRASS_WIDTH, CAMP_HALF, CAMP_HALF, outer);
        cb.P3 = XMFLOAT4(vInv._41, vInv._42, vInv._43, (float)verts);
        cb.P4 = XMFLOAT4(inner, heightMul, widthMul, GRASS_LIMIT);
        ctx->UpdateSubresource(g_CB, 0, nullptr, &cb, 0, 0);
        ctx->DrawInstanced((UINT)verts, (UINT)(side * side), 0, 0);
    };
    drawLayer(NEAR_SIDE, NEAR_CELL, NEAR_VERTS, 0.0f,       NEAR_OUTER, 1.0f, 1.0f);
    drawLayer(FAR_SIDE,  FAR_CELL,  FAR_VERTS,  FAR_INNER,  FAR_OUTER,  1.5f, 2.6f);
    drawLayer(XFAR_SIDE, XFAR_CELL, FAR_VERTS,  XFAR_INNER, XFAR_OUTER, 1.7f, 5.0f);

    ctx->RSSetState(oldRs);
    if (oldRs) oldRs->Release();
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
}
