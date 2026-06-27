#include "render_system.h"
#include "renderer.h"
#include "gameobject.h"
#include "resource_manager.h"
#include "wall.h"
#include <vector>
#include <fstream>
#include <DirectXCollision.h>
#include <algorithm>

// =================================================================
// コンストラクタ / デストラクタ
// =================================================================
RenderSystem::RenderSystem()
{
}

RenderSystem::~RenderSystem()
{
    Uninit();
}

// =================================================================
// 初期化処理
// =================================================================
bool RenderSystem::Init(ID3D11Device* device)
{
    std::ofstream log("debug_log.txt", std::ios::app);
    log << "[RenderSystem::Init] Start. device=" << device << std::endl;

    if (!device)
    {
        log << "[RenderSystem::Init] Error: device is null!" << std::endl;
        return false;
    }

    // 動的インスタンスバッファの作成
    // (書き込み頻度が高いため D3D11_USAGE_DYNAMIC、D3D11_CPU_ACCESS_WRITE を指定)
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.ByteWidth = sizeof(InstanceData) * MAX_INSTANCES;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;

    HRESULT hr = device->CreateBuffer(&bufferDesc, nullptr, &m_InstanceBuffer);
    if (FAILED(hr))
    {
        log << "[RenderSystem::Init] CreateBuffer failed, hr=" << std::hex << hr << std::endl;
        return false;
    }
    log << "[RenderSystem::Init] Instance Buffer created successfully" << std::endl;

    // テクスチャインデックスマップの初期化
    // リリースビルド時は Assets/texture/ サブフォルダのパスを使用する
#ifdef NDEBUG
    m_TextureIndexMap["Assets/texture/enemy.png"]  = 0.0f;
    m_TextureIndexMap["Assets/texture/player.png"] = 1.0f;
    m_TextureIndexMap["Assets/texture/grid.png"]   = 2.0f;
#else
    m_TextureIndexMap["enemy.png"]  = 0.0f;
    m_TextureIndexMap["player.png"] = 1.0f;
    m_TextureIndexMap["grid.png"]   = 2.0f;
#endif

    // シェーダーおよびインプットレイアウトの作成
    bool res = CreateResources(device);
    log << "[RenderSystem::Init] CreateResources result=" << res << std::endl;
    if (!res) return false;

    // テクスチャ配列の生成
    res = CreateTextureArray(device);
    log << "[RenderSystem::Init] CreateTextureArray result=" << res << std::endl;

    return res;
}

// =================================================================
// 解放処理
// =================================================================
void RenderSystem::Uninit()
{
    if (m_InstanceBuffer)
    {
        m_InstanceBuffer->Release();
        m_InstanceBuffer = nullptr;
    }
    if (m_InstancedInputLayout)
    {
        m_InstancedInputLayout->Release();
        m_InstancedInputLayout = nullptr;
    }
    if (m_InstancedVertexShader)
    {
        m_InstancedVertexShader->Release();
        m_InstancedVertexShader = nullptr;
    }
    if (m_InstancedPixelShader)
    {
        m_InstancedPixelShader->Release();
        m_InstancedPixelShader = nullptr;
    }
    if (m_InstancedOutlineInputLayout)
    {
        m_InstancedOutlineInputLayout->Release();
        m_InstancedOutlineInputLayout = nullptr;
    }
    if (m_InstancedOutlineVertexShader)
    {
        m_InstancedOutlineVertexShader->Release();
        m_InstancedOutlineVertexShader = nullptr;
    }
    if (m_TextureArraySRV)
    {
        m_TextureArraySRV->Release();
        m_TextureArraySRV = nullptr;
    }
}

