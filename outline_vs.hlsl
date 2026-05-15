cbuffer WorldBuffer : register(b0) { matrix World; }
cbuffer ViewBuffer : register(b1) { matrix View; }
cbuffer ProjectionBuffer : register(b2) { matrix Projection; }

struct VS_INPUT {
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float4 Diffuse  : COLOR;
    float2 TexCoord : TEXCOORD;
    float3 Tangent  : TANGENT;
};

struct VS_OUTPUT {
    float4 Position : SV_POSITION;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    

    float outlineThickness = 0.03f;
    float3 expandedPos = input.Position + normalize(input.Normal) * outlineThickness;
    
    matrix wvp = mul(mul(World, View), Projection);
    output.Position = mul(float4(expandedPos, 1.0f), wvp);
    
    return output;
}
