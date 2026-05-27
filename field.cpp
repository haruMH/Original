#include "field.h"
#include "renderer.h"
#include "resource_manager.h"

void Field::Init()
{
    m_Position = XMFLOAT3(0.0f, -1.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);

    m_VertexCount = 4;
    VERTEX_3D vertex[4];

    // 広大な床を作成 (-50 to +50)
    vertex[0].Position = XMFLOAT3(-50.0f, 0.0f,  50.0f); vertex[0].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f); vertex[0].Diffuse = XMFLOAT4(1,1,1,1); vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);
    vertex[1].Position = XMFLOAT3( 50.0f, 0.0f,  50.0f); vertex[1].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f); vertex[1].Diffuse = XMFLOAT4(1,1,1,1); vertex[1].TexCoord = XMFLOAT2(25.0f, 0.0f);
    vertex[2].Position = XMFLOAT3(-50.0f, 0.0f, -50.0f); vertex[2].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f); vertex[2].Diffuse = XMFLOAT4(1,1,1,1); vertex[2].TexCoord = XMFLOAT2(0.0f, 25.0f);
    vertex[3].Position = XMFLOAT3( 50.0f, 0.0f, -50.0f); vertex[3].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f); vertex[3].Diffuse = XMFLOAT4(1,1,1,1); vertex[3].TexCoord = XMFLOAT2(25.0f, 25.0f);

    D3D11_BUFFER_DESC bd{};
    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.ByteWidth      = sizeof(VERTEX_3D) * m_VertexCount;
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertex;
    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

    // テクスチャの読み込み
    m_Texture = ResourceManager::GetTexture("grid.png");

    // シェーダーの読み込み
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "vertexShader.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "pixelShader.cso");
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

    ID3D11DeviceContext* deviceContext = Renderer::GetDeviceContext();
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    deviceContext->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

    // トライアングルストリップとして描画
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    deviceContext->IASetInputLayout(m_VertexLayout);
    deviceContext->VSSetShader(m_VertexShader, NULL, 0);
    deviceContext->PSSetShader(m_PixelShader, NULL, 0);

    // テクスチャの設定
    Renderer::SetTexture(m_Texture);

    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = TRUE; // テクスチャを有効にする
    Renderer::SetMaterial(material);

    deviceContext->Draw(m_VertexCount, 0);
}
