#include "player_controller.h"
#include "input.h"

using namespace DirectX;

XMFLOAT3 PlayerController::GetMoveDirection(float camYaw)
{
    // カメラの向きに合わせた移動方向ベクトルの算出
    XMMATRIX camRotY = XMMatrixRotationY(camYaw);
    XMVECTOR camForward = XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), camRotY);
    XMVECTOR camRight   = XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), camRotY);

    XMVECTOR moveDir = XMVectorZero();
    if (Input::GetKeyPress(KEY_MOVE_FORWARD))  moveDir += camForward;
    if (Input::GetKeyPress(KEY_MOVE_BACKWARD)) moveDir -= camForward;
    if (Input::GetKeyPress(KEY_MOVE_LEFT))     moveDir -= camRight;
    if (Input::GetKeyPress(KEY_MOVE_RIGHT))    moveDir += camRight;

    XMFLOAT3 dir(0.0f, 0.0f, 0.0f);
    if (XMVectorGetX(XMVector3LengthSq(moveDir)) > 0.001f) {
        moveDir = XMVector3Normalize(moveDir);
        XMStoreFloat3(&dir, moveDir);
    }
    return dir;
}

bool PlayerController::IsGrabOrThrowAction()
{
    // 左クリック(VK_LBUTTON = 0x01)
    return Input::GetKeyTrigger(KEY_GRAB_THROW);
}

bool PlayerController::IsSpinToggleAction()
{
    // 右クリック(VK_RBUTTON = 0x02) または シフトキー(VK_SHIFT = 0x10)
    return Input::GetKeyTrigger(KEY_SPIN_TOGGLE) || Input::GetKeyTrigger(0x10);
}
