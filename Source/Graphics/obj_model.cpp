#include "obj_model.h"
#include <fstream>
#include <sstream>
#include <iostream>

using namespace DirectX;

namespace {
    // 自然物用の軽いピクセルシェーダー（初めて必要になった時に読み込み、全モデルで共有する）
    ID3D11PixelShader* g_FoliagePS = nullptr;
    bool               g_FoliageTried = false;
    bool               g_FoliageHidden = false;
    // 法線マップ対応のピクセルシェーダー（中世の家など。全モデルで共有）
    ID3D11PixelShader* g_NormalMapPS = nullptr;
    bool               g_NormalMapTried = false;

    ID3D11PixelShader* GetNormalMapPS()
    {
        if (!g_NormalMapTried) {
            g_NormalMapTried = true;
            Renderer::CreatePixelShader(&g_NormalMapPS, Renderer::ResolveShaderPath("medieval_ps.cso").c_str());
        }
        return g_NormalMapPS;
    }

    ID3D11PixelShader* GetFoliagePS()
    {
        if (!g_FoliageTried) {
            g_FoliageTried = true;
            Renderer::CreatePixelShader(&g_FoliagePS, Renderer::ResolveShaderPath("foliage_ps.cso").c_str());
        }
        return g_FoliagePS;
    }

    // アセットの場所を解決する。実行フォルダ(x64/Release 等)にコピーされていない場合でも、
    // プロジェクト直下の Assets を見に行く（開発中の起動で、コピー漏れがあっても読めるようにする）
    std::string ResolveAssetPath(const std::string& path)
    {
        if (path.empty()) return path;
        { std::ifstream f(path); if (f.is_open()) return path; }
        static const char* kPrefixes[] = { "../", "../../", "../../../" };
        for (const char* p : kPrefixes) {
            std::string cand = std::string(p) + path;
            std::ifstream f(cand);
            if (f.is_open()) return cand;
        }
        return path;
    }
}

void ObjModel::SetFoliageHidden(bool hidden)
{
    g_FoliageHidden = hidden;
}

void ObjModel::ReleaseSharedResources()
{
    if (g_FoliagePS) { g_FoliagePS->Release(); g_FoliagePS = nullptr; }
    g_FoliageTried = false;
    if (g_NormalMapPS) { g_NormalMapPS->Release(); g_NormalMapPS = nullptr; }
    g_NormalMapTried = false;
}

// =================================================================
// コンストラクタ / デストラクタ
// =================================================================
ObjModel::ObjModel()
    : m_VertexBuffer(nullptr)
    , m_VertexCount(0)
{
}

ObjModel::~ObjModel()
{
    Uninit();
}

// =================================================================
// 終了・解放処理
// =================================================================
void ObjModel::Uninit()
{
    if (m_VertexBuffer) {
        m_VertexBuffer->Release();
        m_VertexBuffer = nullptr;
    }
    if (m_RasterizerStateNoCull) {
        m_RasterizerStateNoCull->Release();
        m_RasterizerStateNoCull = nullptr;
    }
    if (m_TextureSRV) {
        m_TextureSRV->Release();
        m_TextureSRV = nullptr;
    }
    for (ID3D11ShaderResourceView* t : m_ExtraTextures) {
        if (t) t->Release();
    }
    m_ExtraTextures.clear();
    m_SubMeshes.clear();
    m_VertexCount = 0;
}

