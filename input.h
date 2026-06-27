#pragma once
#include "main.h"

class Input
{
private:
    static BYTE m_OldKeyState[256];
    static BYTE m_KeyState[256];

    // 追加：マウスの移動量
    static long m_MouseMoveX;
    static long m_MouseMoveY;

    // 追加：マウスがロックされているかフラグ
    static bool m_MouseLocked;

public:
    static void Init();
    static void Uninit();
    static void Update();

    static bool GetKeyPress(int vKey);
    static bool GetKeyTrigger(int vKey);

    // 追加：マウス移動量取得用ゲッター
    static long GetMouseMoveX() { return m_MouseMoveX; }
    static long GetMouseMoveY() { return m_MouseMoveY; }

    // 追加：マウスロック状態制御用
    static bool IsMouseLocked() { return m_MouseLocked; }
    static void SetMouseLocked(bool lock) { m_MouseLocked = lock; }
};