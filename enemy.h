#pragma once
#include "gameobject.h"

enum class EnemyState {
    NORMAL,    // 通常状態
    GRABBED,   // プレイヤーにつかまれている状態
    FLYING,    // 投げられて飛んでいる状態
    CHASING,   // プレイヤーを追尾している状態
    DEFEATED,  // 撃破されて消滅演出中の状態
    VACUUMED,  // スピンに吸い込まれている状態
    BLOWN_AWAY // 爆発で吹き飛んでいる状態
};

class Enemy : public GameObject
{
protected:
    ID3D11ShaderResourceView* m_Texture      = nullptr;

    EnemyState m_EnemyState = EnemyState::NORMAL;
    XMFLOAT3   m_Velocity   = XMFLOAT3(0, 0, 0);
    float      m_VelocityY  = 0.0f;

    int        m_ScoreValue = 100;   // 撃破時のスコア加算値
    int        m_UprightTimer = 0;   // 着地後、起き上がるまでのタイマー
    bool       m_IsExplosive = false; // 爆発属性フラグ
    bool       m_IsLightning = false; // 雷電属性フラグ
    bool       m_IsDefeatedCounted = false; // 二重撃破ガード用フラグ

public:
    void Init()   override;
    void Uninit() override;
    void Update() override;
    void Draw()   override;

    EnemyState GetEnemyState() const         { return m_EnemyState; }
    void       SetEnemyState(EnemyState s)   { m_EnemyState = s; }
    void       SetVelocity(XMFLOAT3 v)       { m_Velocity = v; m_VelocityY = v.y; }
    XMFLOAT3   GetVelocity() const           { return m_Velocity; }
    
    bool       IsExplosive() const           { return m_IsExplosive; }
    void       SetExplosive(bool explosive)  { m_IsExplosive = explosive; }

    bool       IsLightning() const           { return m_IsLightning; }
    void       SetLightning(bool lightning)  { m_IsLightning = lightning; }

    // スコア値の取得・設定
    int        GetScoreValue() const         { return m_ScoreValue; }
    void       SetScoreValue(int v)          { m_ScoreValue = v; }
    ObjectType GetObjectType() const override { return ObjectType::Enemy; }

    // 撃破処理（二重カウント防止機能付き）
    void       Defeat(float colorR = 2.5f, float colorG = 1.8f, float colorB = 0.0f);
    bool       IsDefeatedCounted() const     { return m_IsDefeatedCounted; }

    // 攻撃エネミー判定用（dynamic_cast排除の最適化）
    virtual bool IsAttackingEnemy() const    { return false; }
};