#include "input.h"
#include "manager.h"
#include "imgui/imgui.h"

BYTE Input::m_OldKeyState[256];
BYTE Input::m_KeyState[256];
long Input::m_MouseMoveX = 0;
long Input::m_MouseMoveY = 0;
bool Input::m_MouseLocked = true; // デフォルトはロック状態

// カーソルの表示状態を安全に切り替えるヘルパー
static void SetCursorVisibility(bool visible) {
    static bool g_CursorVisible = true; // 初期状態は表示
    if (g_CursorVisible == visible) return;
    g_CursorVisible = visible;
    ShowCursor(visible ? TRUE : FALSE);
}

void Input::Init() {
    ZeroMemory(m_OldKeyState, 256);
    ZeroMemory(m_KeyState, 256);
    m_MouseLocked = true;
}

void Input::Uninit() {}

void Input::Update() {
    // --- キーボード状態の更新 ---
    memcpy(m_OldKeyState, m_KeyState, 256);
    GetKeyboardState(m_KeyState);

    // マウスの左右クリック状態を GetAsyncKeyState で確実に取得して上書き
    m_KeyState[VK_LBUTTON] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 0x80 : 0x00;
    m_KeyState[VK_RBUTTON] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 0x80 : 0x00;

    // アプリケーションウィンドウがアクティブかどうかを取得
    bool isWindowActive = (GetForegroundWindow() == GetWindow());

    // --- マウスロック制御の更新 ---
    // ウィンドウがアクティブな場合のみ、Escキーや左クリックによるロック切り替えを処理する
    if (isWindowActive) {
        // Escキーが押されたらロック解除
        if (GetKeyTrigger(VK_ESCAPE)) {
            m_MouseLocked = false;
        }

        // 左クリックされたらロック状態に戻す（ただし、ImGui操作中を除く）
        if (!m_MouseLocked && GetKeyTrigger(VK_LBUTTON)) {
            if (ImGui::GetCurrentContext() == nullptr || !ImGui::GetIO().WantCaptureMouse) {
                // クリック位置がウィンドウのクライアント領域内にあるかチェック
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(GetWindow(), &pt);
                RECT rect;
                GetClientRect(GetWindow(), &rect);
                if (PtInRect(&rect, pt)) {
                    m_MouseLocked = true;
                }
            }
        }
    }

    // --- マウス位置の更新 ---
    POINT point;
    GetCursorPos(&point); // スクリーン座標でのマウス位置取得

    // ロックが必要か判定（ウィンドウがアクティブで、かつm_MouseLockedがtrueで、テストシーンではない場合のみロック）
    bool shouldLock = isWindowActive && m_MouseLocked && (Manager::GetCurrentScene() != Scene::SHADER_TEST);

    if (!shouldLock) {
        m_MouseMoveX = 0;
        m_MouseMoveY = 0;
        // カーソルを表示する
        SetCursorVisibility(true);
    }
    else {
        // カーソルを非表示にする
        SetCursorVisibility(false);

        // 自ウィンドウのクライアント領域の中心（スクリーン座標）を計算
        // これによりマルチディスプレイ環境やウィンドウモードで移動した場合でも正しく動作します
        RECT rect;
        GetClientRect(GetWindow(), &rect);
        POINT center = { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
        ClientToScreen(GetWindow(), &center);

        // 中央位置からの差分を移動量とする
        m_MouseMoveX = point.x - center.x;
        m_MouseMoveY = point.y - center.y;

        // マウスカーソルを中央に戻す（これによって無限に回転できるようにする）
        SetCursorPos(center.x, center.y);
    }
}

bool Input::GetKeyPress(int k) { return (m_KeyState[k] & 0x80) != 0; }
bool Input::GetKeyTrigger(int k) { return ((m_KeyState[k] & 0x80) != 0) && ((m_OldKeyState[k] & 0x80) == 0); }