// =================================================================
// シェーダーとインプットレイアウトの作成
// =================================================================
bool RenderSystem::CreateResources(ID3D11Device* device)
{
    std::ofstream log("debug_log.txt", std::ios::app);
    log << "[RenderSystem::CreateResources] Start" << std::endl;

    FILE* f = nullptr;
    
    // -------------------------------------------------------------
    // 1. 頂点シェーダー (instanced_vs.cso) の読み込みと生成
    // リリースビルドは Assets/shader/ サブフォルダから読み込む
    // -------------------------------------------------------------
#ifdef NDEBUG
    fopen_s(&f, "Assets/shader/instanced_vs.cso", "rb");
#else
    fopen_s(&f, "instanced_vs.cso", "rb");
#endif
    if (!f)
    {
        log << "[RenderSystem::CreateResources] Failed to open instanced_vs.cso!" << std::endl;
        return false;
    }
    log << "[RenderSystem::CreateResources] Opened instanced_vs.cso successfully" << std::endl;
    
    fseek(f, 0, SEEK_END);
    long vsSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<unsigned char> vsBuffer(vsSize);
    fread(vsBuffer.data(), 1, vsSize, f);
    fclose(f);
    log << "[RenderSystem::CreateResources] Read instanced_vs.cso, size=" << vsSize << std::endl;

    HRESULT hr = device->CreateVertexShader(vsBuffer.data(), vsSize, nullptr, &m_InstancedVertexShader);
    if (FAILED(hr))
    {
        log << "[RenderSystem::CreateResources] CreateVertexShader failed, hr=" << std::hex << hr << std::endl;
        return false;
    }
    log << "[RenderSystem::CreateResources] Created Vertex Shader successfully" << std::endl;

    // -------------------------------------------------------------
    // 2. インプットレイアウトの作成
    // -------------------------------------------------------------
    D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },

        { "WORLD",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD",    1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD",    2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD",    3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXINDEX", 0, DXGI_FORMAT_R32_FLOAT,          1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, // インスタンスごとのテクスチャインデックスを追加
        { "EMISSIVE", 0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 68, D3D11_INPUT_PER_INSTANCE_DATA, 1 }  // インスタンスごとのエミッシブ（自発光）を追加
    };

    hr = device->CreateInputLayout(inputLayoutDesc, _countof(inputLayoutDesc), vsBuffer.data(), vsSize, &m_InstancedInputLayout);
    if (FAILED(hr))
    {
        log << "[RenderSystem::CreateResources] CreateInputLayout failed, hr=" << std::hex << hr << std::endl;
        return false;
    }
    log << "[RenderSystem::CreateResources] Created Input Layout successfully" << std::endl;

    // -------------------------------------------------------------
    // 3. ピクセルシェーダー (instanced_ps.cso) の読み込みと生成
    // リリースビルドは Assets/shader/ サブフォルダから読み込む
    // -------------------------------------------------------------
#ifdef NDEBUG
    fopen_s(&f, "Assets/shader/instanced_ps.cso", "rb");
#else
    fopen_s(&f, "instanced_ps.cso", "rb");
#endif
    if (!f)
    {
        log << "[RenderSystem::CreateResources] Failed to open instanced_ps.cso!" << std::endl;
        return false;
    }
    log << "[RenderSystem::CreateResources] Opened instanced_ps.cso successfully" << std::endl;

    fseek(f, 0, SEEK_END);
    long psSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<unsigned char> psBuffer(psSize);
    fread(psBuffer.data(), 1, psSize, f);
    fclose(f);
    log << "[RenderSystem::CreateResources] Read pixelShader.cso, size=" << psSize << std::endl;

    hr = device->CreatePixelShader(psBuffer.data(), psSize, nullptr, &m_InstancedPixelShader);
    if (FAILED(hr))
    {
        log << "[RenderSystem::CreateResources] CreatePixelShader failed, hr=" << std::hex << hr << std::endl;
        return false;
    }
    log << "[RenderSystem::CreateResources] Created Pixel Shader successfully" << std::endl;

    // -------------------------------------------------------------
    // 4. インスタンスアウトライン用頂点シェーダー (outline_instanced_vs.cso) の読み込みと生成
    // リリースビルドは Assets/shader/ サブフォルダから読み込む
    // -------------------------------------------------------------
#ifdef NDEBUG
    fopen_s(&f, "Assets/shader/outline_instanced_vs.cso", "rb");
#else
    fopen_s(&f, "outline_instanced_vs.cso", "rb");
