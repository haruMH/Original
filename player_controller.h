#pragma once
#include <directxmath.h>

// プレイヤーの生のキー入力を論理的なアクションに変換する静的クラス
// キー設定をここに一元化することで、キー変更時の修正箇所を最小にする
class PlayerController
{
private:
    // キー割り当て
    static const int KEY_MOVE_FORWARD  = 'W';
    static const int KEY_MOVE_BACKWARD = 'S';
    static const int KEY_MOVE_LEFT     = 'A';
    static const int KEY_MOVE_RIGHT    = 'D';
    static const int KEY_GRAB_THROW    = 0x01; // VK_LBUTTON
    static const int KEY_SPIN_TOGGLE   = 0x02; // VK_RBUTTON

public:
    // WASD + カメラのヨー角を考慮した移動ベクトルを返す（正規化済み）
    static DirectX::XMFLOAT3 GetMoveDirection(float camYaw);

    // 「掴む / 投げる」アクション（左クリック）が押されたか
    static bool IsGrabOrThrowAction();

    // 「スピン切り替え」アクション（右クリック）が押されたか
    static bool IsSpinToggleAction();
};
