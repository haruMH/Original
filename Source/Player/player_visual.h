#pragma once
#include <directxmath.h>

class Player;
class Enemy;

// =================================================================
// プレイヤー描画・エフェクト演出管理クラス (PlayerVisual)
// =================================================================
// ガイドライン（エイム）、残像（ダッシュ）、被弾時の放電、
// ガードシールド、ロックオンマーカー、およびジグザグの稲妻エフェクトの描画を担当します。
class PlayerVisual
{
private:
    Player* m_Owner = nullptr;  // 所有者であるPlayerへのポインタ

    // --- 伸縮(スクワッシュ&ストレッチ)演出: ばねで目標へ追従させる ---
    float   m_Stretch = 0.0f;       // 縦方向の伸び量(+で縦に伸び、-で潰れる)
    float   m_StretchVel = 0.0f;    // 伸縮の速度
    float   m_DeformTime = 0.0f;    // 呼吸・揺れ用の経過フレーム
    int     m_LastState = -1;       // 前フレームのプレイヤー状態（切替の瞬間に弾みを付ける）
    int     m_LastStage = 0;        // 前フレームのチャージ段階
    DirectX::XMFLOAT3 m_PrevPos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    bool    m_HasPrevPos = false;
    float   m_MoveSpeed = 0.0f;     // 水平移動速度(フレーム当たり)

public:
    // 描画専用の変形スケール/オフセット（当たり判定には影響しない）
    DirectX::XMFLOAT3 GetDeformScale() const;
    DirectX::XMFLOAT3 GetDeformOffset() const;

    PlayerVisual(Player* owner);
    ~PlayerVisual();


    void Init();
    
    // エフェクトの更新（寿命タイマー管理など）
    void Update();

    // エフェクトの描画（通常パス時のみ実行）
    void Draw();

private:
    // 正面方向を明示するフェイス（両目）パーツの描画
    void DrawEyes();

    // ロックオン対象の敵に専用ターゲットサイト（枠・ポインター・エイムライン）を描画
    void DrawLockOnReticle(Enemy* target);

public:

    // 2点間にジグザグの稲妻を描画する（静的共通エフェクト関数）
    static void DrawLightningBolt(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color);

    // 稲妻描画の実体（静的共通エフェクト関数）
    static void DrawLightningBoltInternal(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color, bool drawBranches, int seedOffset = 0);
};
