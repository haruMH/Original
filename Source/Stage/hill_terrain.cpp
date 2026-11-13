#include "hill_terrain.h"
#include "renderer.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace {
    // ---- 調整用の定数 ----
    const float TERRAIN_HALF   = 320.0f;   // 丘の広がり（±）
    const float CELL           = 4.0f;     // 格子の1マス
    const float GROUND_HALF    = 50.0f;    // 既存の地面の半幅（この内側はメッシュを作らない）
    const float APRON_END      = 70.0f;    // ここまでは地面と同じ高さ（草が埋まらないように）
    const float GROUND_Y       = -1.0f;

    bool g_Enabled = false;
    bool g_Built   = false;
    int  g_VertexCount = 0;

    ID3D11Buffer*            g_VB = nullptr;
    ID3D11VertexShader*      g_VS = nullptr;
    ID3D11PixelShader*       g_PS = nullptr;
    ID3D11InputLayout*       g_IL = nullptr;
    ID3D11RasterizerState*   g_Rs = nullptr;

    float Smooth(float a, float b, float x)
    {
        float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    float Hash2(int x, int y)
    {
        unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= (h >> 16);
        return (float)(h & 0xFFFFFF) / (float)0x1000000;
    }

    float ValueNoise(float x, float y)
    {
        int ix = (int)floorf(x), iy = (int)floorf(y);
        float fx = x - ix, fy = y - iy;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);
        float a = Hash2(ix, iy),     b = Hash2(ix + 1, iy);
        float c = Hash2(ix, iy + 1), d = Hash2(ix + 1, iy + 1);
        return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
    }

    float Fbm(float x, float y)
    {
        float v = 0.0f, a = 0.5f;
        for (int i = 0; i < 4; ++i) {
            v += a * ValueNoise(x, y);
            x = x * 2.03f + 17.1f;
            y = y * 2.03f + 9.3f;
            a *= 0.5f;
        }
        return v / 0.9375f;   // 0〜1 に正規化
    }

    // 丘の高さ。手前(APRON_END まで)は地面と同じ平ら、遠いほど高い山になる
    float HeightAt(float x, float z)
    {
        float d = (std::max)(fabsf(x), fabsf(z));
        float t = Smooth(APRON_END, 190.0f, d);
        float amp = 7.0f + 40.0f * Smooth(110.0f, 300.0f, d);
        float n = Fbm(x * 0.011f, z * 0.011f);
        return GROUND_Y + t * amp * (0.20f + n * 1.15f);
    }

    XMFLOAT3 NormalAt(float x, float z)
    {
        const float e = 2.0f;
        float hx = HeightAt(x + e, z) - HeightAt(x - e, z);
        float hz = HeightAt(x, z + e) - HeightAt(x, z - e);
        XMVECTOR n = XMVector3Normalize(XMVectorSet(-hx, 2.0f * e, -hz, 0));
        XMFLOAT3 r; XMStoreFloat3(&r, n);
        return r;
    }

    XMFLOAT4 TerrainColor(float x, float z, float y, const XMFLOAT3& n)
    {
        float v = Fbm(x * 0.05f + 31.0f, z * 0.05f + 7.0f);               // 草色のむら
        XMFLOAT3 grassA(0.40f, 0.66f, 0.20f), grassB(0.24f, 0.50f, 0.15f);
        XMFLOAT3 c(grassA.x + (grassB.x - grassA.x) * v,
                   grassA.y + (grassB.y - grassA.y) * v,
                   grassA.z + (grassB.z - grassA.z) * v);
        // 高い所と急斜面は岩肌っぽく
        float rock = (std::max)(Smooth(14.0f, 34.0f, y - GROUND_Y), Smooth(0.30f, 0.60f, 1.0f - n.y));
        XMFLOAT3 rockC(0.50f, 0.50f, 0.44f);
        c = XMFLOAT3(c.x + (rockC.x - c.x) * rock, c.y + (rockC.y - c.y) * rock, c.z + (rockC.z - c.z) * rock);
        return XMFLOAT4(c.x, c.y, c.z, 1.0f);
    }

    VERTEX_3D MakeV(const XMFLOAT3& p, const XMFLOAT3& n, const XMFLOAT4& c)
    {
        VERTEX_3D v;
        ZeroMemory(&v, sizeof(v));
        v.Position = p; v.Normal = n; v.Diffuse = c; v.TexCoord = XMFLOAT2(0, 0);
        return v;
    }

    XMFLOAT4 Mul(const XMFLOAT4& c, float k) { return XMFLOAT4(c.x * k, c.y * k, c.z * k, 1.0f); }
    XMFLOAT4 Mix(const XMFLOAT4& a, const XMFLOAT4& b, float t)
    {
        return XMFLOAT4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, 1.0f);
    }

    void PushTri(std::vector<VERTEX_3D>& out,
                 const XMFLOAT3& a, const XMFLOAT3& na, const XMFLOAT4& ca,
                 const XMFLOAT3& b, const XMFLOAT3& nb, const XMFLOAT4& cb,
                 const XMFLOAT3& c, const XMFLOAT3& nc, const XMFLOAT4& cc)
    {
        out.push_back(MakeV(a, na, ca));
        out.push_back(MakeV(b, nb, cb));
        out.push_back(MakeV(c, nc, cc));
    }

    // 円錐（枝葉の1段）。縁を不揃いにし、法線をなめらかに、根元を暗く先端を明るくする
    void AddCone(std::vector<VERTEX_3D>& out, const XMFLOAT3& base, float radius, float height, int segs,
                 int seed, const XMFLOAT4& colBase, const XMFLOAT4& colTip, float droop)
    {
        std::vector<XMFLOAT3> ring(segs + 1), nrm(segs + 1);
        for (int i = 0; i <= segs; ++i) {
            int k = i % segs;
            float a = (float)k / segs * XM_2PI;
            float rj = radius * (0.78f + 0.44f * Hash2(seed, k));            // 縁の出入り
            float yj = (Hash2(seed + 77, k) - 0.5f) * 0.18f * height - droop;  // 枝先のたれ
            ring[i] = XMFLOAT3(base.x + cosf(a) * rj, base.y + yj, base.z + sinf(a) * rj);
            XMVECTOR n = XMVector3Normalize(XMVectorSet(cosf(a) * height, radius * 1.1f, sinf(a) * height, 0));
            XMStoreFloat3(&nrm[i], n);
        }
        XMFLOAT3 apex(base.x, base.y + height, base.z);
        for (int i = 0; i < segs; ++i) {
            XMVECTOR m = XMVector3Normalize(XMVectorAdd(XMLoadFloat3(&nrm[i]), XMLoadFloat3(&nrm[i + 1])));
            XMFLOAT3 nApex; XMStoreFloat3(&nApex, XMVector3Normalize(XMVectorAdd(m, XMVectorSet(0, 0.6f, 0, 0))));
            PushTri(out, ring[i], nrm[i], colBase, ring[i + 1], nrm[i + 1], colBase, apex, nApex, colTip);
        }
    }

    // 幹（細い円錐。根元は暗く）
    void AddTrunk(std::vector<VERTEX_3D>& out, const XMFLOAT3& base, float radius, float height, int seed, const XMFLOAT4& col)
    {
        AddCone(out, base, radius, height, 6, seed, Mul(col, 0.65f), col, 0.0f);
    }

    // 丸い葉のかたまり（楕円体）。表面をうねらせ、上を明るく下を暗くする
    void AddBlob(std::vector<VERTEX_3D>& out, const XMFLOAT3& center, float rx, float ry, int slices, int stacks,
                 int seed, const XMFLOAT4& colBottom, const XMFLOAT4& colTop)
    {
        auto P = [&](int s, int k, XMFLOAT3* nOut, XMFLOAT4* cOut) {
            int sk = s % slices;
            float th = (float)sk / slices * XM_2PI;
            float ph = (float)k / stacks * XM_PI;                          // 0:上 → π:下
            float wob = 1.0f + 0.22f * (Hash2(seed, sk + k * 31) - 0.5f) * ((k == 0 || k == stacks) ? 0.0f : 1.0f);
            XMFLOAT3 dir(cosf(th) * sinf(ph), cosf(ph), sinf(th) * sinf(ph));
            XMVECTOR n = XMVector3Normalize(XMVectorSet(dir.x / rx, dir.y / ry, dir.z / rx, 0));
            XMStoreFloat3(nOut, n);
            *cOut = Mix(colBottom, colTop, std::clamp(0.5f + 0.5f * dir.y, 0.0f, 1.0f));
            return XMFLOAT3(center.x + dir.x * rx * wob, center.y + dir.y * ry * wob, center.z + dir.z * rx * wob);
        };
        for (int k = 0; k < stacks; ++k) {
            for (int s = 0; s < slices; ++s) {
                XMFLOAT3 na, nb, nc, nd; XMFLOAT4 ca, cb, cc, cd;
                XMFLOAT3 a = P(s, k, &na, &ca), b = P(s + 1, k, &nb, &cb);
                XMFLOAT3 c = P(s, k + 1, &nc, &cc), d = P(s + 1, k + 1, &nd, &cd);
                if (k > 0)          PushTri(out, a, na, ca, b, nb, cb, c, nc, cc);
                if (k < stacks - 1) PushTri(out, b, nb, cb, d, nd, cd, c, nc, cc);
            }
        }
    }

    void BuildTrees(std::vector<VERTEX_3D>& out)
    {
        unsigned int seed = 12345u;
        auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return (float)(seed >> 8) / (float)(1 << 24); };

        int treeId = 0;
        const int ATTEMPTS = 3200;
        for (int i = 0; i < ATTEMPTS; ++i) {
            float x = (rnd() * 2.0f - 1.0f) * 230.0f;
            float z = (rnd() * 2.0f - 1.0f) * 230.0f;
            float d = (std::max)(fabsf(x), fabsf(z));
            if (d < 58.0f) continue;                                  // 地面と草原の上には置かない

            // 林のかたまり（ノイズが高い所だけ）。遠いほど少し疎に
            float grove = Fbm(x * 0.018f + 5.0f, z * 0.018f + 11.0f);
            if (grove < 0.52f) continue;
            if (rnd() > 0.95f - 0.45f * Smooth(120.0f, 230.0f, d)) continue;

            float y = HeightAt(x, z);
            XMFLOAT3 n = NormalAt(x, z);
            if (n.y < 0.80f || y - GROUND_Y > 26.0f) continue;        // 急斜面・高山には生やさない

            ++treeId;
            int sid = treeId * 13;
            float s = 0.75f + rnd() * 0.8f;
            float tone = 0.82f + rnd() * 0.30f;
            XMFLOAT3 root(x, y - 0.2f, z);

            if (rnd() < 0.60f) {
                // 針葉樹: 幹 + 4段の枝葉（下の段ほど大きく、少したれる）
                XMFLOAT4 trunk(0.30f, 0.20f, 0.12f, 1.0f);
                AddTrunk(out, root, 0.45f * s, 3.2f * s, sid, trunk);
                XMFLOAT4 dark(0.06f * tone, 0.20f * tone, 0.10f * tone, 1.0f);
                XMFLOAT4 light(0.20f * tone, 0.46f * tone, 0.20f * tone, 1.0f);
                for (int t = 0; t < 4; ++t) {
                    float k = (float)t / 3.0f;                                     // 0:下 → 1:上
                    float r = (3.3f - 2.0f * k) * s;
                    float h = (4.8f - 0.8f * k) * s;
                    AddCone(out, XMFLOAT3(x, y + (1.2f + 2.3f * t) * s, z), r, h, 9, sid + t * 5 + 1,
                            Mul(dark, 0.9f + 0.2f * k), light, 0.35f * s);
                }
            } else {
                // 広葉樹: 幹 + 重なった3つの葉のかたまり（色むらつき）
                XMFLOAT4 trunk(0.34f, 0.22f, 0.13f, 1.0f);
                AddTrunk(out, root, 0.60f * s, 4.4f * s, sid, trunk);
                float hueShift = rnd();                                            // 緑 〜 黄緑
                XMFLOAT4 bottom(0.13f * tone, 0.30f * tone, 0.08f * tone, 1.0f);
                XMFLOAT4 top(0.34f + 0.20f * hueShift, 0.58f, 0.14f, 1.0f);
                top = Mul(top, tone);
                AddBlob(out, XMFLOAT3(x, y + 5.8f * s, z), 3.3f * s, 2.9f * s, 10, 6, sid + 1, bottom, top);
                AddBlob(out, XMFLOAT3(x + 1.9f * s, y + 5.0f * s, z + 0.8f * s), 2.3f * s, 2.1f * s, 8, 5, sid + 2, bottom, top);
                AddBlob(out, XMFLOAT3(x - 1.6f * s, y + 5.3f * s, z - 1.0f * s), 2.2f * s, 2.0f * s, 8, 5, sid + 3, bottom, top);
            }
        }
    }

    void BuildMesh()
    {
        std::vector<VERTEX_3D> verts;
        verts.reserve(200000);

        // ---- 丘の地形（地面の外側だけ）----
        const int N = (int)(TERRAIN_HALF * 2.0f / CELL);
        for (int j = 0; j < N; ++j) {
            for (int i = 0; i < N; ++i) {
                float x0 = -TERRAIN_HALF + CELL * i,       x1 = x0 + CELL;
                float z0 =  TERRAIN_HALF - CELL * j,       z1 = z0 - CELL;   // z0 が上(奥), z1 が手前
                // 全体が地面の内側にあるマスは作らない（ground は別オブジェクト）
                if ((std::max)(fabsf(x0), fabsf(x1)) <= GROUND_HALF && (std::max)(fabsf(z0), fabsf(z1)) <= GROUND_HALF) continue;

                auto mk = [&](float x, float z) {
                    float y = HeightAt(x, z);
                    XMFLOAT3 n = NormalAt(x, z);
                    return MakeV(XMFLOAT3(x, y, z), n, TerrainColor(x, z, y, n));
                };
                VERTEX_3D a = mk(x0, z0), b = mk(x1, z0), c = mk(x0, z1), d = mk(x1, z1);
                // Field と同じ向き（上から見て時計回り）
                verts.push_back(a); verts.push_back(b); verts.push_back(c);
                verts.push_back(b); verts.push_back(d); verts.push_back(c);
            }
        }

        // ---- 柵の外の芝の敷物（石畳の上に薄く敷く。草の刃の下地になり、石畳が透けて見えるのを防ぐ）----
        {
            const float CARPET_INNER = 22.5f;            // 柵(21.2)の少し外から
            const float CARPET_Y     = GROUND_Y + 0.015f; // 地面の 1.5cm 上
            const float C = 2.5f;                        // 敷物の1マス
            const int M = (int)(GROUND_HALF * 2.0f / C);
            for (int j = 0; j < M; ++j) {
                for (int i = 0; i < M; ++i) {
                    float x0 = -GROUND_HALF + C * i, x1 = x0 + C;
                    float z0 =  GROUND_HALF - C * j, z1 = z0 - C;
                    if ((std::max)(fabsf(x0), fabsf(x1)) <= CARPET_INNER && (std::max)(fabsf(z0), fabsf(z1)) <= CARPET_INNER) continue;
                    auto mk = [&](float x, float z) {
                        float v = Fbm(x * 0.09f + 3.0f, z * 0.09f + 9.0f);
                        XMFLOAT4 col(0.14f + 0.10f * v, 0.32f + 0.10f * v, 0.09f + 0.03f * v, 1.0f);
                        return MakeV(XMFLOAT3(x, CARPET_Y, z), XMFLOAT3(0, 1, 0), col);
                    };
                    VERTEX_3D a = mk(x0, z0), b = mk(x1, z0), c = mk(x0, z1), d = mk(x1, z1);
                    verts.push_back(a); verts.push_back(b); verts.push_back(c);
                    verts.push_back(b); verts.push_back(d); verts.push_back(c);
                }
            }
        }

        // ---- 木 ----
        BuildTrees(verts);

        g_VertexCount = (int)verts.size();
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.ByteWidth = sizeof(VERTEX_3D) * g_VertexCount;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem = verts.data();
        if (g_VB) { g_VB->Release(); g_VB = nullptr; }
        Renderer::GetDevice()->CreateBuffer(&bd, &sd, &g_VB);
        g_Built = (g_VB != nullptr);
    }
}

