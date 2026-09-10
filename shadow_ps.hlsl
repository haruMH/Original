// shadow_ps.hlsl
// 接地丸影（ドロップシャドウ）専用ピクセルシェーダー
// テクスチャサンプリングやサンプラーステートに依存せず、
// 中心 (0.5, 0.5) からの距離をピクセル単位で直接計算して真円グラデーションを描画する。
// 円の外側（半径 0.48 以上）は discard により完全破棄されるため、
// 四角い板ポリゴンの枠線やアーティファクトは物理的に 100% 発生しない。

struct MATERIAL_SHADOW {
    float4 Ambient;
    float4 Diffuse;      // .a = 影の最大アルファ値
    float4 Specular;
    float4 Emission;
    float  Shininess;
    bool   TextureEnable;
    float  RimPower;
    float  Dummy;
};
cbuffer MaterialBuffer : register(b3) { MATERIAL_SHADOW Material; }

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
    // 中心 (0.5, 0.5) からのベクトル
    float2 offset = input.TexCoord - float2(0.5f, 0.5f);
    float dist = length(offset);

    // 半径 0.48f 以上は完全破棄（四角い板ポリゴンの四隅・枠線を物理的に100%遮断）
    if (dist >= 0.48f) {
        discard;
    }

    // 0.0〜0.15f: 濃い中心影、0.15f〜0.48f: 滑らかなコサインフォールオフ
    float innerR = 0.15f;
    float outerR = 0.48f;
    float alpha = 1.0f;

    if (dist > innerR) {
        float t = (dist - innerR) / (outerR - innerR);
        alpha = 0.5f * (1.0f + cos(t * 3.14159265f));
    }

    // 最終アルファ = 計算された円形減衰 × マテリアルのアルファ値
    float finalAlpha = alpha * Material.Diffuse.a;

    // 黒色の影
    return float4(0.0f, 0.0f, 0.0f, finalAlpha);
}
