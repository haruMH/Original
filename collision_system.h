#pragma once
#include <DirectXMath.h>

class Player;

// =================================================================
// 衝突判定・物理連鎖管理システム
// =================================================================
class CollisionSystem
{
public:
    // 毎フレームの衝突判定および連鎖処理を実行
    static void Update();

    // 電撃のチェイン処理（Affixからも呼び出し可能にするため公開）
    static void TriggerChainLightning(const DirectX::XMFLOAT3& startPos, Player* player);
};
