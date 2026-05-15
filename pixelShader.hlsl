Texture2D    g_Texture   : register(t0);
Texture2D    g_ShadowMap : register(t2);
SamplerState g_Sampler   : register(s0);
SamplerComparisonState g_ShadowSampler : register(s1);

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
    float4 color = float4(1.0f, 1.0f, 1.0f, 1.0f);

    if (Material.TextureEnable)
    {
        float4 tex = g_Texture.Sample(g_Sampler, input.TexCoord);
        if (tex.r + tex.g + tex.b > 0.01f)
            color = tex;
    }

    bool LightEnable = (LightEnablePacked.x != 0);

    if (LightEnable)
    {
        float shadow = 1.0f;
        float3 proj = input.LightSpacePos.xyz / input.LightSpacePos.w;
        proj.x =  proj.x * 0.5f + 0.5f;
        proj.y = -proj.y * 0.5f + 0.5f;

        if (proj.x >= 0.0f && proj.x <= 1.0f &&
            proj.y >= 0.0f && proj.y <= 1.0f &&
            proj.z >= 0.0f && proj.z <= 1.0f)
        {
            float bias = 0.002f;
            float sum  = 0.0f;
            float2 ts  = 1.0f / 2048.0f;
            for (int x = -1; x <= 1; ++x)
                for (int y = -1; y <= 1; ++y)
                    sum += g_ShadowMap.SampleCmpLevelZero(g_ShadowSampler,
                               proj.xy + float2(x, y) * ts, proj.z - bias);
            shadow = sum / 9.0f;
            if (shadow < 0.5f) shadow = 0.55f;  // 影の最低輝度（明るめ）
            else               shadow = 1.0f;
        }

        float3 N = normalize(input.Normal);
        float3 L = normalize(-LightDirection.xyz);
        float3 viewDir = normalize(CameraPosition.xyz - input.WorldPos);
        
        // トゥーンシェーディング（光の階調化）
        float diffuse = max(dot(N, L), 0.0f);
        if      (diffuse > 0.8f)  diffuse = 1.0f;   // 光が強く当たっている面
        else if (diffuse > 0.3f)  diffuse = 0.7f;   // 少し影になりかけの面
        else                      diffuse = 0.4f;   // 影の面（明るめに）
        
        float4 diffC = diffuse * LightDiffuse * Material.Diffuse;
        float4 ambC  = LightAmbient * Material.Ambient;

        float4 lighting = saturate(diffC * shadow + ambC);

        float4 specC = float4(0.0f, 0.0f, 0.0f, 0.0f);
        if (Material.Shininess > 0.1f) {
            float3 halfDir = normalize(L + viewDir);
            float spec = pow(max(dot(N, halfDir), 0.0f), Material.Shininess);
            spec = smoothstep(0.1f, 0.8f, spec);
            specC = spec * Material.Specular * 0.5f;
        }

        float4 rimC = float4(0.0f, 0.0f, 0.0f, 0.0f);
        if (Material.RimPower > 0.1f) {
            float rim = 1.0f - max(dot(viewDir, N), 0.0f);
            rim = smoothstep(0.6f, 1.0f, rim);
            rim = pow(rim, Material.RimPower);
            rim = smoothstep(0.4f, 0.8f, rim);
            rimC = rim * Material.Specular * 0.5f;
        }

        color = (color * lighting) + (specC * shadow);
        color.rgb += (rimC * shadow).rgb;

        float dist = distance(CameraPosition.xyz, input.WorldPos);
        float fog  = saturate((dist - FogStart) / max(FogEnd - FogStart, 0.001f));
        color.rgb  = lerp(color.rgb, FogColor.rgb, fog);
    }

    color += Material.Emission;
    color.a = 1.0f;
    return color;
}