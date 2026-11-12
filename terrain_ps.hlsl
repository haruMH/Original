// 遠景の丘・木用ピクセルシェーダー。頂点色 × 簡易トゥーンライティング × 遠景向けの穏やかな霧。
// 通常の物体は FogEnd で霧に完全に溶けるが、遠景は層の濃淡が見えるよう霧の上限を抑える（大気遠近法）。
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

struct PS_INPUT
{
    float4 Position      : SV_POSITION;
    float3 Normal        : NORMAL;
    float4 Diffuse       : COLOR;
    float2 TexCoord      : TEXCOORD0;
    float3 WorldPos      : TEXCOORD1;
    float4 LightSpacePos : TEXCOORD2;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float3 N = normalize(input.Normal);
    float3 L = normalize(-LightDirection.xyz);

    // ハーフランバートをやわらかい2段階に（トゥーン調）
    float ndl = dot(N, L) * 0.5f + 0.5f;
    float lightAmt = lerp(0.45f, 1.0f, smoothstep(0.20f, 0.90f, ndl));   // 連続的でやわらかい陰影
    float3 lit = LightAmbient.rgb * 0.95f + LightDiffuse.rgb * 0.60f * lightAmt;
    float3 c = input.Diffuse.rgb * lit;

    // 遠景の霧: 手前は薄く、遠いほど濃く。上限 0.90 で空と完全には同化させない
    float dist = distance(CameraPosition.xyz, input.WorldPos);
    float distFar = saturate((dist - 40.0f) / 280.0f);
    float fog = 0.04f + 0.80f * pow(distFar, 0.85f);
    c = lerp(c, FogColor.rgb, fog);

    return float4(c, 1.0f);
}
