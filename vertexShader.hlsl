struct VS_IN
{
    float3 pos      : POSITION;
    float3 normal   : NORMAL;
    float4 col      : COLOR;
    float2 tex      : TEXCOORD0;
};

struct VS_OUT
{
    float4 pos      : SV_POSITION;
    float4 col      : COLOR;
    float2 tex      : TEXCOORD0;
    float3 normal   : NORMAL;
    float3 worldPos : POSITION0;
};

cbuffer WorldBuffer : register(b0)
{
    matrix World;
}
cbuffer ViewBuffer : register(b1)
{
    matrix View;
}
cbuffer ProjectionBuffer : register(b2)
{
    matrix Projection;
}

VS_OUT main(VS_IN input)
{
    VS_OUT output;
    
    matrix wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    output.pos = mul(float4(input.pos, 1.0f), wvp);
    
    output.worldPos = mul(float4(input.pos, 1.0f), World).xyz;

    output.normal = normalize(mul(input.normal, (float3x3)World));
    
    output.col = input.col;
    output.tex = input.tex;
    
    return output;
}
