// aura_ring_ps.hlsl
// チャージオーラ専用ピクセルシェーダー
// 地面に敷いた1枚のクアッド上に、足元リング全体をプロシージャルに描く。
//   ・ノイズ(FBM)でうねる本体リング + 白いコア/広い色付きハロー(二重グロー)
//   ・半径方向の色グラデーション(内側=高温色 / 外側=色相ずらし)
//   ・外側の回転ルーン目盛り(段階2で二重 / 段階3で外周リング追加 / 段階4で炎の縁)
//   ・リング上を周回する彗星 + 火花(スピン速度で加速 / 残像リングと残像彗星)
//   ・地面ライティング(内側の面発光 + 周囲の薄い照り返し)
//   ・暗い縁取り(明るい地面で色を浮かせる)
//   ・段階到達パルス、出現/消滅のスケールフェード
//   ・円筒メッシュ用の光の壁 / 段階4の光柱(Specular.z=1/2)
// 出力は通常アルファ合成。明るい部分のみHDRに持ち上げる（ブルーム対応）。

// マテリアル定数バッファ (b3: ui_ps.hlsl と同じレジスタを共有)
// パラメータはマテリアルの空きフィールドに載せて渡す
struct MATERIAL_AURA {
    float4 Ambient;
    float4 Diffuse;      // .a = 全体の濃さ
    float4 Specular;     // .x = チャージ段階(1〜4) / .y = 出現フェード(0〜1)
    float4 Emission;     // .rgb = 発光色
    float  Shininess;    // 経過時間(秒)
    bool   TextureEnable;
    float  RimPower;     // スピン速度(0〜1)
    float  Dummy;        // パルス進行(0〜1、1以上で無し)
};
cbuffer MaterialBuffer : register(b3) { MATERIAL_AURA Material; }

// 頂点シェーダーからの入力（vertexShader.hlsl の出力と一致させる）
struct PS_INPUT {
    float4 Position      : SV_POSITION;
    float3 Normal        : NORMAL;
    float4 Diffuse       : COLOR;
    float2 TexCoord      : TEXCOORD0;
    float3 WorldPos      : TEXCOORD1;
    float4 LightSpacePos : TEXCOORD2;
};

static const float TWO_PI = 6.28318530718f;
static const float R0     = 0.40f;   // 定常リング半径（クアッド半幅=1 に対する比）

float Band(float x, float w)
{
    float k = x / w;
    return exp(-k * k);
}

