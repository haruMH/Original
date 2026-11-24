// 海面の頂点シェーダー（タイトルの砂浜用）。Gerstner 波で頂点を実際に動かす（波頭がとがり、うねりが立体的になる）。
// 岸に近いほど波を小さくして砂に刺さらないようにする。SHORE_Z は beach.cpp / sea_ps.hlsl と揃える
// 出力の TexCoord.x = 波頭の度合い(-1〜1)、TexCoord.y = 沖ほど1(岸際の抑制係数)
static const float SHORE_Z = 12.0f;

cbuffer WorldBuffer      : register(b0) { matrix World; }
cbuffer ViewBuffer       : register(b1) { matrix View; }
cbuffer ProjectionBuffer : register(b2) { matrix Projection; }
cbuffer ShadowVPBuffer   : register(b5) { matrix LightVP; }
cbuffer SeaBuffer        : register(b6) { float4 SeaParam; }   // x: 経過秒

struct VS_INPUT
{
    float4 Position : POSITION;
    float3 Normal   : NORMAL;
    float4 Diffuse  : COLOR;
    float2 TexCoord : TEXCOORD;
    float3 Tangent  : TANGENT;
};

struct VS_OUTPUT
{
    float4 Position      : SV_POSITION;
    float3 Normal        : NORMAL;
    float4 Diffuse       : COLOR;
    float2 TexCoord      : TEXCOORD0;
    float3 WorldPos      : TEXCOORD1;
    float4 LightSpacePos : TEXCOORD2;
};

float ShoreZ(float x)
{
    return SHORE_Z + 1.6f * sin(x * 0.13f) + 0.8f * sin(x * 0.31f + 1.3f);
}

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT o;
    float3 wp = mul(input.Position, World).xyz;
    const float t = SeaParam.x;

    // 3つの大きなうねり(向き・波数・振幅)。振幅は 0.22/0.12/0.06 m
    const float2 D[3] = { normalize(float2(0.30f, 1.0f)), normalize(float2(-0.50f, 1.0f)), normalize(float2(0.90f, 0.40f)) };
    const float  K[3] = { 0.40f, 0.70f, 1.10f };
    const float  A[3] = { 0.22f, 0.12f, 0.06f };
    const float  W[3] = { 0.95f, 1.30f, 1.60f };

    float calm = smoothstep(0.0f, 7.0f, wp.z - ShoreZ(wp.x));   // 岸際は穏やか

    float2 disp = 0.0f;
    float  h = 0.0f;
    float2 grad = 0.0f;
    float  steep = 0.0f;
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        float ph = K[i] * dot(D[i], wp.xz) - W[i] * t;
        float c = cos(ph), s = sin(ph);
        disp += D[i] * (A[i] * c);          // 水平方向にも動かす(波頭がとがる)
        h    += A[i] * s;
        grad += D[i] * (K[i] * A[i] * c);
        steep += K[i] * A[i] * s;
    }
    wp.xz += disp * calm;
    wp.y  += h * calm;

    float3 n = normalize(float3(-grad.x * calm, 1.0f - steep * calm, -grad.y * calm));

    matrix vp = mul(View, Projection);
    o.Position      = mul(float4(wp, 1.0f), vp);
    o.Normal        = n;
    o.Diffuse       = input.Diffuse;
    o.TexCoord      = float2(h / 0.40f * calm, calm);
    o.WorldPos      = wp;
    o.LightSpacePos = mul(float4(wp, 1.0f), LightVP);
    return o;
}
