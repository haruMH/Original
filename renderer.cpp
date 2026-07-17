#include "renderer.h"
#include "manager.h"
#include "main.h"
#include "wall.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <stdio.h>
#include <wincodec.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "windowscodecs.lib")

D3D_FEATURE_LEVEL Renderer::m_FeatureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*            Renderer::m_Device           = NULL;
ID3D11DeviceContext*     Renderer::m_DeviceContext     = NULL;
IDXGISwapChain*          Renderer::m_SwapChain         = NULL;
ID3D11RenderTargetView*  Renderer::m_RenderTargetView  = NULL;
ID3D11DepthStencilView*  Renderer::m_DepthStencilView  = NULL;
ID3D11Buffer*            Renderer::m_WorldBuffer       = NULL;
ID3D11Buffer*            Renderer::m_ViewBuffer        = NULL;
ID3D11Buffer*            Renderer::m_ProjectionBuffer  = NULL;
ID3D11Buffer*            Renderer::m_MaterialBuffer    = NULL;
ID3D11Buffer*            Renderer::m_LightBuffer       = NULL;
ID3D11Buffer*            Renderer::m_ShadowVPBuffer    = NULL;
XMMATRIX                 Renderer::m_ViewMatrix        = XMMatrixIdentity();
XMMATRIX                 Renderer::m_ProjectionMatrix  = XMMatrixIdentity();
LIGHT                    Renderer::m_Light             = {};
bool                     Renderer::m_IsShadowMode      = false;
bool                     Renderer::m_IsOutlineMode     = false;
XMMATRIX                 Renderer::m_ShadowMatrix      = XMMatrixIdentity();
ID3D11SamplerState*      Renderer::m_SamplerState      = NULL;
ID3D11DepthStencilState* Renderer::m_DepthStateEnable  = NULL;
ID3D11DepthStencilState* Renderer::m_DepthStateDisable = NULL;
ID3D11DepthStencilState* Renderer::m_DepthStateOutline = NULL;
ID3D11RasterizerState*   Renderer::m_RasterizerStateCullBack = NULL;
ID3D11RasterizerState*   Renderer::m_RasterizerStateCullFront = NULL;
ID3D11BlendState*        Renderer::m_BlendState        = NULL;

ID3D11Texture2D*          Renderer::m_ShadowMapTexture = NULL;
ID3D11DepthStencilView*   Renderer::m_ShadowMapDSV     = NULL;
ID3D11ShaderResourceView* Renderer::m_ShadowMapSRV     = NULL;
ID3D11SamplerState*       Renderer::m_ShadowSampler    = NULL;
D3D11_VIEWPORT            Renderer::m_ShadowViewport   = {};

ID3D11RenderTargetView*   Renderer::m_SceneRTV = NULL;
ID3D11ShaderResourceView* Renderer::m_SceneSRV = NULL;
ID3D11RenderTargetView*   Renderer::m_LumRTV   = NULL;
ID3D11ShaderResourceView* Renderer::m_LumSRV   = NULL;
ID3D11RenderTargetView*   Renderer::m_BlurRTV  = NULL;
ID3D11ShaderResourceView* Renderer::m_BlurSRV  = NULL;

ID3D11VertexShader*       Renderer::m_OutlineVS = NULL;
ID3D11PixelShader*        Renderer::m_OutlinePS = NULL;
ID3D11VertexShader*       Renderer::m_PostVS = NULL;
ID3D11PixelShader*        Renderer::m_PostPS = NULL;
ID3D11Buffer*             Renderer::m_PostBuffer = NULL;
D3D11_VIEWPORT            Renderer::m_PostViewport = {};

ID3D11Buffer*       Renderer::m_CubeVertexBuffer = NULL;
ID3D11VertexShader* Renderer::m_CubeVertexShader = NULL;
ID3D11PixelShader*  Renderer::m_CubePixelShader  = NULL;
ID3D11InputLayout*  Renderer::m_CubeInputLayout  = NULL;
ID3D11PixelShader*  Renderer::m_TestPixelShader  = NULL;

ID3D11VertexShader*  Renderer::m_WaterVS = NULL;
ID3D11PixelShader*   Renderer::m_WaterPS = NULL;
ID3D11PixelShader*   Renderer::m_DissolvePS = NULL;
ID3D11PixelShader*   Renderer::m_RefractPS = NULL;
ID3D11Buffer*        Renderer::m_WaterParamBuffer = NULL;
ID3D11Buffer*        Renderer::m_WaterLightBuffer = NULL;
ID3D11Buffer*        Renderer::m_DissolveBuffer = NULL;
ID3D11Buffer*        Renderer::m_GlassBuffer = NULL;
ID3D11Texture2D*     Renderer::m_BackgroundCopyTexture = NULL;
ID3D11ShaderResourceView* Renderer::m_BackgroundCopySRV = NULL;

// コンスタントバッファ（CBuffer）キャッシュ（更新の最小化用）の実体定義
MATERIAL             Renderer::m_MaterialCache = {};
LIGHT                Renderer::m_LightCache = {};
XMFLOAT4X4           Renderer::m_ViewCache = {};
XMFLOAT4X4           Renderer::m_ProjectionCache = {};
bool                 Renderer::m_IsCacheInitialized = false;