// --- 値ノイズ / FBM ---
float Hash(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float VNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float a = Hash(i);
    float b = Hash(i + float2(1, 0));
    float c = Hash(i + float2(0, 1));
    float d = Hash(i + float2(1, 1));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float Fbm(float2 p)
{
    float v = 0.0f;
    float a = 0.5f;
    [unroll]
    for (int i = 0; i < 3; ++i) {
        v += VNoise(p) * a;
        p = p * 2.03f + 17.1f;
        a *= 0.5f;
    }
    return v;
}

// --- 立ち上る光の壁 / 光柱（円筒メッシュ用）。mode 1=壁 2=光柱 ---
float4 WallMain(PS_INPUT input, float mode)
{
    float t     = Material.Shininess;
    float spin  = Material.RimPower;
    float pulse = Material.Dummy;
    float stage = Material.Specular.x;
    float fade  = Material.Specular.y;

    float u = input.TexCoord.x;
    float h = 1.0f - input.TexCoord.y;                 // 0=足元 1=上端
    float ang = u * TWO_PI;
    float2 cd = float2(cos(ang), sin(ang));             // 継ぎ目の無い円周座標

    // 縦に流れる光の筋（下から上へ駆け上がる）
    float flow = h * 1.3f - t * (1.0f + spin * 2.5f);
    float n = Fbm(cd * 5.0f + float2(flow, flow * 0.7f));
    float streak = smoothstep(0.35f, 0.85f, n);

    float3 col = Material.Emission.rgb / max(max(Material.Emission.r, max(Material.Emission.g, Material.Emission.b)), 1e-3f);
    float a;
    float3 rgb;
    if (mode < 1.5f) {
        float bottom  = pow(saturate(1.0f - h), 2.0f);
        float topFade = smoothstep(1.0f, 0.65f, h);
        a  = (0.10f + streak * 0.55f) * (0.15f + bottom * 0.85f) * topFade * (0.5f + 0.12f * stage);
        a += Band(h, 0.035f) * 0.55f;                   // 足元の明るい縁
        rgb = col * (1.0f + 0.4f * streak);
    } else {
        float topFade = smoothstep(1.0f, 0.5f, h);
        a   = (0.35f + 0.65f * streak) * topFade * (1.0f - pulse);
        rgb = lerp(col, float3(1.0f, 1.0f, 1.0f), 0.45f);
    }
    return float4(rgb, saturate(a) * Material.Diffuse.a * fade);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    if (Material.Specular.z > 0.5f) return WallMain(input, Material.Specular.z);
    float2 p0   = input.TexCoord * 2.0f - 1.0f;
    float  r0   = length(p0);
    float  t    = Material.Shininess;
    float  spin  = Material.RimPower;
    float  pulse = Material.Dummy;
    float  stage = Material.Specular.x;
    float  fade  = Material.Specular.y;

    // --- 出現/消滅: 小さい半径から広がる(ease-out) ---
    float e = 1.0f - (1.0f - fade) * (1.0f - fade);
    float s = lerp(0.35f, 1.0f, e);
    float2 p = p0 / s;
    float r  = length(p);
    float2 dir = p / max(r, 1e-4f);
    float ang = atan2(p.y, p.x);

    float v    = 0.0f;       // 色付きの発光量
    float core = 0.0f;       // 白寄りのコア発光量
    float fill = 0.0f;       // 地面ライティング(面発光)。線より十分暗く保つ

    // --- 縁のうねり（FBM。炎のように流れる） ---
    float n1  = Fbm(dir * 2.5f + float2(t * 0.5f, -t * 0.4f));
    float wob = (n1 - 0.5f) * (0.07f + 0.015f * stage);

    // --- 本体リング: 白コア + 広い色付きハロー ---
    float ringW = 0.016f + 0.004f * stage;
    float rr = r - R0 + wob;
    core += Band(rr, ringW * 0.45f) * 1.4f;
    v    += Band(rr, ringW * 1.2f)  * 1.0f;
    v    += Band(rr, ringW * 4.0f)  * 0.18f;      // 外へ滲むハロー
    v    += Band(r - R0 * 0.80f + wob * 0.5f, 0.010f) * 0.35f;

    // --- スピン連動の残像リング（二重・三重に分裂して見える） ---
    [unroll]
    for (int g = 1; g <= 2; ++g) {
        float on = saturate(spin * 2.2f - 0.3f * g);
        float flick = 0.7f + 0.3f * sin(t * 18.0f + g * 2.0f);
        v += Band(r - R0 * (1.0f + 0.07f * g * (0.3f + spin)) + wob, ringW * 0.9f) * 0.55f * on * flick / g;
    }

    // --- 外側ルーン目盛り（逆回転する短い弧）/ 段階で増える ---
    float ticks = 12.0f + 4.0f * stage;
    float tp = frac(ang / TWO_PI * ticks + t * 0.35f);
    float tick = smoothstep(0.0f, 0.12f, tp) * smoothstep(0.55f, 0.43f, tp);
    v += Band(r - R0 * 1.16f, 0.024f) * tick * 1.6f;
    v += Band(r - R0 * 1.30f, 0.012f) * 0.5f;
    if (stage >= 2.0f) {   // 二重目盛り（順回転）
        float tp2 = frac(ang / TWO_PI * (ticks * 0.5f) - t * 0.25f);
        float tick2 = smoothstep(0.0f, 0.1f, tp2) * smoothstep(0.35f, 0.25f, tp2);
        v += Band(r - R0 * 1.42f, 0.020f) * tick2 * 1.8f;
    }
    if (stage >= 3.0f) {   // 外周リング
        v += Band(r - R0 * 1.62f + wob * 0.6f, 0.016f) * 1.4f;
        v += Band(r - R0 * 1.62f, 0.08f) * 0.24f;
    }
    if (stage >= 4.0f && r > R0) {   // 最大チャージ: 外へ噴き出す炎の舌
        float n2 = Fbm(dir * 3.0f + float2(t * 0.9f, t * 0.6f));
        float tongue = pow(saturate(n2 * 1.6f - 0.35f), 2.0f);
        v += exp(-(r - R0) / (0.10f + 0.10f * tongue)) * tongue * 1.3f;
    }

    // --- 周回する彗星 + 残像彗星 + 火花 ---
    float sparkSpeed = 1.2f + spin * 7.0f;
    float tailLen = 0.55f + 0.5f * spin;
    int   count = 3 + (int)stage;
    [loop]
    for (int k = 0; k < count; ++k) {
        float head = t * sparkSpeed + k * TWO_PI / count;
        [unroll]
        for (int g = 0; g < 3; ++g) {          // g=0:本体 1,2:残像
            float lag = g * 0.20f * spin;
            float delta = frac((head - lag - ang) / TWO_PI) * TWO_PI;
            float amp = (g == 0) ? 1.0f : 0.45f / g;
            float tail = exp(-delta * 4.0f / tailLen) * smoothstep(0.0f, 0.04f, delta);
            float lane = Band(r - R0 + wob * 0.5f, 0.016f + 0.012f * tail);
            float dArc = min(delta, TWO_PI - delta) * R0;
            float headG = exp(-(dArc * dArc + (r - R0) * (r - R0)) / 0.0016f);
            v    += (lane * tail * 1.6f + headG * 2.0f) * amp;
            core += headG * 1.2f * amp;
        }

        // 火花: 頭の少し後ろから外側へ飛び散る小さな点（スピンで増える）
        int sparkN = 2 + (int)(spin * 4.0f);
        [loop]
        for (int q = 0; q < sparkN; ++q) {
            float seed = Hash(float2(k * 7.1f + q, floor(t * 6.0f + q * 1.7f)));
            float life = frac(t * 6.0f + q * 1.7f);                    // 0→1 で飛んで消える
            float sAng = head - (0.05f + seed * 0.4f * tailLen) ;
            float2 dirS = float2(cos(sAng), sin(sAng));
            float  sRad = R0 + 0.02f + life * (0.06f + 0.10f * seed);   // 外へ飛ぶ
            float2 sp = dirS * sRad;
            float  d2 = dot(p - sp, p - sp);
            v += exp(-d2 / 0.00012f) * (1.0f - life) * 1.2f;
        }
    }

    // --- 地面ライティング: 内側の面発光 + 周囲の照り返し ---
    float inner = saturate(1.0f - r / (R0 * 0.98f));
    float gnoise = Fbm(p * 4.0f + float2(t * 0.4f, -t * 0.3f));
    fill += pow(inner, 1.3f) * (0.05f + 0.07f * gnoise);
    fill += Band(r - R0 * 1.30f, 0.12f) * 0.025f;  // リング外の照り返し

    // --- 段階到達パルス ---
    if (pulse < 1.0f) {
        float ep = 1.0f - (1.0f - pulse) * (1.0f - pulse);
        float rp = lerp(R0, 0.95f, ep);
        float pw = 0.012f + 0.045f * (1.0f - pulse);
        v    += Band(r - rp, pw) * 2.2f * (1.0f - pulse);
        core += Band(r - rp, pw * 0.4f) * 1.2f * (1.0f - pulse);
        fill += inner * (1.0f - pulse) * 0.25f;
    }

    // --- 色: 内側=高温(白寄り) / 外側=色相ずらし ---
    // 色相だけ使い最大成分=1に正規化（HDR値のまま足すとブルームで白飛びするため）
    float3 baseCol = Material.Emission.rgb / max(max(Material.Emission.r, max(Material.Emission.g, Material.Emission.b)), 1e-3f);
    float3 hotCol  = lerp(baseCol, max(baseCol.r, max(baseCol.g, baseCol.b)).xxx, 0.45f);
    float3 coolCol = lerp(baseCol, baseCol.zxy, 0.30f);
    float  mixT = smoothstep(R0 * 0.6f, R0 * 1.6f, r);
    float3 col  = lerp(hotCol, coolCol, mixT);

    // 通常アルファ合成: 明るい地面でも白飛びせず色が出る。明るい箇所(彗星の頭)だけ少しHDRに持ち上げる
    float3 rgb = col * (1.1f + 0.5f * saturate(v - 1.0f)) + float3(1.0f, 1.0f, 1.0f) * saturate(core) * 0.15f;

    // クアッド境界でフェードアウト + 出現フェード
    float edge = smoothstep(1.0f, 0.88f, r0);
    // 暗い縁取り: リングの外側にうっすら暗色を敷き、明るい地面でも色を浮かせる
    float b  = saturate(max(v * 0.8f + fill, core * 0.6f));
    float d  = saturate(Band(rr, ringW * 3.2f) * 0.5f) * (1.0f - b);
    float aT = b + d;
    float3 rgbMix = (rgb * b + float3(0.02f, 0.02f, 0.06f) * d) / max(aT, 1e-3f);
    return float4(rgbMix, aT * Material.Diffuse.a * edge * fade);
}
