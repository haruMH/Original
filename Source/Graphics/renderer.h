#pragma once
#include "main.h"
#include <string>
#include <wrl/client.h>

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

// 点光源（焚き火・松明など最大4灯。ピクセルシェーダー b7）
static const int MAX_POINT_LIGHTS = 4;
struct POINTLIGHT {
    XMFLOAT4 PosRadius[MAX_POINT_LIGHTS];   // xyz: 位置, w: 届く半径
    XMFLOAT4 Color[MAX_POINT_LIGHTS];       // rgb: 色, w: 強さ(0で無効)
};

struct POSTEFFECT {
    int      Mode;
    float    Threshold;
    float    BlurIntensity;
    float    SlowMotionIntensity;  // スローモーション中のカラー彩度・色調補正度
    XMFLOAT4 FlashColor;           // 画面フラッシュ / 調色カラー (RGB: 色, A: 強度)
    float    RadialBlurIntensity;  // 放射ブラー強度 (0.0〜1.0)
    float    ChromaticAberration;  // 色収差強度 (0.0〜1.0)
    XMFLOAT2 RadialBlurCenter;     // 放射ブラー中心UV (0.5, 0.5)
    XMFLOAT2 TexelSize;            // 画面テクセルサイズ (1/幅, 1/高さ)
    XMFLOAT2 Dummy;                // 16バイトアライメントパディング
    XMFLOAT4 GradeParams;          // x: 彩度, y: コントラスト
    XMFLOAT4 ShadowTint;           // 暗部の色味 RGB
    XMFLOAT4 HighlightTint;        // 明部の色味 RGB
    XMFLOAT4 AoParams;             // x: SSAO強度(0で無効), y: ニア, z: ファー, w: 半径(ワールド単位)
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
    static D3D_FEATURE_LEVEL                                m_FeatureLevel;
    static Microsoft::WRL::ComPtr<ID3D11Device>             m_Device;
    static Microsoft::WRL::ComPtr<ID3D11DeviceContext>      m_DeviceContext;
    static Microsoft::WRL::ComPtr<IDXGISwapChain>           m_SwapChain;
    static Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_RenderTargetView;
    static Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_DepthStencilView;
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_WorldBuffer;
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_ViewBuffer;
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_ProjectionBuffer;
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_MaterialBuffer;
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_LightBuffer;
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_PointLightBuffer;
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_ShadowVPBuffer;
    static LIGHT                                            m_Light;
    static bool                                             m_IsShadowMode;
    static bool                                             m_IsOutlineMode;
    static XMMATRIX                                         m_ShadowMatrix;
    static Microsoft::WRL::ComPtr<ID3D11SamplerState>       m_SamplerState;
    static Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  m_DepthStateEnable;
    static Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  m_DepthStateDisable;
    static Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  m_DepthStateOutline;
    static Microsoft::WRL::ComPtr<ID3D11RasterizerState>    m_RasterizerStateCullBack;
    static Microsoft::WRL::ComPtr<ID3D11RasterizerState>    m_RasterizerStateCullFront;
    static Microsoft::WRL::ComPtr<ID3D11BlendState>         m_BlendState;
    
    // シャドウマップ
    static Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_ShadowMapTexture;
    static Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_ShadowMapDSV;
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_ShadowMapSRV;
    static Microsoft::WRL::ComPtr<ID3D11SamplerState>       m_ShadowSampler;
    static D3D11_VIEWPORT                                   m_ShadowViewport;

    // ポストプロセス（ブルーム）用リソース
    static Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_SceneRTV;
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_SceneSRV;
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_DepthSRV;   // SSAO用の深度読み取り
    static Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_LumRTV;
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_LumSRV;
    static Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_BlurRTV;
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_BlurSRV;
    static Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_AoRTV;   // SSAO(半解像度)
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_AoSRV;
    static D3D11_VIEWPORT                                   m_AoViewport;

