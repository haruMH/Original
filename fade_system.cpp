#define _CRT_SECURE_NO_WARNINGS
#include "fade_system.h"
#include "main.h"
#include <stdio.h>
#include <d3dcompiler.h>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")

// 静的メンバ変数の実体定義
FadeState                FadeSystem::m_State = FadeState::None;
float                    FadeSystem::m_CurrentAlpha = 0.0f;
float                    FadeSystem::m_Timer = 0.0f;
float                    FadeSystem::m_Duration = 0.4f;
XMFLOAT4                 FadeSystem::m_FadeColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

ID3D11VertexShader*      FadeSystem::m_VS = nullptr;
ID3D11PixelShader*       FadeSystem::m_PS = nullptr;
ID3D11Buffer*            FadeSystem::m_CBuffer = nullptr;
ID3D11BlendState*        FadeSystem::m_BlendState = nullptr;
ID3D11DepthStencilState* FadeSystem::m_DepthState = nullptr;
ID3D11RasterizerState*   FadeSystem::m_RasterizerState = nullptr;

// 汎用 Clamp ヘルパー関数
static float ClampVal(float val, float minVal, float maxVal)
{
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

// ヘルパー: .cso または .hlsl から頂点シェーダー読み込み
static bool LoadVS(ID3D11Device* device, const char* csoName, const char* hlslName, ID3D11VertexShader** vsOut)
{
    FILE* fp = fopen(csoName, "rb");
    if (!fp) {
#ifdef NDEBUG
        std::string assetPath = std::string("Assets/shader/") + csoName;
        fp = fopen(assetPath.c_str(), "rb");
#endif
    }

    if (fp) {
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        unsigned char* buffer = new unsigned char[size];
        fread(buffer, size, 1, fp);
        fclose(fp);
        HRESULT hr = device->CreateVertexShader(buffer, size, NULL, vsOut);
        delete[] buffer;
        if (SUCCEEDED(hr)) return true;
    }

    // CSOからの読み込み失敗時、HLSLから動的にコンパイル
    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    WCHAR wHlslPath[512];
    MultiByteToWideChar(CP_ACP, 0, hlslName, -1, wHlslPath, 512);

    HRESULT hr = D3DCompileFromFile(wHlslPath, nullptr, nullptr, "main", "vs_5_0", 0, 0, &shaderBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA("[FadeSystem] VS Compile Error:\n");
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }
    hr = device->CreateVertexShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), NULL, vsOut);
    shaderBlob->Release();
    return SUCCEEDED(hr);
}

// ヘルパー: .cso または .hlsl からピクセルシェーダー読み込み
static bool LoadPS(ID3D11Device* device, const char* csoName, const char* hlslName, ID3D11PixelShader** psOut)
{
    FILE* fp = fopen(csoName, "rb");
    if (!fp) {
#ifdef NDEBUG
        std::string assetPath = std::string("Assets/shader/") + csoName;
        fp = fopen(assetPath.c_str(), "rb");
#endif
    }

    if (fp) {
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        unsigned char* buffer = new unsigned char[size];
        fread(buffer, size, 1, fp);
        fclose(fp);
        HRESULT hr = device->CreatePixelShader(buffer, size, NULL, psOut);
        delete[] buffer;
        if (SUCCEEDED(hr)) return true;
    }

    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    WCHAR wHlslPath[512];
    MultiByteToWideChar(CP_ACP, 0, hlslName, -1, wHlslPath, 512);

    HRESULT hr = D3DCompileFromFile(wHlslPath, nullptr, nullptr, "main", "ps_5_0", 0, 0, &shaderBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA("[FadeSystem] PS Compile Error:\n");
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }
    hr = device->CreatePixelShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), NULL, psOut);
    shaderBlob->Release();
    return SUCCEEDED(hr);
}

