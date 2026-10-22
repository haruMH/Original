#pragma once
#include <DirectXMath.h>
#include <vector>

class GameObject;
class Player;
class Enemy;
class SpatialGrid;

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

private:
    // プレイヤーとアイテムの衝突判定（各種アイテム取得）
    static void HandlePlayerItemPickup(Player* player, const std::vector<GameObject*>& items);

    // プレイヤーのスピン中におけるなぎ払い判定
    static void HandleSpinSweep(Player* player, SpatialGrid& grid);

    // 飛行エネミーが対象（壁・ボス・通常敵）に衝突したときの衝撃・ダメージ解決を一本化
    static bool ResolveFlyingEnemyImpact(
        Enemy*      flying,
        GameObject* target,
        Player*     player,
        bool&       explosionThisFrame);

    // 飛行エネミー vs 壁・ボス・通常敵の個別判定ヘルパー
    static void HandleFlyingVsWall(
        Enemy*                          flying,
        const std::vector<GameObject*>& walls,
        Player*                         player,
        bool&                           explosionThisFrame);

    static bool HandleFlyingVsBoss(
        Enemy*  flying,
        Player* player,
        bool&   explosionThisFrame);

    static void HandleFlyingVsEnemy(
        Enemy*       flying,
        Player*      player,
        SpatialGrid& grid,
        bool&        explosionThisFrame);
};
