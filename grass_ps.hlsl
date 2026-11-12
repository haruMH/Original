// 草のピクセルシェーダー: 時間帯のライトとフォグに合わせる（他の物体と同じ光量・霧の色）
cbuffer LightBuffer : register(b4)
{
    int4   LightEnablePacked;
    float4 LightDirection;
    float4 LightDiffuse;
    float4 LightAmbient;
    float4 CameraPosition;
    float4 FogColor;
    float  FogStart;
    float  FogEnd;
    float2 FogDummy;
}

struct VS_OUT
{
    float4 pos      : SV_POSITION;
    float3 col      : COLOR;
    float3 worldPos : TEXCOORD0;
};

float4 main(VS_OUT i) : SV_TARGET
{
    float3 lit = LightAmbient.rgb * 1.0f + LightDiffuse.rgb * 0.55f;
    float3 c = i.col * lit;

    // 距離フォグ（他の物体と同じ範囲）
    float dist = distance(CameraPosition.xyz, i.worldPos);
    float distFog = saturate((dist - FogStart) / max(FogEnd - FogStart, 0.001f));
    float fog = saturate(distFog * 1.2f);
    c = lerp(c, FogColor.rgb, fog);

    return float4(c, 1.0f);
}
