#include "obj_model.h"
#include <fstream>
#include <sstream>
#include <iostream>

using namespace DirectX;

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
    m_VertexCount = 0;
}

// =================================================================
// MTLマテリアル読み込み
// =================================================================
void ObjModel::LoadMtl(const std::string& mtlPath, std::unordered_map<std::string, XMFLOAT4>& materials)
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
            char mbuf[128];
            sprintf_s(mbuf, "[ObjModel] Mat: %s -> Kd(%.2f, %.2f, %.2f)\n", currentMatName.c_str(), r, g, b);
            OutputDebugStringA(mbuf);
        }
    }
}

// =================================================================
// OBJファイル読み込み
// =================================================================
bool ObjModel::Load(const std::string& objPath, const std::string& mtlPath)
{
    Uninit();

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

    std::unordered_map<std::string, XMFLOAT4> materials;
    if (!mtlPath.empty()) {
        LoadMtl(mtlPath, materials);
    }

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT2> texCoords;
    std::vector<XMFLOAT3> normals;
    std::vector<VERTEX_3D> vertices;

    XMFLOAT4 currentDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string tag;
        iss >> tag;

        if (tag == "mtllib" && mtlPath.empty()) {
            std::string mtlFileName;
            iss >> mtlFileName;
            LoadMtl(dir + mtlFileName, materials);
        } else if (tag == "usemtl") {
            std::string matName;
            iss >> matName;
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
        OutputDebugStringA("[ObjModel] CreateBuffer failed!\n");
        return false;
    }

    // 両面描画（カリングなし）ラスタライザーステートを作成
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    Renderer::GetDevice()->CreateRasterizerState(&rd, &m_RasterizerStateNoCull);

    char buf[256];
    sprintf_s(buf, "[ObjModel] Successfully loaded: %s (Vertices: %d)\n", objPath.c_str(), m_VertexCount);
    OutputDebugStringA(buf);

    return true;
}

// =================================================================
// 描画処理
// =================================================================
void ObjModel::Draw(const XMMATRIX& worldMatrix)
{
    if (!m_VertexBuffer || m_VertexCount == 0) return;
    if (Renderer::IsOutlineMode() || Renderer::IsShadowMode()) return;

    Renderer::SetWorldMatrix(worldMatrix);

    // テクスチャ無効で頂点カラー（Diffuse）をライティング計算に反映
    ID3D11ShaderResourceView* nullSRV = nullptr;
    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &nullSRV);
    Renderer::GetDeviceContext()->PSSetShaderResources(1, 1, &nullSRV);

    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Specular = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
    material.Shininess = 20.0f;
    material.TextureEnable = FALSE;
    Renderer::SetMaterial(material);

    // キューブ共通のシェーダー・定数バッファ・パイプラインステートをセット
    Renderer::SetupCubeDraw();

    // カリング無効化（両面描画）ステートを設定
    ID3D11RasterizerState* prevRS = nullptr;
    if (m_RasterizerStateNoCull) {
        Renderer::GetDeviceContext()->RSGetState(&prevRS);
        Renderer::GetDeviceContext()->RSSetState(m_RasterizerStateNoCull);
    }

    // 頂点バッファを自前のOBJ頂点バッファに差し替えて描画
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->Draw(m_VertexCount, 0);

    // ラスタライザーステートを復元
    if (prevRS) {
        Renderer::GetDeviceContext()->RSSetState(prevRS);
        prevRS->Release();
    }

    // 後続処理（他オブジェクトやキューブの描画）のために標準キューブVBを再バインド
    Renderer::SetupCubeDraw();
}
