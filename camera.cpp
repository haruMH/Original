#include "camera.h"
#include "renderer.h"
#include "manager.h"
#include "gameobject.h"
#include "player.h"
#include <math.h>
#include "input.h"
#include <DirectXCollision.h>

void Camera::Init()
{
    m_Position = XMFLOAT3(0.0f, 1.5f, -3.0f);  // 壁（Z=5）が見えるよう、前方に位置
    m_Target = XMFLOAT3(0.0f, 0.0f, 5.0f);      // 壁の方向を初期注視点に設定
    m_Up = XMFLOAT3(0.0f, 1.0f, 0.0f);

    m_AngleY = 0.0f;    // 左右回転初期値（正面向き）
    m_AngleX = 0.2f;    // 少し下向き（壁全体が見えるように）
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
    GameObject* player = Manager::GetGameObject<Player>();

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

    if (!player)
    {
        // === プレイヤーがいない場合：フリーカメラモード ===
        // 上下回転の制限（フリーカメラ時はほぼ真上・真下まで向けるように緩和）
        if (m_AngleX > 1.4f) m_AngleX = 1.4f;
        if (m_AngleX < -1.4f) m_AngleX = -1.4f;

        // カメラの向きベクトルを計算
        float cosPitch = cosf(m_AngleX);
        float sinPitch = sinf(m_AngleX);
        float cosYaw = cosf(m_AngleY);
        float sinYaw = sinf(m_AngleY);

        // 前方ベクトル
        XMVECTOR fwd = XMVectorSet(sinYaw * cosPitch, sinPitch, cosYaw * cosPitch, 0.0f);
        fwd = XMVector3Normalize(fwd);
        // 上方向と右方向ベクトル
        XMVECTOR upVec = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR rightVec = XMVector3Cross(upVec, fwd);
        rightVec = XMVector3Normalize(rightVec);

        // キー入力による座標更新 (W/S/A/D/R/F)
        float moveSpeed = 0.15f;
        XMVECTOR pos = XMLoadFloat3(&m_Position);
        if (Input::GetKeyPress('W')) pos += fwd * moveSpeed;
        if (Input::GetKeyPress('S')) pos -= fwd * moveSpeed;
        if (Input::GetKeyPress('D')) pos += rightVec * moveSpeed;
        if (Input::GetKeyPress('A')) pos -= rightVec * moveSpeed;
        if (Input::GetKeyPress('R')) pos += upVec * moveSpeed;
        if (Input::GetKeyPress('F')) pos -= upVec * moveSpeed;
        XMStoreFloat3(&m_Position, pos);

        // 注視点の設定 (カメラの少し先)
        XMVECTOR target = pos + fwd * 5.0f;
        XMStoreFloat3(&m_Target, target);
    }
    else
    {
        // === プレイヤーがいる場合：追従カメラモード ===
        // 上下回転の制限（反転防止）
        if (m_AngleX > 1.4f) m_AngleX = 1.4f;
        if (m_AngleX < -0.3f) m_AngleX = -0.3f;

        XMFLOAT3 playerPos = player->GetPosition();

        float horizontalDistance = m_Distance * cosf(m_AngleX);
        float verticalDistance = m_Distance * sinf(m_AngleX);

        m_Position.x = playerPos.x - horizontalDistance * sinf(m_AngleY);
        m_Position.y = playerPos.y + verticalDistance + 1.5f;
        m_Position.z = playerPos.z - horizontalDistance * cosf(m_AngleY);

        m_Target = playerPos;
        m_Target.y += 1.2f; // プレイヤーの胸元あたりを注視する

        // --- 壁との衝突によるカメラの押し戻し処理 ---
        using namespace DirectX;
        XMVECTOR vTarget = XMLoadFloat3(&m_Target);
        XMVECTOR vCamPos = XMLoadFloat3(&m_Position);
        XMVECTOR vToCam = vCamPos - vTarget;
        float currentDist = XMVectorGetX(XMVector3Length(vToCam));

        if (currentDist > 0.001f) {
            XMVECTOR vRayDir = XMVector3Normalize(vToCam);
            float nearestDist = currentDist;
            bool hit = false;

            for (GameObject* obj : Manager::GetGameObjectList()) {
                if (!obj || obj->IsDestroy()) continue;
                if (obj->GetObjectType() == ObjectType::Wall) {
                    XMFLOAT3 wPos = obj->GetPosition();
                    XMFLOAT3 wScale = obj->GetScale();
                    XMFLOAT3 wSize = obj->GetSize();
                    
                    BoundingBox box(wPos, XMFLOAT3(wSize.x * wScale.x * 0.5f, wSize.y * wScale.y * 0.5f, wSize.z * wScale.z * 0.5f));
                    
                    float dist = 0.0f;
                    if (box.Intersects(vTarget, vRayDir, dist)) {
                        if (dist < nearestDist) {
                            nearestDist = dist;
                            hit = true;
                        }
                    }
                }
            }
            // 壁に衝突している場合、衝突位置より少し手前（マージン0.3m）にカメラを配置する
            float finalDist = nearestDist - 0.3f;
            if (finalDist < 0.5f) finalDist = 0.5f; // 最低でもプレイヤーから0.5mは離す
            
            XMVECTOR vNewCamPos = vTarget + vRayDir * finalDist;
            XMStoreFloat3(&m_Position, vNewCamPos);
        }
    }

    // --- カットシーン補間の更新 ---
    if (m_IsCutsceneMode) {
        m_InterpolationFactor += (1.0f - m_InterpolationFactor) * 0.1f; // 滑らかに 1.0 へ
    } else {
        m_InterpolationFactor += (0.0f - m_InterpolationFactor) * 0.1f; // 滑らかに 0.0 へ
    }

    if (m_InterpolationFactor > 0.001f) {
        XMVECTOR vNormalPos = XMLoadFloat3(&m_Position);
        XMVECTOR vNormalTarget = XMLoadFloat3(&m_Target);
        XMVECTOR vCutscenePos = XMLoadFloat3(&m_CutsceneEye);
        XMVECTOR vCutsceneTarget = XMLoadFloat3(&m_CutsceneTarget);

        XMVECTOR vBlendedPos = XMVectorLerp(vNormalPos, vCutscenePos, m_InterpolationFactor);
        XMVECTOR vBlendedTarget = XMVectorLerp(vNormalTarget, vCutsceneTarget, m_InterpolationFactor);

        XMStoreFloat3(&m_Position, vBlendedPos);
        XMStoreFloat3(&m_Target, vBlendedTarget);
    }

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

XMFLOAT3 Camera::GetForward() const
{
    XMFLOAT3 fwd(0.0f, 0.0f, 1.0f);
    XMVECTOR dir = XMLoadFloat3(&m_Target) - XMLoadFloat3(&m_Position);
    dir = XMVector3Normalize(dir);
    XMStoreFloat3(&fwd, dir);
    return fwd;
}