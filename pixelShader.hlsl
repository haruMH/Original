struct PS_IN
{
    float4 pos      : SV_POSITION;
    float4 col      : COLOR;
    float2 tex      : TEXCOORD0;
    float3 normal   : NORMAL;
    float3 worldPos : POSITION0;
};

cbuffer MaterialBuffer : register(b3)
{
    float4 MaterialAmbient;
    float4 MaterialDiffuse;
    float4 MaterialSpecular;
    float4 MaterialEmission;
    float  MaterialShininess;
    int    TextureEnable;
    float2 Dummy;
}

cbuffer LightBuffer : register(b4)
{
    int    LightEnable;
    int3   LightDummy;
    float4 LightDirection;
    float4 LightDiffuse;
    float4 LightAmbient;
    float4 CameraPosition; // ?????????????
}

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

float4 main(PS_IN input) : SV_TARGET
{
    float4 texColor = Texture.Sample(Sampler, input.tex);
    float4 finalColor = input.col * texColor;

    if (LightEnable)
    {
        float3 N = normalize(input.normal);
        float3 L = normalize(-LightDirection.xyz);
        float3 V = normalize(CameraPosition.xyz - input.worldPos);

        // ?????????????????????
        float NdotL = dot(N, L);
        float halfLambert = NdotL * 0.5f + 0.5f;
        float diffuseIntensity = halfLambert * halfLambert;

        // ???????????????????? Blinn-Phong
        float3 H = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0f);
        float specularIntensity = pow(NdotH, max(MaterialShininess, 1.0f)) * 2.0f; // ?????2???????

        float4 ambient = LightAmbient * MaterialAmbient;
        float4 diffuse = LightDiffuse * MaterialDiffuse * diffuseIntensity;
        float4 specular = LightDiffuse * MaterialSpecular * specularIntensity;

        finalColor = (finalColor * (diffuse + ambient)) + specular;
        
        finalColor.a = input.col.a * texColor.a * MaterialDiffuse.a;
    }

    return finalColor;
}
