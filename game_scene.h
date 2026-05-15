#pragma once
#include "scene.h"

class GameScene : public Scene {
private:
    int  m_Score          = 0;     // 現在のスコア
    int  m_TotalEnemies   = 0;     // 生成した敵の合計数（壁除く）
    int  m_DefeatedCount  = 0;     // 撃破した敵の数
    bool m_IsGameClear    = false; // クリアフラグ
    bool m_IsGameOver     = false; // ゲームオーバーフラグ（将来の拡張用）

    // スコアを加算して撃破カウントを増やす
    void OnEnemyDefeated(int scoreValue);

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    int  GetScore() const         { return m_Score; }
    int  GetDefeatedCount() const { return m_DefeatedCount; }
    bool IsGameClear() const      { return m_IsGameClear; }
};