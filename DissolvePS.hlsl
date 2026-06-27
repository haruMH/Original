// DissolvePS.hlsl
Texture2D    MainTexture : register(t0);
Texture2D    NoiseTexture: register(t1);
SamplerState Sampler     : register(s0);

cbuffer DissolveBuffer : register(b1)
{
    float  Threshold;    // 0.0 to 1.0
    float  EdgeWidth;    // 0.0 to 0.1
    float2 Dummy;
    float4 EdgeColor;    // Neon glow color
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 Normal   : NORMAL;
    float4 Diffuse  : COLOR;
    float2 TexCoord : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float noise = NoiseTexture.Sample(Sampler, input.TexCoord).r;
    
    if (noise < Threshold)
    {
        discard;
    }
    
    float4 baseColor = MainTexture.Sample(Sampler, input.TexCoord) * input.Diffuse;
    
    // Dissolve edge glowing
    if (noise < Threshold + EdgeWidth)
    {
        float edgeLerp = 1.0f - ((noise - Threshold) / EdgeWidth);
        baseColor.rgb += EdgeColor.rgb * edgeLerp;
    }
    
    return baseColor;
}
