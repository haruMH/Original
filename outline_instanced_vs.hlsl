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
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    float outlineThickness = 0.04f;

    // 頂点位置ベクトルを正規化してスムーズ法線を求める
    float3 smoothNormal = normalize(input.Position.xyz);

    // ローカル空間でスムーズ法線方向に押し出す
    float3 expandedPos = input.Position.xyz + smoothNormal * outlineThickness;

    // ワールド・ビュー・プロジェクション変換行列を合成
    matrix wvp = mul(mul(input.World, View), Projection);
    output.Position = mul(float4(expandedPos, 1.0f), wvp);

    return output;
}
