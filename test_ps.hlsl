// test_ps.hlsl
// Test pixel shader mapping normals to colors

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
    float3 normalColor = normalize(input.Normal) * 0.5f + 0.5f;
    return float4(normalColor, 1.0f);
}
