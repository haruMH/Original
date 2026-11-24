#include "renderer.h"
#include "aura_effect.h"
#include "world_look.h"
#include "hit_impact_effect.h"
#include "manager.h"
#include "main.h"
#include "wall.h"
#include "fade_system.h"
#include "render_constants.h"
#include "game_constants.h"
#include "ui_theme.h"
#include "title_logo.h"
#include "graphics_config.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <stdio.h>
#include <wincodec.h>
#include <filesystem>
#include <dxgi1_5.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowscodecs.lib")

// スワップチェーンのFlipモデル / テアリング許可状態（Init時に決定、ResizeBuffers/Presentで使用）
static UINT s_SwapChainFlags = 0;
static bool s_UseFlipModel   = false;
static bool s_AllowTearing   = false;

D3D_FEATURE_LEVEL                                Renderer::m_FeatureLevel = D3D_FEATURE_LEVEL_11_0;
Microsoft::WRL::ComPtr<ID3D11Device>             Renderer::m_Device;
Microsoft::WRL::ComPtr<ID3D11DeviceContext>      Renderer::m_DeviceContext;
Microsoft::WRL::ComPtr<IDXGISwapChain>           Renderer::m_SwapChain;
Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   Renderer::m_RenderTargetView;
Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   Renderer::m_DepthStencilView;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::m_DepthSRV;
Microsoft::WRL::ComPtr<ID3D11Buffer>             Renderer::m_WorldBuffer;
Microsoft::WRL::ComPtr<ID3D11Buffer>             Renderer::m_ViewBuffer;
Microsoft::WRL::ComPtr<ID3D11Buffer>             Renderer::m_ProjectionBuffer;
Microsoft::WRL::ComPtr<ID3D11Buffer>             Renderer::m_MaterialBuffer;
Microsoft::WRL::ComPtr<ID3D11Buffer>             Renderer::m_LightBuffer;
Microsoft::WRL::ComPtr<ID3D11Buffer>             Renderer::m_PointLightBuffer;
Microsoft::WRL::ComPtr<ID3D11Buffer>             Renderer::m_ShadowVPBuffer;
XMMATRIX                                         Renderer::m_ViewMatrix       = XMMatrixIdentity();
XMMATRIX                                         Renderer::m_ProjectionMatrix = XMMatrixIdentity();
LIGHT                                            Renderer::m_Light            = {};
bool                                             Renderer::m_IsShadowMode     = false;
bool                                             Renderer::m_IsOutlineMode    = false;
XMMATRIX                                         Renderer::m_ShadowMatrix     = XMMatrixIdentity();
Microsoft::WRL::ComPtr<ID3D11SamplerState>       Renderer::m_SamplerState;
Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  Renderer::m_DepthStateEnable;
Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  Renderer::m_DepthStateDisable;
Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  Renderer::m_DepthStateOutline;
Microsoft::WRL::ComPtr<ID3D11RasterizerState>    Renderer::m_RasterizerStateCullBack;
Microsoft::WRL::ComPtr<ID3D11RasterizerState>    Renderer::m_RasterizerStateCullFront;
Microsoft::WRL::ComPtr<ID3D11BlendState>         Renderer::m_BlendState;

Microsoft::WRL::ComPtr<ID3D11Texture2D>          Renderer::m_ShadowMapTexture;
Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   Renderer::m_ShadowMapDSV;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::m_ShadowMapSRV;
Microsoft::WRL::ComPtr<ID3D11SamplerState>       Renderer::m_ShadowSampler;
UINT                                             Renderer::m_ScreenWidth    = SCREEN_WIDTH;
UINT                                             Renderer::m_ScreenHeight   = SCREEN_HEIGHT;
D3D11_VIEWPORT                                   Renderer::m_ShadowViewport = {};

Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   Renderer::m_SceneRTV;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::m_SceneSRV;
Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   Renderer::m_LumRTV;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::m_LumSRV;
Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   Renderer::m_AoRTV;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::m_AoSRV;
D3D11_VIEWPORT                                   Renderer::m_AoViewport = {};
UINT                                             Renderer::m_SceneWidth  = SCREEN_WIDTH;
UINT                                             Renderer::m_SceneHeight = SCREEN_HEIGHT;
float                                            Renderer::m_AppliedRenderScale = 1.0f;
Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   Renderer::m_LdrRTV;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::m_LdrSRV;
Microsoft::WRL::ComPtr<ID3D11PixelShader>        Renderer::m_UpscalePS;
Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   Renderer::m_BlurRTV;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::m_BlurSRV;

Microsoft::WRL::ComPtr<ID3D11VertexShader>       Renderer::m_OutlineVS;
Microsoft::WRL::ComPtr<ID3D11PixelShader>        Renderer::m_OutlinePS;
Microsoft::WRL::ComPtr<ID3D11VertexShader>       Renderer::m_PostVS;
Microsoft::WRL::ComPtr<ID3D11PixelShader>        Renderer::m_PostPS;
Microsoft::WRL::ComPtr<ID3D11Buffer>             Renderer::m_PostBuffer;
D3D11_VIEWPORT                                   Renderer::m_PostViewport = {};

Microsoft::WRL::ComPtr<ID3D11Buffer>             Renderer::m_CubeVertexBuffer;
Microsoft::WRL::ComPtr<ID3D11VertexShader>       Renderer::m_CubeVertexShader;
Microsoft::WRL::ComPtr<ID3D11PixelShader>        Renderer::m_CubePixelShader;
Microsoft::WRL::ComPtr<ID3D11InputLayout>        Renderer::m_CubeInputLayout;
Microsoft::WRL::ComPtr<ID3D11PixelShader>        Renderer::m_TestPixelShader;

Microsoft::WRL::ComPtr<ID3D11VertexShader>       Renderer::m_WaterVS;
Microsoft::WRL::ComPtr<ID3D11PixelShader>        Renderer::m_WaterPS;
Microsoft::WRL::ComPtr<ID3D11PixelShader>        Renderer::m_DissolvePS;
Microsoft::WRL::ComPtr<ID3D11PixelShader>        Renderer::m_RefractPS;
Microsoft::WRL::ComPtr<ID3D11Buffer>             Renderer::m_WaterParamBuffer;
Microsoft::WRL::ComPtr<ID3D11Buffer>             Renderer::m_WaterLightBuffer;
Microsoft::WRL::ComPtr<ID3D11Buffer>             Renderer::m_DissolveBuffer;
Microsoft::WRL::ComPtr<ID3D11Buffer>             Renderer::m_GlassBuffer;
Microsoft::WRL::ComPtr<ID3D11Texture2D>          Renderer::m_BackgroundCopyTexture;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::m_BackgroundCopySRV;

// コンスタントバッファ（CBuffer）キャッシュ（更新の最小化用）の実体定義
MATERIAL             Renderer::m_MaterialCache = {};
LIGHT                Renderer::m_LightCache = {};
XMFLOAT4X4           Renderer::m_ViewCache = {};
XMFLOAT4X4           Renderer::m_ProjectionCache = {};
bool                 Renderer::m_IsCacheInitialized = false;

