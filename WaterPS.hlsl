// WaterPS.hlsl
Texture2D    NormalMap1 : register(t0);
Texture2D    NormalMap2 : register(t1);
SamplerState Sampler    : register(s0);

cbuffer WaterLightBuffer : register(b2)
{
    float3 LightDirection;
    float  Shininess;
    float3 CameraPosition;
    float  FresnelPower;
    float4 WaterColorShallow;
    float4 WaterColorDeep;
    float2 ScrollSpeed1; // UV Scroll speed 1
    float2 ScrollSpeed2; // UV Scroll speed 2
    float  TimeVal;
    float3 Dummy;
};

struct PS_INPUT
{
    float4 Position     : SV_POSITION;
    float3 Normal       : NORMAL;
    float4 Diffuse      : COLOR;
    float2 TexCoord     : TEXCOORD0;
    float3 WorldPos     : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    // Scroll coordinates in opposite directions
    float2 uv1 = input.TexCoord + ScrollSpeed1 * TimeVal;
    float2 uv2 = input.TexCoord + ScrollSpeed2 * TimeVal;

    float3 normalT1 = NormalMap1.Sample(Sampler, uv1).rgb * 2.0f - 1.0f;
    float3 normalT2 = NormalMap2.Sample(Sampler, uv2).rgb * 2.0f - 1.0f;
    
    float3 blendedNormal = normalize(normalT1 + normalT2);
    
    float3 N = normalize(input.Normal + blendedNormal * 0.2f);
    float3 V = normalize(CameraPosition - input.WorldPos);
    float3 L = normalize(LightDirection);
    
    // Fresnel blending
    float dotNV = saturate(dot(N, V));
    float fresnel = pow(1.0f - dotNV, FresnelPower);
    float4 waterColor = lerp(WaterColorShallow, WaterColorDeep, fresnel);
    
    // Specular (reflection highlight)
    float3 H = normalize(L + V);
    float specular = pow(max(dot(N, H), 0.0f), Shininess);
    float4 specColor = float4(1.0f, 1.0f, 1.0f, 1.0f) * specular * 0.8f;
    
    float4 finalColor = waterColor + specColor;
    finalColor.a = lerp(0.6f, 0.95f, fresnel);
    
    return finalColor;
}
