#pragma once
#include "scene.h"
#include "render_system.h"

class GameScene : public Scene {
private:
    int  m_Score          = 0;     // 現在のスコア
    int  m_TotalEnemies   = 0;     // 生成した敵の合計数（壁除く）
    int  m_DefeatedCount  = 0;     // 撃破した敵の数
    bool m_IsGameClear    = false; // クリアフラグ
    bool m_IsGameOver     = false; // ゲームオーバーフラグ（将来の拡張用）
    RenderSystem m_RenderSystem;   // インスタンス描画管理システム

    // スコアを加算して撃破カウントを増やす
    void OnEnemyDefeated(int scoreValue);

    // 爆発を発生させ周囲の敵を吹き飛ばす
    void TriggerExplosion(const DirectX::XMFLOAT3& center);

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    int  GetScore() const         { return m_Score; }
    int  GetDefeatedCount() const { return m_DefeatedCount; }
    bool IsGameClear() const      { return m_IsGameClear; }
};