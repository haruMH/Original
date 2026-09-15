#include "game_rule.h"
#include <string>
#include <cstdio>
#include <climits>
#include "main.h"

// 静的変数の実体定義
int  GameRule::m_Score = 0;
int  GameRule::m_TotalEnemies = 0;
int  GameRule::m_DefeatedCount = 0;
bool GameRule::m_IsGameClear = false;

int  GameRule::m_ElapsedFrames = 0;
int  GameRule::m_ClearTimeFrames = 0;
int  GameRule::m_BestTimeFrames = INT_MAX;
bool GameRule::m_IsTimerRunning = false;
bool GameRule::m_IsNewRecord = false;

// ─────────────────────────────────────────────
// 初期化
// ─────────────────────────────────────────────
void GameRule::Init()
{
    m_Score           = 0;
    m_TotalEnemies    = 0;
    m_DefeatedCount   = 0;
    m_IsGameClear     = false;

    m_ElapsedFrames   = 0;
    m_ClearTimeFrames = 0;
    m_IsTimerRunning  = true;
    m_IsNewRecord     = false;
}

void GameRule::StartTimer()
{
    m_IsTimerRunning = true;
}

void GameRule::StopTimer()
{
    if (m_IsTimerRunning) {
        m_IsTimerRunning = false;
        m_ClearTimeFrames = m_ElapsedFrames;

        if (m_ClearTimeFrames < m_BestTimeFrames) {
            m_BestTimeFrames = m_ClearTimeFrames;
            m_IsNewRecord = true;
        }
    }
}

void GameRule::UpdateTimer()
{
    if (m_IsTimerRunning) {
        m_ElapsedFrames++;
    }
}

void GameRule::ReduceTime(float seconds)
{
    int reduceFrames = static_cast<int>(seconds * 60.0f);
    m_ElapsedFrames -= reduceFrames;
    if (m_ElapsedFrames < 0) {
        m_ElapsedFrames = 0;
    }
}

std::string GameRule::FormatTime(int frames)
{
    if (frames < 0) frames = 0;
    int totalCentis = (frames * 100) / 60; // 1/100秒
    int centis = totalCentis % 100;
    int totalSec = totalCentis / 100;
    int sec = totalSec % 60;
    int min = totalSec / 60;

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d.%02d", min, sec, centis);
    return std::string(buf);
}

std::string GameRule::GetCurrentFormattedTime()
{
    return FormatTime(m_ElapsedFrames);
}

std::string GameRule::GetClearFormattedTime()
{
    return FormatTime(m_ClearTimeFrames);
}

std::string GameRule::GetBestFormattedTime()
{
    if (m_BestTimeFrames == INT_MAX) {
        return "--:--.--";
    }
    return FormatTime(m_BestTimeFrames);
}

// ─────────────────────────────────────────────
// スコア加算（互換）
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
}

