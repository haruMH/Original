// 風で揺れる草（頂点バッファなし。SV_InstanceID で位置を決め、SV_VertexID で刃の形を作る）
// DX11_ShaderTest の草シェーダーを移植し、拡張したもの。
//  - 草の格子はカメラ中心で、ワールドのマス目に合わせて動く（刃の位置はワールド座標のハッシュで決まるので、移動しても揺れない）
//  - 近景(密・細い刃・9頂点)と遠景(疎・太い刃・5頂点)の2層を、同じシェーダーで描き分ける
//  - キャンプの内側には生やさない
cbuffer GrassCB : register(b8)
{
    float4x4 View;
    float4x4 Proj;
    float4   P0;   // x:時間 y:風の強さ z:風の速さ w:草の高さ
    float4   P1;   // x:風向X y:風向Z z:刃の間隔(マス) w:1辺の本数
    float4   P2;   // x:幅 y:キャンプ除外の半幅X z:キャンプ除外の半幅Z w:外縁(これより遠い所は描かない)
    float4   P3;   // xyz:カメラ位置 w:1本あたりの頂点数
    float4   P4;   // x:内縁(これより近い所は描かない) y:高さ倍率 z:幅倍率 w:草を生やす範囲(±)
};

struct VS_OUT
{
    float4 pos      : SV_POSITION;
    float3 col      : COLOR;
    float3 worldPos : TEXCOORD0;
};

float hash21(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }

VS_OUT main(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    VS_OUT o;
    o.pos = float4(2.0, 2.0, 2.0, 1.0);   // 既定は画面外（描かない）
    o.col = 0; o.worldPos = 0;

    // 刃の形: 頂点数に応じた段数の三角形ストリップ（先端は幅 0）
    uint  vertCount = (uint)P3.w;
    uint  maxLevel  = (vertCount - 1) / 2;
    uint  level     = min(vid / 2, maxLevel);
    float side      = (vid & 1) ? 1.0 : -1.0;
    float h         = (float)level / (float)maxLevel;           // 0(根元)〜1(先端)
    float width     = P2.x * (1.0 - h * h) * ((vid == vertCount - 1) ? 0.0 : 1.0);

    // 配置: カメラ中心の格子（マス目はワールド固定）+ ジッター
    uint   n    = (uint)P1.w;
    float2 id2  = float2(iid % n, iid / n);
    float  cell = P1.z;
    float2 baseIdx = floor(P3.xz / cell) - (float)(n / 2);
    float2 cellIdx = baseIdx + id2;                              // ワールドのマス番号
    float2 jit  = float2(hash21(cellIdx), hash21(cellIdx + 17.3));
    float3 root = float3((cellIdx.x + jit.x) * cell, 0.0, (cellIdx.y + jit.y) * cell);

    // 範囲外・キャンプ内・内縁より近い所・外縁より遠い所は描かない。縁は確率で間引いて境目を消す
    float2 dc    = root.xz - P3.xz;
    float  distC = length(dc);
    float  outerFade = 1.0 - smoothstep(P2.w * 0.70, P2.w, distC);
    float  innerFade = smoothstep(P4.x, P4.x + 4.0, distC);
    // 遠景層は内縁から離れるほど徐々に高く・太く（境目で急に伸びて見えないように）
    float  rampS = min(P4.x, 11.0);   // 外側の層(内縁が遠い)は最初から遠景の大きさ
    float  ramp = (P4.x > 0.0) ? smoothstep(rampS, rampS + 14.0, distC) : 1.0;
    width *= lerp(lerp(1.0, P4.z, 0.6), P4.z, ramp);   // 遠景は最初から少し太くして、近景との隙間感をなくす
    bool inCamp  = (abs(root.x) < P2.y && abs(root.z) < P2.z);
    bool outside = (abs(root.x) > P4.w || abs(root.z) > P4.w);
    if (inCamp || outside
        || hash21(cellIdx + 9.1) > outerFade
        || (P4.x > 0.0 && hash21(cellIdx + 4.4) > innerFade))
    {
        return o;
    }

    float rnd    = hash21(cellIdx + 3.7);
    float bladeH = P0.w * lerp(1.0, P4.y, ramp) * (0.45 + 1.05 * rnd * rnd + 0.25 * hash21(cellIdx + 21.1)) * lerp(0.85, 1.0, outerFade);
    float ang    = rnd * 6.2831853;
    float2 face  = float2(cos(ang), sin(ang));       // 刃の幅方向

    // 風: 進行波 + 細かい揺れ。先端ほど大きく曲がる（h^2）
    float2 wd = normalize(P1.xy + 1e-4);
    float phase   = dot(root.xz, wd) * 0.35 - P0.x * P0.z;
    float gust    = sin(phase) * 0.6 + sin(phase * 2.3 + root.x * 0.5) * 0.25 + 0.4;
    float flutter = sin(P0.x * P0.z * 3.0 + rnd * 20.0) * 0.1;
    float bend    = (gust + flutter) * P0.y * h * h;

    float3 p = root;
    p.xz += face * side * width;
    p.xz += wd * bend * bladeH;
    p.xz += float2(-wd.y, wd.x) * sin(phase * 1.7 + rnd * 6.0) * 0.15 * P0.y * h * h * bladeH;
    // 刃ごとの固定の傾き（まっすぐ揃って立たず、あちこちへ倒れる）
    float lAng = hash21(cellIdx + 5.3) * 6.2831853;
    float lean = 0.10 + 0.55 * hash21(cellIdx + 8.8);
    p.xz += float2(cos(lAng), sin(lAng)) * lean * h * h * bladeH;
    p.y  += -0.98 + h * bladeH * (1.0 - 0.35 * saturate(bend));   // 地面(-1.0)の上の芝の敷物(-0.985)から生える

    o.worldPos = p;
    o.pos = mul(mul(float4(p, 1.0), View), Proj);

    // 広い範囲の色むら（みずみずしい緑 ↔ 枯れかけの黄緑）。根元は暗く
    float2 pq = root.xz * 0.11;
    float2 pf = frac(pq); pf = pf * pf * (3.0 - 2.0 * pf);
    float2 pi = floor(pq);
    float patch = lerp(lerp(hash21(pi), hash21(pi + float2(1, 0)), pf.x),
                       lerp(hash21(pi + float2(0, 1)), hash21(pi + float2(1, 1)), pf.x), pf.y);
    float3 baseC = lerp(float3(0.06, 0.20, 0.05), float3(0.13, 0.20, 0.05), patch);
    float3 tipC  = lerp(float3(0.42, 0.70, 0.20), float3(0.74, 0.74, 0.30), patch);
    o.col = lerp(baseC, tipC, smoothstep(0.0, 0.85, h)) * (0.78 + 0.45 * rnd) * (0.90 + 0.25 * saturate(gust));
    return o;
}
