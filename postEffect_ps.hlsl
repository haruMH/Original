cbuffer PostEffectBuffer : register(b0) {
    int      Mode;
    float    Threshold;
    float    BlurIntensity;
    float    SlowMotionIntensity;
    float4   FlashColor;           // RGB: 色, A: 強度
    float    RadialBlurIntensity;  // 放射ブラー強度 (0.0〜1.0)
    float    ChromaticAberration;  // 色収差強度 (0.0〜1.0)
    float2   RadialBlurCenter;     // 放射ブラー中心UV
    float2   TexelSize;            // 実画面テクセルサイズ (1/width, 1/height)
    float2   PostDummy;            // 16バイト境界パディング
    float4   GradeParams;          // x: 彩度, y: コントラスト (空ごとのカラーグレード)
    float4   ShadowTint;           // 暗部の色味 RGB
    float4   HighlightTint;        // 明部の色味 RGB
}

Texture2D    g_Texture1 : register(t0);
Texture2D    g_Texture2 : register(t1);
SamplerState g_Sampler  : register(s0);

// ACES Filmic トーンマッピング近似式 (Narkowicz 2015)
// Unreal Engine や映画業界の標準カーブで、白飛びを防ぎ豊かなハイライト・シャドウ階調を実現
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// AMD FidelityFX CAS (Contrast Adaptive Sharpening) 空間的適応型シャープネス
// 局所コントラストに応じてリンギング（白縁）を防止しながら輪郭線とテクスチャを鮮明化
float3 ApplyCAS(Texture2D tex, SamplerState samp, float2 uv, float sharpness)
{
    float2 texel = TexelSize;

    float3 c = tex.Sample(samp, uv).rgb;
    float3 u = tex.Sample(samp, uv + float2( 0.0f, -texel.y)).rgb;
    float3 d = tex.Sample(samp, uv + float2( 0.0f,  texel.y)).rgb;
    float3 l = tex.Sample(samp, uv + float2(-texel.x,  0.0f)).rgb;
    float3 r = tex.Sample(samp, uv + float2( texel.x,  0.0f)).rgb;

    // 局所の最小輝度・最大輝度（RGB独立評価）
    float3 minColor = min(c, min(min(u, d), min(l, r)));
    float3 maxColor = max(c, max(max(u, d), max(l, r)));

    // コントラスト適応重み算出
    float3 amp = saturate(min(minColor, 2.0f - maxColor) / max(maxColor, 0.001f));
    float3 w = -sqrt(amp) * (sharpness * 0.22f);

    // フィルタリング合成
    float3 filtered = (c + (u + d + l + r) * w) / (1.0f + 4.0f * w);
    return saturate(filtered);
}

float4 main(float4 Position : SV_POSITION, float2 TexCoord : TEXCOORD) : SV_TARGET
{
    float4 color = float4(0, 0, 0, 1);

    if (Mode == 0)
    {
        // 1. 高輝度抽出パス
        color = g_Texture1.Sample(g_Sampler, TexCoord);
        float brightness = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
        if (brightness < Threshold) color = float4(0, 0, 0, 1);
    }
    else if (Mode == 1 || Mode == 2)
    {
        // 2 & 3. ガウシアンブラーパス
        float2 offset = (Mode == 1) ? float2(1.0 / 320.0, 0) : float2(0, 1.0 / 180.0);
        float weight[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };
        
        color = g_Texture1.Sample(g_Sampler, TexCoord) * weight[0];
        for (int i = 1; i < 5; i++) {
            color += g_Texture1.Sample(g_Sampler, TexCoord + offset * i) * weight[i];
            color += g_Texture1.Sample(g_Sampler, TexCoord - offset * i) * weight[i];
        }
    }
    else if (Mode == 3)
    {
        // 4. 最終合成パス (色収差 + ラジアルブラー + CASシャープネス + ブルーム + ACES + ヴィネット)
        float2 uvOffset = TexCoord - 0.5f;

        // (a) シネマティック色収差（画面端および衝撃時のRGBプリズム分散）
        float caAmount = 0.0015f + ChromaticAberration * 0.012f;
        float2 caDir = uvOffset * caAmount;
        float rCol = g_Texture1.Sample(g_Sampler, TexCoord + caDir).r;
        float gCol = g_Texture1.Sample(g_Sampler, TexCoord).g;
        float bCol = g_Texture1.Sample(g_Sampler, TexCoord - caDir).b;
        float3 caBase = float3(rCol, gCol, bCol);

        // (b) CAS 適応型シャープネス（中心部・基本テクスチャを鮮明化）
        float casAmount = (PostDummy.x > 0.001f) ? PostDummy.x : 0.60f;
        float3 sharpBase = ApplyCAS(g_Texture1, g_Sampler, TexCoord, casAmount);
        float3 finalBase = lerp(sharpBase, caBase, saturate(length(uvOffset) * 1.5f + ChromaticAberration * 2.0f));

        // (c) アクション連動ラジアルブラー（ダッシュ・スピン・投擲時の放射速度線）
        if (RadialBlurIntensity > 0.01f)
        {
            float2 blurVec = (TexCoord - RadialBlurCenter) * RadialBlurIntensity * 0.06f;
            float3 blurAccum = finalBase;
            [unroll]
            for (int s = 1; s <= 5; ++s) {
                blurAccum += g_Texture1.Sample(g_Sampler, TexCoord - blurVec * (float)s).rgb;
            }
            finalBase = blurAccum / 6.0f;
        }

        float4 bloomColor = g_Texture2.Sample(g_Sampler, TexCoord);
        color = float4(finalBase, 1.0f) + bloomColor * BlurIntensity;

        // スローモーション時のカラー補正（彩度を落とし、青みを加える）
        if (SlowMotionIntensity > 0.0)
        {
            float gray = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
            float3 slowColor = float3(gray * 0.6, gray * 0.75, gray * 1.3); // 青みがかったシネマティックモノクロ
            color.rgb = lerp(color.rgb, slowColor, SlowMotionIntensity);
        }

        // 画面フラッシュ / 調色エフェクト
        if (FlashColor.a > 0.0)
        {
            color.rgb = lerp(color.rgb, FlashColor.rgb, FlashColor.a);
        }

        // ACES Filmic トーンマッピング（シネマティックな明暗カーブ）
        color.rgb = ACESFilm(color.rgb);

        // カラーグレード（彩度・コントラスト・暗部/明部の色味。空の種類に連動）
        float gl = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
        color.rgb = lerp(float3(gl, gl, gl), color.rgb, GradeParams.x);
        color.rgb = saturate((color.rgb - 0.5f) * GradeParams.y + 0.5f);
        color.rgb *= lerp(ShadowTint.rgb, HighlightTint.rgb, saturate(gl * 1.5f));

        // シネマティック・ヴィネット（画面周辺のわずかな減光による重厚感と視線誘導）
        float distSq = dot(uvOffset, uvOffset);
        float vignette = saturate(1.0f - distSq * 0.40f);
        color.rgb *= vignette;
    }

    return color;
}
