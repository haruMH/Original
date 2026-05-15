cbuffer PostEffectBuffer : register(b0) {
    int   Mode;
    float Threshold;
    float BlurIntensity;
    float Dummy;
}

Texture2D    g_Texture1 : register(t0);
Texture2D    g_Texture2 : register(t1);
SamplerState g_Sampler  : register(s0);

float4 main(float4 Position : SV_POSITION, float2 TexCoord : TEXCOORD) : SV_TARGET
{
    float4 color = float4(0, 0, 0, 1);

    if (Mode == 0)
    {
        color = g_Texture1.Sample(g_Sampler, TexCoord);
        float brightness = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
        if (brightness < Threshold) color = float4(0, 0, 0, 1);
    }
    else if (Mode == 1 || Mode == 2)
    {
        float2 offset = (Mode == 1) ? float2(1.0 / 320.0, 0) : float2(0, 1.0 / 180.0);
        float weight[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };
        
        color = g_Texture1.Sample(g_Sampler, TexCoord) * weight[0];
        for (int i = 1; i < 5; i++) {
            color += g_Texture1.Sample(g_Sampler, TexCoord + offset * i) * weight[i];
            color += g_Texture1.Sample(g_Sampler, TexCoord - offset * i) * weight[i];
        }
    }
    else if (Mode == 3)
    {
        float4 baseColor = g_Texture1.Sample(g_Sampler, TexCoord);
        float4 bloomColor = g_Texture2.Sample(g_Sampler, TexCoord);
        color = baseColor + bloomColor * BlurIntensity;
    }

    return color;
}
