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

public:
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
