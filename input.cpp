#include "input.h"

BYTE Input::m_OldKeyState[256];
BYTE Input::m_KeyState[256];

void Input::Init()
{
    ZeroMemory(m_OldKeyState, sizeof(m_OldKeyState));
    ZeroMemory(m_KeyState, sizeof(m_KeyState));
}

void Input::Uninit()
{
}

void Input::Update()
{
    // 前回のフレームのキー状態を保存
    memcpy(m_OldKeyState, m_KeyState, sizeof(m_OldKeyState));
    
    // 現在のフレームのキー状態を取得
    GetKeyboardState(m_KeyState);
}

bool Input::GetKeyPress(int vKey)
{
    // 最上位ビットが1なら押されている
    return (m_KeyState[vKey] & 0x80) != 0;
}

bool Input::GetKeyTrigger(int vKey)
{
    // 今回押されていて、前回押されていなければトリガー
    return ((m_KeyState[vKey] & 0x80) != 0) && ((m_OldKeyState[vKey] & 0x80) == 0);
}
