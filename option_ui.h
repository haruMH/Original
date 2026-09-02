#pragma once

// =================================================================
// オプション設定画面 UI (OptionUI)
// =================================================================
// Tab キーで開閉する設定メニュー。
// 音量（BGM / SE）の調整や、マウス視点感度・上下反転の設定を提供します。
class OptionUI
{
private:
    static bool m_IsOpen;

public:
    static void Init();
    static void Update();
    static void Draw();

    static bool IsOpen() { return m_IsOpen; }
    static void SetOpen(bool open);
};
