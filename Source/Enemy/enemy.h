#pragma once
#include "gameobject.h"
#include <memory>

class IEnemyAffix;

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
    bool       m_IsDefeatedCounted = false; // 二重撃破ガード用フラグ

    // カービィディスカバリー風ヒット演出（Squash & Stretch、被弾フラッシュ）
    int        m_HitFlashTimer = 0;  // 白発光タイマー
    int        m_HitSquashTimer = 0; // モデル潰れ演出タイマー
    XMFLOAT3   m_OriginalScale = XMFLOAT3(1.0f, 1.0f, 1.0f); // 基準スケール

    bool       m_IsThrownByPlayer = false; // プレイヤーによって投げられた弾かどうかの判定フラグ
    float      m_ThrowSpinRatio   = 0.0f;  // 投擲時のスピン速度比率 (0.0f〜1.0f)

public:
    void Init()   override;
    void Uninit() override;
    void Update() override;
    void Draw()   override;

    EnemyState GetEnemyState() const         { return m_EnemyState; }
    void       SetEnemyState(EnemyState s)   { m_EnemyState = s; }
    void       SetVelocity(XMFLOAT3 v)       { m_Velocity = v; m_VelocityY = v.y; }
    XMFLOAT3   GetVelocity() const           { return m_Velocity; }
    
    bool       IsExplosive() const;
    void       SetExplosive(bool explosive);

    bool       IsLightning() const;
    void       SetLightning(bool lightning);

    bool       IsSandbag() const;
    void       SetSandbag(bool enable, int life = 300); // life: チュートリアル用は INT_MAX を渡す

    bool       IsThrownByPlayer() const      { return m_IsThrownByPlayer; }
    void       SetThrownByPlayer(bool v)     { m_IsThrownByPlayer = v; }
    float      GetThrowSpinRatio() const     { return m_ThrowSpinRatio; }
    void       SetThrowSpinRatio(float r)    { m_ThrowSpinRatio = r; }

    // スコア値の取得・設定
    int        GetScoreValue() const         { return m_ScoreValue; }
    void       SetScoreValue(int v)          { m_ScoreValue = v; }
    void       SetUprightTimer(int t)        { m_UprightTimer = t; }
    static ObjectType GetStaticType() { return ObjectType::Enemy; }
    ObjectType GetObjectType() const override { return GetStaticType(); }
    XMFLOAT3   GetEmissive() const override;

    // 敵の目を描画する（通常パス時のみ）
    void       DrawEyes();

    // 撃破処理（二重カウント防止機能付き）
    void       Defeat(float colorR = 2.5f, float colorG = 1.8f, float colorB = 0.0f);
    void       OnHit(const HitInfo& info) override;
    bool       IsDefeatedCounted() const     { return m_IsDefeatedCounted; }

    // 攻撃エネミー判定用（dynamic_cast排除の最適化）
    virtual bool IsAttackingEnemy() const    { return false; }

    // 素材ドロップ生成
    virtual void SpawnMaterialDrops();

    // 属性オブジェクトのゲッター
    std::shared_ptr<IEnemyAffix> GetAffix() const { return m_Affix; }

protected:
    std::shared_ptr<IEnemyAffix> m_Affix; // 敵の属性オブジェクトへのポインタ
};