void Renderer::CreateScreenSizeResources(UINT width, UINT height) {
    // 描画解像度。renderScale < 1 のとき画面より小さく描き、合成パスで画面へ引き伸ばす
    float rs = (std::min)(1.0f, (std::max)(0.5f, GraphicsConfig::GetConst().renderScale));
    const UINT sceneW = (std::max)(1u, (UINT)((float)width  * rs + 0.5f));
    const UINT sceneH = (std::max)(1u, (UINT)((float)height * rs + 0.5f));
    m_SceneWidth  = sceneW;
    m_SceneHeight = sceneH;
    m_AppliedRenderScale = rs;

    // 1. バックバッファからRTVを作成
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pb;
    m_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)pb.GetAddressOf());
    if (pb) {
        m_Device->CreateRenderTargetView(pb.Get(), NULL, m_RenderTargetView.ReleaseAndGetAddressOf());
    }

    // 2. 通常のデプスステンシルビュー作成
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = sceneW;
    td.Height = sceneH;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R24G8_TYPELESS;   // SSAO用にシェーダーからも読めるようにする
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> pd;
    m_Device->CreateTexture2D(&td, NULL, pd.GetAddressOf());
    if (pd) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvd = {};
        dsvd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        m_Device->CreateDepthStencilView(pd.Get(), &dsvd, m_DepthStencilView.ReleaseAndGetAddressOf());

        D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
        srvd.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvd.Texture2D.MipLevels = 1;
        m_Device->CreateShaderResourceView(pd.Get(), &srvd, m_DepthSRV.ReleaseAndGetAddressOf());
    }

    // 3. ブルーム用レンダーターゲット
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tempTex;
    D3D11_TEXTURE2D_DESC rtDesc = {};
    rtDesc.Width = sceneW;
    rtDesc.Height = sceneH;
    rtDesc.MipLevels = 1;
    rtDesc.ArraySize = 1;
    rtDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT; // HDR(帯域は16bit版の半分): 自発光(>1.0)をブルームに活かす
    rtDesc.SampleDesc.Count = 1;
    rtDesc.Usage = D3D11_USAGE_DEFAULT;
    rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    // メインシーン描画用バッファ (等倍)
    m_Device->CreateTexture2D(&rtDesc, NULL, tempTex.ReleaseAndGetAddressOf());
    if (tempTex) {
        m_Device->CreateRenderTargetView(tempTex.Get(), NULL, m_SceneRTV.ReleaseAndGetAddressOf());
        m_Device->CreateShaderResourceView(tempTex.Get(), NULL, m_SceneSRV.ReleaseAndGetAddressOf());
    }

    // 縮小バッファ設定 (1/4サイズ)
    rtDesc.Width = sceneW / RenderConstants::PostProcess::DOWNSAMPLE_SCALE;
    rtDesc.Height = sceneH / RenderConstants::PostProcess::DOWNSAMPLE_SCALE;
    m_PostViewport.Width = (float)rtDesc.Width;
    m_PostViewport.Height = (float)rtDesc.Height;
    m_PostViewport.MinDepth = 0.0f;
    m_PostViewport.MaxDepth = 1.0f;
    m_PostViewport.TopLeftX = 0.0f;
    m_PostViewport.TopLeftY = 0.0f;

    // 高輝度抽出用バッファ
    m_Device->CreateTexture2D(&rtDesc, NULL, tempTex.ReleaseAndGetAddressOf());
    if (tempTex) {
        m_Device->CreateRenderTargetView(tempTex.Get(), NULL, m_LumRTV.ReleaseAndGetAddressOf());
        m_Device->CreateShaderResourceView(tempTex.Get(), NULL, m_LumSRV.ReleaseAndGetAddressOf());
    }

    // ガウシアンぼかし用バッファ
    m_Device->CreateTexture2D(&rtDesc, NULL, tempTex.ReleaseAndGetAddressOf());
    if (tempTex) {
        m_Device->CreateRenderTargetView(tempTex.Get(), NULL, m_BlurRTV.ReleaseAndGetAddressOf());
        m_Device->CreateShaderResourceView(tempTex.Get(), NULL, m_BlurSRV.ReleaseAndGetAddressOf());
    }

    // SSAO用バッファ (1/2サイズ・単チャンネル)
    {
        D3D11_TEXTURE2D_DESC aoDesc = rtDesc;
        aoDesc.Width  = (std::max)(1u, sceneW / 2);
        aoDesc.Height = (std::max)(1u, sceneH / 2);
        aoDesc.Format = DXGI_FORMAT_R8_UNORM;
        m_AoViewport = {};
        m_AoViewport.Width    = (float)aoDesc.Width;
        m_AoViewport.Height   = (float)aoDesc.Height;
        m_AoViewport.MaxDepth = 1.0f;
        m_Device->CreateTexture2D(&aoDesc, NULL, tempTex.ReleaseAndGetAddressOf());
        if (tempTex) {
            m_Device->CreateRenderTargetView(tempTex.Get(), NULL, m_AoRTV.ReleaseAndGetAddressOf());
            m_Device->CreateShaderResourceView(tempTex.Get(), NULL, m_AoSRV.ReleaseAndGetAddressOf());
        }
    }

    // 4. 屈折エフェクト用背景コピーテクスチャ
    D3D11_TEXTURE2D_DESC bgDesc = {};
    bgDesc.Width = sceneW;
    bgDesc.Height = sceneH;
    bgDesc.MipLevels = 1;
    bgDesc.ArraySize = 1;
    bgDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT; // シーンRTと同一形式（CopyResource用）
    bgDesc.SampleDesc.Count = 1;
    bgDesc.Usage = D3D11_USAGE_DEFAULT;
    bgDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    m_Device->CreateTexture2D(&bgDesc, nullptr, m_BackgroundCopyTexture.ReleaseAndGetAddressOf());
    if (m_BackgroundCopyTexture) {
        m_Device->CreateShaderResourceView(m_BackgroundCopyTexture.Get(), nullptr, m_BackgroundCopySRV.ReleaseAndGetAddressOf());
    }

    // 5. renderScale < 1 のみ: 合成結果を受ける一時画像（シーンと同じ解像度のLDR）。
    //    オーラ等はここへシーン深度で描き、最後に画面解像度へ拡大する
    m_LdrRTV.Reset();
    m_LdrSRV.Reset();
    if (sceneW != width || sceneH != height) {
        D3D11_TEXTURE2D_DESC ld = {};
        ld.Width = sceneW;
        ld.Height = sceneH;
        ld.MipLevels = 1;
        ld.ArraySize = 1;
        ld.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   // バックバッファと同じ形式
        ld.SampleDesc.Count = 1;
        ld.Usage = D3D11_USAGE_DEFAULT;
        ld.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> ldrTex;
        if (SUCCEEDED(m_Device->CreateTexture2D(&ld, NULL, ldrTex.GetAddressOf()))) {
            m_Device->CreateRenderTargetView(ldrTex.Get(), NULL, m_LdrRTV.ReleaseAndGetAddressOf());
            m_Device->CreateShaderResourceView(ldrTex.Get(), NULL, m_LdrSRV.ReleaseAndGetAddressOf());
        }
    }
}

void Renderer::ReleaseScreenSizeResources() {
    m_RenderTargetView.Reset();
    m_DepthStencilView.Reset();
    m_LdrRTV.Reset();
    m_LdrSRV.Reset();
    m_DepthSRV.Reset();
    m_SceneRTV.Reset();
    m_SceneSRV.Reset();
    m_LumRTV.Reset();
    m_LumSRV.Reset();
    m_BlurRTV.Reset();
    m_BlurSRV.Reset();
    m_AoRTV.Reset();
    m_AoSRV.Reset();
    m_BackgroundCopyTexture.Reset();
    m_BackgroundCopySRV.Reset();
}

void Renderer::SetScreenViewport() {
    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)m_ScreenWidth;
    vp.Height = (FLOAT)m_ScreenHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    m_DeviceContext->RSSetViewports(1, &vp);
}

// シーン(3D描画・ポスト前)用。renderScale 反映後の解像度
void Renderer::SetSceneViewport() {
    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)m_SceneWidth;
    vp.Height = (FLOAT)m_SceneHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_DeviceContext->RSSetViewports(1, &vp);
}

