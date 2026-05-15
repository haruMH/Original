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
    // --- キーボード更新 ---
    memcpy(m_OldKeyState, m_KeyState, 256);
    GetKeyboardState(m_KeyState);

    // --- マウス更新 ---
    POINT point;
    GetCursorPos(&point); // スクリーン座標でのマウス位置取得

    // 画面中央の座標を計算（SCREEN_WIDTHなどはmain.hで定義されている前提）
    // 本来はウィンドウの中心を取得すべきですが、まずは簡易的にデスクトップ中央付近
    int centerX = GetSystemMetrics(SM_CXSCREEN) / 2;
    int centerY = GetSystemMetrics(SM_CYSCREEN) / 2;

    // 中央からの差分を移動量とする
    m_MouseMoveX = point.x - centerX;
    m_MouseMoveY = point.y - centerY;

    // マウスを中央に戻す（これによって無限に回転し続けられる）
    SetCursorPos(centerX, centerY);
}

bool Input::GetKeyPress(int k) { return (m_KeyState[k] & 0x80) != 0; }
bool Input::GetKeyTrigger(int k) { return ((m_KeyState[k] & 0x80) != 0) && ((m_OldKeyState[k] & 0x80) == 0); }