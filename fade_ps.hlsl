// fade_ps.hlsl
// シーン遷移フェード描画専用ピクセルシェーダー

cbuffer FadeBuffer : register(b0)
{
    float4 g_FadeColor; // .rgb = カラー, .a = アルファ値 (0.0 ～ 1.0)
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return g_FadeColor;
}
