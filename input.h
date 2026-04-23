#pragma once
#include "main.h"

class Input
{
private:
    static BYTE m_OldKeyState[256];
    static BYTE m_KeyState[256];

public:
    static void Init();
    static void Uninit();
    static void Update();

    // キーが押されているか
    static bool GetKeyPress(int vKey);
    // キーが押された瞬間か
    static bool GetKeyTrigger(int vKey);
};
