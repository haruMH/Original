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

    // 地面への潜り込みによる下辺アウトラインの遮蔽・欠けを防止するため、
    // 底面頂点（Y < 0）は下方向(-Y)へは押し出さず、水平方向(XZ)にのみ外側へ押し出す
    if (input.Position.y < -0.1f)
    {
        smoothNormal.y = 0.0f;
        float xzLen = length(smoothNormal.xz);
        if (xzLen > 0.0001f)
        {
            smoothNormal.xz /= xzLen;
        }
    }

    // WVP 行列を合成
    matrix wvp = mul(mul(World, View), Projection);
    float4 clipBase = mul(float4(input.Position, 1.0f), wvp);

    // カメラ距離（clipBase.w）に応じたアウトライン太さの適応補正
    // 至近距離での肥大化や、遠距離でのモデルの黒潰れ（アウトライン埋没）を防止
    float distScale = clamp(clipBase.w * 0.09f, 0.45f, 1.60f);
    float finalThickness = outlineThickness * distScale;

    // ローカル空間でスムーズ法線方向に押し出す
    float3 expandedPos = input.Position + smoothNormal * finalThickness;
    output.Position = mul(float4(expandedPos, 1.0f), wvp);

    return output;
}


