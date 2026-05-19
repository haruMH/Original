#include "player.h"
#include "renderer.h"
#include "input.h"
#include "manager.h"
#include "field.h"
#include "enemy.h"
#include "camera.h"

void Player::Init()
{
    // キューブの中心をY=-0.5にすることで、底辺がY=-1.0（地面）に接するようにする
    m_Position = XMFLOAT3(0.0f, -0.5f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);

    // 頂点データの作成（立体的なキューブ・正六面体に変更：36頂点）
    VERTEX_3D vertex[36];

    // 頂点座標の定義 (1x1x1のキューブ)
    XMFLOAT3 ltf(-0.5f,  0.5f,  0.5f); // Left-Top-Front
    XMFLOAT3 rtf( 0.5f,  0.5f,  0.5f); // Right-Top-Front
    XMFLOAT3 lbf(-0.5f, -0.5f,  0.5f); // Left-Bottom-Front
    XMFLOAT3 rbf( 0.5f, -0.5f,  0.5f); // Right-Bottom-Front
    XMFLOAT3 ltb(-0.5f,  0.5f, -0.5f); // Left-Top-Back
    XMFLOAT3 rtb( 0.5f,  0.5f, -0.5f); // Right-Top-Back
    XMFLOAT3 lbb(-0.5f, -0.5f, -0.5f); // Left-Bottom-Back
    XMFLOAT3 rbb( 0.5f, -0.5f, -0.5f); // Right-Bottom-Back

    // 色の定義
    XMFLOAT4 red   (1.0f, 0.0f, 0.0f, 1.0f); // 前
    XMFLOAT4 green (0.0f, 1.0f, 0.0f, 1.0f); // 右
    XMFLOAT4 blue  (0.0f, 0.0f, 1.0f, 1.0f); // 後ろ
    XMFLOAT4 yellow(1.0f, 1.0f, 0.0f, 1.0f); // 左
    XMFLOAT4 cyan  (0.0f, 1.0f, 1.0f, 1.0f); // 上
    XMFLOAT4 purple(1.0f, 0.0f, 1.0f, 1.0f); // 底

    int idx = 0;
    // 前面 (赤)
    vertex[idx].Position = ltf; vertex[idx++].Diffuse = red;
    vertex[idx].Position = rbf; vertex[idx++].Diffuse = red;
    vertex[idx].Position = rtf; vertex[idx++].Diffuse = red;
    vertex[idx].Position = ltf; vertex[idx++].Diffuse = red;
    vertex[idx].Position = lbf; vertex[idx++].Diffuse = red;
    vertex[idx].Position = rbf; vertex[idx++].Diffuse = red;

    // 右面 (緑)
    vertex[idx].Position = rtf; vertex[idx++].Diffuse = green;
    vertex[idx].Position = rbb; vertex[idx++].Diffuse = green;
    vertex[idx].Position = rtb; vertex[idx++].Diffuse = green;
    vertex[idx].Position = rtf; vertex[idx++].Diffuse = green;
    vertex[idx].Position = rbf; vertex[idx++].Diffuse = green;
    vertex[idx].Position = rbb; vertex[idx++].Diffuse = green;

    // 後面 (青)
    vertex[idx].Position = rtb; vertex[idx++].Diffuse = blue;
    vertex[idx].Position = lbb; vertex[idx++].Diffuse = blue;
    vertex[idx].Position = ltb; vertex[idx++].Diffuse = blue;
    vertex[idx].Position = rtb; vertex[idx++].Diffuse = blue;
    vertex[idx].Position = rbb; vertex[idx++].Diffuse = blue;
    vertex[idx].Position = lbb; vertex[idx++].Diffuse = blue;

    // 左面 (黄)
    vertex[idx].Position = ltb; vertex[idx++].Diffuse = yellow;
    vertex[idx].Position = lbf; vertex[idx++].Diffuse = yellow;
    vertex[idx].Position = ltf; vertex[idx++].Diffuse = yellow;
    vertex[idx].Position = ltb; vertex[idx++].Diffuse = yellow;
    vertex[idx].Position = lbb; vertex[idx++].Diffuse = yellow;
    vertex[idx].Position = lbf; vertex[idx++].Diffuse = yellow;

    // 上面 (シアン)
    vertex[idx].Position = ltb; vertex[idx++].Diffuse = cyan;
    vertex[idx].Position = rtf; vertex[idx++].Diffuse = cyan;
    vertex[idx].Position = rtb; vertex[idx++].Diffuse = cyan;
    vertex[idx].Position = ltb; vertex[idx++].Diffuse = cyan;
    vertex[idx].Position = ltf; vertex[idx++].Diffuse = cyan;
    vertex[idx].Position = rtf; vertex[idx++].Diffuse = cyan;

    // 底面 (紫)
    vertex[idx].Position = lbf; vertex[idx++].Diffuse = purple;
    vertex[idx].Position = rbb; vertex[idx++].Diffuse = purple;
    vertex[idx].Position = rbf; vertex[idx++].Diffuse = purple;
    vertex[idx].Position = lbf; vertex[idx++].Diffuse = purple;
    vertex[idx].Position = lbb; vertex[idx++].Diffuse = purple;
    vertex[idx].Position = rbb; vertex[idx++].Diffuse = purple;

    // 法線とテクスチャ座標の設定
    for (int i = 0; i < 6; i++)  { vertex[i].Normal = XMFLOAT3(0.0f, 0.0f, 1.0f); }  // 前
    for (int i = 6; i < 12; i++) { vertex[i].Normal = XMFLOAT3(1.0f, 0.0f, 0.0f); }  // 右
    for (int i = 12; i < 18; i++){ vertex[i].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f); } // 後ろ
    for (int i = 18; i < 24; i++){ vertex[i].Normal = XMFLOAT3(-1.0f, 0.0f, 0.0f); } // 左
    for (int i = 24; i < 30; i++){ vertex[i].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f); }  // 上
    for (int i = 30; i < 36; i++){ vertex[i].Normal = XMFLOAT3(0.0f, -1.0f, 0.0f); } // 底

    for (int i = 0; i < 36; i += 6) {
        vertex[i + 0].TexCoord = XMFLOAT2(0.0f, 0.0f);
        vertex[i + 1].TexCoord = XMFLOAT2(1.0f, 1.0f);
        vertex[i + 2].TexCoord = XMFLOAT2(1.0f, 0.0f);
        vertex[i + 3].TexCoord = XMFLOAT2(0.0f, 0.0f);
        vertex[i + 4].TexCoord = XMFLOAT2(0.0f, 1.0f);
        vertex[i + 5].TexCoord = XMFLOAT2(1.0f, 1.0f);
    }

    // 頂点バッファの作成
    D3D11_BUFFER_DESC bd{};
    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.ByteWidth      = sizeof(VERTEX_3D) * 36;
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertex;

    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

    // テクスチャの読み込み
    Renderer::CreateTexture("player.png", &m_Texture);

    // シェーダーの読み込み
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "vertexShader.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "pixelShader.cso");
}

