// 風で揺れる草（頂点バッファなし。SV_InstanceID で位置を決め、SV_VertexID で刃の形を作る）
// DX11_ShaderTest の草シェーダーを移植し、拠点の外側（キャンプの外）にだけ生やすよう拡張した。
cbuffer GrassCB : register(b8)
{
    float4x4 View;
    float4x4 Proj;
    float4   P0;   // x:時間 y:風の強さ z:風の速さ w:草の高さ
    float4   P1;   // x:風向X y:風向Z z:範囲(1辺) w:1辺の本数
    float4   P2;   // x:幅 y:キャンプ除外の半幅X z:キャンプ除外の半幅Z w:描画距離
    float4   P3;   // xyz:カメラ位置
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

    // 刃: 4 段(各 左右 2 頂点) + 先端 1 頂点 = 9 頂点の三角形ストリップ
    uint  level = min(vid / 2, 4u);
    float side  = (vid & 1) ? 1.0 : -1.0;
    float h     = level / 4.0;                       // 0(根元)〜1(先端)
    float width = P2.x * (1.0 - h * h) * (vid == 8 ? 0.0 : 1.0);

    // 配置: 格子 + ジッター
    uint   n   = (uint)P1.w;
    float2 id2 = float2(iid % n, iid / n);
    float  cell = P1.z / P1.w;
    float2 jit = float2(hash21(id2), hash21(id2 + 17.3));
    float3 root = float3((id2.x + jit.x) * cell - P1.z * 0.5, 0.0, (id2.y + jit.y) * cell - P1.z * 0.5);

    // キャンプの内側、またはカメラから遠すぎる所は描かない（画面外へ退避させて無効化）
    float2 dc = root.xz - P3.xz;
    float  distC = length(dc);
    // 遠いほど本数を間引き（確率で消す）、残った草も低くする → 境目が見えず霧に溶ける
    float  fade = 1.0 - smoothstep(P2.w * 0.40, P2.w, distC);
    if ((abs(root.x) < P2.y && abs(root.z) < P2.z) || hash21(id2 + 9.1) > fade)
    {
        o.pos = float4(2.0, 2.0, 2.0, 1.0);
        o.col = 0; o.worldPos = 0;
        return o;
    }

    float rnd    = hash21(id2 + 3.7);
    float bladeH = P0.w * (0.45 + 1.05 * rnd * rnd + 0.25 * hash21(id2 + 21.1)) * lerp(0.55, 1.0, fade);
    float ang    = rnd * 6.2831853;
    float2 face  = float2(cos(ang), sin(ang));       // 刃の幅方向

    // 風: 進行波 + 細かい揺れ。先端ほど大きく曲がる（h^2）
    float2 wd = normalize(P1.xy + 1e-4);
    float phase  = dot(root.xz, wd) * 0.35 - P0.x * P0.z;
    float gust   = sin(phase) * 0.6 + sin(phase * 2.3 + root.x * 0.5) * 0.25 + 0.4;
    float flutter = sin(P0.x * P0.z * 3.0 + rnd * 20.0) * 0.1;
    float bend   = (gust + flutter) * P0.y * h * h;

    float3 p = root;
    p.xz += face * side * width;
    p.xz += wd * bend * bladeH;
    p.xz += float2(-wd.y, wd.x) * sin(phase * 1.7 + rnd * 6.0) * 0.15 * P0.y * h * h * bladeH;
    // 刃ごとの固定の傾き（まっすぐ揃って立たず、あちこちへ倒れる）
    float lAng = hash21(id2 + 5.3) * 6.2831853;
    float lean = 0.10 + 0.55 * hash21(id2 + 8.8);
    p.xz += float2(cos(lAng), sin(lAng)) * lean * h * h * bladeH;
    p.y  += -1.0 + h * bladeH * (1.0 - 0.35 * saturate(bend));   // 地面は y=-1

    o.worldPos = p;
    o.pos = mul(mul(float4(p, 1.0), View), Proj);

    // 根元は暗く、先端は明るい黄緑。風で倒れた所は明るく見える
    // 広い範囲の色むら（みずみずしい緑 ↔ 枯れかけの黄緑）
    float2 pq = root.xz * 0.11;
    float2 pf = frac(pq); pf = pf * pf * (3.0 - 2.0 * pf);
    float2 pi = floor(pq);
    float patch = lerp(lerp(hash21(pi), hash21(pi + float2(1, 0)), pf.x),
                       lerp(hash21(pi + float2(0, 1)), hash21(pi + float2(1, 1)), pf.x), pf.y);
    float3 baseC = lerp(float3(0.04, 0.16, 0.04), float3(0.12, 0.17, 0.04), patch);   // 根元は暗く
    float3 tipC  = lerp(float3(0.42, 0.70, 0.20), float3(0.74, 0.74, 0.30), patch);
    o.col = lerp(baseC, tipC, smoothstep(0.0, 0.85, h)) * (0.78 + 0.45 * rnd) * (0.90 + 0.25 * saturate(gust));
    return o;
}
