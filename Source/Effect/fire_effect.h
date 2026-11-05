#pragma once
#include "main.h"

// =================================================================
// 焚き火の炎エフェクト（静的クラス）
// =================================================================
// 加算合成のビルボード粒子で、炎・火の粉・周囲の淡い光を描く。
// シーン側が SetEmitter() で位置を指定すると燃え続け、Clear() で消える。
class FireEffect {
public:
    static void Init();
    static void Uninit();
    static void Clear();                              // シーン遷移時: 消火して粒子を全消去
    static void SetEmitter(const XMFLOAT3& basePos);  // 点火（炎の根元のワールド座標）
    static void Update();
    static void Draw();
};
