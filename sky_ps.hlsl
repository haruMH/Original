// =================================================================
// sky_ps.hlsl
// 手続き生成の空（テクスチャ不使用 → 継ぎ目なし）
// グラデーション・太陽・月・星・雲を方向ベクトルから計算する
// =================================================================
cbuffer SkyBuffer : register(b6)
{
    float4 ZenithColor;   // rgb: 天頂色, a: 星の濃さ
    float4 HorizonColor;  // rgb: 地平線色, a: 雲の明るさ
    float4 SunDirTime;    // xyz: 太陽方向, w: 経過秒
    float4 SunColor;      // rgb: 太陽/朝夕の光
    float4 CamPos;        // カメラ位置
}

struct PS_IN
{
    float4 Position      : SV_POSITION;
    float3 Normal        : NORMAL;
    float4 Diffuse       : COLOR;
    float2 TexCoord      : TEXCOORD0;
    float3 WorldPos      : TEXCOORD1;
    float4 LightSpacePos : TEXCOORD2;
};

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float Hash31(float3 p)
{
    p = frac(p * float3(0.1031f, 0.1030f, 0.0973f));
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

float ValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float a = Hash21(i);
    float b = Hash21(i + float2(1, 0));
    float c = Hash21(i + float2(0, 1));
    float d = Hash21(i + float2(1, 1));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float Fbm(float2 p)
{
    float v = 0.0f;
    float a = 0.5f;
    [unroll]
    for (int i = 0; i < 5; i++)
    {
        v += a * ValueNoise(p);
        p = p * 2.03f + 17.1f;
        a *= 0.5f;
    }
    return v;
}

float4 main(PS_IN input) : SV_TARGET
{
    float3 d = normalize(input.WorldPos - CamPos.xyz);
    float3 sunDir = normalize(SunDirTime.xyz);
    float  t = SunDirTime.w;
    float  h = d.y;

    float sunElev = sunDir.y;
    float sunVis  = smoothstep(-0.18f, 0.08f, sunElev);     // 太陽が見える/光る度合い
    float moonVis = smoothstep(-0.05f, 0.25f, -sunElev);    // 月が見える度合い

    // ---- 基本グラデーション ----
    float up = saturate(h);
    float3 sky = lerp(HorizonColor.rgb, ZenithColor.rgb, pow(up, 0.45f));
    // 地平線より下は暗い地平線色へ
    float below = saturate(-h / 0.30f);
    sky = lerp(sky, HorizonColor.rgb * 0.55f, below * below * (3.0f - 2.0f * below));

    // ---- 太陽方向の空の染まり ----
    float sd = saturate(dot(d, sunDir));
    float horizonBand = exp(-abs(h) * 5.0f);
    sky += SunColor.rgb * pow(sd, 5.0f) * (0.25f + 0.55f * horizonBand) * sunVis;

    // ---- 雲 ----
    float cloudDensity = 0.0f;
    float3 cloudCol = float3(0, 0, 0);
    if (h > 0.0f)
    {
        float2 uv = d.xz / (h + 0.14f) * 0.85f + float2(t * 0.012f, t * 0.005f);
        float n  = Fbm(uv);
        float n2 = Fbm(uv + sunDir.xz * 0.10f);      // 太陽側へずらした濃さ（陰影用）
        float cover = smoothstep(0.50f, 0.78f, n);
        float fade  = smoothstep(0.0f, 0.20f, h);
        cloudDensity = cover * fade;

        float shade = saturate((n - n2) * 3.5f + 0.55f);   // 太陽側が明るい
        float3 ambient = lerp(HorizonColor.rgb, ZenithColor.rgb, 0.35f) * 0.85f + 0.10f;
        float3 lit = ambient + SunColor.rgb * 0.60f * shade * sunVis;
        // 縁(薄い部分)ほど太陽の光が透けて明るい
        lit += SunColor.rgb * pow(sd, 8.0f) * (1.0f - cover) * 0.8f * sunVis;
        cloudCol = lit * HorizonColor.a;
        // 夜は月明かりで青白く
        cloudCol += float3(0.10f, 0.13f, 0.22f) * moonVis * shade;
    }

    // ---- 太陽・月（雲に隠れる） ----
    float3 celestial = float3(0, 0, 0);
    float sunDisk = smoothstep(0.99935f, 0.99975f, sd);
    celestial += SunColor.rgb * (sunDisk * 5.0f + pow(sd, 90.0f) * 0.9f) * sunVis;
    float md = saturate(dot(d, -sunDir));
    float moonDisk = smoothstep(0.9991f, 0.9994f, md);
    celestial += float3(0.85f, 0.92f, 1.10f) * (moonDisk * 2.2f + pow(md, 220.0f) * 0.35f) * moonVis;
    celestial *= saturate(1.0f - cloudDensity * 1.1f) * saturate(h * 6.0f + 0.4f);
    sky += celestial;

    // ---- 星 ----
    float starAmt = ZenithColor.a;
    if (starAmt > 0.01f && h > 0.0f)
    {
        float3 p = d * 170.0f;
        float3 cell = floor(p);
        float3 f = frac(p) - 0.5f;
        float r = Hash31(cell);
        float star = step(0.990f, r);
        float dist = length(f);
        float tw = 0.65f + 0.35f * sin(t * 3.0f + r * 60.0f);
        float s = star * smoothstep(0.30f, 0.0f, dist) * tw * (0.5f + 0.5f * frac(r * 91.7f));
        s *= saturate(h * 3.0f + 0.1f) * starAmt * (1.0f - saturate(cloudDensity * 1.5f));
        s *= (1.0f - moonVis * 0.35f * pow(md, 4.0f));   // 月の近くは少し減らす
        // 星ごとの色味（青白〜白〜橙）
        float3 sc = lerp(float3(0.75f, 0.85f, 1.25f), float3(1.25f, 0.95f, 0.75f), frac(r * 37.3f));
        sky += sc * s * 1.6f;

        // ---- 一等星: 数が少なく、強く大きく光る星（光芒とにじみ付き） ----
        float3 pb = d * 48.0f;
        float3 cellB = floor(pb);
        float rb = Hash31(cellB + 11.7f);
        if (rb > 0.965f)
        {
            // セル内で位置をずらして、格子状に並んで見えないようにする
            float3 jit = float3(Hash31(cellB + 3.1f), Hash31(cellB + 5.3f), Hash31(cellB + 9.7f)) - 0.5f;
            float3 fb = frac(pb) - 0.5f - jit * 0.5f;
            float db = length(fb);
            float mag = (rb - 0.965f) / 0.035f;                       // 0〜1: 大きいほど明るい
            float twB = 0.75f + 0.25f * sin(t * 2.2f + rb * 80.0f);
            float core = smoothstep(0.12f + 0.06f * mag, 0.0f, db);   // 芯
            float halo = exp(-db * (9.0f - 4.0f * mag)) * 0.35f;      // にじみ
            // 十字の光芒（明るい星だけ）
            float3 ax = abs(fb);
            float spike = (exp(-ax.x * 38.0f) * exp(-ax.y * 5.0f) + exp(-ax.y * 38.0f) * exp(-ax.x * 5.0f)) * 0.25f * mag;
            float3 colB = lerp(float3(0.70f, 0.82f, 1.30f), float3(1.30f, 0.90f, 0.65f), frac(rb * 53.1f));
            float vis = saturate(h * 3.0f + 0.1f) * starAmt * (1.0f - saturate(cloudDensity * 1.5f));
            vis *= (1.0f - moonVis * 0.5f * pow(md, 3.0f));
            sky += colB * (core * 3.5f + halo + spike) * (0.6f + 0.8f * mag) * twB * vis;
        }
    }

    sky = lerp(sky, cloudCol, cloudDensity * 0.92f);

    // バンディング防止のディザリング
    sky += (Hash21(input.Position.xy) - 0.5f) / 255.0f;

    return float4(max(sky, 0.0f), 1.0f);
}