void HillTerrain::Init()
{
    ID3D11Device* device = Renderer::GetDevice();
    if (!device) return;

    Renderer::CreateVertexShader(&g_VS, &g_IL, Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(&g_PS, Renderer::ResolveShaderPath("terrain_ps.cso").c_str());

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;       // 木の向きに依らず描く（法線は明示的に外向きにしてある）
    rd.DepthClipEnable = TRUE;
    device->CreateRasterizerState(&rd, &g_Rs);
}

void HillTerrain::Uninit()
{
    if (g_Rs) { g_Rs->Release(); g_Rs = nullptr; }
    if (g_IL) { g_IL->Release(); g_IL = nullptr; }
    if (g_PS) { g_PS->Release(); g_PS = nullptr; }
    if (g_VS) { g_VS->Release(); g_VS = nullptr; }
    if (g_VB) { g_VB->Release(); g_VB = nullptr; }
    g_Built = false;
}

void HillTerrain::Clear()
{
    g_Enabled = false;
}

void HillTerrain::SetEnabled(bool enable)
{
    g_Enabled = enable;
    if (enable && !g_Built && g_VS && g_PS) BuildMesh();   // 初回だけ生成
}

void HillTerrain::Draw()
{
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    if (!g_Enabled || !g_Built || !ctx || !g_VB || !g_VS || !g_PS || !g_IL || !g_Rs) return;

    ID3D11RasterizerState* oldRs = nullptr;
    ctx->RSGetState(&oldRs);

    Renderer::SetWorldMatrix(XMMatrixIdentity());
    UINT stride = sizeof(VERTEX_3D), offset = 0;
    ctx->IASetVertexBuffers(0, 1, &g_VB, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(g_IL);
    ctx->VSSetShader(g_VS, nullptr, 0);
    ctx->PSSetShader(g_PS, nullptr, 0);
    ctx->RSSetState(g_Rs);

    ctx->Draw(g_VertexCount, 0);

    ctx->RSSetState(oldRs);
    if (oldRs) oldRs->Release();
}
