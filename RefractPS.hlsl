// RefractPS.hlsl - 高品質鏡面反射シェーダー（スカイフォールバック版）
// - 反射UV が画面外に出た場合、スカイテクスチャを直接サンプリング
// - グリッドテクスチャを法線マップ歪みから除外（sin/cos 数学波のみ使用）
// - カメラ距離に基づく床テクスチャ×反射ブレンド（足元は透過、遠方は純鏡面）
Texture2D    BackgroundTexture : register(t0); // コピーしたフレームバッファ
Texture2D    NormalMap         : register(t1); // 法線マップ（現在は数学的波紋で代替）
Texture2D    SkyTexture        : register(t2); // スカイテクスチャ（反射フォールバック）
SamplerState Sampler           : register(s0);

cbuffer GlassBuffer : register(b1)
{
    float  RefractionIndex;   // 歪みの強さ
    float  FresnelPower;      // フレネル指数
    float2 ScreenSize;        // 画面解像度 (例: 1920, 1080)
    float4 HighlightColor;    // フレネルエッジのハイライト色
    float  Time;              // 時間（波紋アニメーション用）
    float  WaveStrength;      // 波紋の強度
    float2 Dummy;
    float3 CameraWorldPos;    // カメラのワールド座標
    float  MirrorBlend;       // 鏡面ブレンド率（1.0=完全鏡面）
};

struct PS_INPUT
{
    float4 Position     : SV_POSITION;
    float3 Normal       : NORMAL;
    float4 Diffuse      : COLOR;
    float2 TexCoord     : TEXCOORD0;
    float3 WorldPos     : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    // 画面 UV 計算
    float2 screenUV = input.Position.xy / ScreenSize;

    // 時間変化する多層波紋（鏡の微細なゆらぎ）
    // 鏡モードなので振幅を小さく抑えて穏やかなゆらぎにする
    float t = Time;
    float2 uv = input.TexCoord;

    float wave1X = sin(uv.x * 5.0f + t * 0.4f) * 0.005f;
    float wave1Y = cos(uv.y * 4.0f + t * 0.3f) * 0.004f;
    float wave2X = cos((uv.x + uv.y) * 8.0f - t * 0.7f) * 0.003f;
    float wave2Y = sin((uv.x - uv.y) * 6.0f + t * 0.5f) * 0.003f;

    // RefractionIndex で歪みの強さをスケール
    float2 waveOffset = float2(wave1X + wave2X, wave1Y + wave2Y) * WaveStrength * max(RefractionIndex * 30.0f, 0.1f);

    // 画面 UV を Y 反転して鏡面反射座標を作成
    float2 reflectUV = float2(screenUV.x, 1.0f - screenUV.y) + waveOffset;

    // 反射色のサンプリング
    // 反射 UV が画面内ならバックバッファを、画面外ならスカイテクスチャをサンプリング
    float4 reflectColor;
    bool isOffScreen = (reflectUV.x < 0.01f || reflectUV.x > 0.99f ||
                        reflectUV.y < 0.01f || reflectUV.y > 0.99f);
    if (isOffScreen)
    {
        // スカイテクスチャから直接サンプリング（画面端の矩形アーティファクト解消）
        // 反射UV の X をそのまま、Y を上空にマッピング
        float2 skyUV = float2(clamp(reflectUV.x, 0.0f, 1.0f),
                              clamp(1.0f - reflectUV.y, 0.0f, 0.5f)); // 上半分を使用
        reflectColor = SkyTexture.Sample(Sampler, skyUV);
    }
    else
    {
        reflectColor = BackgroundTexture.Sample(Sampler, reflectUV);
    }

    // カメラからの視線ベクトルを計算
    float3 V = normalize(CameraWorldPos - input.WorldPos);
    float3 N = normalize(input.Normal);

    // フレネル計算: 水平視線ほど完全鏡面、真下ほど透明
    float dotNV = saturate(dot(N, V));
    float fresnel = pow(1.0f - dotNV, FresnelPower);

    // カメラからの距離に基づく鏡面ブレンド率
    // 近距離: 少し透過して地面のわずかな色を混ぜる（リアリティ向上）
    // 遠距離: 完全鏡面（ウユニ塩湖のように地平線が完全な鏡）
    float dist = length(CameraWorldPos.xz - input.WorldPos.xz); // XZ 平面の距離
    float distBlend = saturate(dist / 20.0f); // 20m 以遠は完全鏡面
    float mirrorAmount = saturate(fresnel * 0.6f + distBlend * MirrorBlend);

    // 反射色に微妙な夕焼け色ティント（空の暖色と調和）
    float3 skyTint = lerp(float3(1.0f, 1.0f, 1.0f), float3(1.05f, 0.92f, 0.88f), mirrorAmount);
    reflectColor.rgb *= skyTint;

    // エッジのハイライト（フレネル光沢）
    float4 edgeGlow = HighlightColor * pow(fresnel, 3.0f) * 0.4f;

    // 最終色合成
    float4 finalColor = reflectColor * mirrorAmount + edgeGlow;

    // キラキラスペキュラー（太陽光の反射点）
    float3 L = normalize(float3(0.2f, 0.9f, 0.4f)); // 太陽方向（夕日）
    float3 H = normalize(L + V);
    float specular = pow(max(dot(N, H), 0.0f), 120.0f);
    finalColor.rgb += float3(1.0f, 0.9f, 0.75f) * specular * 0.4f;

    finalColor.a = 1.0f;
    return finalColor;
}