// =================================================================
// MTLマテリアル読み込み
// =================================================================
void ObjModel::LoadMtl(const std::string& mtlPath, std::unordered_map<std::string, XMFLOAT4>& materials, std::string& outTexture,
                       std::unordered_map<std::string, std::string>& matTex)
{
    std::ifstream file(mtlPath);
    if (!file.is_open()) return;

    std::string currentMatName = "";
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string tag;
        iss >> tag;

        if (tag == "newmtl") {
            iss >> currentMatName;
            materials[currentMatName] = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        } else if (tag == "Kd" && !currentMatName.empty()) {
            float r = 1.0f, g = 1.0f, b = 1.0f;
            iss >> r >> g >> b;
            materials[currentMatName] = XMFLOAT4(r, g, b, 1.0f);
            LOG_INFO("[ObjModel] Mat: %s -> Kd(%.2f, %.2f, %.2f)\n", currentMatName.c_str(), r, g, b);
        } else if (tag == "map_Kd") {
            iss >> outTexture;
            if (!currentMatName.empty()) matTex[currentMatName] = outTexture;
        } else if ((tag == "map_Bump" || tag == "bump") && !currentMatName.empty()) {
            // "map_Bump -bm 1.0 path" のようにオプションが付く場合があるので、最後の単語をファイル名とする
            std::string w, last;
            while (iss >> w) last = w;
            if (!last.empty()) m_MatNormal[currentMatName] = last;
        }
    }
}