void Renderer::Init() {
    // スワップチェーンの作成
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = SCREEN_WIDTH;
    sd.BufferDesc.Height = SCREEN_HEIGHT;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = GetWindow();
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &m_SwapChain, &m_Device, &m_FeatureLevel, &m_DeviceContext);

    // バックバッファからRTVを作成
    ID3D11Texture2D* pb = NULL;
    m_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pb);
    m_Device->CreateRenderTargetView(pb, NULL, &m_RenderTargetView);
    pb->Release();

    // 通常のデプスステンシルビュー作成
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = SCREEN_WIDTH;
    td.Height = SCREEN_HEIGHT;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Texture2D* pd = NULL;
    m_Device->CreateTexture2D(&td, NULL, &pd);
    m_Device->CreateDepthStencilView(pd, NULL, &m_DepthStencilView);
    pd->Release();

    // 一時的なテクスチャポインタ（バグ防止用）
    ID3D11Texture2D* tempTex = nullptr;

    // === ブルーム用レンダーターゲット ===
    D3D11_TEXTURE2D_DESC rtDesc = {};
    rtDesc.Width = SCREEN_WIDTH;
    rtDesc.Height = SCREEN_HEIGHT;
    rtDesc.MipLevels = 1;
    rtDesc.ArraySize = 1;
    rtDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtDesc.SampleDesc.Count = 1;
    rtDesc.Usage = D3D11_USAGE_DEFAULT;
    rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    // 1. メインシーン描画用バッファ (等倍)
    m_Device->CreateTexture2D(&rtDesc, NULL, &tempTex);
    m_Device->CreateRenderTargetView(tempTex, NULL, &m_SceneRTV);
    m_Device->CreateShaderResourceView(tempTex, NULL, &m_SceneSRV);
    tempTex->Release(); // RTV, SRVに紐づいたので解放してOK

    // 1/4サイズの縮小バッファ設定
    rtDesc.Width = SCREEN_WIDTH / 4;
    rtDesc.Height = SCREEN_HEIGHT / 4;
    m_PostViewport.Width = (float)rtDesc.Width;
    m_PostViewport.Height = (float)rtDesc.Height;
    m_PostViewport.MinDepth = 0.0f;
    m_PostViewport.MaxDepth = 1.0f;

    // 2. 高輝度抽出用バッファ (1/4)
    m_Device->CreateTexture2D(&rtDesc, NULL, &tempTex);
    m_Device->CreateRenderTargetView(tempTex, NULL, &m_LumRTV);
    m_Device->CreateShaderResourceView(tempTex, NULL, &m_LumSRV);
    tempTex->Release();

    // 3. ガウシアンぼかし用バッファ (1/4)
    m_Device->CreateTexture2D(&rtDesc, NULL, &tempTex);
    m_Device->CreateRenderTargetView(tempTex, NULL, &m_BlurRTV);
    m_Device->CreateShaderResourceView(tempTex, NULL, &m_BlurSRV);
    tempTex->Release();

    // === シャドウマップ ===
    D3D11_TEXTURE2D_DESC sdDesc = {};
    sdDesc.Width = 2048;
    sdDesc.Height = 2048;
    sdDesc.MipLevels = 1;
    sdDesc.ArraySize = 1;
    // 高精度かつ安全なR32系フォーマットに変更
    sdDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    sdDesc.SampleDesc.Count = 1;
    sdDesc.Usage = D3D11_USAGE_DEFAULT;
    sdDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    // メンバ変数にしっかりと実体を確保
    m_Device->CreateTexture2D(&sdDesc, NULL, &m_ShadowMapTexture);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    m_Device->CreateDepthStencilView(m_ShadowMapTexture, &dsvDesc, &m_ShadowMapDSV);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    m_Device->CreateShaderResourceView(m_ShadowMapTexture, &srvDesc, &m_ShadowMapSRV);

    // ※シャドウマップテクスチャはUninitで解放するため、ここではReleaseしません！

    m_ShadowViewport.Width = 2048.0f;
    m_ShadowViewport.Height = 2048.0f;
    m_ShadowViewport.MinDepth = 0.0f;
    m_ShadowViewport.MaxDepth = 1.0f;

    // === 定数バッファ ===
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    bd.ByteWidth = sizeof(XMFLOAT4X4);
    m_Device->CreateBuffer(&bd, NULL, &m_WorldBuffer);
    m_DeviceContext->VSSetConstantBuffers(0, 1, &m_WorldBuffer);

    m_Device->CreateBuffer(&bd, NULL, &m_ViewBuffer);
    m_DeviceContext->VSSetConstantBuffers(1, 1, &m_ViewBuffer);

    m_Device->CreateBuffer(&bd, NULL, &m_ProjectionBuffer);
    m_DeviceContext->VSSetConstantBuffers(2, 1, &m_ProjectionBuffer);

    m_Device->CreateBuffer(&bd, NULL, &m_ShadowVPBuffer);
    m_DeviceContext->VSSetConstantBuffers(5, 1, &m_ShadowVPBuffer);

    bd.ByteWidth = sizeof(MATERIAL);
    m_Device->CreateBuffer(&bd, NULL, &m_MaterialBuffer);
    m_DeviceContext->PSSetConstantBuffers(3, 1, &m_MaterialBuffer);

    bd.ByteWidth = sizeof(LIGHT);
    m_Device->CreateBuffer(&bd, NULL, &m_LightBuffer);
    m_DeviceContext->VSSetConstantBuffers(4, 1, &m_LightBuffer);
    m_DeviceContext->PSSetConstantBuffers(4, 1, &m_LightBuffer);

    bd.ByteWidth = sizeof(POSTEFFECT);
    m_Device->CreateBuffer(&bd, NULL, &m_PostBuffer);

    // 深度ステート
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    m_Device->CreateDepthStencilState(&dsd, &m_DepthStateEnable);

    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    m_Device->CreateDepthStencilState(&dsd, &m_DepthStateDisable);

    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    m_Device->CreateDepthStencilState(&dsd, &m_DepthStateOutline);

    // ブレンドステート
    D3D11_BLEND_DESC bl = {};
    bl.RenderTarget[0].BlendEnable = TRUE;
    bl.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bl.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bl.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bl.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bl.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_Device->CreateBlendState(&bl, &m_BlendState);
    float f[4] = { 0,0,0,0 };
    m_DeviceContext->OMSetBlendState(m_BlendState, f, 0xffffffff);

    // サンプラーステート
    D3D11_SAMPLER_DESC sm = {};
    sm.Filter = D3D11_FILTER_ANISOTROPIC;
    sm.AddressU = sm.AddressV = sm.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sm.MaxAnisotropy = 16;
    sm.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sm.MaxLOD = D3D11_FLOAT32_MAX;
    m_Device->CreateSamplerState(&sm, &m_SamplerState);
    m_DeviceContext->PSSetSamplers(0, 1, &m_SamplerState);

    // シャドウ用比較サンプラー
    sm = {};
    sm.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    sm.AddressU = sm.AddressV = sm.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    sm.BorderColor[0] = sm.BorderColor[1] = sm.BorderColor[2] = sm.BorderColor[3] = 1.0f;
    sm.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    m_Device->CreateSamplerState(&sm, &m_ShadowSampler);
    m_DeviceContext->PSSetSamplers(1, 1, &m_ShadowSampler);

    // ラスタライザステート
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.DepthClipEnable = TRUE;
    m_Device->CreateRasterizerState(&rd, &m_RasterizerStateCullBack);

    rd.CullMode = D3D11_CULL_FRONT; // アウトライン用（裏面を描画）
    rd.DepthBias = 5000;            // 固定深度バイアスを設定して奥に押しやる
    rd.SlopeScaledDepthBias = 2.0f; // 傾斜（スロープ）に応じた深度バイアスを設定
    m_Device->CreateRasterizerState(&rd, &m_RasterizerStateCullFront);
    m_DeviceContext->RSSetState(m_RasterizerStateCullBack);

    // アウトライン用シェーダー読み込み（2-1 対応: ResolveShaderPath でパス解決を一元化）
    FILE* fov = fopen(ResolveShaderPath("outline_vs.cso").c_str(), "rb");
    if (fov) {
        fseek(fov, 0, SEEK_END); long s = ftell(fov); fseek(fov, 0, SEEK_SET);
        unsigned char* b = new unsigned char[s]; fread(b, s, 1, fov); fclose(fov);
        m_Device->CreateVertexShader(b, s, NULL, &m_OutlineVS); delete[] b;
    }
    FILE* fop = fopen(ResolveShaderPath("outline_ps.cso").c_str(), "rb");
    if (fop) {
        fseek(fop, 0, SEEK_END); long s = ftell(fop); fseek(fop, 0, SEEK_SET);
        unsigned char* b = new unsigned char[s]; fread(b, s, 1, fop); fclose(fop);
        m_Device->CreatePixelShader(b, s, NULL, &m_OutlinePS); delete[] b;
    }

    // ポストエフェクト用シェーダー読み込み（2-1 対応: ResolveShaderPath でパス解決を一元化）
    FILE* fvs = fopen(ResolveShaderPath("postEffect_vs.cso").c_str(), "rb");
    if (fvs) {
        fseek(fvs, 0, SEEK_END); long s = ftell(fvs); fseek(fvs, 0, SEEK_SET);
        unsigned char* b = new unsigned char[s]; fread(b, s, 1, fvs); fclose(fvs);
        m_Device->CreateVertexShader(b, s, NULL, &m_PostVS); delete[] b;
    }
    FILE* fps = fopen(ResolveShaderPath("postEffect_ps.cso").c_str(), "rb");
    if (fps) {
        fseek(fps, 0, SEEK_END); long s = ftell(fps); fseek(fps, 0, SEEK_SET);
        unsigned char* b = new unsigned char[s]; fread(b, s, 1, fps); fclose(fps);
        m_Device->CreatePixelShader(b, s, NULL, &m_PostPS); delete[] b;
    }

    // === キューブ共通リソースの作成 ===
    {
        XMFLOAT3 ltf(-0.5f, 0.5f, 0.5f), rtf(0.5f, 0.5f, 0.5f);
        XMFLOAT3 lbf(-0.5f, -0.5f, 0.5f), rbf(0.5f, -0.5f, 0.5f);
        XMFLOAT3 ltb(-0.5f, 0.5f, -0.5f), rtb(0.5f, 0.5f, -0.5f);
        XMFLOAT3 lbb(-0.5f, -0.5f, -0.5f), rbb(0.5f, -0.5f, -0.5f);
        VERTEX_3D v[36]; int k = 0;

        // 前面
        // 通常ライティング用の面法線を使用する。
        // アウトライン「膨張法」用のスムーズ法線は outline_vs.hlsl 内で
        // position.xyz を normalize して計算するため、頂点バッファは
        // 面法線（各面に垂直な方向）のまま維持する。
        v[k].Position = ltf; v[k].Normal = XMFLOAT3(0, 0, 1); v[k].TexCoord = XMFLOAT2(0, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rbf; v[k].Normal = XMFLOAT3(0, 0, 1); v[k].TexCoord = XMFLOAT2(1, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rtf; v[k].Normal = XMFLOAT3(0, 0, 1); v[k].TexCoord = XMFLOAT2(1, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = ltf; v[k].Normal = XMFLOAT3(0, 0, 1); v[k].TexCoord = XMFLOAT2(0, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = lbf; v[k].Normal = XMFLOAT3(0, 0, 1); v[k].TexCoord = XMFLOAT2(0, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rbf; v[k].Normal = XMFLOAT3(0, 0, 1); v[k].TexCoord = XMFLOAT2(1, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        // 右面
        v[k].Position = rtf; v[k].Normal = XMFLOAT3(1, 0, 0); v[k].TexCoord = XMFLOAT2(0, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rbb; v[k].Normal = XMFLOAT3(1, 0, 0); v[k].TexCoord = XMFLOAT2(1, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rtb; v[k].Normal = XMFLOAT3(1, 0, 0); v[k].TexCoord = XMFLOAT2(1, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rtf; v[k].Normal = XMFLOAT3(1, 0, 0); v[k].TexCoord = XMFLOAT2(0, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rbf; v[k].Normal = XMFLOAT3(1, 0, 0); v[k].TexCoord = XMFLOAT2(0, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rbb; v[k].Normal = XMFLOAT3(1, 0, 0); v[k].TexCoord = XMFLOAT2(1, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        // 後面
        v[k].Position = rtb; v[k].Normal = XMFLOAT3(0, 0, -1); v[k].TexCoord = XMFLOAT2(0, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = lbb; v[k].Normal = XMFLOAT3(0, 0, -1); v[k].TexCoord = XMFLOAT2(1, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = ltb; v[k].Normal = XMFLOAT3(0, 0, -1); v[k].TexCoord = XMFLOAT2(1, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rtb; v[k].Normal = XMFLOAT3(0, 0, -1); v[k].TexCoord = XMFLOAT2(0, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rbb; v[k].Normal = XMFLOAT3(0, 0, -1); v[k].TexCoord = XMFLOAT2(0, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = lbb; v[k].Normal = XMFLOAT3(0, 0, -1); v[k].TexCoord = XMFLOAT2(1, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        // 左面
        v[k].Position = ltb; v[k].Normal = XMFLOAT3(-1, 0, 0); v[k].TexCoord = XMFLOAT2(0, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = lbf; v[k].Normal = XMFLOAT3(-1, 0, 0); v[k].TexCoord = XMFLOAT2(1, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = ltf; v[k].Normal = XMFLOAT3(-1, 0, 0); v[k].TexCoord = XMFLOAT2(1, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = ltb; v[k].Normal = XMFLOAT3(-1, 0, 0); v[k].TexCoord = XMFLOAT2(0, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = lbb; v[k].Normal = XMFLOAT3(-1, 0, 0); v[k].TexCoord = XMFLOAT2(0, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = lbf; v[k].Normal = XMFLOAT3(-1, 0, 0); v[k].TexCoord = XMFLOAT2(1, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        // 上面
        v[k].Position = ltb; v[k].Normal = XMFLOAT3(0, 1, 0); v[k].TexCoord = XMFLOAT2(0, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rtf; v[k].Normal = XMFLOAT3(0, 1, 0); v[k].TexCoord = XMFLOAT2(1, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rtb; v[k].Normal = XMFLOAT3(0, 1, 0); v[k].TexCoord = XMFLOAT2(1, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = ltb; v[k].Normal = XMFLOAT3(0, 1, 0); v[k].TexCoord = XMFLOAT2(0, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = ltf; v[k].Normal = XMFLOAT3(0, 1, 0); v[k].TexCoord = XMFLOAT2(0, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rtf; v[k].Normal = XMFLOAT3(0, 1, 0); v[k].TexCoord = XMFLOAT2(1, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        // 下面
        v[k].Position = lbf; v[k].Normal = XMFLOAT3(0, -1, 0); v[k].TexCoord = XMFLOAT2(0, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rbb; v[k].Normal = XMFLOAT3(0, -1, 0); v[k].TexCoord = XMFLOAT2(1, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rbf; v[k].Normal = XMFLOAT3(0, -1, 0); v[k].TexCoord = XMFLOAT2(1, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = lbf; v[k].Normal = XMFLOAT3(0, -1, 0); v[k].TexCoord = XMFLOAT2(0, 0); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = lbb; v[k].Normal = XMFLOAT3(0, -1, 0); v[k].TexCoord = XMFLOAT2(0, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;
        v[k].Position = rbb; v[k].Normal = XMFLOAT3(0, -1, 0); v[k].TexCoord = XMFLOAT2(1, 1); v[k].Diffuse = XMFLOAT4(1, 1, 1, 1); k++;

        D3D11_BUFFER_DESC bdc = {};
        bdc.Usage = D3D11_USAGE_DEFAULT;
        bdc.ByteWidth = sizeof(VERTEX_3D) * 36;
        bdc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA sdc = {};
        sdc.pSysMem = v;
        m_Device->CreateBuffer(&bdc, &sdc, &m_CubeVertexBuffer);

        // キューブ用シェーダー読み込み（2-1 対応: ResolveShaderPath でパス解決を一元化）
        CreateVertexShader(&m_CubeVertexShader, &m_CubeInputLayout, ResolveShaderPath("vertexShader.cso").c_str());
        CreatePixelShader(&m_CubePixelShader,   ResolveShaderPath("pixelShader.cso").c_str());
        CreatePixelShader(&m_TestPixelShader,   ResolveShaderPath("test_ps.cso").c_str());
        CreateVertexShader(&m_WaterVS, nullptr,  ResolveShaderPath("WaterVS.cso").c_str());
        CreatePixelShader(&m_WaterPS,            ResolveShaderPath("WaterPS.cso").c_str());
        CreatePixelShader(&m_DissolvePS,         ResolveShaderPath("DissolvePS.cso").c_str());
        CreatePixelShader(&m_RefractPS,          ResolveShaderPath("RefractPS.cso").c_str());

        // 新規エフェクト用定数バッファの生成
        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        // WaterParamCB (16 bytes)
        cbd.ByteWidth = sizeof(WaterParamCB);
        m_Device->CreateBuffer(&cbd, nullptr, &m_WaterParamBuffer);

        // WaterLightCB (96 bytes)
        cbd.ByteWidth = sizeof(WaterLightCB);
        m_Device->CreateBuffer(&cbd, nullptr, &m_WaterLightBuffer);

        // DissolveCB (32 bytes)
        cbd.ByteWidth = sizeof(DissolveCB);
        m_Device->CreateBuffer(&cbd, nullptr, &m_DissolveBuffer);

        // GlassCB (32 bytes)
        cbd.ByteWidth = sizeof(GlassCB);
        m_Device->CreateBuffer(&cbd, nullptr, &m_GlassBuffer);

        // 屈折エフェクト用背景コピーテクスチャの生成
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = SCREEN_WIDTH;
        td.Height = SCREEN_HEIGHT;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        m_Device->CreateTexture2D(&td, nullptr, &m_BackgroundCopyTexture);
        m_Device->CreateShaderResourceView(m_BackgroundCopyTexture, nullptr, &m_BackgroundCopySRV);
    }

    // ImGui の初期化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(GetWindow());
    ImGui_ImplDX11_Init(m_Device, m_DeviceContext);



    // キャッシュ機構の初期化完了フラグを立てる
    m_IsCacheInitialized = true;
}
void Renderer::Uninit() {
    if(m_SceneRTV)m_SceneRTV->Release(); if(m_SceneSRV)m_SceneSRV->Release();
    if(m_LumRTV)m_LumRTV->Release(); if(m_LumSRV)m_LumSRV->Release();
    if(m_BlurRTV)m_BlurRTV->Release(); if(m_BlurSRV)m_BlurSRV->Release();
    if(m_OutlineVS)m_OutlineVS->Release(); if(m_OutlinePS)m_OutlinePS->Release(); if(m_RasterizerStateCullBack)m_RasterizerStateCullBack->Release(); if(m_RasterizerStateCullFront)m_RasterizerStateCullFront->Release();
    if(m_PostVS)m_PostVS->Release(); if(m_PostPS)m_PostPS->Release(); if(m_PostBuffer)m_PostBuffer->Release();
    if(m_ShadowMapTexture) m_ShadowMapTexture->Release(); if(m_ShadowMapDSV) m_ShadowMapDSV->Release(); if(m_ShadowMapSRV) m_ShadowMapSRV->Release(); if(m_ShadowSampler) m_ShadowSampler->Release();
    if(m_ShadowVPBuffer) m_ShadowVPBuffer->Release(); if(m_WorldBuffer)m_WorldBuffer->Release(); if(m_ViewBuffer)m_ViewBuffer->Release(); if(m_ProjectionBuffer)m_ProjectionBuffer->Release();
    if(m_MaterialBuffer)m_MaterialBuffer->Release(); if(m_LightBuffer)m_LightBuffer->Release(); if(m_SamplerState)m_SamplerState->Release();
    if(m_DepthStateEnable)m_DepthStateEnable->Release(); if(m_DepthStateDisable)m_DepthStateDisable->Release(); if(m_DepthStateOutline)m_DepthStateOutline->Release(); if(m_BlendState)m_BlendState->Release();
    if(m_RenderTargetView)m_RenderTargetView->Release(); if(m_DepthStencilView)m_DepthStencilView->Release(); if(m_SwapChain)m_SwapChain->Release();
    if(m_DeviceContext)m_DeviceContext->Release(); if(m_Device)m_Device->Release();
    // ImGui の終了処理
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if(m_CubeVertexBuffer) m_CubeVertexBuffer->Release();
    if(m_CubeInputLayout)  m_CubeInputLayout->Release();
    if(m_CubePixelShader)  m_CubePixelShader->Release();
    if(m_CubeVertexShader) m_CubeVertexShader->Release();
    if(m_TestPixelShader)  m_TestPixelShader->Release();

    if(m_WaterVS) m_WaterVS->Release();
    if(m_WaterPS) m_WaterPS->Release();
    if(m_DissolvePS) m_DissolvePS->Release();
    if(m_RefractPS) m_RefractPS->Release();
    if(m_WaterParamBuffer) m_WaterParamBuffer->Release();
    if(m_WaterLightBuffer) m_WaterLightBuffer->Release();
    if(m_DissolveBuffer) m_DissolveBuffer->Release();
    if(m_GlassBuffer) m_GlassBuffer->Release();
    if(m_BackgroundCopyTexture) m_BackgroundCopyTexture->Release();
    if(m_BackgroundCopySRV) m_BackgroundCopySRV->Release();
}

void Renderer::BeginShadowPass() {
    ID3D11ShaderResourceView* nullSRV = NULL; m_DeviceContext->PSSetShaderResources(2, 1, &nullSRV);
    m_DeviceContext->OMSetRenderTargets(0, NULL, m_ShadowMapDSV);
    m_DeviceContext->ClearDepthStencilView(m_ShadowMapDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
    m_DeviceContext->RSSetViewports(1, &m_ShadowViewport);
}

void Renderer::EndShadowPass() {
    // シャドウマップの書き込み（DSV）を解除する
    m_DeviceContext->OMSetRenderTargets(0, NULL, NULL);

    D3D11_VIEWPORT vp; vp.Width = (FLOAT)SCREEN_WIDTH; vp.Height = (FLOAT)SCREEN_HEIGHT; vp.MinDepth = 0; vp.MaxDepth = 1; vp.TopLeftX = 0; vp.TopLeftY = 0;
    m_DeviceContext->RSSetViewports(1, &vp);

    // DSVが解除されたので、安全にテクスチャ（SRV）として読み込めるようにバインド
    m_DeviceContext->PSSetShaderResources(2, 1, &m_ShadowMapSRV);
}

void Renderer::BeginOutlinePass() {
    m_IsOutlineMode = true;
    m_DeviceContext->RSSetState(m_RasterizerStateCullFront); // 裏面を描画
    m_DeviceContext->OMSetDepthStencilState(m_DepthStateOutline, 0);
    m_DeviceContext->VSSetShader(m_OutlineVS, NULL, 0);      // 頂点を膨らませる
    m_DeviceContext->PSSetShader(m_OutlinePS, NULL, 0);      // 黒く塗る
}

void Renderer::EndOutlinePass() {
    m_IsOutlineMode = false;
    m_DeviceContext->RSSetState(m_RasterizerStateCullBack);  // 表面描画に戻す
    m_DeviceContext->OMSetDepthStencilState(m_DepthStateEnable, 0);
}

void Renderer::Begin() {
    // バックバッファではなく、SceneRTV（中間バッファ）に描画する
    float c[4] = {0.1f, 0.1f, 0.2f, 1.0f};
    m_DeviceContext->OMSetRenderTargets(1, &m_SceneRTV, m_DepthStencilView);
    m_DeviceContext->ClearRenderTargetView(m_SceneRTV, c);
    m_DeviceContext->ClearDepthStencilView(m_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void Renderer::BeginNewFrame() {
    // ImGui のフレーム開始
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Renderer::End() {
    // ─── ブルームポストプロセス ─────────────────────────────
    m_DeviceContext->IASetInputLayout(NULL);
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_DeviceContext->VSSetShader(m_PostVS, NULL, 0);
    m_DeviceContext->PSSetShader(m_PostPS, NULL, 0);
    m_DeviceContext->OMSetDepthStencilState(m_DepthStateDisable, 0);
    ID3D11ShaderResourceView* nullSRV[] = { NULL, NULL };

    POSTEFFECT pe;
    ZeroMemory(&pe, sizeof(pe));
    pe.Threshold     = 0.85f;
    pe.BlurIntensity = 0.6f;

    // 1. 高輝度抽出 (SceneRTV → LumRTV)
    m_DeviceContext->RSSetViewports(1, &m_PostViewport);
    m_DeviceContext->OMSetRenderTargets(1, &m_LumRTV, NULL);
    m_DeviceContext->PSSetShaderResources(0, 1, &m_SceneSRV);
    pe.Mode = 0;
    m_DeviceContext->UpdateSubresource(m_PostBuffer, 0, NULL, &pe, 0, 0);
    m_DeviceContext->PSSetConstantBuffers(0, 1, &m_PostBuffer);
    m_DeviceContext->Draw(3, 0);

    // 2. 横ぼかし (LumRTV → BlurRTV)
    m_DeviceContext->PSSetShaderResources(0, 2, nullSRV);
    m_DeviceContext->OMSetRenderTargets(1, &m_BlurRTV, NULL);
    m_DeviceContext->PSSetShaderResources(0, 1, &m_LumSRV);
    pe.Mode = 1;
    m_DeviceContext->UpdateSubresource(m_PostBuffer, 0, NULL, &pe, 0, 0);
    m_DeviceContext->Draw(3, 0);

    // 3. 縦ぼかし (BlurRTV → LumRTV)
    m_DeviceContext->PSSetShaderResources(0, 2, nullSRV);
    m_DeviceContext->OMSetRenderTargets(1, &m_LumRTV, NULL);
    m_DeviceContext->PSSetShaderResources(0, 1, &m_BlurSRV);
    pe.Mode = 2;
    m_DeviceContext->UpdateSubresource(m_PostBuffer, 0, NULL, &pe, 0, 0);
    m_DeviceContext->Draw(3, 0);

    // 4. 合成 (SceneRTV + LumRTV → バックバッファ)
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)SCREEN_WIDTH; vp.Height = (FLOAT)SCREEN_HEIGHT;
    vp.MinDepth = 0; vp.MaxDepth = 1; vp.TopLeftX = 0; vp.TopLeftY = 0;
    m_DeviceContext->RSSetViewports(1, &vp);
    m_DeviceContext->PSSetShaderResources(0, 2, nullSRV);
    m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, NULL);
    m_DeviceContext->PSSetShaderResources(0, 1, &m_SceneSRV);
    m_DeviceContext->PSSetShaderResources(1, 1, &m_LumSRV);
    pe.Mode = 3;
    pe.SlowMotionIntensity = Manager::GetSlowMotionIntensity();
    m_DeviceContext->UpdateSubresource(m_PostBuffer, 0, NULL, &pe, 0, 0);
    m_DeviceContext->Draw(3, 0);

    // SRV バインド解除（次フレームへの干渉を防ぐ）
    m_DeviceContext->PSSetShaderResources(0, 2, nullSRV);
    m_DeviceContext->OMSetDepthStencilState(m_DepthStateEnable, 0);

    // ImGui のレンダリングと描画実行
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    m_SwapChain->Present(1, 0);
}
void Renderer::SetWorldMatrix(XMMATRIX m) {
    XMMATRIX f = m; if(m_IsShadowMode) f = m * m_ShadowMatrix;
    XMFLOAT4X4 wf; XMStoreFloat4x4(&wf, XMMatrixTranspose(f)); m_DeviceContext->UpdateSubresource(m_WorldBuffer, 0, NULL, &wf, 0, 0);
}
void Renderer::SetViewMatrix(XMMATRIX m) {
    m_ViewMatrix = m;
    XMFLOAT4X4 vf;
    XMStoreFloat4x4(&vf, XMMatrixTranspose(m));
    if (!m_IsCacheInitialized || memcmp(&m_ViewCache, &vf, sizeof(XMFLOAT4X4)) != 0) {
        m_ViewCache = vf;
        m_DeviceContext->UpdateSubresource(m_ViewBuffer, 0, NULL, &vf, 0, 0);
    }
}
void Renderer::SetProjectionMatrix(XMMATRIX m) {
    m_ProjectionMatrix = m;
    XMFLOAT4X4 pf;
    XMStoreFloat4x4(&pf, XMMatrixTranspose(m));
    if (!m_IsCacheInitialized || memcmp(&m_ProjectionCache, &pf, sizeof(XMFLOAT4X4)) != 0) {
        m_ProjectionCache = pf;
        m_DeviceContext->UpdateSubresource(m_ProjectionBuffer, 0, NULL, &pf, 0, 0);
    }
}
void Renderer::SetShadowVPMatrix(XMMATRIX m) { XMFLOAT4X4 svp; XMStoreFloat4x4(&svp, XMMatrixTranspose(m)); m_DeviceContext->UpdateSubresource(m_ShadowVPBuffer, 0, NULL, &svp, 0, 0); }
void Renderer::SetMaterial(MATERIAL m) {
    if (!m_IsCacheInitialized || memcmp(&m_MaterialCache, &m, sizeof(MATERIAL)) != 0) {
        m_MaterialCache = m;
        m_DeviceContext->UpdateSubresource(m_MaterialBuffer, 0, NULL, &m, 0, 0);
    }
}
void Renderer::SetLight(LIGHT l) {
    m_Light = l;
    if (!m_IsCacheInitialized || memcmp(&m_LightCache, &l, sizeof(LIGHT)) != 0) {
        m_LightCache = l;
        m_DeviceContext->UpdateSubresource(m_LightBuffer, 0, NULL, &l, 0, 0);
    }
}
void Renderer::SetCameraPosition(XMFLOAT3 p) {
    m_Light.CameraPosition = XMFLOAT4(p.x, p.y, p.z, 1.0f);
    if (!m_IsCacheInitialized || memcmp(&m_LightCache, &m_Light, sizeof(LIGHT)) != 0) {
        m_LightCache = m_Light;
        m_DeviceContext->UpdateSubresource(m_LightBuffer, 0, NULL, &m_Light, 0, 0);
    }
}
void Renderer::SetShadowMode(bool e) { m_IsShadowMode = e; }
void Renderer::SetShadowMatrix(XMMATRIX m) { m_ShadowMatrix = m; }
// ─────────────────────────────────────────────
// シェーダーパス解決（2-1 対応）
// テクスチャの ResourceManager::GetTexture と同じ考え方で一元化する。
// 呼び出し元は常にベース名（例: "vertexShader.cso"）を渡すだけでよい。
// ─────────────────────────────────────────────
std::string Renderer::ResolveShaderPath(const std::string& fileName)
{
#ifdef NDEBUG
    // リリースビルド: "Assets/shader/" で始まっていなければプレフィックスを付与
    if (fileName.rfind("Assets/shader/", 0) != 0) {
        return "Assets/shader/" + fileName;
    }
    return fileName;
#else
    // デバッグビルド: ベース名のみ（カレントディレクトリから読み込み）
    const std::string prefix = "Assets/shader/";
    if (fileName.rfind(prefix, 0) == 0) {
        return fileName.substr(prefix.size());
    }
    return fileName;
#endif
}

void Renderer::CreateVertexShader(ID3D11VertexShader** vs, ID3D11InputLayout** il, const char* name) {
    FILE* f = fopen(name, "rb"); if(!f) return; fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char* b = new unsigned char[s]; fread(b, s, 1, f); fclose(f); m_Device->CreateVertexShader(b, s, NULL, vs);
    if (il) {
        D3D11_INPUT_ELEMENT_DESC l[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        m_Device->CreateInputLayout(l, 5, b, s, il);
    }
    delete[] b;
}
void Renderer::CreatePixelShader(ID3D11PixelShader** ps, const char* name) {
    FILE* f = fopen(name, "rb"); if(!f) return; fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char* b = new unsigned char[s]; fread(b, s, 1, f); fclose(f); m_Device->CreatePixelShader(b, s, NULL, ps); delete[] b;
}
void Renderer::CreateTexture(const char* name, ID3D11ShaderResourceView** tex) {
    *tex = nullptr;
    if (!name || strlen(name) == 0) return;

    IWICImagingFactory* fc = NULL; 
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fc));
    if (FAILED(hr) || !fc) return;

    WCHAR wn[MAX_PATH]; 
    MultiByteToWideChar(CP_ACP, 0, name, -1, wn, MAX_PATH);

    IWICBitmapDecoder* dec = NULL; 
    hr = fc->CreateDecoderFromFilename(wn, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec);
    if (FAILED(hr) || !dec) {
        fc->Release();
        return;
    }

    IWICBitmapFrameDecode* fr = NULL; 
    hr = dec->GetFrame(0, &fr);
    if (FAILED(hr) || !fr) {
        dec->Release();
        fc->Release();
        return;
    }

    IWICFormatConverter* cnv = NULL; 
    hr = fc->CreateFormatConverter(&cnv);
    if (FAILED(hr) || !cnv) {
        fr->Release();
        dec->Release();
        fc->Release();
        return;
    }

    hr = cnv->Initialize(fr, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        cnv->Release();
        fr->Release();
        dec->Release();
        fc->Release();
        return;
    }

    UINT w, h; 
    cnv->GetSize(&w, &h); 
    BYTE* px = new BYTE[w*h*4]; 
    hr = cnv->CopyPixels(NULL, w*4, w*h*4, px);
    if (FAILED(hr)) {
        delete[] px;
        cnv->Release();
        fr->Release();
        dec->Release();
        fc->Release();
        return;
    }

    D3D11_TEXTURE2D_DESC td; 
    ZeroMemory(&td, sizeof(td)); 
    td.Width = w; 
    td.Height = h; 
    td.MipLevels = 0; 
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; 
    td.SampleDesc.Count = 1; 
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; 
    td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* t2 = NULL; 
    hr = m_Device->CreateTexture2D(&td, NULL, &t2);
    if (SUCCEEDED(hr) && t2) {
        m_DeviceContext->UpdateSubresource(t2, 0, NULL, px, w*4, 0);
        hr = m_Device->CreateShaderResourceView(t2, NULL, tex); 
        if (SUCCEEDED(hr) && *tex) {
            m_DeviceContext->GenerateMips(*tex);
        }
        t2->Release(); 
    }
    
    delete[] px; 
    cnv->Release(); 
    fr->Release(); 
    dec->Release(); 
    fc->Release();
}
void Renderer::SetTexture(ID3D11ShaderResourceView* t) { m_DeviceContext->PSSetShaderResources(0, 1, &t); }
void Renderer::SetNormalMap(ID3D11ShaderResourceView* t) { m_DeviceContext->PSSetShaderResources(1, 1, &t); }
void Renderer::SetupCubeDraw() {
    UINT stride = sizeof(VERTEX_3D), offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, &m_CubeVertexBuffer, &stride, &offset);
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_DeviceContext->IASetInputLayout(m_CubeInputLayout);
    
    // アウトライン描画パスの場合は、専用のアウトラインシェーダーをバインドする
    if (m_IsOutlineMode) {
        m_DeviceContext->VSSetShader(m_OutlineVS, NULL, 0);
        m_DeviceContext->PSSetShader(m_OutlinePS, NULL, 0);
    } else {
        m_DeviceContext->VSSetShader(m_CubeVertexShader, NULL, 0);
        m_DeviceContext->PSSetShader(m_CubePixelShader, NULL, 0);
    }
}

// Rendererクラス側（renderer.cpp など）に実装を追加
void Renderer::DrawCube(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* texture)
{
    SetWorldMatrix(worldMatrix);
    SetTexture(texture);

    ID3D11ShaderResourceView* nullSRV = NULL;
    m_DeviceContext->PSSetShaderResources(1, 1, &nullSRV);

    // 共通のマテリアル設定
    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse = XMFLOAT4(1, 1, 1, 1);
    material.Ambient = XMFLOAT4(1, 1, 1, 1);
    material.Specular = XMFLOAT4(0.6f, 0.6f, 0.6f, 1);
    material.Shininess = 20.0f;
    material.TextureEnable = TRUE;
    SetMaterial(material);

    SetupCubeDraw();
    GetDeviceContext()->Draw(36, 0);
}

void Renderer::DrawCubeWithTestShader(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* texture)
{
    SetWorldMatrix(worldMatrix);
    SetTexture(texture);

    ID3D11ShaderResourceView* nullSRV = NULL;
    m_DeviceContext->PSSetShaderResources(1, 1, &nullSRV);

    // 共通のマテリアル設定
    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse = XMFLOAT4(1, 1, 1, 1);
    material.Ambient = XMFLOAT4(1, 1, 1, 1);
    material.Specular = XMFLOAT4(0.6f, 0.6f, 0.6f, 1);
    material.Shininess = 20.0f;
    material.TextureEnable = TRUE;
    SetMaterial(material);

    // SetupCubeDraw() と同様だが、ピクセルシェーダーだけ m_TestPixelShader を使う
    UINT stride = sizeof(VERTEX_3D), offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, &m_CubeVertexBuffer, &stride, &offset);
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_DeviceContext->IASetInputLayout(m_CubeInputLayout);
    
    if (m_IsOutlineMode) {
        m_DeviceContext->VSSetShader(m_OutlineVS, NULL, 0);
        m_DeviceContext->PSSetShader(m_OutlinePS, NULL, 0);
    } else {
        m_DeviceContext->VSSetShader(m_CubeVertexShader, NULL, 0);
        m_DeviceContext->PSSetShader(m_TestPixelShader, NULL, 0);
    }

    GetDeviceContext()->Draw(36, 0);
}

void Renderer::CopySceneTexture()
{
    if (!m_DeviceContext || !m_BackgroundCopyTexture || !m_SceneRTV) return;

    ID3D11Resource* pRtvResource = nullptr;
    m_SceneRTV->GetResource(&pRtvResource);

    if (pRtvResource)
    {
        ID3D11Texture2D* pSceneTex = nullptr;
        HRESULT hr = pRtvResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&pSceneTex);
        if (SUCCEEDED(hr) && pSceneTex)
        {
            m_DeviceContext->CopyResource(m_BackgroundCopyTexture, pSceneTex);
            pSceneTex->Release();
        }
        pRtvResource->Release();
    }
}

void Renderer::DrawCubeWithWaterShader(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* normalMap1, ID3D11ShaderResourceView* normalMap2, float time, const XMFLOAT3& waveParams, float shininess, float fresnelPower, const XMFLOAT4& shallowColor, const XMFLOAT4& deepColor, const XMFLOAT2& scrollSpeed1, const XMFLOAT2& scrollSpeed2)
{
    SetWorldMatrix(worldMatrix);

    WaterParamCB paramCB;
    paramCB.Time = time;
    paramCB.WaveParams = waveParams;
    m_DeviceContext->UpdateSubresource(m_WaterParamBuffer, 0, nullptr, &paramCB, 0, 0);

    WaterLightCB lightCB;
    lightCB.LightDirection = XMFLOAT3(m_Light.Direction.x, m_Light.Direction.y, m_Light.Direction.z);
    lightCB.Shininess = shininess;
    lightCB.CameraPosition = XMFLOAT3(m_Light.CameraPosition.x, m_Light.CameraPosition.y, m_Light.CameraPosition.z);
    lightCB.FresnelPower = fresnelPower;
    lightCB.WaterColorShallow = shallowColor;
    lightCB.WaterColorDeep = deepColor;
    lightCB.ScrollSpeed1 = scrollSpeed1;
    lightCB.ScrollSpeed2 = scrollSpeed2;
    lightCB.TimeVal = time;
    lightCB.Dummy = XMFLOAT3(0, 0, 0);
    m_DeviceContext->UpdateSubresource(m_WaterLightBuffer, 0, nullptr, &lightCB, 0, 0);

    m_DeviceContext->VSSetConstantBuffers(3, 1, &m_WaterParamBuffer);
    m_DeviceContext->PSSetConstantBuffers(2, 1, &m_WaterLightBuffer);

    ID3D11ShaderResourceView* srvs[2] = { normalMap1, normalMap2 };
    m_DeviceContext->PSSetShaderResources(0, 2, srvs);

    UINT stride = sizeof(VERTEX_3D), offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, &m_CubeVertexBuffer, &stride, &offset);
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_DeviceContext->IASetInputLayout(m_CubeInputLayout);

    if (m_IsOutlineMode) {
        m_DeviceContext->VSSetShader(m_OutlineVS, NULL, 0);
        m_DeviceContext->PSSetShader(m_OutlinePS, NULL, 0);
    } else {
        m_DeviceContext->VSSetShader(m_WaterVS, NULL, 0);
        m_DeviceContext->PSSetShader(m_WaterPS, NULL, 0);
    }

    m_DeviceContext->Draw(36, 0);

    ID3D11Buffer* nullBuffer = nullptr;
    m_DeviceContext->VSSetConstantBuffers(3, 1, &nullBuffer);
    m_DeviceContext->PSSetConstantBuffers(2, 1, &nullBuffer);

    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
    m_DeviceContext->PSSetShaderResources(0, 2, nullSRVs);
}

void Renderer::DrawCubeWithDissolveShader(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* mainTexture, ID3D11ShaderResourceView* noiseTexture, float threshold, float edgeWidth, const XMFLOAT4& edgeColor)
{
    SetWorldMatrix(worldMatrix);

    DissolveCB cb;
    cb.Threshold = threshold;
    cb.EdgeWidth = edgeWidth;
    cb.Dummy = XMFLOAT2(0, 0);
    cb.EdgeColor = edgeColor;
    m_DeviceContext->UpdateSubresource(m_DissolveBuffer, 0, nullptr, &cb, 0, 0);

    m_DeviceContext->PSSetConstantBuffers(1, 1, &m_DissolveBuffer);

    ID3D11ShaderResourceView* srvs[2] = { mainTexture, noiseTexture };
    m_DeviceContext->PSSetShaderResources(0, 2, srvs);

    UINT stride = sizeof(VERTEX_3D), offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, &m_CubeVertexBuffer, &stride, &offset);
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_DeviceContext->IASetInputLayout(m_CubeInputLayout);

    if (m_IsOutlineMode) {
        m_DeviceContext->VSSetShader(m_OutlineVS, NULL, 0);
        m_DeviceContext->PSSetShader(m_OutlinePS, NULL, 0);
    } else {
        m_DeviceContext->VSSetShader(m_CubeVertexShader, NULL, 0);
        m_DeviceContext->PSSetShader(m_DissolvePS, NULL, 0);
    }

    m_DeviceContext->Draw(36, 0);

    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
    m_DeviceContext->PSSetShaderResources(0, 2, nullSRVs);
}

void Renderer::DrawCubeWithRefractShader(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* normalMap, float refractionIndex, float fresnelPower, const XMFLOAT4& highlightColor)
{
    SetWorldMatrix(worldMatrix);

    GlassCB cb;
    cb.RefractionIndex = refractionIndex;
    cb.FresnelPower = fresnelPower;
    cb.ScreenSize = XMFLOAT2((float)SCREEN_WIDTH, (float)SCREEN_HEIGHT);
    cb.HighlightColor = highlightColor;
    m_DeviceContext->UpdateSubresource(m_GlassBuffer, 0, nullptr, &cb, 0, 0);

    m_DeviceContext->PSSetConstantBuffers(1, 1, &m_GlassBuffer);

    ID3D11ShaderResourceView* srvs[2] = { m_BackgroundCopySRV, normalMap };
    m_DeviceContext->PSSetShaderResources(0, 2, srvs);

    UINT stride = sizeof(VERTEX_3D), offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, &m_CubeVertexBuffer, &stride, &offset);
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_DeviceContext->IASetInputLayout(m_CubeInputLayout);

    if (m_IsOutlineMode) {
        m_DeviceContext->VSSetShader(m_OutlineVS, NULL, 0);
        m_DeviceContext->PSSetShader(m_OutlinePS, NULL, 0);
    } else {
        m_DeviceContext->VSSetShader(m_CubeVertexShader, NULL, 0);
        m_DeviceContext->PSSetShader(m_RefractPS, NULL, 0);
    }

    m_DeviceContext->Draw(36, 0);

    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
    m_DeviceContext->PSSetShaderResources(0, 2, nullSRVs);
}

// Field（平面）用の鏡面反射描画関数
void Renderer::DrawFieldWithRefractShader(ID3D11Buffer* vertexBuffer, int vertexCount, ID3D11InputLayout* layout, ID3D11VertexShader* vs, ID3D11ShaderResourceView* normalMap, float refractionIndex, float fresnelPower, const XMFLOAT4& highlightColor, float time, const XMFLOAT3& cameraPos, ID3D11ShaderResourceView* skyTexture)
{
    // 現在のバックバッファをコピー
    CopySceneTexture();

    // 鏡面反射ピクセルシェーダーを設定
    m_DeviceContext->PSSetShader(m_RefractPS, NULL, 0);

    // テクスチャとサンプラーを設定
    // t0: バックバッファコピー, t1: 法線マップ, t2: スカイテクスチャ（フォールバック）
    m_DeviceContext->PSSetShaderResources(0, 1, &m_BackgroundCopySRV);
    m_DeviceContext->PSSetShaderResources(1, 1, &normalMap);
    m_DeviceContext->PSSetShaderResources(2, 1, &skyTexture); // スカイテクスチャをフォールバックとしてバインド
    m_DeviceContext->PSSetSamplers(0, 1, &m_SamplerState);

    // 定数バッファの更新（波紋アニメーション・カメラ座標追加）
    GlassCB cb;
    cb.RefractionIndex = refractionIndex;
    cb.FresnelPower    = fresnelPower;
    cb.ScreenSize      = XMFLOAT2(1920.0f, 1080.0f);
    cb.HighlightColor  = highlightColor;
    cb.Time            = time;
    cb.WaveStrength    = 1.0f;
    cb.Dummy           = XMFLOAT2(0.0f, 0.0f);
    cb.CameraWorldPos  = cameraPos;
    cb.MirrorBlend     = 0.85f;
    m_DeviceContext->UpdateSubresource(m_GlassBuffer, 0, nullptr, &cb, 0, 0);
    m_DeviceContext->PSSetConstantBuffers(1, 1, &m_GlassBuffer);

    // 頂点バッファの設定と入力レイアウト・頂点シェーダー設定
    UINT stride = sizeof(VERTEX_3D), offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_DeviceContext->IASetInputLayout(layout);
    m_DeviceContext->VSSetShader(vs, NULL, 0);

    // マテリアル設定
    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = TRUE;
    SetMaterial(material);

    // 描画実行
    m_DeviceContext->Draw(vertexCount, 0);

    // バインド解除（t0, t1, t2 の 3 スロットを解放）
    ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
    m_DeviceContext->PSSetShaderResources(0, 3, nullSRVs);
}

void Renderer::DrawFieldWithWaterShader(ID3D11Buffer* vertexBuffer, int vertexCount, ID3D11InputLayout* layout, ID3D11VertexShader* vs, ID3D11ShaderResourceView* normalMap1, ID3D11ShaderResourceView* normalMap2, float time, const XMFLOAT3& waveParams, float shininess, float fresnelPower, const XMFLOAT4& shallowColor, const XMFLOAT4& deepColor, const XMFLOAT2& scrollSpeed1, const XMFLOAT2& scrollSpeed2)
{
    // 水面ピクセルシェーダーを設定
    m_DeviceContext->PSSetShader(m_WaterPS, NULL, 0);

    // 法線マップ 2 枚と頂点シェーダーを水面用に切り替え
    m_DeviceContext->PSSetShaderResources(0, 1, &normalMap1);
    m_DeviceContext->PSSetShaderResources(1, 1, &normalMap2);
    m_DeviceContext->PSSetSamplers(0, 1, &m_SamplerState);

    // WaterLightCB の更新
    WaterLightCB wcb;
    ZeroMemory(&wcb, sizeof(wcb));
    wcb.LightDirection  = XMFLOAT3(m_Light.Direction.x, m_Light.Direction.y, m_Light.Direction.z);
    wcb.Shininess       = shininess;
    XMFLOAT3 camPos;
    XMStoreFloat3(&camPos, XMVectorSet(m_Light.CameraPosition.x, m_Light.CameraPosition.y, m_Light.CameraPosition.z, 0));
    wcb.CameraPosition  = camPos;
    wcb.FresnelPower    = fresnelPower;
    wcb.WaterColorShallow = shallowColor;
    wcb.WaterColorDeep    = deepColor;
    wcb.ScrollSpeed1    = scrollSpeed1;
    wcb.ScrollSpeed2    = scrollSpeed2;
    wcb.TimeVal         = time;
    m_DeviceContext->UpdateSubresource(m_WaterLightBuffer, 0, nullptr, &wcb, 0, 0);
    m_DeviceContext->PSSetConstantBuffers(2, 1, &m_WaterLightBuffer);

    // 頂点バッファ・レイアウト・頂点シェーダーの設定
    // 水面用頂点シェーダー（WaterVS）を使用して波を頂点レベルで変形する
    UINT stride = sizeof(VERTEX_3D), offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 三角形リストで描画
    m_DeviceContext->IASetInputLayout(layout);
    m_DeviceContext->VSSetShader(m_WaterVS, NULL, 0);

    // WaterParamCB の更新（頂点シェーダー側の波変形パラメータ）
    // WaterVS.hlsl は register(b3) を使用しているため slot 3 にバインドする
    WaterParamCB wpcb;
    wpcb.Time       = time;
    wpcb.WaveParams = waveParams;
    m_DeviceContext->UpdateSubresource(m_WaterParamBuffer, 0, nullptr, &wpcb, 0, 0);
    m_DeviceContext->VSSetConstantBuffers(3, 1, &m_WaterParamBuffer); // b3 に合わせる

    // マテリアル設定
    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
    material.TextureEnable = FALSE; // WaterPS は法線マップを t0/t1 で使うためここでは無効
    SetMaterial(material);

    // ブレンドステートを半透明に切り替えて描画
    m_DeviceContext->Draw(vertexCount, 0);

    // バインド解除（後処理）
    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
    m_DeviceContext->PSSetShaderResources(0, 2, nullSRVs);
}