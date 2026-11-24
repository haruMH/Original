// =================================================================
// outline_instanced_vs.hlsl
// インスタンス描画用のアウトライン頂点シェーダー
// =================================================================

cbuffer ViewBuffer       : register(b1) { matrix View; }
cbuffer ProjectionBuffer : register(b2) { matrix Projection; }

struct VS_INPUT
{
    // スロット0: 頂点データ (D3D11_INPUT_PER_VERTEX_DATA)
    float4 Position : POSITION;  // 位置
    float3 Normal   : NORMAL;    // 法線
    float4 Diffuse  : COLOR;     // ディフューズ色
    float2 TexCoord : TEXCOORD;   // テクスチャ座標
    float3 Tangent  : TANGENT;    // 接線ベクトル

    // スロット1: インスタンスデータ (D3D11_INPUT_PER_INSTANCE_DATA)
    row_major float4x4 World : WORLD; // ワールド行列
    float TextureIndex : TEXINDEX; // インプットレイアウト互換用のダミー入力
    float3 Emissive : EMISSIVE; // インプットレイアウト互換用のダミー入力
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    // アウトライン無効の物体(TextureIndex=-1)は画面外へ飛ばして描かない
    if (input.TextureIndex < -0.5f)
    {
        output.Position = float4(2.0f, 2.0f, 2.0f, 1.0f);
        return output;
    }

    float outlineThickness = 0.04f;

    // 頂点位置ベクトルを正規化してスムーズ法線を求める
    float3 smoothNormal = normalize(input.Position.xyz);

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

    // ワールド・ビュー・プロジェクション変換行列を合成
    matrix wvp = mul(mul(input.World, View), Projection);
    float4 clipBase = mul(float4(input.Position.xyz, 1.0f), wvp);

    // カメラ距離（clipBase.w）に応じたアウトライン太さの適応補正
    // 遠距離でのモデルの黒潰れ（アウトライン埋没）を防止
    float distScale = clamp(clipBase.w * 0.09f, 0.45f, 1.60f);
    float finalThickness = outlineThickness * distScale;

    // ローカル空間でスムーズ法線方向に押し出す
    // 軸ごとに拡大率が違う物体(梁・柱など)は、押し出しも軸ごとに引き伸ばされて太い黒塊になる。
    // 軸ごとの拡大率を平均にそろえて、ワールドでの太さを均一にする（等倍率の物体は変化なし）
    float3 sc = float3(length(mul(float3(1, 0, 0), (float3x3)input.World)),
                       length(mul(float3(0, 1, 0), (float3x3)input.World)),
                       length(mul(float3(0, 0, 1), (float3x3)input.World)));
    float3 compensate = ((sc.x + sc.y + sc.z) / 3.0f) / max(sc, 0.0001f);
    float3 expandedPos = input.Position.xyz + smoothNormal * compensate * finalThickness;
    output.Position = mul(float4(expandedPos, 1.0f), wvp);

    return output;
}
