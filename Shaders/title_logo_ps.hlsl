// =================================================================
// title_logo_ps.hlsl
// タイトルロゴの登場アニメーション（黒と白）
//   輪郭が左から描かれ → 黒く塗りつぶされ白縁が付き → 下線が中央から伸びる → 光が時々走る
// 文字の形は距離場テクスチャ（R8: 0.5 が文字の縁、>0.5 が文字の外側）から求める
// =================================================================
cbuffer TitleLogoBuffer : register(b6)
{
    float4 P0;   // x: 経過秒, y: 全体アルファ, z/w: 未使用
    float4 P1;   // ロゴ矩形(画面ピクセル): x, y, 幅, 高さ
    float4 P2;   // x: マスク幅, y: マスク高さ, z: 距離場の最大距離(マスク画素), w: 未使用
}

Texture2D    g_Sdf     : register(t0);
SamplerState g_Sampler : register(s0);

struct PS_IN
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

// 文字の縁までの距離（マスク画素単位。外側が正）
float Sdf(float2 uv)
{
    return (g_Sdf.SampleLevel(g_Sampler, uv, 0).r - 0.5f) * 2.0f * P2.z;
}

// 通常のアルファ合成（premultiplied で蓄積）
void Over(inout float4 acc, float3 rgb, float a)
{
    a = saturate(a);
    acc.rgb = rgb * a + acc.rgb * (1.0f - a);
    acc.a   = a + acc.a * (1.0f - a);
}

float4 main(PS_IN input) : SV_TARGET
{
    float t = P0.x;
    float2 uv = (input.Position.xy - P1.xy) / P1.zw;
    float scale = P1.z / P2.x;              // 画面ピクセル / マスク画素
    float aa = 1.0f / max(scale, 0.1f);     // 縁のぼかし幅（マスク画素）

    float d = Sdf(uv);
    float4 acc = float4(0, 0, 0, 0);

    // ---- 進行度 ----
    // 輪郭: 0.0〜1.3秒で左から右へ描かれる
    float xr = saturate(t / 1.3f) * 1.25f - 0.12f;
    float revealOut = smoothstep(xr, xr - 0.05f, uv.x);
    // 塗り: 0.9〜2.2秒で左上から斜めに
    float fs = saturate((t - 0.9f) / 1.3f);
    float c  = fs * 1.4f - 0.2f;
    float s  = uv.x * 0.85f + (1.0f - uv.y) * 0.15f;
    float fillAmt = smoothstep(c, c - 0.12f, s) * step(0.001f, fs);

    // ---- 影（右下へずらした暗いにじみ）----
    float2 shOff = float2(5.0f / P2.x, 7.0f / P2.y);
    float dS = Sdf(uv - shOff);
    float shadow = (1.0f - smoothstep(6.0f, 18.0f, dS)) * 0.50f * fillAmt;
    Over(acc, float3(0, 0, 0), shadow);

    // ---- 輪郭線（白い細線＋暗いにじみで明るい背景でも見える）----
    float lineD = abs(d);
    float outlineHalo = (1.0f - smoothstep(1.6f, 3.6f + aa, lineD)) * revealOut * 0.45f;
    Over(acc, float3(0, 0, 0), outlineHalo);

    // ---- 白い縁取り（ステッカー風）----
    const float R = 10.0f;
    float whiteBase = (1.0f - smoothstep(R, R + aa, d)) * fillAmt;
    Over(acc, float3(1, 1, 1), whiteBase);

    // ---- 黒い塗り ----
    float inside = 1.0f - smoothstep(-aa, aa, d);
    Over(acc, float3(0.025f, 0.025f, 0.035f), inside * fillAmt);

    // ---- 内側のハイライト線 ----
    float hl = (1.0f - smoothstep(0.9f, 0.9f + aa, abs(d + 7.0f))) * fillAmt * 0.40f * inside;
    Over(acc, float3(1, 1, 1), hl);

    // ---- 輪郭の白線（描かれている間は強く、塗りが来ると縁の一部になる）----
    float ow = (1.0f - smoothstep(1.5f, 1.5f + aa, lineD)) * revealOut;
    Over(acc, float3(1, 1, 1), ow);

    // 輪郭を描いている先端の光
    float head = exp(-pow((uv.x - xr) * 38.0f, 2.0f)) * exp(-lineD * 0.30f) * step(t, 1.5f) * step(0.0f, t);
    Over(acc, float3(1, 1, 1), head * 0.9f * smoothstep(-0.12f, 0.0f, xr));

    // ---- 光の走り（塗り完了後、数秒おきに斜めの帯が文字をなぞる）----
    float tt = t - 3.0f;
    if (tt > 0.0f)
    {
        float ph = frac(tt / 5.0f);
        float bandPos = ph * 2.4f - 0.7f;
        float coord = uv.x + uv.y * 0.35f;
        float band = exp(-pow((coord - bandPos) * 8.0f, 2.0f));
        Over(acc, float3(1, 1, 1), band * 0.55f * inside * fillAmt);
    }

    // ---- 下線: 中央から左右へ伸びる細い線＋ひし形 ----
    float u = saturate((t - 2.0f) / 0.9f);
    float ease = 1.0f - pow(1.0f - u, 3.0f);
    if (ease > 0.001f)
    {
        float uy  = 0.885f;
        float dx  = uv.x - 0.5f;
        float dxp = dx * P2.x;                 // マスク画素
        float dyp = (uv.y - uy) * P2.y;
        float L   = 0.46f * ease;              // 片側の長さ(uv)
        float taper = saturate(1.0f - pow(abs(dx) / max(L, 0.0001f), 3.0f));
        float th = 2.6f * taper;
        float inLen = step(abs(dx), L);
        float gap = step(40.0f, abs(dxp));     // 中央のひし形の周りを空ける

        // 黒い縁 → 白い線
        float edge = (1.0f - smoothstep(th + 2.6f, th + 2.6f + aa, abs(dyp))) * inLen * gap;
        Over(acc, float3(0, 0, 0), edge * 0.85f);
        float core = (1.0f - smoothstep(th, th + aa, abs(dyp))) * inLen * gap;
        Over(acc, float3(1, 1, 1), core);

        // 中央の大きなひし形と、左右の小さなひし形
        float appear = smoothstep(0.15f, 0.5f, u);
        float dmC = abs(dxp) + abs(dyp);
        Over(acc, float3(0, 0, 0), (1.0f - smoothstep(15.0f, 15.0f + aa, dmC)) * appear * 0.9f);
        Over(acc, float3(1, 1, 1), (1.0f - smoothstep(10.5f, 10.5f + aa, dmC)) * appear);

        float dmS = abs(abs(dxp) - 62.0f) + abs(dyp);
        float smallOn = smoothstep(0.35f, 0.7f, u);
        Over(acc, float3(0, 0, 0), (1.0f - smoothstep(9.5f, 9.5f + aa, dmS)) * smallOn * 0.9f);
        Over(acc, float3(1, 1, 1), (1.0f - smoothstep(5.5f, 5.5f + aa, dmS)) * smallOn);
    }

    float a = acc.a * P0.y;
    if (a <= 0.001f) discard;
    return float4(acc.rgb / max(acc.a, 0.0001f), a);
}