    // 描画解像度スケール(renderScale)。シーン/深度/ポストは scene サイズ、合成の出力だけ画面サイズ
    static UINT                                             m_SceneWidth;
    static UINT                                             m_SceneHeight;
    static float                                            m_AppliedRenderScale;
    static Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_LdrRTV;          // scale<1 のみ。合成結果を受けるシーン解像度の一時画像(オーラ等もここへ描く)
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_LdrSRV;
    static Microsoft::WRL::ComPtr<ID3D11PixelShader>        m_UpscalePS;       // 一時画像 → バックバッファの拡大

    static Microsoft::WRL::ComPtr<ID3D11VertexShader>       m_OutlineVS;
    static Microsoft::WRL::ComPtr<ID3D11PixelShader>        m_OutlinePS;
    static Microsoft::WRL::ComPtr<ID3D11VertexShader>       m_PostVS;
    static Microsoft::WRL::ComPtr<ID3D11PixelShader>        m_PostPS;
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_PostBuffer;
    static D3D11_VIEWPORT                                   m_PostViewport;

    static XMMATRIX m_ViewMatrix;
    static XMMATRIX m_ProjectionMatrix;

    // キューブ描画用の共通 GPU リソース
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_CubeVertexBuffer;
    static Microsoft::WRL::ComPtr<ID3D11VertexShader>       m_CubeVertexShader;
    static Microsoft::WRL::ComPtr<ID3D11PixelShader>        m_CubePixelShader;
    static Microsoft::WRL::ComPtr<ID3D11InputLayout>        m_CubeInputLayout;
    static Microsoft::WRL::ComPtr<ID3D11PixelShader>        m_TestPixelShader;

    // 新規エフェクト用リソース
    static Microsoft::WRL::ComPtr<ID3D11VertexShader>       m_WaterVS;
    static Microsoft::WRL::ComPtr<ID3D11PixelShader>        m_WaterPS;
    static Microsoft::WRL::ComPtr<ID3D11PixelShader>        m_DissolvePS;
    static Microsoft::WRL::ComPtr<ID3D11PixelShader>        m_RefractPS;
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_WaterParamBuffer;
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_WaterLightBuffer;
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_DissolveBuffer;
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_GlassBuffer;
    static Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_BackgroundCopyTexture;
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_BackgroundCopySRV;

    // コンスタントバッファ（CBuffer）キャッシュ（更新の最小化用）
    static MATERIAL             m_MaterialCache;
    static LIGHT                m_LightCache;
    static XMFLOAT4X4           m_ViewCache;
    static XMFLOAT4X4           m_ProjectionCache;
    static bool                 m_IsCacheInitialized;

    // 内部共通化ヘルパー（重複排除用）
    static void CreateScreenSizeResources(UINT width, UINT height);
    static void ReleaseScreenSizeResources();
    static void SetScreenViewport();
    static void SetSceneViewport();
    static void ApplyRenderScaleIfChanged();
    static void BindCubePipeline(ID3D11VertexShader* customVS = nullptr, ID3D11PixelShader* customPS = nullptr);
    static void DrawCubeInternal(const XMMATRIX& worldMatrix, ID3D11ShaderResourceView* texture, ID3D11PixelShader* customPS);

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

    // GPU 処理時間の計測（タイムスタンプクエリ。結果は 2 フレーム遅れで取れる）。
    // 区間(12): 0:影など準備 1:空 2:地面 3:物体 4:アウトライン 5:丘と木 6:草 7:エフェクト・UI
    //           8:ブルーム 9:SSAO 10:合成 11:ImGui・オーラなど
    static void  GpuFrameBegin();
    static void  GpuMark(int index);                 // index 1〜12 の区切りを記録（0 は GpuFrameBegin が記録）
    static void  GpuFrameEnd();
    static float GetGpuSegmentMs(int segment);       // segment 0〜11
    static void SetPointLight(int index, const XMFLOAT3& pos, float radius, const XMFLOAT3& color, float intensity); // intensity 0 で消灯
    static void SetCameraPosition(XMFLOAT3 p);
    static void SetShadowMode(bool e);
    static bool IsOutlineMode() { return m_IsOutlineMode; }
    static void SetShadowMatrix(XMMATRIX m);
    
