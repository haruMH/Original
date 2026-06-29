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

// 水面シェーダー用定数バッファ構造体 (VS)
struct WaterParamCB {
    float Time;
    XMFLOAT3 WaveParams; // x: 振幅, y: 周波数, z: 移動速度
};

// 水面シェーダー用定数バッファ構造体 (PS)
struct WaterLightCB {
    XMFLOAT3 LightDirection;
    float  Shininess;
    XMFLOAT3 CameraPosition;
    float  FresnelPower;
    XMFLOAT4 WaterColorShallow;
    XMFLOAT4 WaterColorDeep;
    XMFLOAT2 ScrollSpeed1;
    XMFLOAT2 ScrollSpeed2;
    float  TimeVal;
    XMFLOAT3 Dummy;
};

// ディゾルブシェーダー用定数バッファ構造体
struct DissolveCB {
    float  Threshold;
    float  EdgeWidth;
    XMFLOAT2 Dummy;
    XMFLOAT4 EdgeColor;
};

// 屈折ガラスシェーダー用定数バッファ構造体
struct GlassCB {
    float    RefractionIndex;
    float    FresnelPower;
    XMFLOAT2 ScreenSize;
    XMFLOAT4 HighlightColor;
    float    Time;           // 波紋アニメーション用時間
    float    WaveStrength;   // 波紋の強度
    XMFLOAT2 Dummy;
    XMFLOAT3 CameraWorldPos; // カメラのワールド座標
    float    MirrorBlend;    // 鏡面ブレンド率（1.0=完全鏡面）
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
    static ID3D11PixelShader*   m_TestPixelShader;

    // 新規エフェクト用リソース
    static ID3D11VertexShader*  m_WaterVS;
    static ID3D11PixelShader*   m_WaterPS;
    static ID3D11PixelShader*   m_DissolvePS;
    static ID3D11PixelShader*   m_RefractPS;
    static ID3D11Buffer*        m_WaterParamBuffer;
    static ID3D11Buffer*        m_WaterLightBuffer;
    static ID3D11Buffer*        m_DissolveBuffer;
    static ID3D11Buffer*        m_GlassBuffer;
    static ID3D11Texture2D*     m_BackgroundCopyTexture;
    static ID3D11ShaderResourceView* m_BackgroundCopySRV;

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
    static void BeginNewFrame();
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
	static void DrawCubeWithTestShader(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* texture);
    static void CopySceneTexture();
    static void DrawCubeWithWaterShader(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* normalMap1, ID3D11ShaderResourceView* normalMap2, float time, const XMFLOAT3& waveParams, float shininess, float fresnelPower, const XMFLOAT4& shallowColor, const XMFLOAT4& deepColor, const XMFLOAT2& scrollSpeed1, const XMFLOAT2& scrollSpeed2);
    static void DrawCubeWithDissolveShader(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* mainTexture, ID3D11ShaderResourceView* noiseTexture, float threshold, float edgeWidth, const XMFLOAT4& edgeColor);
    static void DrawCubeWithRefractShader(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* normalMap, float refractionIndex, float fresnelPower, const XMFLOAT4& highlightColor);
    
    // カリングモード切り替え関数
    static void SetCullMode(bool cullBack)
    {
        if (cullBack)
            m_DeviceContext->RSSetState(m_RasterizerStateCullBack);
        else
            m_DeviceContext->RSSetState(m_RasterizerStateCullFront);
    }

    // Field（平面）用の鏡面反射描画関数（スカイテクスチャフォールバック対応）
    static void DrawFieldWithRefractShader(ID3D11Buffer* vertexBuffer, int vertexCount, ID3D11InputLayout* layout, ID3D11VertexShader* vs, ID3D11ShaderResourceView* normalMap, float refractionIndex, float fresnelPower, const XMFLOAT4& highlightColor, float time, const XMFLOAT3& cameraPos, ID3D11ShaderResourceView* skyTexture);
    // Field（平面）用の水面描画関数
    static void DrawFieldWithWaterShader(ID3D11Buffer* vertexBuffer, int vertexCount, ID3D11InputLayout* layout, ID3D11VertexShader* vs, ID3D11ShaderResourceView* normalMap1, ID3D11ShaderResourceView* normalMap2, float time, const XMFLOAT3& waveParams, float shininess, float fresnelPower, const XMFLOAT4& shallowColor, const XMFLOAT4& deepColor, const XMFLOAT2& scrollSpeed1, const XMFLOAT2& scrollSpeed2);
};