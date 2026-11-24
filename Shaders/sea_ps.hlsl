// 海面ピクセルシェーダー（タイトルの砂浜用）。波の揺れ・水深の色・波打ち際の泡・太陽の反射・遠景のフォグ馴染み。
// 水面は y=-1 の平面。海底は岸から沖へ SEA_SLOPE の傾きで深くなる(砂浜メッシュ=beach.cpp と同じ式)。
// 調整用: SEA_SLOPE/SHORE_Z は beach.cpp の定数と必ず揃える
static const float SEA_SLOPE = 0.16f;
static const float SHORE_Z   = 12.0f;

Texture2D    g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

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

cbuffer SeaBuffer : register(b6) {
    float4 SeaParam;    // x: 経過秒
    float4 SunDir;      // xyz: 太陽の方向
    float4 SunCol;      // rgb: 太陽色
    float4 HorizonCol;  // rgb: 地平線色
    float4 ZenithCol;   // rgb: 天頂色
}

struct PS_INPUT {
    float4 Position      : SV_POSITION;
    float3 Normal        : NORMAL;
    float4 Diffuse       : COLOR;
    float2 TexCoord      : TEXCOORD0;
    float3 WorldPos      : TEXCOORD1;
    float4 LightSpacePos : TEXCOORD2;
};

// 岸線のZ座標(x によってうねる)。beach.cpp の ShoreZ と同じ式
float ShoreZ(float x)
{
    return SHORE_Z + 1.6f * sin(x * 0.13f) + 0.8f * sin(x * 0.31f + 1.3f);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    const float t = SeaParam.x;
    const float2 p = input.WorldPos.xz;

    // 波打ち際は潮の満ち引きで少し前後する
    const float tide = 0.7f * sin(t * 0.55f);
    float depth = (p.y - (ShoreZ(p.x) + tide)) * SEA_SLOPE;
    if (depth <= 0.0f) discard;

    // 大きなうねりは頂点シェーダー(sea_vs)が作った法線を使い、ここでは細かい波紋だけを重ねる
    float2 d3 = normalize(float2( 0.9f, 0.4f));
    float2 d4 = normalize(float2(-0.2f, 0.8f));
    float2 g = 0.0f;
    g += d3 * cos(dot(p, d3) * 1.70f + t * 2.1f) * 1.70f * 0.018f;
    g += d4 * cos(dot(p, d4) * 3.10f + t * 2.9f) * 3.10f * 0.008f;
    float3 Nb = normalize(input.Normal);
    float3 N = normalize(float3(Nb.x - g.x, Nb.y, Nb.z - g.y));

    float3 V = normalize(CameraPosition.xyz - input.WorldPos);
    float fres = pow(saturate(1.0f - dot(N, V)), 4.0f);
    fres = lerp(0.04f, 1.0f, fres);

    // 昼夜の明るさ(空・光の強さに追従)
    float lum = dot(LightAmbient.rgb + LightDiffuse.rgb, float3(0.299f, 0.587f, 0.114f));
    float daylight = clamp(0.22f + 0.85f * lum, 0.22f, 1.15f);

    // 水深の色: 浅い=ターコイズ、深い=青
    float3 shallow = float3(0.30f, 0.82f, 0.80f);
    float3 deep    = float3(0.02f, 0.24f, 0.46f);
    float3 body = lerp(shallow, deep, saturate(depth / 3.2f)) * daylight;

    // 反射する空の色(視線が低いほど地平線の色)
    float3 R = reflect(-V, N);
    float3 sky = lerp(HorizonCol.rgb, ZenithCol.rgb, saturate(R.y * 1.6f));
    float3 col = lerp(body, sky, fres);

    // 太陽の反射(きらめき)
    float3 L = normalize(SunDir.xyz);
    float spec = pow(saturate(dot(R, L)), 220.0f) * 1.6f + pow(saturate(dot(R, L)), 24.0f) * 0.12f;
    col += SunCol.rgb * spec * step(0.0f, L.y);

    // 波打ち際の泡: 寄せてくる白い筋と、岸際の薄い泡
    float edge = depth / SEA_SLOPE;                           // 岸からの距離(m)
    float sweep = sin(edge * 1.8f - t * 1.6f + sin(p.x * 0.7f) * 1.5f);
    float lines = smoothstep(0.55f, 0.95f, sweep) * (1.0f - smoothstep(1.0f, 5.5f, edge));
    float rim   = 1.0f - smoothstep(0.0f, 0.55f, edge);
    // 波頭の泡: うねりの頂上(sea_vs が TexCoord.x に渡す)に、ちぎれた泡を散らす
    float crest = input.TexCoord.x;
    float calm  = input.TexCoord.y;
    float n1 = 0.5f + 0.5f * sin(p.x * 2.3f + t * 0.9f) * sin(p.y * 1.9f - t * 1.3f);
    float n2 = 0.5f + 0.5f * sin(p.x * 5.7f - t * 1.7f + p.y * 1.1f) * sin(p.y * 4.9f + t * 1.1f);
    float crestFoam = smoothstep(0.55f, 0.95f, crest) * smoothstep(0.25f, 0.75f, n1 * 0.6f + n2 * 0.4f) * calm;
    float foam  = saturate(lines * 0.8f + rim + crestFoam * 0.85f);
    col = lerp(col, float3(0.95f, 0.98f, 1.0f) * daylight, foam);

    // 浅瀬の光の網目(コースティクス): 水底に揺れる明るい模様。浅いところだけ足す
    {
        float2 q = p * 0.55f;
        float2 ii = q;
        float c = 1.0f;
        float tt = t * 0.5f;
        [unroll]
        for (int n = 0; n < 3; ++n)
        {
            float t2 = tt * (1.0f - 3.5f / (n + 1.0f));
            ii = q + float2(cos(t2 - ii.x) + sin(t2 + ii.y), sin(t2 - ii.y) + cos(t2 + ii.x));
            c += 1.0f / length(float2(q.x / (sin(ii.x + t2) / 0.005f), q.y / (cos(ii.y + t2) / 0.005f)));
        }
        c = c / 3.0f;
        c = 1.17f - pow(abs(c), 1.4f);
        float caus = saturate(pow(abs(c), 8.0f));
        float shallowW = (1.0f - smoothstep(0.25f, 2.0f, depth)) * smoothstep(0.02f, 0.25f, depth);
        col += float3(0.85f, 1.0f, 0.95f) * caus * shallowW * 0.55f * daylight;
    }

    // 透明度: 岸際ほど透けて砂が見える。沖ほど不透明
    float alpha = smoothstep(0.0f, 0.12f, depth) * lerp(0.50f, 0.97f, saturate(depth / 2.5f));
    alpha = max(alpha, foam * 0.9f);

    // 遠景は地平線の色へ溶かす(水平線をなじませる)
    float dist = distance(CameraPosition.xyz, input.WorldPos);
    float fog = smoothstep(FogStart * 2.0f, FogEnd * 4.5f, dist);
    col = lerp(col, FogColor.rgb, fog);
    alpha = lerp(alpha, 1.0f, fog);

    return float4(col, alpha);
}
