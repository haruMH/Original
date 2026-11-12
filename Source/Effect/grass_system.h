#pragma once
#include "main.h"

// =================================================================
// 風で揺れる草（静的クラス）
// =================================================================
// 頂点バッファなしのインスタンス描画。キャンプの外側の地面に草原を生やす。
// シーン側が SetEnabled(true) にすると描画され、Clear() で無効に戻る。
class GrassSystem {
public:
    static void Init();
    static void Uninit();
    static void Clear();                 // シーン遷移時: 無効化
    static void SetEnabled(bool enable);
    static void Update();
    static void Draw();
};
