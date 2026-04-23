#include "enemy.h"
#include "renderer.h"

void Enemy::Init() {
  m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
  m_Scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
  m_Size = XMFLOAT3(1.0f, 1.0f, 1.0f); // 当たり判定のサイズ

  // 頂点データの作成（キューブ）
  VERTEX_3D vertex[36];

  XMFLOAT3 ltf(-0.5f, 0.5f, 0.5f);
  XMFLOAT3 rtf(0.5f, 0.5f, 0.5f);
  XMFLOAT3 lbf(-0.5f, -0.5f, 0.5f);
  XMFLOAT3 rbf(0.5f, -0.5f, 0.5f);
  XMFLOAT3 ltb(-0.5f, 0.5f, -0.5f);
  XMFLOAT3 rtb(0.5f, 0.5f, -0.5f);
  XMFLOAT3 lbb(-0.5f, -0.5f, -0.5f);
  XMFLOAT3 rbb(0.5f, -0.5f, -0.5f);

  // 色は黄色にする
  XMFLOAT4 yellow(1.0f, 1.0f, 0.0f, 1.0f);

  int idx = 0;
  // 前面
  vertex[idx].Position = ltf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rbf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rtf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = ltf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = lbf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rbf;
  vertex[idx++].Diffuse = yellow;
  // 右面
  vertex[idx].Position = rtf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rbb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rtb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rtf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rbf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rbb;
  vertex[idx++].Diffuse = yellow;
  // 後面
  vertex[idx].Position = rtb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = lbb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = ltb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rtb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rbb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = lbb;
  vertex[idx++].Diffuse = yellow;
  // 左面
  vertex[idx].Position = ltb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = lbf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = ltf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = ltb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = lbb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = lbf;
  vertex[idx++].Diffuse = yellow;
  // 上面
  vertex[idx].Position = ltb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rtf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rtb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = ltb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = ltf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rtf;
  vertex[idx++].Diffuse = yellow;
  // 底面
  vertex[idx].Position = lbf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rbb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rbf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = lbf;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = lbb;
  vertex[idx++].Diffuse = yellow;
  vertex[idx].Position = rbb;
  vertex[idx++].Diffuse = yellow;

  for (int i = 0; i < 36; i++) {
    vertex[i].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f); // 簡易設定
    vertex[i].TexCoord = XMFLOAT2(0.0f, 0.0f);
  }
  // 法線の設定（正しいライティングのため）
  for (int i = 0; i < 6; i++)
    vertex[i].Normal = XMFLOAT3(0.0f, 0.0f, 1.0f);
  for (int i = 6; i < 12; i++)
    vertex[i].Normal = XMFLOAT3(1.0f, 0.0f, 0.0f);
  for (int i = 12; i < 18; i++)
    vertex[i].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
  for (int i = 18; i < 24; i++)
    vertex[i].Normal = XMFLOAT3(-1.0f, 0.0f, 0.0f);
  for (int i = 24; i < 30; i++)
    vertex[i].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
  for (int i = 30; i < 36; i++)
    vertex[i].Normal = XMFLOAT3(0.0f, -1.0f, 0.0f);

  for (int i = 0; i < 36; i += 6) {
    vertex[i + 0].TexCoord = XMFLOAT2(0.0f, 0.0f);
    vertex[i + 1].TexCoord = XMFLOAT2(1.0f, 1.0f);
    vertex[i + 2].TexCoord = XMFLOAT2(1.0f, 0.0f);
    vertex[i + 3].TexCoord = XMFLOAT2(0.0f, 0.0f);
    vertex[i + 4].TexCoord = XMFLOAT2(0.0f, 1.0f);
    vertex[i + 5].TexCoord = XMFLOAT2(1.0f, 1.0f);
  }

  D3D11_BUFFER_DESC bd{};
  bd.Usage = D3D11_USAGE_DEFAULT;
  bd.ByteWidth = sizeof(VERTEX_3D) * 36;
  bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA sd{};
  sd.pSysMem = vertex;
  Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

  // テクスチャの読み込み
  Renderer::CreateTexture("enemy.png", &m_Texture);

  Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
                               "vertexShader.cso");
  Renderer::CreatePixelShader(&m_PixelShader, "pixelShader.cso");
}

void Enemy::Uninit() {
  if (m_VertexLayout)
    m_VertexLayout->Release();
  if (m_PixelShader)
    m_PixelShader->Release();
  if (m_VertexShader)
    m_VertexShader->Release();
  if (m_VertexBuffer)
    m_VertexBuffer->Release();
}

void Enemy::Update() {}

void Enemy::Draw() {
  XMMATRIX worldMatrix =
      XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
      XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z) *
      XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
  Renderer::SetWorldMatrix(worldMatrix);

  ID3D11DeviceContext *deviceContext = Renderer::GetDeviceContext();
  UINT stride = sizeof(VERTEX_3D);
  UINT offset = 0;
  deviceContext->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
  deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  deviceContext->IASetInputLayout(m_VertexLayout);
  deviceContext->VSSetShader(m_VertexShader, NULL, 0);
  deviceContext->PSSetShader(m_PixelShader, NULL, 0);

  // テクスチャをセット
  Renderer::SetTexture(m_Texture);

  MATERIAL material;
  ZeroMemory(&material, sizeof(material));
  material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
  material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
  material.Specular =
      XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f); // エネミーは鈍い光
  material.Shininess = 10.0f;
  Renderer::SetMaterial(material);

  deviceContext->Draw(36, 0);
}
