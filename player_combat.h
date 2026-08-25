#pragma once
#include <directxmath.h>

class Player;
class Enemy;

struct HitInfo;

// =================================================================
// プレイヤー戦闘アクション管理クラス (PlayerCombat)
// =================================================================
// プレイヤーの「つかみ」「スピン」「投げ」「ガード」「パリィ」
// 「ロックオン」「スラッシュ」「タックル」など戦闘アクション全般を担当します。
class PlayerCombat
{
private:
    Player* m_Owner = nullptr;  // 所有者であるPlayerへのポインタ

public:
    PlayerCombat(Player* owner);
    ~PlayerCombat();

    void Init();
    
    // 戦闘アクションの毎フレーム更新
    void Update();

    // 被弾ダメージの適用
    void OnHit(const HitInfo& info);

    // パリィ成功時のカウンター実行
    void ExecuteParryCounter(DirectX::XMFLOAT3 bulletPos);

    // 投げアクション
    void Throw();

private:
    // 各プレイヤー状態ごとの戦闘アクション更新
    void UpdateIdle();
    void UpdateGrabbed();
    void UpdateSpinning();

    // スローモーション中のロックオン対象探索
    void FindLockOnTarget();
};
