#include "game_rule.h"
#include <string>
#include "main.h"

// 静的変数の実体定義
int  GameRule::m_Score = 0;
int  GameRule::m_TotalEnemies = 0;
int  GameRule::m_DefeatedCount = 0;
bool GameRule::m_IsGameClear = false;

// ─────────────────────────────────────────────
// 初期化
// ─────────────────────────────────────────────
void GameRule::Init()
{
    m_Score         = 0;
    m_TotalEnemies  = 0;
    m_DefeatedCount = 0;
    m_IsGameClear   = false;
}

// ─────────────────────────────────────────────
// スコア加算
// ─────────────────────────────────────────────
void GameRule::AddScore(int score)
{
    m_Score += score;
}

// ─────────────────────────────────────────────
// 敵撃破時の処理
// ─────────────────────────────────────────────
void GameRule::OnEnemyDefeated(int scoreValue)
{
    m_Score += scoreValue;
    m_DefeatedCount++;

    // デバッグ出力
    std::string msg = "[GameRule] 撃破! スコア: " + std::to_string(m_Score)
                    + " / 撃破数: " + std::to_string(m_DefeatedCount)
                    + " / 合計: "  + std::to_string(m_TotalEnemies) + "\n";
    OutputDebugStringA(msg.c_str());
}