void Player::Uninit()
{
    if (m_VertexLayout) m_VertexLayout->Release();
    if (m_PixelShader)  m_PixelShader->Release();
    if (m_VertexShader) m_VertexShader->Release();
    if (m_VertexBuffer) m_VertexBuffer->Release();
}

void Player::Update()
{
    float oldRotY = m_Rotation.y;

    // マウスの移動量を取得
    float mouseMoveX = (float)Input::GetMouseMoveX();

    // カメラの角度(Yaw)を取得
    float camYaw = 0.0f;
    if (g_Camera) {
        camYaw = g_Camera->GetAngleY();
    }

    // マウス操作がある場合は、プレイヤーの向きをカメラの向き（マウスで向いた方向）に合わせる
    if (abs(mouseMoveX) > 0.1f) {
        m_Rotation.y = camYaw;
    }

    // カメラの向きを基準にした前後左右ベクトル
    XMMATRIX camRotY = XMMatrixRotationY(camYaw);
    XMVECTOR camForward = XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), camRotY);
    XMVECTOR camRight   = XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), camRotY);

    // WASD入力による移動方向の決定
    XMVECTOR moveDir = XMVectorZero();
    if (Input::GetKeyPress('W')) moveDir += camForward;
    if (Input::GetKeyPress('S')) moveDir -= camForward;
    if (Input::GetKeyPress('A')) moveDir -= camRight;
    if (Input::GetKeyPress('D')) moveDir += camRight;

    // プレイヤーの移動
    float speed = 0.1f;
    XMVECTOR pos = XMLoadFloat3(&m_Position);
    
    if (XMVectorGetX(XMVector3LengthSq(moveDir)) > 0.001f) {
        moveDir = XMVector3Normalize(moveDir);
        pos += moveDir * speed;

        // マウス操作がない場合は、プレイヤーのモデルを移動方向に回転させる
        if (abs(mouseMoveX) <= 0.1f) {
            m_Rotation.y = atan2f(XMVectorGetX(moveDir), XMVectorGetZ(moveDir));
        }
    }

    // 前方ベクトルを再計算（現在のm_Rotation.yを使用）
    XMMATRIX rotY = XMMatrixRotationY(m_Rotation.y);
    XMVECTOR forward = XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotY);
    XMFLOAT3 fwdF;
    XMStoreFloat3(&fwdF, forward);

    // 旋回速度（角速度）の計算
    m_AngularVelocity = m_Rotation.y - oldRotY;

    // 掴んでいる敵の位置をプレイヤーの前方に固定
    if (m_State == PlayerState::ATTACK && m_GrabbedEnemy) {
        m_GrabbedEnemy->SetPosition(XMFLOAT3(m_Position.x + fwdF.x * 2.0f, 0.0f, m_Position.z + fwdF.z * 2.0f));
    }

    // 当たり判定チェック (AABB)
    XMFLOAT3 nextPos;
    XMStoreFloat3(&nextPos, pos);

    // 他のオブジェクトとの衝突をチェック
    for (GameObject* obj : Manager::GetScene()->GetGameObjectList()) {
        if (obj == this || obj == m_GrabbedEnemy) continue; // 自分自身と掴んでいる敵は無視
        if (dynamic_cast<Field*>(obj)) continue; // 地面は無視

        XMFLOAT3 objPos = obj->GetPosition();
        XMFLOAT3 objSize = obj->GetSize();
        XMFLOAT3 objScale = obj->GetScale();

        // X, Y, Z 各軸で重なりをチェック (スケールを考慮)
        bool collisionX = abs(nextPos.x - objPos.x) < (m_Size.x * m_Scale.x + objSize.x * objScale.x) * 0.5f;
        bool collisionY = abs(nextPos.y - objPos.y) < (m_Size.y * m_Scale.y + objSize.y * objScale.y) * 0.5f;
        bool collisionZ = abs(nextPos.z - objPos.z) < (m_Size.z * m_Scale.z + objSize.z * objScale.z) * 0.5f;

        if (collisionX && collisionY && collisionZ) {
            // 衝突している場合は移動をキャンセル（元の座標に戻す）
            pos = XMLoadFloat3(&m_Position);
            break;
        }
    }
    
    XMStoreFloat3(&m_Position, pos);
}

