// WaterVS.hlsl
cbuffer WorldBuffer      : register(b0) { matrix World; }
cbuffer ViewBuffer       : register(b1) { matrix View; }
cbuffer ProjectionBuffer : register(b2) { matrix Projection; }

cbuffer WaterParamBuffer : register(b3)
{
    float Time;
    float3 WaveParams; // x: Amplitude, y: Frequency, z: Speed
};

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
    float4 Position     : SV_POSITION;
    float3 Normal       : NORMAL;
    float4 Diffuse      : COLOR;
    float2 TexCoord     : TEXCOORD0;
    float3 WorldPos     : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    float4 worldPos = mul(input.Position, World);
    
    // Wave animation (Y-axis displacement)
    float wave1 = sin(worldPos.x * WaveParams.y + Time * WaveParams.z) * WaveParams.x;
    float wave2 = cos(worldPos.z * WaveParams.y * 1.5f + Time * WaveParams.z * 1.2f) * (WaveParams.x * 0.6f);
    worldPos.y += wave1 + wave2;

    output.WorldPos = worldPos.xyz;
    output.Position = mul(worldPos, mul(View, Projection));
    
    // Simple normal recalculation
    float3 normal = input.Normal;
    normal.x -= cos(worldPos.x * WaveParams.y + Time * WaveParams.z) * WaveParams.x * WaveParams.y;
    normal.z += sin(worldPos.z * WaveParams.y * 1.5f + Time * WaveParams.z * 1.2f) * WaveParams.x * WaveParams.y * 0.9f;
    output.Normal = normalize(mul(normal, (float3x3)World));
    
    output.TexCoord = input.TexCoord;
    output.Diffuse = input.Diffuse;
    
    return output;
}
