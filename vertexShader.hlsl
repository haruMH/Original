cbuffer WorldBuffer      : register(b0) { matrix World; }
cbuffer ViewBuffer       : register(b1) { matrix View; }
cbuffer ProjectionBuffer : register(b2) { matrix Projection; }
cbuffer ShadowVPBuffer   : register(b5) { matrix LightVP; }

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

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    matrix wvp = mul(mul(World, View), Projection);
    output.Position      = mul(input.Position, wvp);
    output.Normal        = normalize(mul(input.Normal, (float3x3)World));
    output.Diffuse       = input.Diffuse;
    output.TexCoord      = input.TexCoord;
    output.WorldPos      = mul(input.Position, World).xyz;
    output.LightSpacePos = mul(float4(output.WorldPos, 1.0f), LightVP);
    return output;
}