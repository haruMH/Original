#pragma once

// =================================================================
// ゲームルール・スコア進行管理クラス
// =================================================================
class GameRule
{
private:
    static int  m_Score;         // 現在のスコア
    static int  m_TotalEnemies;   // 生成した敵の総数
    static int  m_DefeatedCount;  // 撃破した敵の数
    static bool m_IsGameClear;    // ゲームクリアフラグ

public:
    // 初期化
    static void Init();

    // スコア加算
    static void AddScore(int score);

    // 敵撃破時のイベント処理
    static void OnEnemyDefeated(int scoreValue);

    // ゲッターとセッター
    static int  GetScore()         { return m_Score; }
    static int  GetDefeatedCount() { return m_DefeatedCount; }
    static int  GetTotalEnemies()  { return m_TotalEnemies; }
    static void SetTotalEnemies(int count) { m_TotalEnemies = count; }
    static bool IsGameClear()      { return m_IsGameClear; }
    static void SetGameClear(bool clear) { m_IsGameClear = clear; }
};