// renderScale が変わっていたら、解像度依存リソースを作り直す（フレーム先頭で呼ぶ）
void Renderer::ApplyRenderScaleIfChanged() {
    if (!m_Device || !m_DeviceContext || !m_SwapChain) return;
    const float rs = (std::min)(1.0f, (std::max)(0.5f, GraphicsConfig::GetConst().renderScale));
    if (std::fabs(rs - m_AppliedRenderScale) < 0.001f) return;

    ID3D11RenderTargetView* nullRTV = nullptr;
    m_DeviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);
    ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
    m_DeviceContext->PSSetShaderResources(0, 4, nullSRVs);
    m_DeviceContext->Flush();
    ReleaseScreenSizeResources();
    CreateScreenSizeResources(m_ScreenWidth, m_ScreenHeight);
}

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

    // デバイスのみ先に作成し、DXGIファクトリからFlipモデルのスワップチェーンを作成する
    HRESULT hrDev = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, m_Device.ReleaseAndGetAddressOf(), &m_FeatureLevel, m_DeviceContext.ReleaseAndGetAddressOf());
    if (SUCCEEDED(hrDev)) {
        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
        Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
        if (SUCCEEDED(m_Device.As(&dxgiDevice)) &&
            SUCCEEDED(dxgiDevice->GetAdapter(&dxgiAdapter)) &&
            SUCCEEDED(dxgiAdapter->GetParent(IID_PPV_ARGS(&factory)))) {

            // テアリング（可変リフレッシュ/無制限fps）対応の確認
            BOOL allowTearing = FALSE;
            Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
            if (SUCCEEDED(factory.As(&factory5))) {
                if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)))) {
                    allowTearing = FALSE;
                }
            }

            DXGI_SWAP_CHAIN_DESC flipDesc = sd;
            flipDesc.BufferCount = 2;
            flipDesc.BufferDesc.RefreshRate.Numerator = 0;
            flipDesc.BufferDesc.RefreshRate.Denominator = 1;
            flipDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            flipDesc.Flags = allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

            if (SUCCEEDED(factory->CreateSwapChain(m_Device.Get(), &flipDesc, m_SwapChain.ReleaseAndGetAddressOf()))) {
                s_UseFlipModel = true;
                s_AllowTearing = (allowTearing != FALSE);
                s_SwapChainFlags = flipDesc.Flags;
                // Alt+Enter は WndProc 側（ボーダレス全画面切替）で処理するため、DXGI標準の排他全画面切替は無効化
                factory->MakeWindowAssociation(GetWindow(), DXGI_MWA_NO_ALT_ENTER);
            }
        }
    }

    // Flipモデルが使えない環境では、従来のBitBltモデルにフォールバック
    if (!m_SwapChain) {
        m_DeviceContext.Reset();
        m_Device.Reset();
        s_UseFlipModel = false;
        s_AllowTearing = false;
        s_SwapChainFlags = 0;
        D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, m_SwapChain.ReleaseAndGetAddressOf(), m_Device.ReleaseAndGetAddressOf(), &m_FeatureLevel, m_DeviceContext.ReleaseAndGetAddressOf());
    }

    // 解像度依存リソース（RTV / DSV / ブルーム / 屈折背景コピー）の一括作成
    CreateScreenSizeResources(SCREEN_WIDTH, SCREEN_HEIGHT);

    // === シャドウマップ ===
    D3D11_TEXTURE2D_DESC sdDesc = {};
    sdDesc.Width = RenderConstants::Shadow::MAP_WIDTH;
    sdDesc.Height = RenderConstants::Shadow::MAP_HEIGHT;
    sdDesc.MipLevels = 1;
    sdDesc.ArraySize = 1;
    // 高精度かつ安全なR32系フォーマットに変更
    sdDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    sdDesc.SampleDesc.Count = 1;
    sdDesc.Usage = D3D11_USAGE_DEFAULT;
    sdDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    // メンバ変数にしっかりと実体を確保
    m_Device->CreateTexture2D(&sdDesc, NULL, m_ShadowMapTexture.ReleaseAndGetAddressOf());

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    m_Device->CreateDepthStencilView(m_ShadowMapTexture.Get(), &dsvDesc, m_ShadowMapDSV.ReleaseAndGetAddressOf());

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    m_Device->CreateShaderResourceView(m_ShadowMapTexture.Get(), &srvDesc, m_ShadowMapSRV.ReleaseAndGetAddressOf());

    m_ShadowViewport.Width = (float)RenderConstants::Shadow::MAP_WIDTH;
    m_ShadowViewport.Height = (float)RenderConstants::Shadow::MAP_HEIGHT;
    m_ShadowViewport.MinDepth = 0.0f;
    m_ShadowViewport.MaxDepth = 1.0f;

    // === 定数バッファ ===
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    bd.ByteWidth = sizeof(XMFLOAT4X4);
    m_Device->CreateBuffer(&bd, NULL, m_WorldBuffer.ReleaseAndGetAddressOf());
    m_DeviceContext->VSSetConstantBuffers(0, 1, m_WorldBuffer.GetAddressOf());

    m_Device->CreateBuffer(&bd, NULL, m_ViewBuffer.ReleaseAndGetAddressOf());
    m_DeviceContext->VSSetConstantBuffers(1, 1, m_ViewBuffer.GetAddressOf());

    m_Device->CreateBuffer(&bd, NULL, m_ProjectionBuffer.ReleaseAndGetAddressOf());
    m_DeviceContext->VSSetConstantBuffers(2, 1, m_ProjectionBuffer.GetAddressOf());

    m_Device->CreateBuffer(&bd, NULL, m_ShadowVPBuffer.ReleaseAndGetAddressOf());
    m_DeviceContext->VSSetConstantBuffers(5, 1, m_ShadowVPBuffer.GetAddressOf());

    bd.ByteWidth = sizeof(MATERIAL);
    m_Device->CreateBuffer(&bd, NULL, m_MaterialBuffer.ReleaseAndGetAddressOf());
    m_DeviceContext->PSSetConstantBuffers(3, 1, m_MaterialBuffer.GetAddressOf());

    bd.ByteWidth = sizeof(LIGHT);
    m_Device->CreateBuffer(&bd, NULL, m_LightBuffer.ReleaseAndGetAddressOf());
    m_DeviceContext->VSSetConstantBuffers(4, 1, m_LightBuffer.GetAddressOf());
    m_DeviceContext->PSSetConstantBuffers(4, 1, m_LightBuffer.GetAddressOf());

    bd.ByteWidth = sizeof(POINTLIGHT);
    {
        POINTLIGHT off = {};   // 初期は消灯
        D3D11_SUBRESOURCE_DATA plInit = { &off, 0, 0 };
        m_Device->CreateBuffer(&bd, &plInit, m_PointLightBuffer.ReleaseAndGetAddressOf());
        m_DeviceContext->PSSetConstantBuffers(7, 1, m_PointLightBuffer.GetAddressOf());
    }

    bd.ByteWidth = sizeof(POSTEFFECT);
    m_Device->CreateBuffer(&bd, NULL, m_PostBuffer.ReleaseAndGetAddressOf());

    // 深度ステート
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    m_Device->CreateDepthStencilState(&dsd, m_DepthStateEnable.ReleaseAndGetAddressOf());

    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    m_Device->CreateDepthStencilState(&dsd, m_DepthStateDisable.ReleaseAndGetAddressOf());

    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    m_Device->CreateDepthStencilState(&dsd, m_DepthStateOutline.ReleaseAndGetAddressOf());

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
    m_Device->CreateBlendState(&bl, m_BlendState.ReleaseAndGetAddressOf());
    float f[4] = { 0,0,0,0 };
    m_DeviceContext->OMSetBlendState(m_BlendState.Get(), f, 0xffffffff);

    // サンプラーステート
    D3D11_SAMPLER_DESC sm = {};
    sm.Filter = D3D11_FILTER_ANISOTROPIC;
    sm.AddressU = sm.AddressV = sm.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sm.MaxAnisotropy = 16;
    sm.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sm.MaxLOD = D3D11_FLOAT32_MAX;
    m_Device->CreateSamplerState(&sm, m_SamplerState.ReleaseAndGetAddressOf());
    m_DeviceContext->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());

    // シャドウ用比較サンプラー
    sm = {};
    sm.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    sm.AddressU = sm.AddressV = sm.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    sm.BorderColor[0] = sm.BorderColor[1] = sm.BorderColor[2] = sm.BorderColor[3] = 1.0f;
    sm.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    m_Device->CreateSamplerState(&sm, m_ShadowSampler.ReleaseAndGetAddressOf());
    m_DeviceContext->PSSetSamplers(1, 1, m_ShadowSampler.GetAddressOf());

    // ラスタライザステート
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.DepthClipEnable = TRUE;
    m_Device->CreateRasterizerState(&rd, m_RasterizerStateCullBack.ReleaseAndGetAddressOf());

    rd.CullMode = D3D11_CULL_FRONT; // アウトライン用（裏面を描画）
    rd.DepthBias = RenderConstants::Outline::DEPTH_BIAS; // 床面との深度遮蔽・隙間を防ぐため定数化
    rd.SlopeScaledDepthBias = RenderConstants::Outline::SLOPE_SCALED_DEPTH_BIAS;
    m_Device->CreateRasterizerState(&rd, m_RasterizerStateCullFront.ReleaseAndGetAddressOf());
    m_DeviceContext->RSSetState(m_RasterizerStateCullBack.Get());

    // アウトライン用シェーダー読み込み（2-1 対応: ResolveShaderPath でパス解決を一元化）
    CreateVertexShader(m_OutlineVS.ReleaseAndGetAddressOf(), nullptr, ResolveShaderPath("outline_vs.cso").c_str());
    CreatePixelShader(m_OutlinePS.ReleaseAndGetAddressOf(),           ResolveShaderPath("outline_ps.cso").c_str());

    // ポストエフェクト用シェーダー読み込み（2-1 対応: ResolveShaderPath でパス解決を一元化）
    CreateVertexShader(m_PostVS.ReleaseAndGetAddressOf(), nullptr, ResolveShaderPath("postEffect_vs.cso").c_str());
    CreatePixelShader(m_PostPS.ReleaseAndGetAddressOf(),           ResolveShaderPath("postEffect_ps.cso").c_str());

    // renderScale < 1 用: 一時画像を画面解像度へ拡大するシェーダー
    CreatePixelShader(m_UpscalePS.ReleaseAndGetAddressOf(),        ResolveShaderPath("upscale_ps.cso").c_str());

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
        m_Device->CreateBuffer(&bdc, &sdc, m_CubeVertexBuffer.ReleaseAndGetAddressOf());

        // キューブ用シェーダー読み込み（2-1 対応: ResolveShaderPath でパス解決を一元化）
        CreateVertexShader(m_CubeVertexShader.ReleaseAndGetAddressOf(), m_CubeInputLayout.ReleaseAndGetAddressOf(), ResolveShaderPath("vertexShader.cso").c_str());
        CreatePixelShader(m_CubePixelShader.ReleaseAndGetAddressOf(),   ResolveShaderPath("pixelShader.cso").c_str());
        CreatePixelShader(m_TestPixelShader.ReleaseAndGetAddressOf(),   ResolveShaderPath("test_ps.cso").c_str());
        CreateVertexShader(m_WaterVS.ReleaseAndGetAddressOf(), nullptr, ResolveShaderPath("WaterVS.cso").c_str());
        CreatePixelShader(m_WaterPS.ReleaseAndGetAddressOf(),           ResolveShaderPath("WaterPS.cso").c_str());
        CreatePixelShader(m_DissolvePS.ReleaseAndGetAddressOf(),        ResolveShaderPath("DissolvePS.cso").c_str());
        CreatePixelShader(m_RefractPS.ReleaseAndGetAddressOf(),         ResolveShaderPath("RefractPS.cso").c_str());

        // 新規エフェクト用定数バッファの生成
        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        // WaterParamCB (16 bytes)
        cbd.ByteWidth = sizeof(WaterParamCB);
        m_Device->CreateBuffer(&cbd, nullptr, m_WaterParamBuffer.ReleaseAndGetAddressOf());

        // WaterLightCB (96 bytes)
        cbd.ByteWidth = sizeof(WaterLightCB);
        m_Device->CreateBuffer(&cbd, nullptr, m_WaterLightBuffer.ReleaseAndGetAddressOf());

        // DissolveCB (32 bytes)
        cbd.ByteWidth = sizeof(DissolveCB);
        m_Device->CreateBuffer(&cbd, nullptr, m_DissolveBuffer.ReleaseAndGetAddressOf());

        // GlassCB (32 bytes)
        cbd.ByteWidth = sizeof(GlassCB);
        m_Device->CreateBuffer(&cbd, nullptr, m_GlassBuffer.ReleaseAndGetAddressOf());
    }

    // ImGui の初期化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    UITheme::Apply();   // ゲームの絵柄に合わせた配色・丸み

    // 日本語フォント（Windows標準 游ゴシック Medium → Meiryo → MS Gothic の順）の読み込み
    if (std::filesystem::exists("C:\\Windows\\Fonts\\YuGothM.ttc")) {
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\YuGothM.ttc", 19.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    } else if (std::filesystem::exists("C:\\Windows\\Fonts\\meiryo.ttc")) {
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\meiryo.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    } else if (std::filesystem::exists("C:\\Windows\\Fonts\\msgothic.ttc")) {
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msgothic.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    } else {
        io.Fonts->AddFontDefault();
    }

    ImGui_ImplWin32_Init(GetWindow());
    ImGui_ImplDX11_Init(m_Device.Get(), m_DeviceContext.Get());

    // キャッシュ機構の初期化完了フラグを立てる
    m_IsCacheInitialized = true;
}
void Renderer::Uninit() {
    // ImGui の終了処理
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    ReleaseScreenSizeResources();
    m_OutlineVS.Reset();
    m_OutlinePS.Reset();
    m_RasterizerStateCullBack.Reset();
    m_RasterizerStateCullFront.Reset();
    m_PostVS.Reset();
    m_PostPS.Reset();
    m_UpscalePS.Reset();
    m_PostBuffer.Reset();
    m_ShadowMapTexture.Reset();
    m_ShadowMapDSV.Reset();
    m_ShadowMapSRV.Reset();
    m_ShadowSampler.Reset();
    m_ShadowVPBuffer.Reset();
    m_WorldBuffer.Reset();
    m_ViewBuffer.Reset();
    m_ProjectionBuffer.Reset();
    m_MaterialBuffer.Reset();
    m_LightBuffer.Reset();
    m_PointLightBuffer.Reset();
    m_SamplerState.Reset();
    m_DepthStateEnable.Reset();
    m_DepthStateDisable.Reset();
    m_DepthStateOutline.Reset();
    m_BlendState.Reset();
    m_CubeVertexBuffer.Reset();
    m_CubeInputLayout.Reset();
    m_CubePixelShader.Reset();
    m_CubeVertexShader.Reset();
    m_TestPixelShader.Reset();
    m_WaterVS.Reset();
    m_WaterPS.Reset();
    m_DissolvePS.Reset();
    m_RefractPS.Reset();
    m_WaterParamBuffer.Reset();
    m_WaterLightBuffer.Reset();
    m_DissolveBuffer.Reset();
    m_GlassBuffer.Reset();
    m_SwapChain.Reset();
    m_DeviceContext.Reset();
    m_Device.Reset();
}

