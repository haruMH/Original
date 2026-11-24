// 床用ピクセルシェーダー（石畳・道）。medieval_ps（pixelShader + 法線マップ）に視差オクルージョンマッピング(POM)を足したもの。
// 法線マップのアルファ(a)を高さ(1=石の上面, 0=継ぎ目)として使う。石の継ぎ目が深く見え、斜めから見ても凹凸が立体的になる。
// 調整用: POM_SCALE=凹みの深さ(UV単位。3mで1周なので0.02≒6cm)、POM_DIST=効かせる距離(遠くは省略して軽く)
static const float POM_SCALE = 0.022f;
static const float POM_DIST  = 22.0f;
// OBJに接線が無いので、画面上の微分(ddx/ddy)から接線空間を作る。
// 調整用: NORMAL_STRENGTH=凹凸の強さ(0で無効)、NORMAL_FLIP_Y=1 で緑チャンネルを反転(OpenGL形式の法線マップ用)
static const float NORMAL_STRENGTH = 1.0f;
static const float NORMAL_FLIP_Y   = 1.0f;

Texture2D    g_Texture   : register(t0);
Texture2D    g_NormalMap : register(t1);
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

cbuffer PointLightBuffer : register(b7) {
    float4 PLPosRadius[4];   // xyz: 位置, w: 届く半径
    float4 PLColor[4];       // rgb: 色, w: 強さ(0で無効)
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
// 先頭4点は上下左右に散らした「下見用」。半影判定の早期打ち切りに使う
static const float2 kPoissonDisk[16] = {
    float2(-0.94201624, -0.39906216),
    float2( 0.97484398,  0.75648377),
    float2(-0.09418410, -0.92938870),
    float2(-0.24188840,  0.99706507),
    float2( 0.94558609, -0.76890725),
    float2( 0.34495938,  0.29387760),
    float2(-0.91588581,  0.45771432),
    float2(-0.81544232, -0.87912464),
    float2(-0.38277543,  0.27676845),
    float2( 0.44323325, -0.97511554),
    float2( 0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023),
    float2( 0.79197514,  0.19090188),
    float2(-0.81409955,  0.91437590),
    float2( 0.19984126,  0.78641367),
    float2( 0.14383161, -0.14100790)
};

// 面の法線 N に、接線空間の法線 tn を重ねてワールド空間の法線にする
float3 PerturbNormal(float3 N, float3 worldPos, float2 uv, float3 tn)
{
    float3 dp1  = ddx(worldPos);
    float3 dp2  = ddy(worldPos);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invMax = rsqrt(max(max(dot(T, T), dot(B, B)), 1e-12f));
    return normalize(T * (tn.x * invMax) + B * (tn.y * invMax) + N * tn.z);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 color = input.Diffuse;

    // ── 視差オクルージョンマッピング: 視線を高さマップの中へ進めて、実際に見える点のUVを求める ──
    float2 uv = input.TexCoord;
    {
        uint pw, ph;
        g_NormalMap.GetDimensions(pw, ph);
        float dP = distance(CameraPosition.xyz, input.WorldPos);
        if (pw > 0 && dP < POM_DIST)
        {
            float3 Nn  = normalize(input.Normal);
            float3 dp1 = ddx(input.WorldPos), dp2 = ddy(input.WorldPos);
            float2 duvX = ddx(input.TexCoord), duvY = ddy(input.TexCoord);
            float3 dp2perp = cross(dp2, Nn), dp1perp = cross(Nn, dp1);
            float3 Tt = normalize(dp2perp * duvX.x + dp1perp * duvY.x);
            float3 Bt = normalize(dp2perp * duvX.y + dp1perp * duvY.y);
            float3 Vw = normalize(CameraPosition.xyz - input.WorldPos);
            float3 Vt = float3(dot(Vw, Tt), dot(Vw, Bt), max(dot(Vw, Nn), 0.15f));
            float fade = 1.0f - smoothstep(POM_DIST * 0.55f, POM_DIST, dP);
            float2 maxShift = Vt.xy / Vt.z * (POM_SCALE * fade);
            const int STEPS = 14;
            float2 stepUV = maxShift / (float)STEPS;
            float  layer = 1.0f / (float)STEPS;
            float2 cuv = input.TexCoord;
            float  curLayer = 0.0f;
            float  curDepth = 1.0f - g_NormalMap.SampleGrad(g_Sampler, cuv, duvX, duvY).a;
            float2 prevUV = cuv; float prevLayer = 0.0f, prevDepth = curDepth;
            [loop]
            for (int k = 0; k < STEPS && curLayer < curDepth; ++k)
            {
                prevUV = cuv; prevLayer = curLayer; prevDepth = curDepth;
                cuv -= stepUV;
                curLayer += layer;
                curDepth = 1.0f - g_NormalMap.SampleGrad(g_Sampler, cuv, duvX, duvY).a;
            }
            // 交差の前後を線形補間して、段差のない滑らかな位置にする
            float after  = curDepth - curLayer;
            float before = prevDepth - prevLayer;
            float w = saturate(after / (after - before - 1e-5f));
            uv = lerp(cuv, prevUV, w);
        }
    }

    if (Material.TextureEnable)
    {
        float4 tex = g_Texture.Sample(g_Sampler, uv);
        // 葉などの透明部分を切り抜く（Material.Dummy をしきい値として使う。0 なら無効）
        if (Material.Dummy > 0.0f && tex.a < Material.Dummy) discard;
        if (tex.r + tex.g + tex.b > 0.01f)
            color = tex * input.Diffuse;
    }

    bool LightEnable = (LightEnablePacked.x != 0);
    float dist = distance(CameraPosition.xyz, input.WorldPos);

    // 床面のタイル反復を目立たなくする（ワールド座標の低周波ノイズで明るさ・色味をばらつかせる）
    // フォグで完全に隠れる距離(FogEnd)以降は計算しない。FogEnd の半分から徐々に弱める
    if (LightEnable && input.Normal.y > 0.9f && dist < FogEnd)
    {
        float2 wp = input.WorldPos.xz;
        float2 i0 = floor(wp * 0.11f);
        float2 f0 = frac(wp * 0.11f);
        f0 = f0 * f0 * (3.0f - 2.0f * f0);
        float n00 = frac(sin(dot(i0,                  float2(127.1f, 311.7f))) * 43758.5453f);
        float n10 = frac(sin(dot(i0 + float2(1, 0),   float2(127.1f, 311.7f))) * 43758.5453f);
        float n01 = frac(sin(dot(i0 + float2(0, 1),   float2(127.1f, 311.7f))) * 43758.5453f);
        float n11 = frac(sin(dot(i0 + float2(1, 1),   float2(127.1f, 311.7f))) * 43758.5453f);
        float n = lerp(lerp(n00, n10, f0.x), lerp(n01, n11, f0.x), f0.y);
        float nFade = 1.0f - smoothstep(FogEnd * 0.5f, FogEnd, dist);
        color.rgb *= lerp(float3(1.0f, 1.0f, 1.0f),
                          lerp(float3(0.84f, 0.88f, 0.84f), float3(1.10f, 1.08f, 1.02f), n), nFade);
    }

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

            // まず4点だけ見て、全部影/全部日向なら半影ではないので残り12点を省略（大半の画素はここで終わる）
            [unroll]
            for (int i = 0; i < 4; ++i) {
                sum += g_ShadowMap.SampleCmpLevelZero(g_ShadowSampler,
                           proj.xy + kPoissonDisk[i] * filterRadius, proj.z - bias);
            }
            float rawShadow = sum * 0.25f;
            if (sum > 0.001f && sum < 3.999f)
            {
                [unroll]
                for (int j = 4; j < 16; ++j) {
                    sum += g_ShadowMap.SampleCmpLevelZero(g_ShadowSampler,
                               proj.xy + kPoissonDisk[j] * filterRadius, proj.z - bias);
                }
                rawShadow = sum / 16.0f;
            }
            // ギザギザのない滑らかな半影フォールオフ（暗部 0.28 〜 明部 1.0）
            shadow = lerp(0.28f, 1.0f, smoothstep(0.15f, 0.85f, rawShadow));
        }

        // ── 2. ベクトル定義 ──
        float3 N = normalize(input.Normal);
        {
            uint nw, nh;
            g_NormalMap.GetDimensions(nw, nh);
            if (nw > 0 && NORMAL_STRENGTH > 0.001f)
            {
                float3 tn = g_NormalMap.Sample(g_Sampler, uv).xyz * 2.0f - 1.0f;
                tn.y *= (NORMAL_FLIP_Y > 0.5f) ? -1.0f : 1.0f;
                tn.xy *= NORMAL_STRENGTH;
                tn = normalize(tn);
                // 面の法線とのブレンド。トゥーン階調が荒れないよう、少しだけ面の法線を残す
                N = normalize(lerp(N, PerturbNormal(N, input.WorldPos, input.TexCoord, tn), 0.85f));
            }
        }
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
        // 天空光の色味を空(フォグ)の色相へ少し寄せ、空色が物体にかかるようにする
        float3 fogHue = FogColor.rgb / max(max(FogColor.r, max(FogColor.g, FogColor.b)), 0.001f);
        float3 skyLight    = LightAmbient.rgb * lerp(float3(0.95f, 1.05f, 1.30f), 0.55f + 0.9f * fogHue, 0.30f);
        float3 groundLight = LightAmbient.rgb * float3(1.20f, 1.05f, 0.85f); // 暖かみのある地面バウンス光
        float3 hemiAmbient = lerp(groundLight, skyLight, hemiFactor);

        // 影面の色彩補正（濁った黒を解消し、青紫がかったリッチなセル影に）
        float3 shadowTint = float3(0.70f, 0.75f, 0.95f);
        float4 diffC = toonDiff * LightDiffuse * Material.Diffuse;
        float4 ambC  = float4(hemiAmbient, 1.0f) * Material.Ambient;
        
        // 影が落ちている部分にもアンビエント照り返しを反映
        float3 shadowFinal = lerp(shadowTint * 0.32f, float3(1.0f, 1.0f, 1.0f), shadow);
        float4 lighting = saturate(diffC * float4(shadowFinal, 1.0f) + ambC + float4(sssColor * shadow, 0.0f));

        // ── 点光源（焚き火など）。saturate の後に足すのでHDRとしてブルームにも乗る ──
        [unroll]
        for (int pli = 0; pli < 4; ++pli)
        {
            if (PLColor[pli].w <= 0.001f) continue;
            float3 toP = PLPosRadius[pli].xyz - input.WorldPos;
            float pd2 = dot(toP, toP);
            if (pd2 >= PLPosRadius[pli].w * PLPosRadius[pli].w) continue; // 届かない画素は sqrt 以降を省略
            float pd = sqrt(pd2);
            float att = saturate(1.0f - pd / PLPosRadius[pli].w);
            att *= att;
            float ndlP = saturate(dot(N, toP / max(pd, 0.001f))) * 0.75f + 0.25f;
            lighting.rgb += PLColor[pli].rgb * PLColor[pli].w * att * ndlP;
        }

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

        // 暗所の脱色＋青寄せ（光量が少ないほど素材の彩度が落ちる）
        float lightLum = dot(LightAmbient.rgb + LightDiffuse.rgb, float3(0.299f, 0.587f, 0.114f));
        float nightAmt = 1.0f - smoothstep(0.30f, 0.90f, lightLum);
        float colLum = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
        color.rgb = lerp(color.rgb, colLum * float3(0.80f, 0.92f, 1.25f), nightAmt * 0.55f);
        color.rgb += (rimC * shadow).rgb;

        // ── 5. ボリュメトリック調ハイトフォグ & 大気散乱 ──
        // (a) 距離ベース減衰
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