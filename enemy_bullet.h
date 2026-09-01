#pragma once
#include "gameobject.h"

// =================================================================
// エネミーの弾クラス
// =================================================================
class EnemyBullet : public GameObject
{
private:
    static constexpr float BULLET_SPEED = 0.20f; // 移動速度（少し避けやすく調整）
    static constexpr int   BULLET_LIFE  = 180;   // 寿命（フレーム数。3秒）

    XMFLOAT3 m_Direction = XMFLOAT3(0.0f, 0.0f, 1.0f); // 移動方向
    float    m_Speed     = BULLET_SPEED;                // 現在の速度
    int      m_Life      = BULLET_LIFE;                 // 残り寿命
    bool     m_IsPlayerOwned = false;                   // プレイヤーが反射した味方弾フラグ
    bool     m_IsBossBullet  = false;                   // ボスが発射した弾フラグ
    XMFLOAT3 m_EmissiveColor = XMFLOAT3(2.5f, 0.5f, 0.0f); // 自己発光カラー
    int      m_Damage = 2;                              // 反射弾の攻撃力（デフォルト2）

public:
    void Init()   override;
    void Uninit() override;
    void Update() override;
    void Draw()   override;

    // 移動方向の設定
    void SetDirection(XMFLOAT3 dir) { m_Direction = dir; }

    // 移動速度の設定
    void SetSpeed(float speed) { m_Speed = speed; }

    // 味方弾フラグの設定・取得
    void SetPlayerOwned(bool owned) { m_IsPlayerOwned = owned; }
    bool IsPlayerOwned() const { return m_IsPlayerOwned; }

    // ボス弾フラグの設定・取得
    void SetIsBossBullet(bool b) { m_IsBossBullet = b; }
    bool IsBossBullet() const { return m_IsBossBullet; }

    // 寿命（ライフタイム）の設定
    void SetLife(int life) { m_Life = life; }

    // 自己発光カラーの設定
    void SetEmissiveColor(XMFLOAT3 color) { m_EmissiveColor = color; }

    // 反射弾の攻撃力設定・取得
    void SetDamage(int damage) { m_Damage = damage; }
    int  GetDamage() const { return m_Damage; }

    // 自発光（Emissive）情報の取得
    XMFLOAT3 GetEmissive() const override { return m_EmissiveColor; }
    static ObjectType GetStaticType() { return ObjectType::Bullet; }
    ObjectType GetObjectType() const override { return GetStaticType(); }

public:
    // メモリプールの初期化・解放
    static void InitPool();
    static void UninitPool();

    // 高速アロケーション用プールアロケータ
    void* operator new(size_t size);
    void operator delete(void* p);

private:
    static void* m_PoolMemory;
    static bool* m_UsedList;
    static const size_t POOL_SIZE = 256;
};
