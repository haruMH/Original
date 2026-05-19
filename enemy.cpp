#include "enemy.h"
#include "renderer.h"

void Enemy::Init()
{
    m_Position = XMFLOAT3(0.0f, -0.5f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_Size     = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_UprightTimer = 0;

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

    Renderer::CreateTexture("enemy.png",        &m_Texture);
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "vertexShader.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "pixelShader.cso");
}

void Enemy::Uninit()
{
    if (m_VertexLayout) m_VertexLayout->Release();
    if (m_PixelShader)  m_PixelShader->Release();
    if (m_VertexShader) m_VertexShader->Release();
    if (m_VertexBuffer) m_VertexBuffer->Release();
}

void Enemy::Update()
{
    if (m_EnemyState == EnemyState::FLYING) {
        // 摩擦（空気抵抗）で徐々に減速させる（摩擦を強くして飛びすぎを防止）
        m_Velocity.x *= 0.94f;
        m_Velocity.z *= 0.94f;

        // 重力の適用（Y軸の落下）
        m_VelocityY -= 0.015f; 

        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;
        m_Position.y += m_VelocityY;

        // 回転中のめり込みを防ぐため、飛行中のクランプ床面を少し浮かせた高さ(-0.3f)にする
        float flightFloorY = -0.3f;

        // 地面（床）への着地クランプ
        if (m_Position.y < flightFloorY) {
            m_Position.y = flightFloorY;
            m_VelocityY = 0.0f;
        }

        // 速度が十分に落ち、かつ浮かせた地面に着地しているならNORMALに戻す
        if (m_Position.y <= flightFloorY && abs(m_Velocity.x) < 0.05f && abs(m_Velocity.z) < 0.05f) {
            m_Position.y = -0.5f; // 静止時に本来の地面の高さ(-0.5f)に密着させる
            m_Velocity   = XMFLOAT3(0, 0, 0);
            m_VelocityY  = 0.0f;
            m_EnemyState = EnemyState::NORMAL;
            m_UprightTimer = 60;  // 1秒後にゆっくり起き上がるようにタイマーをセット
        }

        m_Rotation.x += 0.2f;
        m_Rotation.z += 0.15f;
    }
    else if (m_EnemyState == EnemyState::DEFEATED) {
        // 撃破演出：縮小しながら高速回転して飛んでいく
        m_Scale.x *= 0.85f;
        m_Scale.y *= 0.85f;
        m_Scale.z *= 0.85f;

        m_Rotation.x += 0.5f;
        m_Rotation.y += 0.5f;
        m_Rotation.z += 0.5f;

        // 吹っ飛ぶ慣性を少しだけ維持してスライドさせる
        m_Velocity.x *= 0.9f;
        m_Velocity.z *= 0.9f;
        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;

        if (m_Scale.x < 0.05f) {
            SetDestroy(); // 完全に消滅
        }
    }
    else if (m_EnemyState == EnemyState::NORMAL) {
        // 静止後、1秒間（60フレーム）待機してからゆっくり直立に戻す
        if (m_UprightTimer > 0) {
            m_UprightTimer--;
        }
        else {
            // X軸とZ軸の回転を徐々に0（直立）に近づける（LERP）
            m_Rotation.x *= 0.9f;
            m_Rotation.z *= 0.9f;

            if (abs(m_Rotation.x) < 0.001f) m_Rotation.x = 0.0f;
            if (abs(m_Rotation.z) < 0.001f) m_Rotation.z = 0.0f;
        }
    }
}

void Enemy::Draw()
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
    material.Specular      = XMFLOAT4(0.6f,0.6f,0.6f,1);
    material.Shininess     = 20.0f;
    material.TextureEnable = TRUE;
    Renderer::SetMaterial(material);

    dc->Draw(36, 0);
}