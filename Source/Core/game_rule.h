#pragma once
#include <string>

// =================================================================
// ゲームルール・タイムアタック進行管理クラス
// =================================================================
class GameRule
{
private:
    static int   m_Score;            // 互換用スコア
    static int   m_TotalEnemies;     // 生成した敵の総数
    static int   m_DefeatedCount;    // 撃破した敵の数
    static bool  m_IsGameClear;      // ゲームクリアフラグ

    // タイムアタック計測変数
    static int   m_ElapsedFrames;    // 経過フレーム（60fps）
    static int   m_ClearTimeFrames;  // 確定クリアタイム（フレーム数）
    static int   m_BestTimeFrames;   // セッション内ベストクリアタイム（フレーム数）
    static bool  m_IsTimerRunning;   // タイマー稼働中フラグ
    static bool  m_IsNewRecord;      // 自己最速更新フラグ

public:
    // 初期化
    static void Init();

    // タイマー制御
    static void StartTimer();
    static void StopTimer();
    static void UpdateTimer();
    static void ReduceTime(float seconds); // タイム短縮ボーナス（雑魚撃破 -1.0s, パリィ -2.0s 等）

    // タイムフォーマット変換（フレーム数 -> "01:23.45"）
    static std::string FormatTime(int frames);
    static std::string GetCurrentFormattedTime();
    static std::string GetClearFormattedTime();
    static std::string GetBestFormattedTime();

    // スコア加算（互換用）
    static void AddScore(int score);

    // 敵撃破時のイベント処理
    static void OnEnemyDefeated(int scoreValue);

    // ゲッターとセッター
    static int   GetElapsedFrames()   { return m_ElapsedFrames; }
    static int   GetClearTimeFrames() { return m_ClearTimeFrames; }
    static int   GetBestTimeFrames()  { return m_BestTimeFrames; }
    static bool  IsNewRecord()        { return m_IsNewRecord; }
    static bool  IsTimerRunning()     { return m_IsTimerRunning; }

    static int   GetScore()           { return m_Score; }
    static int   GetDefeatedCount()   { return m_DefeatedCount; }
    static int   GetTotalEnemies()    { return m_TotalEnemies; }
    static void  SetTotalEnemies(int count) { m_TotalEnemies = count; }
    static bool  IsGameClear()        { return m_IsGameClear; }
    static void  SetGameClear(bool clear)   { m_IsGameClear = clear; }
};
