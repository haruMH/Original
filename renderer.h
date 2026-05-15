#pragma once
#include "main.h"

struct VERTEX_3D {
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT4 Diffuse;
    XMFLOAT2 TexCoord;
    XMFLOAT3 Tangent;
};

struct MATERIAL {
    XMFLOAT4 Ambient;
    XMFLOAT4 Diffuse;
    XMFLOAT4 Specular;
    XMFLOAT4 Emission;
    float    Shininess;
    BOOL     TextureEnable;
    float    RimPower;
    float    Dummy;
};

struct LIGHT {
    BOOL     Enable;
    BOOL     Dummy[3];
    XMFLOAT4 Direction;
    XMFLOAT4 Diffuse;
    XMFLOAT4 Ambient;
    XMFLOAT4 CameraPosition;
    XMFLOAT4 FogColor;
    float    FogStart;
    float    FogEnd;
    float    Dummy2[2];
};

struct POSTEFFECT {
    int   Mode;
    float Threshold;
    float BlurIntensity;
    float Dummy;
};

class Renderer {
private:
    static D3D_FEATURE_LEVEL        m_FeatureLevel;
    static ID3D11Device*            m_Device;
    static ID3D11DeviceContext*     m_DeviceContext;
    static IDXGISwapChain*          m_SwapChain;
    static ID3D11RenderTargetView*  m_RenderTargetView;
    static ID3D11DepthStencilView*  m_DepthStencilView;
    static ID3D11Buffer*            m_WorldBuffer;
    static ID3D11Buffer*            m_ViewBuffer;
    static ID3D11Buffer*            m_ProjectionBuffer;
    static ID3D11Buffer*            m_MaterialBuffer;
    static ID3D11Buffer*            m_LightBuffer;
    static ID3D11Buffer*            m_ShadowVPBuffer;
    static LIGHT                    m_Light;
    static bool                     m_IsShadowMode;
    static bool                     m_IsOutlineMode;
    static XMMATRIX                 m_ShadowMatrix;
    static ID3D11SamplerState*      m_SamplerState;
    static ID3D11DepthStencilState* m_DepthStateEnable;
    static ID3D11DepthStencilState* m_DepthStateDisable;
    static ID3D11RasterizerState*   m_RasterizerStateCullBack;
    static ID3D11RasterizerState*   m_RasterizerStateCullFront;
    static ID3D11BlendState*        m_BlendState;
    
    // シャドウマップ
    static ID3D11Texture2D*          m_ShadowMapTexture;
    static ID3D11DepthStencilView*   m_ShadowMapDSV;
    static ID3D11ShaderResourceView* m_ShadowMapSRV;
    static ID3D11SamplerState*       m_ShadowSampler;
    static D3D11_VIEWPORT            m_ShadowViewport;

    // ポストプロセス（ブルーム）用リソース
    static ID3D11RenderTargetView*   m_SceneRTV;
    static ID3D11ShaderResourceView* m_SceneSRV;
    static ID3D11RenderTargetView*   m_LumRTV;
    static ID3D11ShaderResourceView* m_LumSRV;
    static ID3D11RenderTargetView*   m_BlurRTV;
    static ID3D11ShaderResourceView* m_BlurSRV;
    
    static ID3D11VertexShader*       m_OutlineVS;
    static ID3D11PixelShader*        m_OutlinePS;
    static ID3D11VertexShader*       m_PostVS;
    static ID3D11PixelShader*        m_PostPS;
    static ID3D11Buffer*             m_PostBuffer;
    static D3D11_VIEWPORT            m_PostViewport;

    static XMMATRIX m_ViewMatrix;
    static XMMATRIX m_ProjectionMatrix;

public:
    static void Init();
    static void Uninit();
    static void Begin();
    static void End();
    static void BeginShadowPass();
    static void EndShadowPass();
    static void BeginOutlinePass();
    static void EndOutlinePass();
    static void SetWorldMatrix(XMMATRIX m);
    static void SetViewMatrix(XMMATRIX m);
    static void SetProjectionMatrix(XMMATRIX m);
    static void SetShadowVPMatrix(XMMATRIX m);
    static void SetMaterial(MATERIAL m);
    static void SetLight(LIGHT l);
    static void SetCameraPosition(XMFLOAT3 p);
    static void SetShadowMode(bool e);
    static bool IsOutlineMode() { return m_IsOutlineMode; }
    static void SetShadowMatrix(XMMATRIX m);
    
    static LIGHT GetLight() { return m_Light; }
    static XMMATRIX GetViewMatrix() { return m_ViewMatrix; }
    static XMMATRIX GetProjectionMatrix() { return m_ProjectionMatrix; }
    
    static ID3D11Device* GetDevice() { return m_Device; }
    static ID3D11DeviceContext* GetDeviceContext() { return m_DeviceContext; }
    static void CreateVertexShader(ID3D11VertexShader** vs, ID3D11InputLayout** il, const char* name);
    static void CreatePixelShader(ID3D11PixelShader** ps, const char* name);
    static void CreateTexture(const char* name, ID3D11ShaderResourceView** tex);
    static void SetTexture(ID3D11ShaderResourceView* tex);
    static void SetNormalMap(ID3D11ShaderResourceView* tex);
};