void Renderer::OnResize(UINT width, UINT height) {
    if (width == 0 || height == 0) return;
    if (!m_Device || !m_DeviceContext || !m_SwapChain) return;
    if (m_ScreenWidth == width && m_ScreenHeight == height) return;

    // 1. パイプラインのレンダーターゲットとSRVのバインドを解除
    ID3D11RenderTargetView* nullRTV = nullptr;
    m_DeviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);
    ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
    m_DeviceContext->PSSetShaderResources(0, 4, nullSRVs);
    m_DeviceContext->Flush();

    // 2. 解像度依存の全リソースを安全に解放
    ReleaseScreenSizeResources();

    // 3. スワップチェーンのバッファをリサイズ
    HRESULT hr = m_SwapChain->ResizeBuffers(s_UseFlipModel ? 2 : 1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, s_SwapChainFlags);
    if (FAILED(hr)) return;

    m_ScreenWidth = width;
    m_ScreenHeight = height;

    // 4. 新解像度でRTV・DSV・ブルーム・背景コピーリソースを一括再作成
    CreateScreenSizeResources(m_ScreenWidth, m_ScreenHeight);

    // 8. カメラのプロジェクション行列（アスペクト比）を更新
    XMMATRIX proj = XMMatrixPerspectiveFovLH(
        Constants::Camera::FOV,
        (float)m_ScreenWidth / (float)m_ScreenHeight,
        Constants::Camera::NEAR_CLIP,
        Constants::Camera::FAR_CLIP);
    SetProjectionMatrix(proj);
}

