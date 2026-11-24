#include "field.h"
#include "renderer.h"
#include "world_look.h"
#include "resource_manager.h"
#include "wall.h"
#include "manager.h"
#include "camera.h"
#include "input.h"
#include <vector>


void Field::Init()
{
    m_Position = XMFLOAT3(0.0f, -1.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);

    // 高密度グリッドメッシュを生成（波の頂点変位を見えるようにするため）
    // 50x50 分割 = 100x100 頂点 = 99x99 クワッド = 99x99x2 三角形
    const int GRID_DIV  = 99; // 分割数
    const float SIZE    = 50.0f; // 片側の広さ
    const float UV_TILE = 25.0f; // UV タイリング数

    int cols = GRID_DIV + 1; // 横頂点数
    int rows = GRID_DIV + 1; // 縦頂点数
    int triCount = GRID_DIV * GRID_DIV * 2;
    m_VertexCount = triCount * 3;

    std::vector<VERTEX_3D> vertices;
    vertices.reserve(m_VertexCount);

    for (int z = 0; z < GRID_DIV; ++z)
    {
        for (int x = 0; x < GRID_DIV; ++x)
        {
            // グリッドの4頂点を計算
            float x0 = -SIZE + (2.0f * SIZE / GRID_DIV) * x;
            float x1 = -SIZE + (2.0f * SIZE / GRID_DIV) * (x + 1);
            float z0 =  SIZE - (2.0f * SIZE / GRID_DIV) * z;
            float z1 =  SIZE - (2.0f * SIZE / GRID_DIV) * (z + 1);
            float u0 = UV_TILE / GRID_DIV * x;
            float u1 = UV_TILE / GRID_DIV * (x + 1);
            float v0 = UV_TILE / GRID_DIV * z;
            float v1 = UV_TILE / GRID_DIV * (z + 1);

            auto mkv = [](float px, float pz, float u, float v) -> VERTEX_3D {
                VERTEX_3D vt;
                vt.Position = XMFLOAT3(px, 0.0f, pz);
                vt.Normal   = XMFLOAT3(0.0f, 1.0f, 0.0f);
                vt.Diffuse  = XMFLOAT4(1,1,1,1);
                vt.TexCoord = XMFLOAT2(u, v);
                return vt;
            };
            // 三角形1（左上 → 右上 → 左下）
            vertices.push_back(mkv(x0, z0, u0, v0));
            vertices.push_back(mkv(x1, z0, u1, v0));
            vertices.push_back(mkv(x0, z1, u0, v1));
            // 三角形2（右上 → 右下 → 左下）
            vertices.push_back(mkv(x1, z0, u1, v0));
            vertices.push_back(mkv(x1, z1, u1, v1));
            vertices.push_back(mkv(x0, z1, u0, v1));
        }
    }

    D3D11_BUFFER_DESC bd{};
    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.ByteWidth      = sizeof(VERTEX_3D) * m_VertexCount;
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertices.data();
    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

    // テクスチャの読み込み
    // リリースビルド時は Assets/texture/ サブフォルダから読み込む
    m_Texture    = ResourceManager::GetTexture("grid.png");
    m_SkyTexture = ResourceManager::GetTexture("sky.png"); // 反射フォールバック用

    // シェーダーの読み込み（2-1 対応: Renderer::ResolveShaderPath でパス解決を一元化）
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(&m_PixelShader, Renderer::ResolveShaderPath("pixelShader.cso").c_str());
}

void Field::SetFloorTexture(const char* albedoName, const char* normalName)
{
    ID3D11ShaderResourceView* a = ResourceManager::GetTexture(albedoName);
    if (a) m_Texture = a;
    if (normalName && !m_NormalPS)
    {
        m_NormalTexture = ResourceManager::GetTexture(normalName);
        if (m_NormalTexture)
            Renderer::CreatePixelShader(&m_NormalPS, Renderer::ResolveShaderPath("medieval_ps.cso").c_str());
    }
}

void Field::Uninit()
{
    if (m_NormalPS)     m_NormalPS->Release();
    if (m_VertexLayout) m_VertexLayout->Release();
    if (m_PixelShader)  m_PixelShader->Release();
    if (m_VertexShader) m_VertexShader->Release();
    if (m_VertexBuffer) m_VertexBuffer->Release();
}

void Field::Update()
{
}

