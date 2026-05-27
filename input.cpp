#include "input.h"

BYTE Input::m_OldKeyState[256];
BYTE Input::m_KeyState[256];
long Input::m_MouseMoveX = 0;
long Input::m_MouseMoveY = 0;

void Input::Init() {
    ZeroMemory(m_OldKeyState, 256);
    ZeroMemory(m_KeyState, 256);
}

void Input::Uninit() {}

void Input::Update() {
    // --- キーボード状態の更新 ---
    memcpy(m_OldKeyState, m_KeyState, 256);
    GetKeyboardState(m_KeyState);

    // マウスの左右クリック状態を GetAsyncKeyState で確実に取得して上書き
    m_KeyState[VK_LBUTTON] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 0x80 : 0x00;
    m_KeyState[VK_RBUTTON] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 0x80 : 0x00;

    // --- マウス位置の更新 ---
    POINT point;
    GetCursorPos(&point); // スクリーン座標でのマウス位置取得

    // 画面中央の位置を計算（SCREEN_WIDTHなどはmain.hで定義されている前提）
    // 本来はウィンドウの中心を取得すべきですが、ここでは簡易的にデスクトップから算出します
    int centerX = GetSystemMetrics(SM_CXSCREEN) / 2;
    int centerY = GetSystemMetrics(SM_CYSCREEN) / 2;

    // 中央位置からの差分を移動量とする
    m_MouseMoveX = point.x - centerX;
    m_MouseMoveY = point.y - centerY;

    // マウスカーソルを中央に戻す（これによって無限に回転できるようにする）
    SetCursorPos(centerX, centerY);
}

bool Input::GetKeyPress(int k) { return (m_KeyState[k] & 0x80) != 0; }
bool Input::GetKeyTrigger(int k) { return ((m_KeyState[k] & 0x80) != 0) && ((m_OldKeyState[k] & 0x80) == 0); }