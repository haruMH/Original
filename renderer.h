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
    float SlowMotionIntensity; // スローモーション中のカラー彩度・色調補正度
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
    static ID3D11DepthStencilState* m_DepthStateOutline;
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

    // キューブ描画用の共通 GPU リソース
    static ID3D11Buffer*        m_CubeVertexBuffer;
    static ID3D11VertexShader*  m_CubeVertexShader;
    static ID3D11PixelShader*   m_CubePixelShader;
    static ID3D11InputLayout*   m_CubeInputLayout;

    // コンスタントバッファ（CBuffer）キャッシュ（更新の最小化用）
    static MATERIAL             m_MaterialCache;
    static LIGHT                m_LightCache;
    static XMFLOAT4X4           m_ViewCache;
    static XMFLOAT4X4           m_ProjectionCache;
    static bool                 m_IsCacheInitialized;

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
    static XMMATRIX GetShadowMatrix() { return m_ShadowMatrix; }
    static ID3D11PixelShader* GetOutlinePixelShader() { return m_OutlinePS; }
    
    static ID3D11Device* GetDevice() { return m_Device; }
    static ID3D11DeviceContext* GetDeviceContext() { return m_DeviceContext; }
    static ID3D11Buffer* GetCubeVertexBuffer() { return m_CubeVertexBuffer; }
    static bool IsShadowMode() { return m_IsShadowMode; }
    static void CreateVertexShader(ID3D11VertexShader** vs, ID3D11InputLayout** il, const char* name);
    static void CreatePixelShader(ID3D11PixelShader** ps, const char* name);
    static void CreateTexture(const char* name, ID3D11ShaderResourceView** tex);
    static void SetTexture(ID3D11ShaderResourceView* tex);
    static void SetNormalMap(ID3D11ShaderResourceView* tex);
    // キューブ描画の共通 GPU リソースをパイプラインにセットする
    static void SetupCubeDraw();
	static void DrawCube(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* texture);
};