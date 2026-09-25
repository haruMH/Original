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

// Poisson Disk 16点サンプリング定数（放射状均等分散による高品質ソフトシャドウ）
static const float2 kPoissonDisk[16] = {
    float2(-0.94201624, -0.39906216),
    float2( 0.94558609, -0.76890725),
    float2(-0.09418410, -0.92938870),
    float2( 0.34495938,  0.29387760),
    float2(-0.91588581,  0.45771432),
    float2(-0.81544232, -0.87912464),
    float2(-0.38277543,  0.27676845),
    float2( 0.97484398,  0.75648377),
    float2( 0.44323325, -0.97511554),
    float2( 0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023),
    float2( 0.79197514,  0.19090188),
    float2(-0.24188840,  0.99706507),
    float2(-0.81409955,  0.91437590),
    float2( 0.19984126,  0.78641367),
    float2( 0.14383161, -0.14100790)
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 color = input.Diffuse;

    if (Material.TextureEnable)
    {
        float4 tex = g_Texture.Sample(g_Sampler, input.TexCoord);
        if (tex.r + tex.g + tex.b > 0.01f)
            color = tex * input.Diffuse;
    }

    bool LightEnable = (LightEnablePacked.x != 0);

    if (LightEnable)
    {
        // ── 1. Poisson Disk 高品質ソフトシャドウ ──
        float shadow = 1.0f;
        float3 proj = input.LightSpacePos.xyz / input.LightSpacePos.w;
        proj.x =  proj.x * 0.5f + 0.5f;
        proj.y = -proj.y * 0.5f + 0.5f;

        if (proj.x >= 0.0f && proj.x <= 1.0f &&
            proj.y >= 0.0f && proj.y <= 1.0f &&
            proj.z >= 0.0f && proj.z <= 1.0f)
        {
            float bias = 0.0025f;
            float filterRadius = 2.2f / 2048.0f; // 自然な半影の広がり
            float sum = 0.0f;

            [unroll]
            for (int i = 0; i < 16; ++i) {
                sum += g_ShadowMap.SampleCmpLevelZero(g_ShadowSampler,
                           proj.xy + kPoissonDisk[i] * filterRadius, proj.z - bias);
            }
            float rawShadow = sum / 16.0f;
            // ギザギザのない滑らかな半影フォールオフ（暗部 0.28 〜 明部 1.0）
            shadow = lerp(0.28f, 1.0f, smoothstep(0.15f, 0.85f, rawShadow));
        }

        // ── 2. ベクトル定義 ──
        float3 N = normalize(input.Normal);
        float3 L = normalize(-LightDirection.xyz);
        float3 viewDir = normalize(CameraPosition.xyz - input.WorldPos);
        
        // ── 3. トゥーン階調ライティング & 擬似GI（半球アンビエント） ──
        float rawNdotL = dot(N, L);
        float diffuse = max(rawNdotL, 0.0f);
        
        // 滑らかでジャギーのないトゥーン階調化 (smoothstep補間)
        float toonDiff;
        if (diffuse > 0.35f) {
            toonDiff = lerp(0.65f, 1.00f, smoothstep(0.76f, 0.82f, diffuse));
        } else {
            toonDiff = lerp(0.28f, 0.65f, smoothstep(0.33f, 0.38f, diffuse));
        }

        // サブサーフェイス・スキャッタリング（SSS）風の明暗境界の血色感（ターミネーターラインの温かみ）
        float sssMask = smoothstep(0.26f, 0.35f, diffuse) * (1.0f - smoothstep(0.35f, 0.45f, diffuse));
        float3 sssColor = float3(1.0f, 0.38f, 0.20f) * sssMask * 0.28f;

        // 半球ライティング（天空光＋地面照り返し擬似GI）
        float hemiFactor = N.y * 0.5f + 0.5f; // 0.0(真下) 〜 1.0(真上)
        float3 skyLight    = LightAmbient.rgb * float3(0.95f, 1.05f, 1.30f); // 澄んだ青白い天空光
        float3 groundLight = LightAmbient.rgb * float3(1.20f, 1.05f, 0.85f); // 暖かみのある地面バウンス光
        float3 hemiAmbient = lerp(groundLight, skyLight, hemiFactor);

        // 影面の色彩補正（濁った黒を解消し、青紫がかったリッチなセル影に）
        float3 shadowTint = float3(0.70f, 0.75f, 0.95f);
        float4 diffC = toonDiff * LightDiffuse * Material.Diffuse;
        float4 ambC  = float4(hemiAmbient, 1.0f) * Material.Ambient;
        
        // 影が落ちている部分にもアンビエント照り返しを反映
        float3 shadowFinal = lerp(shadowTint * 0.32f, float3(1.0f, 1.0f, 1.0f), shadow);
        float4 lighting = saturate(diffC * float4(shadowFinal, 1.0f) + ambC + float4(sssColor * shadow, 0.0f));

        // ── 4. PBR調 GGX & フレネルハイライト & 床面光沢 ──
        float NdotV = saturate(dot(N, viewDir));
        float fresnel = pow(1.0f - NdotV, 3.5f) * 0.5f; // エッジでの光沢増加

        // 床面（上向き平面 N.y > 0.7）のフレネル光沢・ツルツル反射
        if (N.y > 0.70f) {
            float floorFresnel = pow(1.0f - NdotV, 3.5f);
            float3 floorGazeLight = skyLight * floorFresnel * 0.40f;
            lighting.rgb += floorGazeLight * shadow;
        }

        float4 specC = float4(0.0f, 0.0f, 0.0f, 0.0f);
        if (Material.Shininess > 0.1f) {
            float3 H = normalize(L + viewDir);
            float NdotH = saturate(dot(N, H));
            float spec = pow(NdotH, Material.Shininess * 1.5f);
            spec = smoothstep(0.08f, 0.70f, spec);
            specC = spec * Material.Specular * (1.0f + fresnel);
        }

        // リムライティング（シネマティックな輪郭バックライト ＆ 属性オーラグロー）
        float4 rimC = float4(0.0f, 0.0f, 0.0f, 0.0f);
        if (Material.RimPower > 0.1f) {
            float rim = 1.0f - max(dot(viewDir, N), 0.0f);
            rim = pow(saturate(rim), Material.RimPower);
            rim = smoothstep(0.35f, 0.80f, rim);
            
            // マテリアル固有の Emission や Specular を帯びた鮮やかな属性リムオーラ
            float3 rimColor = (Material.Emission.r + Material.Emission.g + Material.Emission.b > 0.1f)
                            ? Material.Emission.rgb * 1.4f
                            : Material.Specular.rgb * 0.8f + float3(0.20f, 0.25f, 0.35f);
            rimC = float4(rimColor * rim, 1.0f);
        }

        color = (color * lighting) + (specC * shadow);
        color.rgb += (rimC * shadow).rgb;

        // ── 5. ボリュメトリック調ハイトフォグ & 大気散乱 ──
        // (a) 距離ベース減衰
        float dist = distance(CameraPosition.xyz, input.WorldPos);
        float distFog = saturate((dist - FogStart) / max(FogEnd - FogStart, 0.001f));

        // (b) 高度減衰（地面付近に漂う朝靄、上空の抜け感）
        float heightAboveGround = max(0.0f, input.WorldPos.y - (-1.0f));
        float heightFog = exp(-heightAboveGround * 0.20f);
        float totalFog = saturate(distFog * 0.65f + distFog * heightFog * 0.55f);

        // (c) 太陽方向のインスキャッタリング（大気の前方光散乱）
        float sunScatter = pow(max(dot(viewDir, L), 0.0f), 6.0f);
        float3 finalFogColor = lerp(FogColor.rgb, LightDiffuse.rgb * 1.15f, sunScatter * 0.35f);

        color.rgb = lerp(color.rgb, finalFogColor, totalFog);
    }

    color += Material.Emission;
    color.a = 1.0f;
    return color;
}