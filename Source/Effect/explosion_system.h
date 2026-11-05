#pragma once
#include "main.h"

// =================================================================
// 爆発エフェクト＆物理連鎖処理システム
// =================================================================
class ExplosionSystem
{
public:
    // 指定座標を中心に爆発を発生させ、周囲の敵を吹き飛ばす
    static void TriggerExplosion(const DirectX::XMFLOAT3& center, float power = 1.0f); // power = hit effect power (0-1)
};
