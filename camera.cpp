#include "camera.h"
#include "renderer.h"
#include "manager.h"
#include "gameobject.h"

void Camera::Init()
{
    m_Position = XMFLOAT3(0.0f, 2.0f, -5.0f);
    m_Target   = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Up       = XMFLOAT3(0.0f, 1.0f, 0.0f);

    m_AngleX = 0.2f;
    m_AngleY = 0.0f;

    XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(
        XM_PIDIV4, // 視野角 (45度)
        (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, // アスペクト比
        0.1f,      // ニアクリップ
        1000.0f    // ファークリップ
    );
    Renderer::SetProjectionMatrix(projectionMatrix);
}

void Camera::Uninit()
{
}

void Camera::Update()
{
    // プレイヤーの取得
    GameObject* player = Manager::GetPlayer();
    if (!player) return;

    XMFLOAT3 playerPos = player->GetPosition();

    // プレイヤーの後ろ(-5.0f)、少し上(2.0f)にカメラを配置する（固定追従）
    m_Position.x = playerPos.x;
    m_Position.y = playerPos.y + 2.0f;
    m_Position.z = playerPos.z - 5.0f;

    // カメラの注視点をプレイヤーに合わせる
    m_Target = playerPos;

    // ビュー行列の作成と設定
    XMVECTOR eye = XMLoadFloat3(&m_Position);
    XMVECTOR at  = XMLoadFloat3(&m_Target);
    XMVECTOR up  = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX viewMatrix = XMMatrixLookAtLH(eye, at, up);
    Renderer::SetViewMatrix(viewMatrix);
    
    // 視点位置をRendererに通知（スペキュラ計算用）
    Renderer::SetCameraPosition(m_Position);
}

void Camera::Draw()
{
}