bool FadeSystem::Init(ID3D11Device* device)
{
    if (!device) return false;

    // 1. 頂点シェーダー読み込み (postEffect_vs)
    if (!LoadVS(device, "postEffect_vs.cso", "postEffect_vs.hlsl", &m_VS)) {
        OutputDebugStringA("[FadeSystem] Failed to load VS (postEffect_vs)\n");
        return false;
    }

    // 2. ピクセルシェーダー読み込み (fade_ps)
    if (!LoadPS(device, "fade_ps.cso", "fade_ps.hlsl", &m_PS)) {
        OutputDebugStringA("[FadeSystem] Failed to load PS (fade_ps)\n");
        return false;
    }

    // 3. 定数バッファ作成 (g_FadeColor)
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(XMFLOAT4);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;

    HRESULT hr = device->CreateBuffer(&bd, NULL, &m_CBuffer);
    if (FAILED(hr)) return false;

    // 4. アルファブレンディングステート作成
    D3D11_BLEND_DESC bdDesc = {};
    bdDesc.RenderTarget[0].BlendEnable = TRUE;
    bdDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bdDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bdDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bdDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bdDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bdDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bdDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = device->CreateBlendState(&bdDesc, &m_BlendState);
    if (FAILED(hr)) return false;

    // 5. 深度無効ステート作成 (全画面フェード用)
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = FALSE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;

    hr = device->CreateDepthStencilState(&dsd, &m_DepthState);
    if (FAILED(hr)) return false;

    // 6. カリング無効ラスタライザーステート作成
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE; // 全画面ポリゴンが裏面で消えないようカリング無効
    rd.DepthClipEnable = FALSE;

    hr = device->CreateRasterizerState(&rd, &m_RasterizerState);
    if (FAILED(hr)) return false;

    m_State = FadeState::None;
    m_CurrentAlpha = 0.0f;
    m_Timer = 0.0f;
    m_Duration = 0.4f;

    OutputDebugStringA("[FadeSystem] Successfully Initialized!\n");
    return true;
}

void FadeSystem::Uninit()
{
    if (m_VS) { m_VS->Release(); m_VS = nullptr; }
    if (m_PS) { m_PS->Release(); m_PS = nullptr; }
    if (m_CBuffer) { m_CBuffer->Release(); m_CBuffer = nullptr; }
    if (m_BlendState) { m_BlendState->Release(); m_BlendState = nullptr; }
    if (m_DepthState) { m_DepthState->Release(); m_DepthState = nullptr; }
    if (m_RasterizerState) { m_RasterizerState->Release(); m_RasterizerState = nullptr; }

    m_State = FadeState::None;
}

void FadeSystem::StartFadeOut(float duration, XMFLOAT4 color)
{
    m_State = FadeState::FadeOut;
    m_Duration = (duration > 0.0f) ? duration : 0.001f;
    m_Timer = 0.0f;
    m_CurrentAlpha = 0.0f;
    m_FadeColor = color;
    OutputDebugStringA("[FadeSystem] StartFadeOut Started\n");
}

void FadeSystem::StartFadeIn(float duration, XMFLOAT4 color)
{
    m_State = FadeState::FadeIn;
    m_Duration = (duration > 0.0f) ? duration : 0.001f;
    m_Timer = 0.0f;
    m_CurrentAlpha = 1.0f;
    m_FadeColor = color;
    OutputDebugStringA("[FadeSystem] StartFadeIn Started\n");
}

void FadeSystem::Update(float deltaTime)
{
    if (m_State == FadeState::None) return;

    m_Timer += deltaTime;
    float progress = ClampVal(m_Timer / m_Duration, 0.0f, 1.0f);

    // SmoothStep イージング関数による滑らかな遷移
    float smoothProgress = progress * progress * (3.0f - 2.0f * progress);

    if (m_State == FadeState::FadeOut) {
        m_CurrentAlpha = smoothProgress;
        if (progress >= 1.0f) {
            m_CurrentAlpha = 1.0f;
        }
    }
    else if (m_State == FadeState::FadeIn) {
        m_CurrentAlpha = 1.0f - smoothProgress;
        if (progress >= 1.0f) {
            m_CurrentAlpha = 0.0f;
            m_State = FadeState::None;
            OutputDebugStringA("[FadeSystem] FadeIn Complete -> None\n");
        }
    }
}

void FadeSystem::Draw(ID3D11DeviceContext* context)
{
    if (m_State == FadeState::None || m_CurrentAlpha <= 0.001f) return;
    if (!context || !m_VS || !m_PS || !m_CBuffer) return;

    // ビューポートの設定 (全画面 1280x720)
    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)SCREEN_WIDTH;
    vp.Height = (FLOAT)SCREEN_HEIGHT;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    context->RSSetViewports(1, &vp);

    // ラスタライザーステート設定 (カリング無効)
    if (m_RasterizerState) {
        context->RSSetState(m_RasterizerState);
    }

    // 定数バッファの更新
    XMFLOAT4 currentColor = m_FadeColor;
    currentColor.w = m_CurrentAlpha;
    context->UpdateSubresource(m_CBuffer, 0, NULL, &currentColor, 0, 0);

    // パイプラインの設定
    context->VSSetShader(m_VS, NULL, 0);
    context->PSSetShader(m_PS, NULL, 0);
    context->PSSetConstantBuffers(0, 1, &m_CBuffer);

    // ブレンドステートと深度ステートの適用
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(m_BlendState, blendFactor, 0xffffffff);
    context->OMSetDepthStencilState(m_DepthState, 0);

    // 全画面描画
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(NULL);
    context->Draw(3, 0);
}
