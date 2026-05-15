#include "wall.h"
#include "renderer.h"

void Wall::Init()
{
    m_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_Size     = XMFLOAT3(1.0f, 1.0f, 1.0f);

    XMFLOAT3 ltf(-0.5f,  0.5f,  0.5f), rtf( 0.5f,  0.5f,  0.5f);
    XMFLOAT3 lbf(-0.5f, -0.5f,  0.5f), rbf( 0.5f, -0.5f,  0.5f);
    XMFLOAT3 ltb(-0.5f,  0.5f, -0.5f), rtb( 0.5f,  0.5f, -0.5f);
    XMFLOAT3 lbb(-0.5f, -0.5f, -0.5f), rbb( 0.5f, -0.5f, -0.5f);

    VERTEX_3D vertex[36];
    for (int i = 0; i < 36; i++)
    {
        vertex[i].Diffuse  = XMFLOAT4(1,1,1,1);
        vertex[i].Normal   = XMFLOAT3(0,1,0);
        vertex[i].TexCoord = XMFLOAT2(0,0);
    }

    int i = 0;
    // 前面 Z+
    vertex[i].Position=ltf; vertex[i].Normal=XMFLOAT3(0,0,1); vertex[i].TexCoord=XMFLOAT2(0,0); i++;
    vertex[i].Position=rbf; vertex[i].Normal=XMFLOAT3(0,0,1); vertex[i].TexCoord=XMFLOAT2(1,1); i++;
    vertex[i].Position=rtf; vertex[i].Normal=XMFLOAT3(0,0,1); vertex[i].TexCoord=XMFLOAT2(1,0); i++;
    vertex[i].Position=ltf; vertex[i].Normal=XMFLOAT3(0,0,1); vertex[i].TexCoord=XMFLOAT2(0,0); i++;
    vertex[i].Position=lbf; vertex[i].Normal=XMFLOAT3(0,0,1); vertex[i].TexCoord=XMFLOAT2(0,1); i++;
    vertex[i].Position=rbf; vertex[i].Normal=XMFLOAT3(0,0,1); vertex[i].TexCoord=XMFLOAT2(1,1); i++;
    // 右面 X+
    vertex[i].Position=rtf; vertex[i].Normal=XMFLOAT3(1,0,0); vertex[i].TexCoord=XMFLOAT2(0,0); i++;
    vertex[i].Position=rbb; vertex[i].Normal=XMFLOAT3(1,0,0); vertex[i].TexCoord=XMFLOAT2(1,1); i++;
    vertex[i].Position=rtb; vertex[i].Normal=XMFLOAT3(1,0,0); vertex[i].TexCoord=XMFLOAT2(1,0); i++;
    vertex[i].Position=rtf; vertex[i].Normal=XMFLOAT3(1,0,0); vertex[i].TexCoord=XMFLOAT2(0,0); i++;
    vertex[i].Position=rbf; vertex[i].Normal=XMFLOAT3(1,0,0); vertex[i].TexCoord=XMFLOAT2(0,1); i++;
    vertex[i].Position=rbb; vertex[i].Normal=XMFLOAT3(1,0,0); vertex[i].TexCoord=XMFLOAT2(1,1); i++;
    // 後面 Z-
    vertex[i].Position=rtb; vertex[i].Normal=XMFLOAT3(0,0,-1); vertex[i].TexCoord=XMFLOAT2(0,0); i++;
    vertex[i].Position=lbb; vertex[i].Normal=XMFLOAT3(0,0,-1); vertex[i].TexCoord=XMFLOAT2(1,1); i++;
    vertex[i].Position=ltb; vertex[i].Normal=XMFLOAT3(0,0,-1); vertex[i].TexCoord=XMFLOAT2(1,0); i++;
    vertex[i].Position=rtb; vertex[i].Normal=XMFLOAT3(0,0,-1); vertex[i].TexCoord=XMFLOAT2(0,0); i++;
    vertex[i].Position=rbb; vertex[i].Normal=XMFLOAT3(0,0,-1); vertex[i].TexCoord=XMFLOAT2(0,1); i++;
    vertex[i].Position=lbb; vertex[i].Normal=XMFLOAT3(0,0,-1); vertex[i].TexCoord=XMFLOAT2(1,1); i++;
    // 左面 X-
    vertex[i].Position=ltb; vertex[i].Normal=XMFLOAT3(-1,0,0); vertex[i].TexCoord=XMFLOAT2(0,0); i++;
    vertex[i].Position=lbf; vertex[i].Normal=XMFLOAT3(-1,0,0); vertex[i].TexCoord=XMFLOAT2(1,1); i++;
    vertex[i].Position=ltf; vertex[i].Normal=XMFLOAT3(-1,0,0); vertex[i].TexCoord=XMFLOAT2(1,0); i++;
    vertex[i].Position=ltb; vertex[i].Normal=XMFLOAT3(-1,0,0); vertex[i].TexCoord=XMFLOAT2(0,0); i++;
    vertex[i].Position=lbb; vertex[i].Normal=XMFLOAT3(-1,0,0); vertex[i].TexCoord=XMFLOAT2(0,1); i++;
    vertex[i].Position=lbf; vertex[i].Normal=XMFLOAT3(-1,0,0); vertex[i].TexCoord=XMFLOAT2(1,1); i++;
    // 上面 Y+
    vertex[i].Position=ltb; vertex[i].Normal=XMFLOAT3(0,1,0); vertex[i].TexCoord=XMFLOAT2(0,0); i++;
    vertex[i].Position=rtf; vertex[i].Normal=XMFLOAT3(0,1,0); vertex[i].TexCoord=XMFLOAT2(1,1); i++;
    vertex[i].Position=rtb; vertex[i].Normal=XMFLOAT3(0,1,0); vertex[i].TexCoord=XMFLOAT2(1,0); i++;
    vertex[i].Position=ltb; vertex[i].Normal=XMFLOAT3(0,1,0); vertex[i].TexCoord=XMFLOAT2(0,0); i++;
    vertex[i].Position=ltf; vertex[i].Normal=XMFLOAT3(0,1,0); vertex[i].TexCoord=XMFLOAT2(0,1); i++;
    vertex[i].Position=rtf; vertex[i].Normal=XMFLOAT3(0,1,0); vertex[i].TexCoord=XMFLOAT2(1,1); i++;
    // 下面 Y-
    vertex[i].Position=lbf; vertex[i].Normal=XMFLOAT3(0,-1,0); vertex[i].TexCoord=XMFLOAT2(0,0); i++;
    vertex[i].Position=rbb; vertex[i].Normal=XMFLOAT3(0,-1,0); vertex[i].TexCoord=XMFLOAT2(1,1); i++;
    vertex[i].Position=rbf; vertex[i].Normal=XMFLOAT3(0,-1,0); vertex[i].TexCoord=XMFLOAT2(1,0); i++;
    vertex[i].Position=lbf; vertex[i].Normal=XMFLOAT3(0,-1,0); vertex[i].TexCoord=XMFLOAT2(0,0); i++;
    vertex[i].Position=lbb; vertex[i].Normal=XMFLOAT3(0,-1,0); vertex[i].TexCoord=XMFLOAT2(0,1); i++;
    vertex[i].Position=rbb; vertex[i].Normal=XMFLOAT3(0,-1,0); vertex[i].TexCoord=XMFLOAT2(1,1); i++;

    D3D11_BUFFER_DESC bd; ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT; bd.ByteWidth = sizeof(VERTEX_3D) * 36; bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd; ZeroMemory(&sd, sizeof(sd)); sd.pSysMem = vertex;
    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

    Renderer::CreateTexture("grid.png",        &m_Texture);
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "vertexShader.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "pixelShader.cso");
}

void Wall::Uninit()
{
    if (m_VertexLayout) m_VertexLayout->Release();
    if (m_PixelShader)  m_PixelShader->Release();
    if (m_VertexShader) m_VertexShader->Release();
    if (m_VertexBuffer) m_VertexBuffer->Release();
}

void Wall::Update()
{
    // 壁は動かないため特に処理なし
}

void Wall::Draw()
{
    XMMATRIX worldMatrix =
        XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
        XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z) *
        XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    Renderer::SetWorldMatrix(worldMatrix);

    ID3D11DeviceContext* dc = Renderer::GetDeviceContext();
    UINT stride = sizeof(VERTEX_3D), offset = 0;
    dc->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc->IASetInputLayout(m_VertexLayout);
    dc->VSSetShader(m_VertexShader, NULL, 0);
    dc->PSSetShader(m_PixelShader, NULL, 0);

    Renderer::SetTexture(m_Texture);

    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse       = XMFLOAT4(1,1,1,1);
    material.Ambient       = XMFLOAT4(1,1,1,1);
    material.TextureEnable = TRUE;
    Renderer::SetMaterial(material);

    dc->Draw(36, 0);
}