#endif
    if (!f)
    {
        log << "[RenderSystem::CreateResources] Failed to open outline_instanced_vs.cso!" << std::endl;
        return false;
    }
    log << "[RenderSystem::CreateResources] Opened outline_instanced_vs.cso successfully" << std::endl;
    
    fseek(f, 0, SEEK_END);
    long ovsSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<unsigned char> ovsBuffer(ovsSize);
    fread(ovsBuffer.data(), 1, ovsSize, f);
    fclose(f);
    log << "[RenderSystem::CreateResources] Read outline_instanced_vs.cso, size=" << ovsSize << std::endl;

    hr = device->CreateVertexShader(ovsBuffer.data(), ovsSize, nullptr, &m_InstancedOutlineVertexShader);
    if (FAILED(hr))
    {
        log << "[RenderSystem::CreateResources] CreateVertexShader for outline failed, hr=" << std::hex << hr << std::endl;
        return false;
    }
    log << "[RenderSystem::CreateResources] Created Instanced Outline Vertex Shader successfully" << std::endl;

    // インプットレイアウトの作成（通常インスタンス用と同じレイアウト定義を流用）
    hr = device->CreateInputLayout(inputLayoutDesc, _countof(inputLayoutDesc), ovsBuffer.data(), ovsSize, &m_InstancedOutlineInputLayout);
    if (FAILED(hr))
    {
        log << "[RenderSystem::CreateResources] CreateInputLayout for outline failed, hr=" << std::hex << hr << std::endl;
        return false;
    }
    log << "[RenderSystem::CreateResources] Created Instanced Outline Input Layout successfully" << std::endl;

    return true;
}