void Field::Draw()
{
    XMMATRIX worldMatrix = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
                           XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z) *
                           XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
                           
    Renderer::SetWorldMatrix(worldMatrix);

    ID3D11DeviceContext* deviceContext = Renderer::GetDeviceContext();
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    deviceContext->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext->IASetInputLayout(m_VertexLayout);
    deviceContext->VSSetShader(m_VertexShader, NULL, 0);
    const bool useNormal = (m_NormalPS && m_NormalTexture);
    deviceContext->PSSetShader(useNormal ? m_NormalPS : m_PixelShader, NULL, 0);

    Renderer::SetTexture(m_Texture);
    if (useNormal) deviceContext->PSSetShaderResources(1, 1, &m_NormalTexture);

    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Specular      = useNormal ? XMFLOAT4(0.08f, 0.08f, 0.1f, 1.0f) : XMFLOAT4(0.35f, 0.35f, 0.45f, 1.0f); // 石畳は光沢を抑える
    material.Shininess     = useNormal ? 12.0f : 36.0f;                                                          // 磨かれたアリーナ風の光沢
    material.TextureEnable = TRUE;
    Renderer::SetMaterial(material);

    deviceContext->Draw(m_VertexCount, 0);
}


// =================================================================
// =================================================================
// Skybox / Skydome クラスの実装
// =================================================================
void Skybox::CreateDomeMesh(int slices, int stacks, float radius)
{
    std::vector<VERTEX_3D> vertices;
    std::vector<UINT> indices;

    // 緯度: -PI/2 (最下部) から +PI/2 (天頂)
    for (int i = 0; i <= stacks; ++i)
    {
        float lat = -XM_PIDIV2 + XM_PI * ((float)i / stacks);
        float sinLat = sinf(lat);
        float cosLat = cosf(lat);

        // テクスチャ V: 天頂 (lat = +PI/2) -> 0.0, 水平 (lat = 0) -> 0.82, 最下部 (lat = -PI/2) -> 1.0
        float v = (lat >= 0.0f)
            ? 0.82f * (1.0f - sinLat)
            : 0.82f + 0.18f * (-sinLat);

        for (int j = 0; j <= slices; ++j)
        {
            // 経度: 0 ～ 2*PI
            float lon = XM_2PI * ((float)j / slices);
            float sinLon = sinf(lon);
            float cosLon = cosf(lon);

            VERTEX_3D vt{};
            vt.Position = XMFLOAT3(
                radius * cosLat * sinLon,
                radius * sinLat,
                radius * cosLat * cosLon
            );

            // 法線
            vt.Normal   = XMFLOAT3(0.0f, -1.0f, 0.0f);
            vt.Diffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            vt.TexCoord = XMFLOAT2((float)j / slices, v);
            vt.Tangent  = XMFLOAT3(1.0f, 0.0f, 0.0f);

            vertices.push_back(vt);
        }
    }

    // 外向き面のインデックス構成（Renderer::SetCullMode(false) の表面カリングにより内側が描画される）
    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            int row1 = i * (slices + 1);
            int row2 = (i + 1) * (slices + 1);

            indices.push_back(row1 + j);
            indices.push_back(row1 + j + 1);
            indices.push_back(row2 + j);

            indices.push_back(row1 + j + 1);
            indices.push_back(row2 + j + 1);
            indices.push_back(row2 + j);
        }
    }

    m_IndexCount = (int)indices.size();

    // 頂点バッファ作成
    D3D11_BUFFER_DESC vbd{};
    vbd.Usage     = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(VERTEX_3D) * (UINT)vertices.size();
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vsd{};
    vsd.pSysMem = vertices.data();
    Renderer::GetDevice()->CreateBuffer(&vbd, &vsd, &m_VertexBuffer);

    // インデックスバッファ作成
    D3D11_BUFFER_DESC ibd{};
    ibd.Usage     = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(UINT) * (UINT)indices.size();
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA isd{};
    isd.pSysMem = indices.data();
    Renderer::GetDevice()->CreateBuffer(&ibd, &isd, &m_IndexBuffer);
}

void Skybox::Init()
{
    m_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(450.0f, 450.0f, 450.0f); // 巨大なスカイドーム

    m_RotationAngle = 0.0f;
    m_RotationSpeed = 0.006f; // ゆっくり自転 (約100秒で1周)

    // sky is procedural (sky_ps): no textures, no seam. time of day comes from WorldLook
    m_CurrentType = SkyType::Day;

    // ドームメッシュ生成 (slices=48, stacks=24, radius=1.0f)
    CreateDomeMesh(48, 24, 1.0f);

    // シェーダーの読み込み
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(&m_PixelShader, Renderer::ResolveShaderPath("sky_ps.cso").c_str());

    D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth = sizeof(float) * 4 * 5;
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Renderer::GetDevice()->CreateBuffer(&cbd, nullptr, &m_SkyBuffer);
}

