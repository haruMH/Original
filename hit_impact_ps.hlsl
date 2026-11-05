// hit_impact_ps.hlsl
// 命中エフェクト専用ピクセルシェーダー（パワー/段階に応じて見た目が変わる）
//   mode 1: ヒットフラッシュ（白熱した光球）
//   mode 2: 衝撃リング（ノイズで途切れながら広がる）
//   mode 3: 火花（柔らかい点 + 白熱コア）
// 通常アルファ合成。パラメータはマテリアルの空きフィールドに載せて渡す。

struct MATERIAL_HIT {
    float4 Ambient;
    float4 Diffuse;      // .a = 全体の濃さ
    float4 Specular;     // .x = 段階(0〜4) / .z = モード(1=フラッシュ 2=リング 3=火花)
    float4 Emission;     // .rgb = 発光色
    float  Shininess;    // 乱数シード(ノイズの位相)
    bool   TextureEnable;
    float  RimPower;     // パワー(0〜1)
    float  Dummy;        // 進行度(0〜1)
};
cbuffer MaterialBuffer : register(b3) { MATERIAL_HIT Material; }

struct PS_INPUT {
    float4 Position      : SV_POSITION;
    float3 Normal        : NORMAL;
    float4 Diffuse       : COLOR;
    float2 TexCoord      : TEXCOORD0;
    float3 WorldPos      : TEXCOORD1;
    float4 LightSpacePos : TEXCOORD2;
};

float Band(float x, float w)
{
    float k = x / w;
    return exp(-k * k);
}

float Hash(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float VNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    return lerp(lerp(Hash(i), Hash(i + float2(1, 0)), f.x),
                lerp(Hash(i + float2(0, 1)), Hash(i + float2(1, 1)), f.x), f.y);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float2 p    = input.TexCoord * 2.0f - 1.0f;
    float  r    = length(p);
    float2 dir  = p / max(r, 1e-4f);
    float  mode = Material.Specular.z;
    float  tier = Material.Specular.x;
    float  pw   = Material.RimPower;
    float  t    = Material.Dummy;
    float  seed = Material.Shininess;

    float3 col = Material.Emission.rgb / max(max(Material.Emission.r, max(Material.Emission.g, Material.Emission.b)), 1e-3f);
    float edge = smoothstep(1.0f, 0.85f, r);
    float3 rgb;
    float  a;

    if (mode < 1.5f) {
        // フラッシュ: 白熱コア + 色付きの広いグロー
        float core = exp(-r * r * 14.0f);
        float glow = pow(saturate(1.0f - r), 2.0f);
        rgb = lerp(col, float3(1.0f, 1.0f, 1.0f), saturate(core * 1.2f));
        a   = saturate(core * 1.6f + glow * 0.9f);
    } else if (mode < 2.5f) {
        // リング: 半径は固定(0.82)で、クアッドの拡大で広がる。進むほど細く、途切れる
        float w = lerp(0.12f, 0.035f, t) * (1.0f + 0.4f * pw);
        float n = VNoise(dir * 3.0f + seed * 7.0f + t * 2.0f);
        float breakup = lerp(1.0f, 0.35f + 1.1f * n, t);
        float ring = Band(r - 0.82f, w) * breakup;
        float inner = saturate(1.0f - r / 0.82f) * 0.18f * (1.0f - t);
        rgb = lerp(col, float3(1.0f, 1.0f, 1.0f), 0.35f * ring * (1.0f - t));
        a   = saturate(ring * 1.3f + inner);
    } else {
        // 火花
        float hot = exp(-r * r * 22.0f);
        float soft = exp(-r * r * 6.0f);
        rgb = lerp(col, float3(1.0f, 1.0f, 1.0f), hot);
        a   = saturate(soft * 0.8f + hot);
    }

    return float4(rgb, a * edge * Material.Diffuse.a);
}