// =================================================================
// OBJファイル読み込み
// =================================================================
bool ObjModel::Load(const std::string& objPathIn, const std::string& mtlPathIn)
{
    Uninit();

    const std::string objPath = ResolveAssetPath(objPathIn);
    const std::string mtlPath = ResolveAssetPath(mtlPathIn);
    m_UseFoliageShader = (objPath.find("/nature/") != std::string::npos);
    m_UseNormalMap = (objPath.find("/medieval/") != std::string::npos);
    m_MatNormal.clear();

    std::ifstream file(objPath);
    if (!file.is_open()) {
        return false;
    }

    // ディレクトリパス抽出
    std::string dir = "";
    size_t lastSlash = objPath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        dir = objPath.substr(0, lastSlash + 1);
    }

    std::string textureFileName = "";
    std::unordered_map<std::string, XMFLOAT4> materials;
    std::unordered_map<std::string, std::string> matTex;
    if (!mtlPath.empty()) {
        LoadMtl(mtlPath, materials, textureFileName, matTex);
    }

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT2> texCoords;
    std::vector<XMFLOAT3> normals;
    std::vector<VERTEX_3D> vertices;

    XMFLOAT4 currentDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    // usemtl の切り替わりごとに頂点範囲を記録する（マテリアルごとにテクスチャを切り替えるため）
    struct MatRange { std::string name; size_t start; size_t count; };
    std::vector<MatRange> ranges;
    std::string curMatName;
    size_t subStart = 0;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string tag;
        iss >> tag;

        if (tag == "mtllib" && mtlPath.empty()) {
            std::string mtlFileName;
            iss >> mtlFileName;
            LoadMtl(dir + mtlFileName, materials, textureFileName, matTex);
        } else if (tag == "usemtl") {
            std::string matName;
            iss >> matName;
            if (vertices.size() > subStart) {
                ranges.push_back({ curMatName, subStart, vertices.size() - subStart });
            }
            subStart = vertices.size();
            curMatName = matName;
            auto it = materials.find(matName);
            if (it != materials.end()) {
                currentDiffuse = it->second;
            } else {
                currentDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            }
        } else if (tag == "v") {
            float x, y, z;
            iss >> x >> y >> z;
            positions.push_back(XMFLOAT3(x, y, z));
        } else if (tag == "vt") {
            float u, v;
            iss >> u >> v;
            texCoords.push_back(XMFLOAT2(u, 1.0f - v));
        } else if (tag == "vn") {
            float nx, ny, nz;
            iss >> nx >> ny >> nz;
            normals.push_back(XMFLOAT3(nx, ny, nz));
        } else if (tag == "f") {
            std::vector<std::string> faceTokens;
            std::string token;
            while (iss >> token) {
                faceTokens.push_back(token);
            }

            if (faceTokens.size() < 3) continue;

            auto parseVertex = [&](const std::string& str) -> VERTEX_3D {
                VERTEX_3D v = {};
                v.Diffuse = currentDiffuse;
                v.Tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);

                int vIdx = 0, vtIdx = 0, vnIdx = 0;
                size_t firstSlash = str.find('/');
                if (firstSlash == std::string::npos) {
                    vIdx = std::stoi(str);
                } else {
                    vIdx = std::stoi(str.substr(0, firstSlash));
                    size_t secondSlash = str.find('/', firstSlash + 1);
                    if (secondSlash == std::string::npos) {
                        vtIdx = std::stoi(str.substr(firstSlash + 1));
                    } else {
                        if (secondSlash > firstSlash + 1) {
                            vtIdx = std::stoi(str.substr(firstSlash + 1, secondSlash - firstSlash - 1));
                        }
                        if (secondSlash + 1 < str.length()) {
                            vnIdx = std::stoi(str.substr(secondSlash + 1));
                        }
                    }
                }

                // 1-based to 0-based
                if (vIdx > 0 && vIdx <= (int)positions.size()) {
                    v.Position = positions[vIdx - 1];
                }
                if (vtIdx > 0 && vtIdx <= (int)texCoords.size()) {
                    v.TexCoord = texCoords[vtIdx - 1];
                }
                if (vnIdx > 0 && vnIdx <= (int)normals.size()) {
                    v.Normal = normals[vnIdx - 1];
                } else {
                    v.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
                }

                return v;
            };

            // ファン分割で三角ポリゴン化 (0, i, i+1)
            VERTEX_3D v0 = parseVertex(faceTokens[0]);
            for (size_t i = 1; i + 1 < faceTokens.size(); ++i) {
                VERTEX_3D v1 = parseVertex(faceTokens[i]);
                VERTEX_3D v2 = parseVertex(faceTokens[i + 1]);

                vertices.push_back(v0);
                vertices.push_back(v1);
                vertices.push_back(v2);
            }
        }
    }

    if (vertices.empty()) {
        return false;
    }
    if (vertices.size() > subStart) {
        ranges.push_back({ curMatName, subStart, vertices.size() - subStart });
    }

    m_VertexCount = static_cast<int>(vertices.size());

    // 頂点バッファ作成
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(VERTEX_3D) * m_VertexCount;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = vertices.data();

    HRESULT hr = Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);
    if (FAILED(hr)) {
        LOG_INFO("[ObjModel] CreateBuffer failed!\n");
        return false;
    }

    // 両面描画（カリングなし）ラスタライザーステートを作成
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    Renderer::GetDevice()->CreateRasterizerState(&rd, &m_RasterizerStateNoCull);

    // テクスチャがある場合はロード
    if (!textureFileName.empty()) {
        std::string texPath = dir + textureFileName;
        std::ifstream tf(texPath);
        if (!tf.is_open()) {
            texPath = ResolveAssetPath("Assets/model/Textures/colormap.png");
        }
        Renderer::CreateTexture(texPath.c_str(), &m_TextureSRV);
    }

    // マテリアルごとのテクスチャを解決して、描画範囲（SubMesh）を作る。
    // 個別の map_Kd が無い／読めないマテリアルは、従来どおりモデル共通のテクスチャを使う（既存モデルの見た目は変えない）
    {
        std::unordered_map<std::string, ID3D11ShaderResourceView*> loaded;
        for (const MatRange& r : ranges) {
            SubMesh sm;
            sm.start = static_cast<int>(r.start);
            sm.count = static_cast<int>(r.count);
            sm.tex = m_TextureSRV;

            auto it = matTex.find(r.name);
            if (it != matTex.end() && it->second != textureFileName) {
                auto cached = loaded.find(it->second);
                if (cached != loaded.end()) {
                    sm.tex = cached->second;
                } else {
                    std::string p = dir + it->second;
                    std::ifstream tf2(p);
                    if (tf2.is_open()) {
                        ID3D11ShaderResourceView* srv = nullptr;
                        Renderer::CreateTexture(p.c_str(), &srv);
                        if (srv) {
                            m_ExtraTextures.push_back(srv);
                            loaded[it->second] = srv;
                            sm.tex = srv;
                        }
                    }
                }
            }
            // 法線マップ（中世の家など）。同じファイルは1回だけ読む
            if (m_UseNormalMap) {
                auto nIt = m_MatNormal.find(r.name);
                if (nIt != m_MatNormal.end()) {
                    auto cachedN = loaded.find("N:" + nIt->second);
                    if (cachedN != loaded.end()) {
                        sm.nrm = cachedN->second;
                    } else {
                        std::string np = dir + nIt->second;
                        std::ifstream nf(np);
                        if (nf.is_open()) {
                            ID3D11ShaderResourceView* nsrv = nullptr;
                            Renderer::CreateTexture(np.c_str(), &nsrv);   // 同名の .dds(BC7)があればそれを読む
                            if (nsrv) {
                                m_ExtraTextures.push_back(nsrv);
                                loaded["N:" + nIt->second] = nsrv;
                                sm.nrm = nsrv;
                            }
                        }
                    }
                }
            }
            m_SubMeshes.push_back(sm);
        }
        if (m_SubMeshes.empty()) {
            SubMesh sm;
            sm.start = 0;
            sm.count = m_VertexCount;
            sm.tex = m_TextureSRV;
            m_SubMeshes.push_back(sm);
        }
    }

    LOG_INFO("[ObjModel] Successfully loaded: %s (Vertices: %d, Tex: %s)\n",
        objPath.c_str(), m_VertexCount, m_TextureSRV ? "Yes" : "No");

    return true;
}