void Skybox::Uninit()
{
    if (m_SkyBuffer)    { m_SkyBuffer->Release();    m_SkyBuffer = nullptr; }
    if (m_VertexLayout) { m_VertexLayout->Release(); m_VertexLayout = nullptr; }
    if (m_PixelShader)  { m_PixelShader->Release();  m_PixelShader = nullptr; }
    if (m_VertexShader) { m_VertexShader->Release(); m_VertexShader = nullptr; }
    if (m_IndexBuffer)  { m_IndexBuffer->Release();  m_IndexBuffer = nullptr; }
    if (m_VertexBuffer) { m_VertexBuffer->Release(); m_VertexBuffer = nullptr; }
}

void Skybox::SetSkyType(SkyType type)
{
    m_CurrentType = type;
    WorldLook::SetTarget((int)type, true);
}

void Skybox::Update()
{
    if (g_Camera) {
        m_Position = g_Camera->GetPosition();
    }

    // ゆっくりと自転（空・雲の流れるアニメーション演出）
    m_RotationAngle += m_RotationSpeed * (1.0f / 60.0f);
    if (m_RotationAngle >= XM_2PI) {
        m_RotationAngle -= XM_2PI;
    }
    m_Rotation.y = m_RotationAngle;

    // キー入力による空の切り替え ([7]:青空, [8]:夕暮れ, [9]:夜空)
    if (Input::GetKeyTrigger('6') || Input::GetKeyTrigger(VK_NUMPAD6)) {
        WorldLook::GoToDawn();
    }
    else if (Input::GetKeyTrigger('7') || Input::GetKeyTrigger(VK_NUMPAD7)) {
        m_CurrentType = SkyType::Day;
        WorldLook::SetTarget(0, false);
    }
    else if (Input::GetKeyTrigger('8') || Input::GetKeyTrigger(VK_NUMPAD8)) {
        m_CurrentType = SkyType::Sunset;
        WorldLook::SetTarget(1, false);
    }
    else if (Input::GetKeyTrigger('9') || Input::GetKeyTrigger(VK_NUMPAD9)) {
        m_CurrentType = SkyType::Night;
        WorldLook::SetTarget(2, false);
    }

    // light/fog follow the sky smoothly
    WorldLook::Update();
}

void Skybox::Draw()
{
    if (!g_Camera || !m_VertexBuffer || !m_IndexBuffer) return;

    XMMATRIX worldMatrix = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
                           XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z) *
                           XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    Renderer::SetWorldMatrix(worldMatrix);

    Renderer::SetCullMode(false); // CullFront -> ドームの内側が見える

    // 一時的にライトを無効化してフォグによる白飛びを回避する
    LIGHT originalLight = Renderer::GetLight();
    LIGHT skyLight = originalLight;
    skyLight.Enable = FALSE;
    Renderer::SetLight(skyLight);

    // Emission を 0 にしてテクスチャ色の白飛びを防ぐ
    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Specular      = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    material.Emission      = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    material.TextureEnable = TRUE;
    Renderer::SetMaterial(material);

    ID3D11DeviceContext* dc = Renderer::GetDeviceContext();
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    dc->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
    dc->IASetIndexBuffer(m_IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc->IASetInputLayout(m_VertexLayout);
    dc->VSSetShader(m_VertexShader, NULL, 0);
    dc->PSSetShader(m_PixelShader, NULL, 0);

    // sky constants (b6)
    if (m_SkyBuffer) {
        float cb[20] = {};
        const float* z = WorldLook::GetZenithColor();
        const float* hz = WorldLook::GetHorizonColor();
        const float* sc = WorldLook::GetSunColor();
        float sun[3];
        WorldLook::GetSunDirection(sun);
        cb[0] = z[0];  cb[1] = z[1];  cb[2] = z[2];  cb[3] = WorldLook::GetStarAmount();
        cb[4] = hz[0]; cb[5] = hz[1]; cb[6] = hz[2]; cb[7] = WorldLook::GetCloudBrightness();
        cb[8] = sun[0]; cb[9] = sun[1]; cb[10] = sun[2]; cb[11] = WorldLook::GetElapsedSeconds();
        cb[12] = sc[0]; cb[13] = sc[1]; cb[14] = sc[2];
        XMFLOAT3 cam = g_Camera->GetPosition();
        cb[16] = cam.x; cb[17] = cam.y; cb[18] = cam.z;
        dc->UpdateSubresource(m_SkyBuffer, 0, nullptr, cb, 0, 0);
        dc->PSSetConstantBuffers(6, 1, &m_SkyBuffer);
    }

    dc->DrawIndexed(m_IndexCount, 0, 0);

    Renderer::SetCullMode(true);

    // ライト設定を復元する
    Renderer::SetLight(originalLight);
}
