#pragma once
#include "main.h"

// =================================================================
// 環境パーティクル（静的クラス）
// =================================================================
// カメラ周囲を漂う粒子。昼は舞う埃、夜は蛍のように光る。
// 固定数の粒子をカメラ基準の箱の中で循環させるので、確保や生成は発生しない。
// シーン側が SetEnabled(true) にすると有効になり、Clear() で無効に戻る。
class AmbientEffect {
public:
    static void Init();
    static void Uninit();
    static void Clear();                 // シーン遷移時: 無効化
    static void SetEnabled(bool enable);
    static void Update();
    static void Draw();
};
