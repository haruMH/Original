#include "field.h"
#include "renderer.h"
#include "resource_manager.h"
#include "wall.h"
#include "manager.h"
#include "camera.h"
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

            VERTEX_3D v[4];
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
#ifdef NDEBUG
    m_Texture    = ResourceManager::GetTexture("Assets/texture/grid.png");
    m_SkyTexture = ResourceManager::GetTexture("Assets/texture/sky.png"); // 反射フォールバック用
#else
    m_Texture    = ResourceManager::GetTexture("grid.png");
    m_SkyTexture = ResourceManager::GetTexture("sky.png"); // 反射フォールバック用
#endif

    // シェーダーの読み込み
    // リリースビルド時は Assets/shader/ サブフォルダから読み込む
#ifdef NDEBUG
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "Assets/shader/vertexShader.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "Assets/shader/pixelShader.cso");
#else
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "vertexShader.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "pixelShader.cso");
#endif
}

void Field::Uninit()
{
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

    // シーン内の Wall からエフェクトタイプとパラメータを取得する
    Wall::EffectType activeEffect = Wall::EffectType::Normal;
    float refractionIndex = 0.03f;
    float fresnelPower    = 4.0f;
    XMFLOAT4 highlightColor(1, 1, 1, 1);
    // 水面パラメータ
    float    waterTime    = 0.0f;
    XMFLOAT3 waveParams(0.08f, 3.0f, 2.0f);
    float    waterShininess = 32.0f;
    float    waterFresnel   = 4.0f;
    XMFLOAT4 shallowColor(0.0f, 0.55f, 0.75f, 0.5f);
    XMFLOAT4 deepColor(0.0f, 0.08f, 0.25f, 0.9f);
    XMFLOAT2 scroll1(0.03f, 0.01f);
    XMFLOAT2 scroll2(-0.02f, 0.04f);
    Wall* activeWall = nullptr;

    for (Wall* wall : Manager::GetGameObjects<Wall>()) {
        activeEffect    = wall->GetEffectType();
        refractionIndex = wall->GetRefractionIndex();
        fresnelPower    = wall->GetRefractFresnelPower();
        highlightColor  = wall->GetRefractHighlightColor();
        waterTime       = wall->GetWaterTime();
        waveParams      = wall->GetWaveParams();
        waterShininess  = wall->GetWaterShininess();
        waterFresnel    = wall->GetWaterFresnelPower();
        shallowColor    = wall->GetWaterColorShallow();
        deepColor       = wall->GetWaterColorDeep();
        scroll1         = wall->GetScrollSpeed1();
        scroll2         = wall->GetScrollSpeed2();
        activeWall      = wall;
        break;
    }

    if (activeEffect == Wall::EffectType::Refraction)
    {
        // Wall ブロックを表示（鏡ブロックとして見せる）
        if (activeWall) activeWall->SetVisible(true);

        // カメラ座標を取得して鏡面反射シェーダーに渡す
        XMFLOAT3 camPos = g_Camera ? g_Camera->GetPosition() : XMFLOAT3(0, 5, 0);
        Renderer::DrawFieldWithRefractShader(
            m_VertexBuffer, m_VertexCount, m_VertexLayout, m_VertexShader,
            m_Texture, refractionIndex, fresnelPower, highlightColor,
            waterTime, camPos, m_SkyTexture); // スカイテクスチャをフォールバックとして渡す
    }
    else if (activeEffect == Wall::EffectType::Water)
    {
        // 水面モード: Wall ブロックを非表示にして地面全体を水面にする
        if (activeWall) activeWall->SetVisible(false);

        Renderer::DrawFieldWithWaterShader(
            m_VertexBuffer, m_VertexCount, m_VertexLayout, m_VertexShader,
            m_Texture, m_Texture,
            waterTime, waveParams, waterShininess, waterFresnel,
            shallowColor, deepColor, scroll1, scroll2);
    }
    else
    {
        // 通常モード: Wall ブロックを表示して通常の石畳を描画
        if (activeWall) activeWall->SetVisible(true);

        ID3D11DeviceContext* deviceContext = Renderer::GetDeviceContext();
        UINT stride = sizeof(VERTEX_3D);
        UINT offset = 0;
        deviceContext->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
        deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 高密度メッシュは TriangleList で生成
        deviceContext->IASetInputLayout(m_VertexLayout);
        deviceContext->VSSetShader(m_VertexShader, NULL, 0);
        deviceContext->PSSetShader(m_PixelShader, NULL, 0);

        Renderer::SetTexture(m_Texture);

        MATERIAL material;
        ZeroMemory(&material, sizeof(material));
        material.Diffuse       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        material.Ambient       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        material.TextureEnable = TRUE;
        Renderer::SetMaterial(material);

        deviceContext->Draw(m_VertexCount, 0);
    }
}


// =================================================================
// Skybox クラスの実装
// =================================================================
void Skybox::Init()
{
    m_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(200.0f, 200.0f, 200.0f);

#ifdef NDEBUG
    m_Texture = ResourceManager::GetTexture("Assets/texture/sky.png");
#else
    m_Texture = ResourceManager::GetTexture("sky.png");
#endif
}

void Skybox::Uninit()
{
}

void Skybox::Update()
{
    if (g_Camera) {
        m_Position = g_Camera->GetPosition();
    }
}

void Skybox::Draw()
{
    if (!g_Camera) return;

    XMMATRIX worldMatrix = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
                           XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    Renderer::SetWorldMatrix(worldMatrix);

    Renderer::SetCullMode(false);

    // 一時的にライトを無効化してフォグによる白飛びを回避する
    LIGHT originalLight = Renderer::GetLight();
    LIGHT skyLight = originalLight;
    skyLight.Enable = FALSE;
    Renderer::SetLight(skyLight);

    // Emission を 0 にしてテクスチャ色の白飛びを防ぐ
    // ライトは無効化済みなので Diffuse/Ambient は無視されるが
    // pixelShader の Emission 加算が白飛びを引き起こすため明示的に 0 に設定する
    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient  = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    material.Emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f); // 加算ゼロで白飛びを解消
    material.TextureEnable = TRUE;
    Renderer::SetMaterial(material);

    Renderer::SetupCubeDraw();
    Renderer::SetTexture(m_Texture);
    Renderer::GetDeviceContext()->Draw(36, 0);

    Renderer::SetCullMode(true);

    // ライト設定を復元する
    Renderer::SetLight(originalLight);
}
