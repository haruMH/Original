// =================================================================
// instanced_vs.hlsl
// インスタンス描画用の頂点シェーダー
// =================================================================

// 定数バッファ
// ※ インスタンスごとのワールド行列は入力スロット1(WORLD)から直接受け取るため、
//    従来の WorldBuffer (b0) は使用しません。
cbuffer ViewBuffer       : register(b1) { matrix View; }
cbuffer ProjectionBuffer : register(b2) { matrix Projection; }
cbuffer ShadowVPBuffer   : register(b5) { matrix LightVP; }

// 頂点シェーダーへの入力構造体
struct VS_INPUT
{
    // スロット0：頂点データ (D3D11_INPUT_PER_VERTEX_DATA)
    float4 Position : POSITION;  // 位置
    float3 Normal   : NORMAL;    // 法線
    float4 Diffuse  : COLOR;     // ディフューズ色
    float2 TexCoord : TEXCOORD;   // テクスチャ座標
    float3 Tangent  : TANGENT;    // 接線ベクトル

    // スロット1：インスタンスデータ (D3D11_INPUT_PER_INSTANCE_DATA)
    // C++側で D3D11_INPUT_PER_INSTANCE_DATA を指定してバインドしたワールド行列
    row_major float4x4 World : WORLD;
    float TextureIndex : TEXINDEX; // テクスチャ配列インデックス
};

// 頂点シェーダーからの出力構造体（既存のピクセルシェーダーへの入力と一致させる）
struct VS_OUTPUT
{
    float4 Position      : SV_POSITION;
    float3 Normal        : NORMAL;
    float4 Diffuse       : COLOR;
    float2 TexCoord      : TEXCOORD0;
    float3 WorldPos      : TEXCOORD1;
    float4 LightSpacePos : TEXCOORD2;
    float  TextureIndex  : TEXCOORD3; // ピクセルシェーダーへの受け渡し用
};

// メイン関数
VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    // ワールド・ビュー・プロジェクション変換行列を合成
    // C++側から送られる World 行列を行優先 (row_major) として解釈し、変換を計算します。
    matrix wvp = mul(mul(input.World, View), Projection);
    output.Position = mul(input.Position, wvp);

    // 法線情報をワールド空間へ変換
    output.Normal = normalize(mul(input.Normal, (float3x3)input.World));
    
    // 頂点カラー、テクスチャ座標をそのまま出力
    output.Diffuse = input.Diffuse;
    output.TexCoord = input.TexCoord;

    // ワールド空間での頂点座標を計算
    output.WorldPos = mul(input.Position, input.World).xyz;

    // シャドウマップ用のライト空間座標を計算
    output.LightSpacePos = mul(float4(output.WorldPos, 1.0f), LightVP);

    // テクスチャ配列インデックスを出力
    output.TextureIndex = input.TextureIndex;

    return output;
}