    static LIGHT GetLight() { return m_Light; }
    static XMMATRIX GetViewMatrix() { return m_ViewMatrix; }
    static XMMATRIX GetProjectionMatrix() { return m_ProjectionMatrix; }
    static XMMATRIX GetShadowMatrix() { return m_ShadowMatrix; }
    static ID3D11PixelShader* GetOutlinePixelShader() { return m_OutlinePS.Get(); }
    
    static ID3D11Device* GetDevice() { return m_Device.Get(); }
    static ID3D11DeviceContext* GetDeviceContext() { return m_DeviceContext.Get(); }
    static ID3D11Buffer* GetCubeVertexBuffer() { return m_CubeVertexBuffer.Get(); }
    static ID3D11RasterizerState* GetRasterizerStateCullBack() { return m_RasterizerStateCullBack.Get(); }
    static ID3D11RasterizerState* GetRasterizerStateCullFront() { return m_RasterizerStateCullFront.Get(); }
    static ID3D11DepthStencilState* GetDepthStateEnable() { return m_DepthStateEnable.Get(); }
    static ID3D11DepthStencilState* GetDepthStateOutline() { return m_DepthStateOutline.Get(); }
    static ID3D11BlendState*        GetBlendState() { return m_BlendState.Get(); }
    static bool IsShadowMode() { return m_IsShadowMode; }

    // 解像度管理
    static UINT m_ScreenWidth;
    static UINT m_ScreenHeight;
    static UINT GetScreenWidth() { return m_ScreenWidth; }
    static UINT GetScreenHeight() { return m_ScreenHeight; }
    static UINT GetSceneWidth()  { return m_SceneWidth; }    // 実際に3Dを描く解像度(renderScale 反映)
    static UINT GetSceneHeight() { return m_SceneHeight; }
    static void OnResize(UINT width, UINT height);
    static void CreateVertexShader(ID3D11VertexShader** vs, ID3D11InputLayout** il, const char* name);
    static void CreatePixelShader(ID3D11PixelShader** ps, const char* name);
    // シェーダーファイルのパスを構成に合わせて解決する（2-1 対応）
    // 呼び出し元は常にベース名（例: "vertexShader.cso"）を渡すだけでよい
    static std::string ResolveShaderPath(const std::string& fileName);
    // allowCompressed=false のときは .dds を使わず元画像(RGBA8)を読む（テクスチャ配列など形式を揃える必要がある用途向け）
    static void CreateTexture(const char* name, ID3D11ShaderResourceView** tex, bool allowCompressed = true);
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
    static void SetAnisotropy(int maxAniso);
    
    // カリングモード切り替え関数
    static void SetCullMode(bool cullBack)
    {
        if (cullBack)
            m_DeviceContext->RSSetState(m_RasterizerStateCullBack.Get());
        else
            m_DeviceContext->RSSetState(m_RasterizerStateCullFront.Get());
    }

    // Field（平面）用の鏡面反射描画関数（スカイテクスチャフォールバック対応）
    static void DrawFieldWithRefractShader(ID3D11Buffer* vertexBuffer, int vertexCount, ID3D11InputLayout* layout, ID3D11VertexShader* vs, ID3D11ShaderResourceView* normalMap, float refractionIndex, float fresnelPower, const XMFLOAT4& highlightColor, float time, const XMFLOAT3& cameraPos, ID3D11ShaderResourceView* skyTexture);
    // Field（平面）用の水面描画関数
    static void DrawFieldWithWaterShader(ID3D11Buffer* vertexBuffer, int vertexCount, ID3D11InputLayout* layout, ID3D11VertexShader* vs, ID3D11ShaderResourceView* normalMap1, ID3D11ShaderResourceView* normalMap2, float time, const XMFLOAT3& waveParams, float shininess, float fresnelPower, const XMFLOAT4& shallowColor, const XMFLOAT4& deepColor, const XMFLOAT2& scrollSpeed1, const XMFLOAT2& scrollSpeed2);
};