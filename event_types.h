#pragma once
#include <directxmath.h>

class Enemy;

// =================================================================
// イベントシステム用の各種イベントデータ構造定義
// =================================================================

// 1. プレイヤーがダメージを受けた時のイベント
struct PlayerHitEvent
{
    int damage;                        // ダメージ量
    DirectX::XMFLOAT3 hitSourcePos;    // 被弾の起点となった座標
};

// 2. 敵が撃破された時のイベント (スコア加算、ポップアップ等)
struct EnemyDefeatedEvent
{
    int scoreValue;                    // スコア加算値
    DirectX::XMFLOAT3 position;        // 撃破された座標
    DirectX::XMFLOAT3 popupColor;      // スコアポップアップの表示色 (RGB)
};

// 3. ボスがダメージを受けた時のイベント
struct BossHitEvent
{
    int damage;                        // ダメージ量
    DirectX::XMFLOAT3 hitSourcePos;    // 攻撃の起点となった座標
};

// 4. プレイヤーがパリィ（反射）に成功した時のイベント
struct PlayerParriedEvent
{
    DirectX::XMFLOAT3 position;        // パリィが発生した座標
};

// 5. ボスが登場（カットシーン開始）した時のイベント
struct BossSpawnEvent
{
    DirectX::XMFLOAT3 bossPosition;    // ボスのスポーン位置
};

// 6. ボス登場カットシーンが終了し、戦闘が開始された時のイベント
struct BossBattleStartEvent
{
};