// =================================================================
// 描画処理
// =================================================================
void ObjModel::Draw(const XMMATRIX& worldMatrix)
{
    if (!m_VertexBuffer || m_VertexCount == 0) return;
    if (Renderer::IsOutlineMode() || Renderer::IsShadowMode()) return;
    if (m_UseFoliageShader && g_FoliageHidden) return;

    Renderer::SetWorldMatrix(worldMatrix);

    // キューブ共通のシェーダー・定数バッファ・パイプラインステートをセット
    Renderer::SetupCubeDraw();

    // カリング無効化（両面描画）ステートを設定
    ID3D11RasterizerState* prevRS = nullptr;
    if (m_RasterizerStateNoCull) {
        Renderer::GetDeviceContext()->RSGetState(&prevRS);
        Renderer::GetDeviceContext()->RSSetState(m_RasterizerStateNoCull);
    }

    // 自然物は影サンプリングなどを省いた軽いシェーダーで描く（標準シェーダーは後でまた設定される）
    if (m_UseFoliageShader) {
        if (ID3D11PixelShader* fps = GetFoliagePS()) {
            Renderer::GetDeviceContext()->PSSetShader(fps, nullptr, 0);
        }
    }

    // 中世の家などは法線マップ対応のシェーダーで描く（無ければ標準のまま）
    if (m_UseNormalMap) {
        if (ID3D11PixelShader* nps = GetNormalMapPS()) {
            Renderer::GetDeviceContext()->PSSetShader(nps, nullptr, 0);
        }
    }

    // 頂点バッファを自前のOBJ頂点バッファに差し替えて描画
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

    // マテリアル（テクスチャ）ごとに描画。葉などの透明部分は、ピクセルシェーダーで切り抜く（アルファテスト）
    ID3D11ShaderResourceView* nullSRV = nullptr;
    for (const SubMesh& sm : m_SubMeshes) {
        if (sm.tex) {
            ID3D11ShaderResourceView* t = sm.tex;
            Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &t);
        } else {
            Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &nullSRV);
        }
        {
            ID3D11ShaderResourceView* n = sm.nrm;   // 法線マップが無いときは null（medieval_ps 側で面の法線のまま）
            Renderer::GetDeviceContext()->PSSetShaderResources(1, 1, n ? &n : &nullSRV);
        }

        MATERIAL material;
        ZeroMemory(&material, sizeof(material));
        material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        material.Specular = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
        material.Shininess = 20.0f;
        material.TextureEnable = (sm.tex != nullptr) ? TRUE : FALSE;
        material.Dummy = (sm.tex != nullptr) ? 0.5f : 0.0f;   // アルファテストのしきい値
        Renderer::SetMaterial(material);

        Renderer::GetDeviceContext()->Draw(sm.count, sm.start);
    }

    // ラスタライザーステートを復元
    if (prevRS) {
        Renderer::GetDeviceContext()->RSSetState(prevRS);
        prevRS->Release();
    }

    // 後続処理（他オブジェクトやキューブの描画）のために標準キューブVBを再バインド
    Renderer::SetupCubeDraw();
}
