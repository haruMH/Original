#pragma once
#include <directxmath.h>

class Player;

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

    // 2点間にジグザグの稲妻を描画する（外部公開）
    void DrawLightningBolt(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color);

    // 稲妻描画の実体（再帰的な枝分かれやランダムノイズ計算）
    void DrawLightningBoltInternal(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color, bool drawBranches, int seedOffset = 0);
};
