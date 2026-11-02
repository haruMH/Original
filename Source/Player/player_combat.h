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
    int     m_NonSpinThrowCooldown = 0; // 非回転投げの残りクールダウンフレーム
    int     m_LastChargeStage = 0;      // 段階到達演出用の前フレームのチャージ段階

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

    // 非回転投げのクールダウン状態（UI表示用）
    int GetNonSpinThrowCooldown() const { return m_NonSpinThrowCooldown; }

    // スピン速度からチャージ段階(0〜4)を算出
    static int CalcChargeStage(float spinSpeed);
    // 現在のチャージ段階（SPINNING中のみ有効、それ以外は0）
    int GetChargeStage() const;

private:
    // 各プレイヤー状態ごとの戦闘アクション更新
    void UpdateIdle();
    void UpdateGrabbed();
    void UpdateSpinning();

    // スローモーション中のロックオン対象探索
    void FindLockOnTarget();
};