void Player::Throw()
{
    if (m_GrabbedEnemy) {
        XMMATRIX rotY = XMMatrixRotationY(m_Rotation.y);
        XMVECTOR forward = XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotY);
        XMVECTOR right   = XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rotY);
        XMFLOAT3 fwdF, rightF;
        XMStoreFloat3(&fwdF, forward);
        XMStoreFloat3(&rightF, right);

        // 基本の投擲速度（前方）
        float baseThrowSpeed = 0.5f;
        
        // 遠心力による接線方向の速度（マウスの振り幅に応じて横に飛ばす）
        // ※プレイヤーが右に回転（正の角速度）していれば敵は右(接線)方向へ飛ぶ
        float swingSpeed = m_AngularVelocity * 15.0f; 

        // 前方＋遠心力のベクトルを合成
        XMFLOAT3 throwVelocity = XMFLOAT3(
            fwdF.x * baseThrowSpeed + rightF.x * swingSpeed,
            0.4f,
            fwdF.z * baseThrowSpeed + rightF.z * swingSpeed
        );

        m_GrabbedEnemy->SetVelocity(throwVelocity);
        m_GrabbedEnemy->SetEnemyState(EnemyState::FLYING);
        m_GrabbedEnemy = nullptr;
        m_State = PlayerState::IDLE;
    }
}

void Player::Draw()
{
    // ワールド行列の作成
    XMMATRIX worldMatrix = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
                           XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z) *
                           XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
                           
    Renderer::SetWorldMatrix(worldMatrix);

    ID3D11DeviceContext* deviceContext = Renderer::GetDeviceContext();

    // 頂点バッファをセット
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    deviceContext->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

    // トポロジーをセット（三角形リスト）
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // シェーダーと入力レイアウトをセット
    deviceContext->IASetInputLayout(m_VertexLayout);
    deviceContext->VSSetShader(m_VertexShader, NULL, 0);
    deviceContext->PSSetShader(m_PixelShader, NULL, 0);

    // テクスチャをセット
    Renderer::SetTexture(m_Texture);

    // マテリアルの設定
    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse        = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient        = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Specular       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Shininess      = 50.0f;  // プレイヤーは少し光沢強め
    material.TextureEnable  = TRUE;   // テクスチャを有効にする
    material.RimPower       = 3.0f;   // リムライトの強さ
    Renderer::SetMaterial(material);

    // 描画 (36頂点)
    deviceContext->Draw(36, 0);
}
