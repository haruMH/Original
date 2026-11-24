// ui_ps.hlsl
// UIポップアップ専用ピクセルシェーダー
// テクスチャのアルファ値をテキスト形状マスクとして使い、
// Emissionカラーで発光テキストを描画する（ブルーム対応）

Texture2D    g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

// マテリアル定数バッファ (b3: pixelShader.hlsl と同じレジスタを共有)
struct MATERIAL_UI {
    float4 Ambient;
    float4 Diffuse;      // .a = フェードアウトアルファ
    float4 Specular;
    float4 Emission;     // .rgb = テキスト発光カラー（ブルームにも影響する）
    float  Shininess;
    bool   TextureEnable;
    float  RimPower;
    float  Dummy;
};
cbuffer MaterialBuffer : register(b3) { MATERIAL_UI Material; }

// 頂点シェーダーからの入力（vertexShader.hlsl の出力と一致させる）
struct PS_INPUT {
    float4 Position      : SV_POSITION;
    float3 Normal        : NORMAL;
    float4 Diffuse       : COLOR;
    float2 TexCoord      : TEXCOORD0;
    float3 WorldPos      : TEXCOORD1;
    float4 LightSpacePos : TEXCOORD2;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_Texture.Sample(g_Sampler, input.TexCoord);

    if (Material.TextureEnable) {
        // テクスチャのカラーをそのまま使用する（マテリアルカラーでフェード）
        return float4(texColor.rgb, texColor.a * Material.Diffuse.a);
    } else {
        // テクスチャのアルファチャンネルをテキスト形状マスクとして取得する
        float mask = texColor.a;

        // マスク × Material.Diffuse.a でフェードアルファを計算する
        float finalAlpha = mask * Material.Diffuse.a;

        // Emissionカラーをテキストカラーとして使用する
        // 高輝度値（>0.85）を設定することでブルームが発動する
        return float4(Material.Emission.rgb, finalAlpha);
    }
}
