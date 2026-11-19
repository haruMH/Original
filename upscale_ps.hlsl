// renderScale < 1 のとき、シーン解像度の一時画像(合成済み)を画面解像度のバックバッファへ拡大する。
// ポスト用の全画面三角形(postEffect_vs)で呼ぶ。
Texture2D    g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

float4 main(float4 Position : SV_POSITION, float2 TexCoord : TEXCOORD) : SV_TARGET
{
    // 端の1画素が反対側と混ざらないよう、UV を半画素内側へ収める（サンプラーは WRAP のため）
    uint w, h;
    g_Texture.GetDimensions(w, h);
    float2 hp = 0.5f / float2(w, h);
    float2 uv = clamp(TexCoord, hp, 1.0f - hp);
    return float4(g_Texture.Sample(g_Sampler, uv).rgb, 1.0f);
}
