cbuffer WorldBuffer      : register(b0) { matrix World; }
cbuffer ViewBuffer       : register(b1) { matrix View; }
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

    float outlineThickness = 0.04f;

    // 頂点位置ベクトルを正規化してスムーズ法線を求める
    float3 smoothNormal = normalize(input.Position);

    // ローカル空間でスムーズ法線方向に押し出す
    float3 expandedPos = input.Position + smoothNormal * outlineThickness;

    // WVP 行列を合成してクリップ座標に変換
    matrix wvp = mul(mul(World, View), Projection);
    output.Position = mul(float4(expandedPos, 1.0f), wvp);

    return output;
}


