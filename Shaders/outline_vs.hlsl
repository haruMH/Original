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
    // 軸ごとに拡大率が違うモデル(梁・柱など)では、押し出しも軸ごとに引き伸ばされて太い黒塊になる。
    // 軸ごとの拡大率を平均にそろえて、ワールドでの太さを均一にする（等倍率のモデルは変化なし）
    float3 sc = float3(length(mul(float3(1, 0, 0), (float3x3)World)),
                       length(mul(float3(0, 1, 0), (float3x3)World)),
                       length(mul(float3(0, 0, 1), (float3x3)World)));
    float3 compensate = ((sc.x + sc.y + sc.z) / 3.0f) / max(sc, 0.0001f);
    float3 expandedPos = input.Position + smoothNormal * compensate * finalThickness;
    output.Position = mul(float4(expandedPos, 1.0f), wvp);

    return output;
}


