#include "camera.h"
#include "renderer.h"
#include "manager.h"
#include "gameobject.h"
#include <math.h>
#include "input.h"

void Camera::Init()
{
    m_Position = XMFLOAT3(0.0f, 2.0f, -5.0f);
    m_Target = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Up = XMFLOAT3(0.0f, 1.0f, 0.0f);

    m_AngleY = 0.0f;    // 左右回転初期値
    m_AngleX = 0.3f;    // 少し上から見下ろす角度 (約17度)
    m_Distance = 6.0f;    // プレイヤーとの距離

    m_ShakeIntensity = 0.0f;
    m_ShakeTimer = 0;

    // プロジェクション行列の設定
    XMMATRIX proj = XMMatrixPerspectiveFovLH(
        XM_PIDIV4,
        (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT,
        0.1f,
        1000.0f);
    Renderer::SetProjectionMatrix(proj);
}

void Camera::Uninit() {}

void Camera::Update()
{
    // 1. プレイヤーを取得
    GameObject* player = Manager::GetPlayer();
    if (!player) return;

    // --- 回転速度の設定 ---
    float keyRotationSpeed = 0.03f; // キーボードでの回転速度
    float sensitivity = 0.002f;     // マウス感度

    // 2. 左右回転 (AngleY)
    // マウス
    m_AngleY += (float)Input::GetMouseMoveX() * sensitivity;
    // Q / E キー
    if (Input::GetKeyPress('Q')) m_AngleY -= keyRotationSpeed;
    if (Input::GetKeyPress('E')) m_AngleY += keyRotationSpeed;
    // 矢印キー（左 / 右）
    if (Input::GetKeyPress(VK_LEFT))  m_AngleY -= keyRotationSpeed;
    if (Input::GetKeyPress(VK_RIGHT)) m_AngleY += keyRotationSpeed;

    // 3. 上下回転 (AngleX)
    // マウス
    m_AngleX += (float)Input::GetMouseMoveY() * sensitivity;
    // 矢印キー（上 / 下）
    if (Input::GetKeyPress(VK_UP))   m_AngleX -= keyRotationSpeed; // 上を向く
    if (Input::GetKeyPress(VK_DOWN)) m_AngleX += keyRotationSpeed; // 下を向く

    // 4. 上下回転の制限（反転防止）
    if (m_AngleX > 1.4f) m_AngleX = 1.4f;
    if (m_AngleX < -0.3f) m_AngleX = -0.3f;

    // --- 座標計算 ---
    XMFLOAT3 playerPos = player->GetPosition();

	float horizontalDistance = m_Distance * cosf(m_AngleX);//直角三角形の斜めの部分(m_Distance)にcosで水平距離を求める
    float verticalDistance = m_Distance * sinf(m_AngleX);//直角三角形の斜めの部分(m_Distance)にsinで高さを求める

    m_Position.x = playerPos.x - horizontalDistance * sinf(m_AngleY);
    m_Position.y = playerPos.y + verticalDistance + 1.5f;
    m_Position.z = playerPos.z - horizontalDistance * cosf(m_AngleY);

    m_Target = playerPos;
    m_Target.y += 1.2f;//プレイヤーの地面ではなく背中のあたりを見るために＋1.2fしている

    // --- カメラシェイク（振動）の適用 ---
    XMFLOAT3 finalPosition = m_Position;
    XMFLOAT3 finalTarget = m_Target;
    if (m_ShakeTimer > 0) {
        float rx = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * m_ShakeIntensity;
        float ry = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * m_ShakeIntensity;
        float rz = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * m_ShakeIntensity;
        
        finalPosition.x += rx;
        finalPosition.y += ry;
        finalPosition.z += rz;

        finalTarget.x += rx;
        finalTarget.y += ry;
        finalTarget.z += rz;

        m_ShakeTimer--;
        m_ShakeIntensity *= 0.9f; // 徐々に減衰
    }

    // 行列設定
    XMVECTOR eye = XMLoadFloat3(&finalPosition);
    XMVECTOR at = XMLoadFloat3(&finalTarget);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eye, at, up);

    Renderer::SetViewMatrix(view);
    Renderer::SetCameraPosition(finalPosition);
}


void Camera::Draw() {}