// =================================================================
// インスタンス描画の実行
// =================================================================
void RenderSystem::RenderCubeInstances(ID3D11DeviceContext* context, const std::vector<GameObject*>& objects, RenderPass pass)
{
    // 1. 視錐台の構築（通常描画・アウトライン描画時のみカリングを行う。シャドウマップ描画時は画面外の影も落とすためスキップ）
    bool useCulling = (pass != RenderPass::Shadow);
    DirectX::BoundingFrustum frustum;
    if (useCulling)
    {
        DirectX::BoundingFrustum::CreateFromMatrix(frustum, Renderer::GetProjectionMatrix());
        DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, Renderer::GetViewMatrix());
        frustum.Transform(frustum, invView);
    }

    // 2. シーン上の GameObject を走査し、インスタンシング描画対象のオブジェクトをフラットなリストに格納する
    std::vector<GameObject*> drawList;

    for (GameObject* obj : objects)
    {
        if (!obj) continue;

        // シェーダーテスト用の壁は個別で特殊描画するため、インスタンシング一括描画から除外する
        if (obj->GetObjectType() == ObjectType::Wall)
        {
            Wall* wall = static_cast<Wall*>(obj);
            if (wall->IsShaderTest())
            {
                continue;
            }
        }

        const RenderComponent& rc = obj->GetRenderComponent();
        if (!rc.visible || rc.meshType != MeshType::Cube || rc.textureKey.empty())
        {
            continue; // 非表示、キューブメッシュ以外、またはテクスチャキーが空の場合はスキップ
        }

        // テクスチャキーがインデックスマップに存在することを確認
        if (m_TextureIndexMap.find(rc.textureKey) != m_TextureIndexMap.end())
        {
            // 視錐台カリング判定
            if (useCulling)
            {
                // オブジェクトのスケールを考慮して、簡易バウンディング球（マージン付き）を構築
                DirectX::XMFLOAT3 pos = obj->GetPosition();
                DirectX::XMFLOAT3 scale = obj->GetScale();
                float maxScale = (std::max)(scale.x, (std::max)(scale.y, scale.z));
                float radius = 1.0f * maxScale; // 安全のためのマージン係数1.0

                DirectX::BoundingSphere sphere(pos, radius);
                if (!frustum.Intersects(sphere))
                {
                    continue; // 視界外の場合は描画リストに追加しない（カリング）
                }
            }

            drawList.push_back(obj);
        }
    }

    // 描画対象が存在しない場合は何もしない
    if (drawList.empty())
    {
        return;
    }

    // 2. インスタンス描画用のステート設定
    if (pass == RenderPass::Outline)
    {
        context->IASetInputLayout(m_InstancedOutlineInputLayout);
        context->VSSetShader(m_InstancedOutlineVertexShader, nullptr, 0);
        context->PSSetShader(Renderer::GetOutlinePixelShader(), nullptr, 0);
    }
    else // 通常パスまたはシャドウマップパス
    {
        context->IASetInputLayout(m_InstancedInputLayout);
        context->VSSetShader(m_InstancedVertexShader, nullptr, 0);
        if (pass == RenderPass::Shadow)
        {
            context->PSSetShader(nullptr, nullptr, 0);
        }
        else
        {
            context->PSSetShader(m_InstancedPixelShader, nullptr, 0);
        }
    }
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 共通のキューブ用頂点バッファを取得
    ID3D11Buffer* cubeVertexBuffer = Renderer::GetCubeVertexBuffer();
    if (!cubeVertexBuffer) {
        return;
    }

    // スロット0（頂点）とスロット1（インスタンス）の入力ストライドとオフセットを設定
    UINT strides[2] = { sizeof(VERTEX_3D), sizeof(InstanceData) };
    UINT offsets[2] = { 0, 0 };
    ID3D11Buffer* buffers[2] = { cubeVertexBuffer, m_InstanceBuffer };

    // 頂点バッファとインスタンスバッファを同時にパイプラインにバインド
    context->IASetVertexBuffers(0, 2, buffers, strides, offsets);

    // 共通のマテリアル情報を適用
    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Specular = DirectX::XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f);
    material.Shininess = 20.0f;
    material.TextureEnable = TRUE;
    Renderer::SetMaterial(material);

    // 通常描画のときは、テクスチャ配列をスロット0にセット
    if (pass == RenderPass::Normal)
    {
        Renderer::SetTexture(m_TextureArraySRV);
    }

    // バッチ処理
    size_t totalInstances = drawList.size();
    size_t indexOffset = 0;

    while (indexOffset < totalInstances)
    {
        size_t batchSize = (totalInstances - indexOffset > MAX_INSTANCES) 
                           ? MAX_INSTANCES 
                           : (totalInstances - indexOffset);

        // インスタンスバッファをマップして、CPU側からワールド行列とテクスチャインデックスを書き込み
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = context->Map(m_InstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if (SUCCEEDED(hr))
        {
            InstanceData* dataPtr = reinterpret_cast<InstanceData*>(mappedResource.pData);
            
            for (size_t i = 0; i < batchSize; ++i)
            {
                GameObject* obj = drawList[indexOffset + i];
                const RenderComponent& rc = obj->GetRenderComponent();

                DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(obj->GetScale().x, obj->GetScale().y, obj->GetScale().z);
                DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw(obj->GetRotation().x, obj->GetRotation().y, obj->GetRotation().z);
                DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(obj->GetPosition().x, obj->GetPosition().y, obj->GetPosition().z);

                DirectX::XMMATRIX world = scale * rotation * translation;
                if (pass == RenderPass::Shadow)
                {
                    // シャドウパス時は影のビュープロジェクション行列を乗算する
                    world = world * Renderer::GetShadowMatrix();
                }
                DirectX::XMStoreFloat4x4(&dataPtr[i].World, world);

                // テクスチャインデックスを設定
                dataPtr[i].TextureIndex = m_TextureIndexMap[rc.textureKey];
                
                // 自発光（Emissive）を設定
                dataPtr[i].Emissive = obj->GetEmissive();
            }

            context->Unmap(m_InstanceBuffer, 0);

            // インスタンス描画を実行
            context->DrawInstanced(36, static_cast<UINT>(batchSize), 0, 0);
        }

        indexOffset += batchSize;
    }

    // ─── パイプラインステートのクリーンアップ ───
    ID3D11Buffer* nullBuffers[2] = { nullptr, nullptr };
    UINT nullStrides[2] = { 0, 0 };
    UINT nullOffsets[2] = { 0, 0 };
    context->IASetVertexBuffers(0, 2, nullBuffers, nullStrides, nullOffsets);
    
    context->IASetInputLayout(nullptr);
    context->VSSetShader(nullptr, nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
}

// =================================================================
// テクスチャ配列の生成
// =================================================================
bool RenderSystem::CreateTextureArray(ID3D11Device* device)
{
    std::ofstream log("debug_log.txt", std::ios::app);
    log << "[RenderSystem::CreateTextureArray] Start" << std::endl;

    if (!device) return false;

    // 配列にするテクスチャのキー一覧
    // リリースビルド時は Assets/texture/ サブフォルダのパスを使用する
    std::vector<std::string> textureKeys = {
#ifdef NDEBUG
        "Assets/texture/enemy.png",
        "Assets/texture/player.png",
        "Assets/texture/grid.png"
#else
        "enemy.png",
        "player.png",
        "grid.png"
#endif
    };

    std::vector<ID3D11Texture2D*> textures;
    std::vector<ID3D11Resource*> resources;

    // 1. 各テクスチャリソースの取得と検証
    for (const auto& key : textureKeys)
    {
        ID3D11ShaderResourceView* srv = ResourceManager::GetTexture(key);
        if (!srv)
        {
            log << "[RenderSystem::CreateTextureArray] Failed to get texture SRV for: " << key << std::endl;
            for (auto* r : resources) r->Release();
            for (auto* t : textures) t->Release();
            return false;
        }

        ID3D11Resource* res = nullptr;
        srv->GetResource(&res);
        if (!res)
        {
            log << "[RenderSystem::CreateTextureArray] GetResource failed for: " << key << std::endl;
            for (auto* r : resources) r->Release();
            for (auto* t : textures) t->Release();
            return false;
        }
        resources.push_back(res);

        ID3D11Texture2D* tex2D = nullptr;
        HRESULT hr = res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex2D);
        if (FAILED(hr) || !tex2D)
        {
            log << "[RenderSystem::CreateTextureArray] QueryInterface failed for ID3D11Texture2D: " << key << std::endl;
            for (auto* r : resources) r->Release();
            for (auto* t : textures) t->Release();
            return false;
        }
        textures.push_back(tex2D);
    }

    // 2. 代表テクスチャ（最初のテクスチャ）の設定を取得
    D3D11_TEXTURE2D_DESC desc;
    textures[0]->GetDesc(&desc);

    log << "[RenderSystem::CreateTextureArray] Base texture info: Width=" << desc.Width 
        << ", Height=" << desc.Height 
        << ", MipLevels=" << desc.MipLevels 
        << ", Format=" << desc.Format << std::endl;

    // 3. テクスチャ配列の作成
    D3D11_TEXTURE2D_DESC arrayDesc = desc;
    arrayDesc.ArraySize = static_cast<UINT>(textureKeys.size());
    arrayDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    arrayDesc.CPUAccessFlags = 0;
    arrayDesc.MiscFlags = 0;

    ID3D11Texture2D* textureArray = nullptr;
    HRESULT hr = device->CreateTexture2D(&arrayDesc, nullptr, &textureArray);
    if (FAILED(hr) || !textureArray)
    {
        log << "[RenderSystem::CreateTextureArray] CreateTexture2D for array failed, hr=" << std::hex << hr << std::endl;
        for (auto* r : resources) r->Release();
        for (auto* t : textures) t->Release();
        return false;
    }

    // 4. デバイスコンテキストの取得
    ID3D11DeviceContext* context = nullptr;
    device->GetImmediateContext(&context);
    if (!context)
    {
        log << "[RenderSystem::CreateTextureArray] GetImmediateContext failed!" << std::endl;
        textureArray->Release();
        for (auto* r : resources) r->Release();
        for (auto* t : textures) t->Release();
        return false;
    }

    // 5. 各テクスチャのデータを配列にコピー
    for (size_t i = 0; i < textureKeys.size(); ++i)
    {
        for (UINT mip = 0; mip < desc.MipLevels; ++mip)
        {
            UINT srcSubresource = D3D11CalcSubresource(mip, 0, desc.MipLevels);
            UINT dstSubresource = D3D11CalcSubresource(mip, static_cast<UINT>(i), desc.MipLevels);
            context->CopySubresourceRegion(textureArray, dstSubresource, 0, 0, 0, textures[i], srcSubresource, nullptr);
        }
    }

    // 6. ShaderResourceView の作成
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = arrayDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = arrayDesc.MipLevels;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = arrayDesc.ArraySize;

    hr = device->CreateShaderResourceView(textureArray, &srvDesc, &m_TextureArraySRV);
    if (FAILED(hr))
    {
        log << "[RenderSystem::CreateTextureArray] CreateShaderResourceView failed, hr=" << std::hex << hr << std::endl;
        textureArray->Release();
        context->Release();
        for (auto* r : resources) r->Release();
        for (auto* t : textures) t->Release();
        return false;
    }

    // 7. リソースの解放
    textureArray->Release();
    context->Release();
    for (auto* r : resources) r->Release();
    for (auto* t : textures) t->Release();

    log << "[RenderSystem::CreateTextureArray] Successfully created Texture Array SRV." << std::endl;
    return true;
}

