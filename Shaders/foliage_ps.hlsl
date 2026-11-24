// 木・茂み・花など自然物用の軽いピクセルシェーダー。
// 標準の pixelShader.hlsl との違い: 影のサンプリング(16タップ)・床の光沢・リムなどを省く。
// 葉の板は重なりが多く（アルファテストで早期深度が効かない）、標準シェーダーだと高コストになるため。
// ライティングと霧の計算は標準シェーダーに合わせてあり、他の物体と並べても浮かない。
Texture2D    g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

struct MATERIAL {
    float4 Ambient; float4 Diffuse; float4 Specular; float4 Emission;
    float Shininess; bool TextureEnable; float RimPower; float Dummy;
};
cbuffer MaterialBuffer : register(b3) { MATERIAL Material; }

cbuffer LightBuffer : register(b4) {
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

cbuffer PointLightBuffer : register(b7) {
    float4 PLPosRadius[4];
    float4 PLColor[4];
}

struct PS_INPUT {
    float4 Position      : SV_POSITION;
    float3 Normal        : NORMAL;
    float4 Diffuse       : COLOR;
    float2 TexCoord      : TEXCOORD0;
    float3 WorldPos      : TEXCOORD1;
    float4 LightSpacePos : TEXCOORD2;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 color = input.Diffuse;
    if (Material.TextureEnable)
    {
        float4 tex = g_Texture.Sample(g_Sampler, input.TexCoord);
        if (Material.Dummy > 0.0f && tex.a < Material.Dummy) discard;   // 葉の透明部分を切り抜く
        if (tex.r + tex.g + tex.b > 0.01f) color = tex * input.Diffuse;
    }

    float3 N = normalize(input.Normal);
    float3 L = normalize(-LightDirection.xyz);

    // トゥーン階調（標準シェーダーと同じ式）
    float diffuse = max(dot(N, L), 0.0f);
    float toonDiff;
    if (diffuse > 0.35f) toonDiff = lerp(0.65f, 1.00f, smoothstep(0.76f, 0.82f, diffuse));
    else                 toonDiff = lerp(0.28f, 0.65f, smoothstep(0.33f, 0.38f, diffuse));

    // 半球アンビエント
    float hemiFactor = N.y * 0.5f + 0.5f;
    float3 fogHue = FogColor.rgb / max(max(FogColor.r, max(FogColor.g, FogColor.b)), 0.001f);
    float3 skyLight    = LightAmbient.rgb * lerp(float3(0.95f, 1.05f, 1.30f), 0.55f + 0.9f * fogHue, 0.30f);
    float3 groundLight = LightAmbient.rgb * float3(1.20f, 1.05f, 0.85f);
    float3 hemiAmbient = lerp(groundLight, skyLight, hemiFactor);

    float3 lighting = saturate(toonDiff * LightDiffuse.rgb * Material.Diffuse.rgb + hemiAmbient * Material.Ambient.rgb);

    // 点光源（焚き火・かがり火）。saturate の後に足す
    [unroll]
    for (int pli = 0; pli < 4; ++pli)
    {
        if (PLColor[pli].w <= 0.001f) continue;
        float3 toP = PLPosRadius[pli].xyz - input.WorldPos;
        float pd = length(toP);
        float att = saturate(1.0f - pd / PLPosRadius[pli].w);
        att *= att;
        float ndlP = saturate(dot(N, toP / max(pd, 0.001f))) * 0.75f + 0.25f;
        lighting += PLColor[pli].rgb * PLColor[pli].w * att * ndlP;
    }

    color.rgb *= lighting;

    // 暗所の脱色＋青寄せ（標準シェーダーと同じ）
    float lightLum = dot(LightAmbient.rgb + LightDiffuse.rgb, float3(0.299f, 0.587f, 0.114f));
    float nightAmt = 1.0f - smoothstep(0.30f, 0.90f, lightLum);
    float colLum = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
    color.rgb = lerp(color.rgb, colLum * float3(0.80f, 0.92f, 1.25f), nightAmt * 0.55f);

    // 距離フォグ + 高度フォグ（標準シェーダーと同じ）
    float dist = distance(CameraPosition.xyz, input.WorldPos);
    float distFog = saturate((dist - FogStart) / max(FogEnd - FogStart, 0.001f));
    float heightFog = exp(-max(0.0f, input.WorldPos.y + 1.0f) * 0.20f);
    float totalFog = saturate(distFog * 0.65f + distFog * heightFog * 0.55f);
    color.rgb = lerp(color.rgb, FogColor.rgb, totalFog);

    color.rgb += Material.Emission.rgb;
    color.a = 1.0f;
    return color;
}