void Renderer::BeginShadowPass() {
    ID3D11ShaderResourceView* nullSRV = NULL; m_DeviceContext->PSSetShaderResources(2, 1, &nullSRV);
    m_DeviceContext->OMSetRenderTargets(0, NULL, m_ShadowMapDSV.Get());
    m_DeviceContext->ClearDepthStencilView(m_ShadowMapDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    m_DeviceContext->RSSetViewports(1, &m_ShadowViewport);
}

void Renderer::EndShadowPass() {
    // シャドウマップの書き込み（DSV）を解除する
    m_DeviceContext->OMSetRenderTargets(0, NULL, NULL);

    SetSceneViewport();

    // DSVが解除されたので、安全にテクスチャ（SRV）として読み込めるようにバインド
    m_DeviceContext->PSSetShaderResources(2, 1, m_ShadowMapSRV.GetAddressOf());
}

void Renderer::BeginOutlinePass() {
    m_IsOutlineMode = true;
    m_DeviceContext->RSSetState(m_RasterizerStateCullFront.Get()); // 裏面を描画
    m_DeviceContext->OMSetDepthStencilState(m_DepthStateOutline.Get(), 0);
    m_DeviceContext->VSSetShader(m_OutlineVS.Get(), NULL, 0);      // 頂点を膨らませる
    m_DeviceContext->PSSetShader(m_OutlinePS.Get(), NULL, 0);      // 黒く塗る
}

void Renderer::EndOutlinePass() {
    m_IsOutlineMode = false;
    m_DeviceContext->RSSetState(m_RasterizerStateCullBack.Get());  // 表面描画に戻す
    m_DeviceContext->OMSetDepthStencilState(m_DepthStateEnable.Get(), 0);
}

// ─────────────────────────────────────────────────────────────────
// GPU 処理時間の計測（D3D11 タイムスタンプクエリ）
// ─────────────────────────────────────────────────────────────────
namespace {
    const int TS_RING  = 3;     // 3 フレーム分を循環（古い結果から読むので待たない）
    const int TS_MARKS = 13;    // 区切りの数（区間は 12）
    ID3D11Query* g_TsDisjoint[TS_RING] = {};
    ID3D11Query* g_TsMark[TS_RING][TS_MARKS] = {};
    bool         g_TsWritten[TS_RING] = {};
    int          g_TsWrite = 0;
    bool         g_TsReady = false;
    float        g_GpuSegMs[TS_MARKS - 1] = {};
}

void Renderer::GpuFrameBegin()
{
    if (!m_Device || !m_DeviceContext) return;
    if (!g_TsReady) {
        D3D11_QUERY_DESC qd = {};
        bool ok = true;
        for (int r = 0; r < TS_RING && ok; ++r) {
            qd.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
            ok &= SUCCEEDED(m_Device->CreateQuery(&qd, &g_TsDisjoint[r]));
            for (int i = 0; i < TS_MARKS && ok; ++i) {
                qd.Query = D3D11_QUERY_TIMESTAMP;
                ok &= SUCCEEDED(m_Device->CreateQuery(&qd, &g_TsMark[r][i]));
            }
        }
        if (!ok) return;
        g_TsReady = true;
    }

    // いちばん古い結果（2 フレーム前）を読む。まだ準備できていなければ前回値のまま
    int rd = (g_TsWrite + 1) % TS_RING;
    if (g_TsWritten[rd]) {
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj = {};
        if (m_DeviceContext->GetData(g_TsDisjoint[rd], &dj, sizeof(dj), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK && !dj.Disjoint) {
            UINT64 t[TS_MARKS] = {};
            bool all = true;
            for (int i = 0; i < TS_MARKS && all; ++i) {
                all &= (m_DeviceContext->GetData(g_TsMark[rd][i], &t[i], sizeof(UINT64), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK);
            }
            if (all) {
                for (int i = 0; i < TS_MARKS - 1; ++i) {
                    g_GpuSegMs[i] = (t[i + 1] >= t[i]) ? (float)((double)(t[i + 1] - t[i]) / (double)dj.Frequency * 1000.0) : 0.0f;
                }
            }
        }
    }

    g_TsWrite = (g_TsWrite + 1) % TS_RING;
    g_TsWritten[g_TsWrite] = false;
    m_DeviceContext->Begin(g_TsDisjoint[g_TsWrite]);
    m_DeviceContext->End(g_TsMark[g_TsWrite][0]);
}

void Renderer::GpuMark(int index)
{
    if (!g_TsReady || !m_DeviceContext || index < 1 || index >= TS_MARKS) return;
    m_DeviceContext->End(g_TsMark[g_TsWrite][index]);
}

void Renderer::GpuFrameEnd()
{
    if (!g_TsReady || !m_DeviceContext) return;
    m_DeviceContext->End(g_TsDisjoint[g_TsWrite]);
    g_TsWritten[g_TsWrite] = true;
}

float Renderer::GetGpuSegmentMs(int segment)
{
    return (segment >= 0 && segment < TS_MARKS - 1) ? g_GpuSegMs[segment] : 0.0f;
}

void Renderer::Begin() {
    // 中間バッファ（SceneRTV）に描画し、ビューポートを描画解像度(renderScale 反映)に設定
    SetSceneViewport();
    GpuMark(1);   // 準備(影など)の終わり = 通常描画の始まり

    // バックバッファではなく、SceneRTV（中間バッファ）に描画する
    float c[4] = {0.1f, 0.1f, 0.2f, 1.0f};
    m_DeviceContext->OMSetRenderTargets(1, m_SceneRTV.GetAddressOf(), m_DepthStencilView.Get());
    m_DeviceContext->ClearRenderTargetView(m_SceneRTV.Get(), c);
    m_DeviceContext->ClearDepthStencilView(m_DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    // 前フレームのポストプロセス・フェード・ImGuiによるパイプラインステート汚染をリセット
    float f[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_DeviceContext->OMSetBlendState(m_BlendState.Get(), f, 0xffffffff);
    m_DeviceContext->OMSetDepthStencilState(m_DepthStateEnable.Get(), 0);
    m_DeviceContext->RSSetState(m_RasterizerStateCullBack.Get());
}

void Renderer::BeginNewFrame() {
    ApplyRenderScaleIfChanged();
    GpuFrameBegin();
    // ImGui のフレーム開始
    UITheme::Update();   // UI の配色を空の時間に連動させる
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Renderer::End() {
    GpuMark(8);   // シーン描画（エフェクト・UI含む）の終わり
    // ─── ブルームポストプロセス ─────────────────────────────
    m_DeviceContext->IASetInputLayout(NULL);
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_DeviceContext->VSSetShader(m_PostVS.Get(), NULL, 0);
    m_DeviceContext->PSSetShader(m_PostPS.Get(), NULL, 0);
    m_DeviceContext->OMSetDepthStencilState(m_DepthStateDisable.Get(), 0);
    ID3D11ShaderResourceView* nullSRV[] = { NULL, NULL };

    const GraphicsSettings& gs = GraphicsConfig::GetConst();
    bool enableBloom = (gs.postProcess != PostProcessQuality::Post_Low);

    // ポストプロセス定数バッファをピクセルシェーダーのスロット0に確実にバインド
    m_DeviceContext->PSSetConstantBuffers(0, 1, m_PostBuffer.GetAddressOf());

    POSTEFFECT pe;
    ZeroMemory(&pe, sizeof(pe));
    pe.Threshold     = 0.95f;
    // ブルーム用バッファ(縮小)のテクセルサイズ。ウィンドウサイズに依らず同じ見た目のぼかしにする
    pe.TexelSize     = XMFLOAT2(1.0f / m_PostViewport.Width, 1.0f / m_PostViewport.Height);
    pe.BlurIntensity = enableBloom ? (gs.postProcess == PostProcessQuality::Post_High ? 0.28f : 0.18f) : 0.0f;

    if (enableBloom) {
        // 1. 高輝度抽出 (SceneRTV → LumRTV)
        m_DeviceContext->RSSetViewports(1, &m_PostViewport);
        m_DeviceContext->OMSetRenderTargets(1, m_LumRTV.GetAddressOf(), NULL);
        m_DeviceContext->PSSetShaderResources(0, 1, m_SceneSRV.GetAddressOf());
        pe.Mode = 0;
        m_DeviceContext->UpdateSubresource(m_PostBuffer.Get(), 0, NULL, &pe, 0, 0);
        m_DeviceContext->Draw(3, 0);

        // 2. 横ぼかし (LumRTV → BlurRTV)
        m_DeviceContext->PSSetShaderResources(0, 2, nullSRV);
        m_DeviceContext->OMSetRenderTargets(1, m_BlurRTV.GetAddressOf(), NULL);
        m_DeviceContext->PSSetShaderResources(0, 1, m_LumSRV.GetAddressOf());
        pe.Mode = 1;
        m_DeviceContext->UpdateSubresource(m_PostBuffer.Get(), 0, NULL, &pe, 0, 0);
        m_DeviceContext->Draw(3, 0);

        // 3. 縦ぼかし (BlurRTV → LumRTV)
        m_DeviceContext->PSSetShaderResources(0, 2, nullSRV);
        m_DeviceContext->OMSetRenderTargets(1, m_LumRTV.GetAddressOf(), NULL);
        m_DeviceContext->PSSetShaderResources(0, 1, m_BlurSRV.GetAddressOf());
        pe.Mode = 2;
        m_DeviceContext->UpdateSubresource(m_PostBuffer.Get(), 0, NULL, &pe, 0, 0);
        m_DeviceContext->Draw(3, 0);
    }

    GpuMark(9);   // ブルームの終わり
    // 3.5 SSAO (深度 → AoRTV, 半解像度。品質Lowでは無効)
    const bool enableAo = (m_DepthSRV && m_AoRTV && gs.postProcess != PostProcessQuality::Post_Low);
    if (enableAo) {
        m_DeviceContext->PSSetShaderResources(0, 2, nullSRV);
        m_DeviceContext->RSSetViewports(1, &m_AoViewport);
        m_DeviceContext->OMSetRenderTargets(1, m_AoRTV.GetAddressOf(), NULL);
        m_DeviceContext->PSSetShaderResources(2, 1, m_DepthSRV.GetAddressOf());
        pe.Mode = 4;
        pe.AoParams = XMFLOAT4(0.85f, Constants::Camera::NEAR_CLIP, Constants::Camera::FAR_CLIP, 0.9f);
        m_DeviceContext->UpdateSubresource(m_PostBuffer.Get(), 0, NULL, &pe, 0, 0);
        m_DeviceContext->Draw(3, 0);
        ID3D11ShaderResourceView* nullDepth[] = { NULL };
        m_DeviceContext->PSSetShaderResources(2, 1, nullDepth);
    }

    GpuMark(10);  // SSAO の終わり
    // 4. 合成 (SceneRTV + LumRTV + AO → バックバッファ)
    //    renderScale < 1 のときは、シーンと同じ解像度の一時画像(m_LdrRTV)へ出し、最後に画面解像度へ拡大する
    const bool useLdr = (m_LdrRTV && m_LdrSRV && m_UpscalePS);
    if (useLdr) SetSceneViewport(); else SetScreenViewport();
    m_DeviceContext->PSSetShaderResources(0, 2, nullSRV);
    m_DeviceContext->OMSetRenderTargets(1, useLdr ? m_LdrRTV.GetAddressOf() : m_RenderTargetView.GetAddressOf(), NULL);
    m_DeviceContext->PSSetShaderResources(0, 1, m_SceneSRV.GetAddressOf());
    m_DeviceContext->PSSetShaderResources(1, 1, enableBloom ? m_LumSRV.GetAddressOf() : m_SceneSRV.GetAddressOf());
    m_DeviceContext->PSSetShaderResources(2, 1, m_DepthSRV.GetAddressOf());
    m_DeviceContext->PSSetShaderResources(3, 1, enableAo ? m_AoSRV.GetAddressOf() : nullSRV);
    pe.Mode = 3;
    // SSAO（品質Low では無効）。AO値は半解像度バッファから読む
    pe.AoParams = XMFLOAT4(enableAo ? 0.85f : 0.0f,
                           Constants::Camera::NEAR_CLIP, Constants::Camera::FAR_CLIP, 0.9f);
    pe.SlowMotionIntensity = Manager::GetSlowMotionIntensity();
    pe.FlashColor = Manager::GetFlashColor();
    pe.RadialBlurIntensity = gs.radialBlur ? Manager::GetRadialBlurIntensity() : 0.0f;
    pe.ChromaticAberration = (gs.postProcess == PostProcessQuality::Post_High) ? Manager::GetChromaticAberration() : 0.0f;
    pe.RadialBlurCenter = Manager::GetRadialBlurCenter();
    // 合成の読み取り元(シーン・AO)は描画解像度。CAS/AOぼかしの隣接距離はシーンの1画素
    pe.TexelSize = XMFLOAT2(1.0f / (float)m_SceneWidth, 1.0f / (float)m_SceneHeight);
    pe.Dummy.x = gs.casSharpness;
    // 空に連動したカラーグレード
    pe.GradeParams = XMFLOAT4(WorldLook::GetSaturation(), WorldLook::GetContrast(), 0.0f, 0.0f);
    const float* st = WorldLook::GetShadowTint();
    const float* ht = WorldLook::GetHighlightTint();
    pe.ShadowTint    = XMFLOAT4(st[0], st[1], st[2], 0.0f);
    pe.HighlightTint = XMFLOAT4(ht[0], ht[1], ht[2], 0.0f);
    m_DeviceContext->UpdateSubresource(m_PostBuffer.Get(), 0, NULL, &pe, 0, 0);
    m_DeviceContext->PSSetConstantBuffers(0, 1, m_PostBuffer.GetAddressOf());
    m_DeviceContext->Draw(3, 0);
    GpuMark(11);  // 合成の終わり

    // SRV バインド解除（次フレームへの干渉を防ぐ。深度SRVはDSVと同時に使えないため必須）
    ID3D11ShaderResourceView* nullSRV3[] = { NULL, NULL, NULL, NULL };
    m_DeviceContext->PSSetShaderResources(0, 4, nullSRV3);
    m_DeviceContext->OMSetDepthStencilState(m_DepthStateEnable.Get(), 0);

    // チャージオーラはポスト合成(ラジアルブラー)の後に、合成結果へ直接描く（深度はシーンのものを利用）
    // renderScale < 1 のときの合成結果は、シーンと同じ解像度の一時画像(m_LdrRTV)。深度と大きさが揃うのでそのまま使える
    ID3D11RenderTargetView* overlayRTV = useLdr ? m_LdrRTV.Get() : m_RenderTargetView.Get();
    m_DeviceContext->OMSetRenderTargets(1, &overlayRTV, m_DepthStencilView.Get());
    m_DeviceContext->RSSetState(m_RasterizerStateCullBack.Get());
    AuraEffect::Draw();
    HitImpactEffect::Draw();
    TitleLogo::Draw();   // タイトルロゴ（ポスト合成の影響を受けない純粋な黒白）
    // フェード描画のためポストプロセス直後と同じパイプライン状態へ戻す
    m_DeviceContext->OMSetRenderTargets(1, m_RenderTargetView.GetAddressOf(), NULL);
    m_DeviceContext->IASetInputLayout(NULL);
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_DeviceContext->VSSetShader(m_PostVS.Get(), NULL, 0);
    m_DeviceContext->PSSetShader(m_PostPS.Get(), NULL, 0);

    // renderScale < 1: 一時画像を画面解像度のバックバッファへ拡大する（フェード・ImGui は画面解像度のまま描く）
    if (useLdr && m_UpscalePS) {
        float bf[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        SetScreenViewport();
        m_DeviceContext->RSSetState(m_RasterizerStateCullBack.Get());
        m_DeviceContext->OMSetBlendState(m_BlendState.Get(), bf, 0xffffffff);
        m_DeviceContext->OMSetDepthStencilState(m_DepthStateDisable.Get(), 0);
        m_DeviceContext->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());
        m_DeviceContext->PSSetShader(m_UpscalePS.Get(), NULL, 0);
        m_DeviceContext->PSSetShaderResources(0, 1, m_LdrSRV.GetAddressOf());
        m_DeviceContext->Draw(3, 0);
        m_DeviceContext->PSSetShaderResources(0, 4, nullSRV3);
        m_DeviceContext->PSSetShader(m_PostPS.Get(), NULL, 0);
    }

    // バックバッファ最前面にフェードオーバーレイを描画
    FadeSystem::Draw(m_DeviceContext.Get());

    // ImGui のレンダリングと描画実行
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    GpuMark(12);
    GpuFrameEnd();

    // 垂直同期（VSync）はオフにし、メインループ側で指定FPS（30/60/120/180/240/無制限）に高精度制御
    // Flipモデル＋テアリング許可時は DXGI_PRESENT_ALLOW_TEARING を指定（DWM合成待ちを回避）
    m_SwapChain->Present(0, s_AllowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
}
void Renderer::SetWorldMatrix(XMMATRIX m) {
    XMMATRIX f = m; if(m_IsShadowMode) f = m * m_ShadowMatrix;
    XMFLOAT4X4 wf; XMStoreFloat4x4(&wf, XMMatrixTranspose(f)); m_DeviceContext->UpdateSubresource(m_WorldBuffer.Get(), 0, NULL, &wf, 0, 0);
}
void Renderer::SetViewMatrix(XMMATRIX m) {
    m_ViewMatrix = m;
    XMFLOAT4X4 vf;
    XMStoreFloat4x4(&vf, XMMatrixTranspose(m));
    if (!m_IsCacheInitialized || memcmp(&m_ViewCache, &vf, sizeof(XMFLOAT4X4)) != 0) {
        m_ViewCache = vf;
        m_DeviceContext->UpdateSubresource(m_ViewBuffer.Get(), 0, NULL, &vf, 0, 0);
    }
}
void Renderer::SetProjectionMatrix(XMMATRIX m) {
    m_ProjectionMatrix = m;
    XMFLOAT4X4 pf;
    XMStoreFloat4x4(&pf, XMMatrixTranspose(m));
    if (!m_IsCacheInitialized || memcmp(&m_ProjectionCache, &pf, sizeof(XMFLOAT4X4)) != 0) {
        m_ProjectionCache = pf;
        m_DeviceContext->UpdateSubresource(m_ProjectionBuffer.Get(), 0, NULL, &pf, 0, 0);
    }
}
void Renderer::SetShadowVPMatrix(XMMATRIX m) { XMFLOAT4X4 svp; XMStoreFloat4x4(&svp, XMMatrixTranspose(m)); m_DeviceContext->UpdateSubresource(m_ShadowVPBuffer.Get(), 0, NULL, &svp, 0, 0); }
void Renderer::SetMaterial(MATERIAL m) {
    if (!m_IsCacheInitialized || memcmp(&m_MaterialCache, &m, sizeof(MATERIAL)) != 0) {
        m_MaterialCache = m;
        m_DeviceContext->UpdateSubresource(m_MaterialBuffer.Get(), 0, NULL, &m, 0, 0);
    }
}
void Renderer::SetPointLight(int index, const XMFLOAT3& pos, float radius, const XMFLOAT3& color, float intensity) {
    if (!m_DeviceContext || !m_PointLightBuffer || index < 0 || index >= MAX_POINT_LIGHTS) return;
    static POINTLIGHT s_Lights = {};   // 全灯分を保持し、変更のたびに丸ごと転送する
    s_Lights.PosRadius[index] = XMFLOAT4(pos.x, pos.y, pos.z, radius);
    s_Lights.Color[index]     = XMFLOAT4(color.x, color.y, color.z, intensity);
    m_DeviceContext->UpdateSubresource(m_PointLightBuffer.Get(), 0, NULL, &s_Lights, 0, 0);
}

void Renderer::SetLight(LIGHT l) {
    m_Light = l;
    if (!m_IsCacheInitialized || memcmp(&m_LightCache, &l, sizeof(LIGHT)) != 0) {
        m_LightCache = l;
        m_DeviceContext->UpdateSubresource(m_LightBuffer.Get(), 0, NULL, &l, 0, 0);
    }
}
void Renderer::SetCameraPosition(XMFLOAT3 p) {
    m_Light.CameraPosition = XMFLOAT4(p.x, p.y, p.z, 1.0f);
    if (!m_IsCacheInitialized || memcmp(&m_LightCache, &m_Light, sizeof(LIGHT)) != 0) {
        m_LightCache = m_Light;
        m_DeviceContext->UpdateSubresource(m_LightBuffer.Get(), 0, NULL, &m_Light, 0, 0);
    }
}
void Renderer::SetShadowMode(bool e) { m_IsShadowMode = e; }
void Renderer::SetShadowMatrix(XMMATRIX m) { m_ShadowMatrix = m; }
void Renderer::SetAnisotropy(int maxAniso) {
    if (!m_Device || !m_DeviceContext) return;
    D3D11_SAMPLER_DESC sm = {};
    sm.Filter = D3D11_FILTER_ANISOTROPIC;
    sm.AddressU = sm.AddressV = sm.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sm.MaxAnisotropy = (UINT)(maxAniso < 1 ? 1 : (maxAniso > 16 ? 16 : maxAniso));
    sm.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sm.MaxLOD = D3D11_FLOAT32_MAX;
    m_Device->CreateSamplerState(&sm, m_SamplerState.ReleaseAndGetAddressOf());
    m_DeviceContext->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());
}
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
// 同名の .dds(BC1/BC3/BC7、ミップ入り)があれば読む。無ければ false を返し、呼び出し側が従来どおり WIC で読む。
// sRGB 指定のDDSも UNORM として扱う（従来の RGBA8_UNORM と同じ色味にするため）
static bool LoadDdsTexture(ID3D11Device* dev, const char* pngPath, ID3D11ShaderResourceView** out)
{
    std::string path(pngPath);
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;
    path = path.substr(0, dot) + ".dds";

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 128) { fclose(f); return false; }
    std::vector<BYTE> buf((size_t)sz);
    size_t rd = fread(buf.data(), 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) return false;

    auto u32 = [&](size_t off) { UINT v; memcpy(&v, &buf[off], 4); return v; };
    if (u32(0) != 0x20534444) return false;          // "DDS "
    const UINT height = u32(12), width = u32(16);
    UINT mips = u32(28); if (mips == 0) mips = 1;
    const UINT fourCC = u32(84);

    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    size_t dataOff = 128;
    if (fourCC == 0x30315844) {                       // "DX10" 拡張ヘッダ
        if (sz < 148) return false;
        const UINT dxgi = u32(128);
        dataOff = 148;
        if (dxgi == 71 || dxgi == 72) fmt = DXGI_FORMAT_BC1_UNORM;
        else if (dxgi == 77 || dxgi == 78) fmt = DXGI_FORMAT_BC3_UNORM;
        else if (dxgi == 98 || dxgi == 99) fmt = DXGI_FORMAT_BC7_UNORM;
    } else if (fourCC == 0x31545844) fmt = DXGI_FORMAT_BC1_UNORM;   // "DXT1"
    else if (fourCC == 0x35545844)   fmt = DXGI_FORMAT_BC3_UNORM;   // "DXT5"
    if (fmt == DXGI_FORMAT_UNKNOWN || width == 0 || height == 0 || mips > 15) return false;

    const UINT blockBytes = (fmt == DXGI_FORMAT_BC1_UNORM) ? 8 : 16;
    D3D11_SUBRESOURCE_DATA sub[15] = {};
    size_t off = dataOff;
    UINT w = width, h = height;
    for (UINT m = 0; m < mips; ++m) {
        const UINT bw = (std::max)(1u, (w + 3) / 4), bh = (std::max)(1u, (h + 3) / 4);
        const size_t pitch = (size_t)bw * blockBytes, size = pitch * bh;
        if (off + size > (size_t)sz) return false;
        sub[m].pSysMem = &buf[off];
        sub[m].SysMemPitch = (UINT)pitch;
        off += size;
        w = (std::max)(1u, w / 2); h = (std::max)(1u, h / 2);
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = width; td.Height = height; td.MipLevels = mips; td.ArraySize = 1;
    td.Format = fmt; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> t2;
    if (FAILED(dev->CreateTexture2D(&td, sub, t2.GetAddressOf())) || !t2) return false;
    return SUCCEEDED(dev->CreateShaderResourceView(t2.Get(), NULL, out)) && *out;
}

void Renderer::CreateTexture(const char* name, ID3D11ShaderResourceView** tex, bool allowCompressed) {
    *tex = nullptr;
    if (!name || strlen(name) == 0) return;

    // 圧縮済み(.dds)があれば優先（読み出し帯域・メモリが減る）
    if (allowCompressed && LoadDdsTexture(m_Device.Get(), name, tex)) return;
    *tex = nullptr;

    Microsoft::WRL::ComPtr<IWICImagingFactory> fc;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fc));
    if (FAILED(hr) || !fc) return;

    WCHAR wn[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, name, -1, wn, MAX_PATH);

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> dec;
    hr = fc->CreateDecoderFromFilename(wn, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec);
    if (FAILED(hr) || !dec) return;

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> fr;
    hr = dec->GetFrame(0, &fr);
    if (FAILED(hr) || !fr) return;

    Microsoft::WRL::ComPtr<IWICFormatConverter> cnv;
    hr = fc->CreateFormatConverter(&cnv);
    if (FAILED(hr) || !cnv) return;

    hr = cnv->Initialize(fr.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return;

    UINT w, h;
    cnv->GetSize(&w, &h);
    BYTE* px = new BYTE[w * h * 4];
    hr = cnv->CopyPixels(NULL, w * 4, w * h * 4, px);
    if (FAILED(hr)) {
        delete[] px;
        return;
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w;
    td.Height = h;
    td.MipLevels = 0;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> t2;
    hr = m_Device->CreateTexture2D(&td, NULL, t2.GetAddressOf());
    if (SUCCEEDED(hr) && t2) {
        m_DeviceContext->UpdateSubresource(t2.Get(), 0, NULL, px, w * 4, 0);
        hr = m_Device->CreateShaderResourceView(t2.Get(), NULL, tex);
        if (SUCCEEDED(hr) && *tex) {
            m_DeviceContext->GenerateMips(*tex);
        }
    }

    delete[] px;
}
void Renderer::SetTexture(ID3D11ShaderResourceView* t) { m_DeviceContext->PSSetShaderResources(0, 1, &t); }
void Renderer::SetNormalMap(ID3D11ShaderResourceView* t) { m_DeviceContext->PSSetShaderResources(1, 1, &t); }
void Renderer::BindCubePipeline(ID3D11VertexShader* customVS, ID3D11PixelShader* customPS) {
    UINT stride = sizeof(VERTEX_3D), offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, m_CubeVertexBuffer.GetAddressOf(), &stride, &offset);
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_DeviceContext->IASetInputLayout(m_CubeInputLayout.Get());

    // アウトライン描画パスの場合は専用のアウトラインシェーダーを優先バインド
    if (m_IsOutlineMode) {
        m_DeviceContext->VSSetShader(m_OutlineVS.Get(), NULL, 0);
        m_DeviceContext->PSSetShader(m_OutlinePS.Get(), NULL, 0);
    } else {
        m_DeviceContext->VSSetShader(customVS ? customVS : m_CubeVertexShader.Get(), NULL, 0);
        m_DeviceContext->PSSetShader(customPS ? customPS : m_CubePixelShader.Get(), NULL, 0);
    }
}

void Renderer::DrawCubeInternal(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* texture, ID3D11PixelShader* customPS) {
    SetWorldMatrix(worldMatrix);
    SetTexture(texture);

    ID3D11ShaderResourceView* nullSRV = NULL;
    m_DeviceContext->PSSetShaderResources(1, 1, &nullSRV);

    MATERIAL material = {};
    material.Diffuse = XMFLOAT4(1, 1, 1, 1);
    material.Ambient = XMFLOAT4(1, 1, 1, 1);
    material.Specular = XMFLOAT4(0.6f, 0.6f, 0.6f, 1);
    material.Shininess = 20.0f;
    material.TextureEnable = TRUE;
    SetMaterial(material);

    BindCubePipeline(m_CubeVertexShader.Get(), customPS);
    m_DeviceContext->Draw(36, 0);
}

void Renderer::SetupCubeDraw() {
    BindCubePipeline();
}

void Renderer::DrawCube(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* texture)
{
    DrawCubeInternal(worldMatrix, texture, m_CubePixelShader.Get());
}

void Renderer::DrawCubeWithTestShader(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* texture)
{
    DrawCubeInternal(worldMatrix, texture, m_TestPixelShader.Get());
}

void Renderer::CopySceneTexture()
{
    if (!m_DeviceContext || !m_BackgroundCopyTexture || !m_SceneRTV) return;
    if (!GraphicsConfig::GetConst().refractionCopy) return;

    Microsoft::WRL::ComPtr<ID3D11Resource> pRtvResource;
    m_SceneRTV->GetResource(pRtvResource.GetAddressOf());

    if (pRtvResource)
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> pSceneTex;
        if (SUCCEEDED(pRtvResource.As(&pSceneTex)) && pSceneTex)
        {
            m_DeviceContext->CopyResource(m_BackgroundCopyTexture.Get(), pSceneTex.Get());
        }
    }
}

void Renderer::DrawCubeWithWaterShader(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* normalMap1, ID3D11ShaderResourceView* normalMap2, float time, const XMFLOAT3& waveParams, float shininess, float fresnelPower, const XMFLOAT4& shallowColor, const XMFLOAT4& deepColor, const XMFLOAT2& scrollSpeed1, const XMFLOAT2& scrollSpeed2)
{
    SetWorldMatrix(worldMatrix);

    WaterParamCB paramCB;
    paramCB.Time = time;
    paramCB.WaveParams = waveParams;
    m_DeviceContext->UpdateSubresource(m_WaterParamBuffer.Get(), 0, nullptr, &paramCB, 0, 0);

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
    m_DeviceContext->UpdateSubresource(m_WaterLightBuffer.Get(), 0, nullptr, &lightCB, 0, 0);

    m_DeviceContext->VSSetConstantBuffers(3, 1, m_WaterParamBuffer.GetAddressOf());
    m_DeviceContext->PSSetConstantBuffers(2, 1, m_WaterLightBuffer.GetAddressOf());

    ID3D11ShaderResourceView* srvs[2] = { normalMap1, normalMap2 };
    m_DeviceContext->PSSetShaderResources(0, 2, srvs);

    BindCubePipeline(m_WaterVS.Get(), m_WaterPS.Get());

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
    m_DeviceContext->UpdateSubresource(m_DissolveBuffer.Get(), 0, nullptr, &cb, 0, 0);

    m_DeviceContext->PSSetConstantBuffers(1, 1, m_DissolveBuffer.GetAddressOf());

    ID3D11ShaderResourceView* srvs[2] = { mainTexture, noiseTexture };
    m_DeviceContext->PSSetShaderResources(0, 2, srvs);

    BindCubePipeline(m_CubeVertexShader.Get(), m_DissolvePS.Get());

    m_DeviceContext->Draw(36, 0);

    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
    m_DeviceContext->PSSetShaderResources(0, 2, nullSRVs);
}

void Renderer::DrawCubeWithRefractShader(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* normalMap, float refractionIndex, float fresnelPower, const XMFLOAT4& highlightColor)
{
    SetWorldMatrix(worldMatrix);

    GlassCB cb = {};
    cb.RefractionIndex = refractionIndex;
    cb.FresnelPower = fresnelPower;
    cb.ScreenSize = XMFLOAT2((float)m_SceneWidth, (float)m_SceneHeight);   // 背景コピーはシーン解像度
    cb.HighlightColor = highlightColor;
    m_DeviceContext->UpdateSubresource(m_GlassBuffer.Get(), 0, nullptr, &cb, 0, 0);

    m_DeviceContext->PSSetConstantBuffers(1, 1, m_GlassBuffer.GetAddressOf());

    ID3D11ShaderResourceView* srvs[2] = { m_BackgroundCopySRV.Get(), normalMap };
    m_DeviceContext->PSSetShaderResources(0, 2, srvs);

    BindCubePipeline(m_CubeVertexShader.Get(), m_RefractPS.Get());

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
    m_DeviceContext->PSSetShader(m_RefractPS.Get(), NULL, 0);

    // テクスチャとサンプラーを設定
    // t0: バックバッファコピー, t1: 法線マップ, t2: スカイテクスチャ（フォールバック）
    m_DeviceContext->PSSetShaderResources(0, 1, m_BackgroundCopySRV.GetAddressOf());
    m_DeviceContext->PSSetShaderResources(1, 1, &normalMap);
    m_DeviceContext->PSSetShaderResources(2, 1, &skyTexture); // スカイテクスチャをフォールバックとしてバインド
    m_DeviceContext->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());

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
    m_DeviceContext->UpdateSubresource(m_GlassBuffer.Get(), 0, nullptr, &cb, 0, 0);
    m_DeviceContext->PSSetConstantBuffers(1, 1, m_GlassBuffer.GetAddressOf());

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
    m_DeviceContext->PSSetShader(m_WaterPS.Get(), NULL, 0);

    // 法線マップ 2 枚と頂点シェーダーを水面用に切り替え
    m_DeviceContext->PSSetShaderResources(0, 1, &normalMap1);
    m_DeviceContext->PSSetShaderResources(1, 1, &normalMap2);
    m_DeviceContext->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());

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
    m_DeviceContext->UpdateSubresource(m_WaterLightBuffer.Get(), 0, nullptr, &wcb, 0, 0);
    m_DeviceContext->PSSetConstantBuffers(2, 1, m_WaterLightBuffer.GetAddressOf());

    // 頂点バッファ・レイアウト・頂点シェーダーの設定
    // 水面用頂点シェーダー（WaterVS）を使用して波を頂点レベルで変形する
    UINT stride = sizeof(VERTEX_3D), offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 三角形リストで描画
    m_DeviceContext->IASetInputLayout(layout);
    m_DeviceContext->VSSetShader(m_WaterVS.Get(), NULL, 0);

    // WaterParamCB の更新（頂点シェーダー側の波変形パラメータ）
    // WaterVS.hlsl は register(b3) を使用しているため slot 3 にバインドする
    WaterParamCB wpcb;
    wpcb.Time       = time;
    wpcb.WaveParams = waveParams;
    m_DeviceContext->UpdateSubresource(m_WaterParamBuffer.Get(), 0, nullptr, &wpcb, 0, 0);
    m_DeviceContext->VSSetConstantBuffers(3, 1, m_WaterParamBuffer.GetAddressOf()); // b3 に合わせる

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