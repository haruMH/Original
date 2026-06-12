#pragma once
#include "gameobject.h"
#include <vector>
#include <list>

class Enemy;

// 稲妻エフェクト用のデータ構造
struct LightningSegment {
    DirectX::XMFLOAT3 Start;
    DirectX::XMFLOAT3 End;
};

struct LightningEffect {
    std::vector<LightningSegment> Segments;
    int Timer; // 残り表示フレーム数
};

enum class PlayerState {
    IDLE,       // 何も掴んでいない通常状態
    GRABBED,    // 敵を掴んで狙いを定めている状態
    SPINNING    // 掴んだ敵を自動スピンで振り回している状態
};

class Player : public GameObject
{
private:
    ID3D11ShaderResourceView* m_Texture = nullptr;

    PlayerState m_State = PlayerState::IDLE;
    Enemy*      m_GrabbedEnemy = nullptr;
    float       m_AngularVelocity = 0.0f; // プレイヤーの旋回速度（遠心力用）
    bool        m_IsAutoSpinning = false;  // 右クリックによる自動回転状態フラグ
    float       m_CurrentSpinSpeed = 0.0f; // 現在の自動スピン速度（徐々に加速させるため）
    bool        m_HasVacuumItem = false;   // 吸引アイテム取得フラグ
    bool        m_HasGigantItem = false;   // 巨大化アイテム取得フラグ
    bool        m_HasLightningItem = false;  // 雷電アイテム取得フラグ
    int         m_MarkerTimer = 0;         // マーカー用アニメーションタイマー

    std::list<LightningEffect> m_LightningEffects; // 放電・連鎖雷電エフェクトのリスト

    int         m_HP               = 5;         // プレイヤーの現在HP
    int         m_MaxHP            = 5;         // プレイヤーの最大HP
    int         m_DamageTimer      = 0;         // 被弾スタンタイマー
    int         m_InvincibleTimer  = 0;         // 被弾無敵タイマー
    XMFLOAT3    m_KnockbackVelocity = XMFLOAT3(0, 0, 0); // ノックバック速度
    int         m_GuardTimer       = 0;         // ガード入力フレームタイマー（0:非ガード、>0:ガード中）

    void UpdateIdle();
    void UpdateGrabbed();
    void UpdateSpinning();
    void Restart();

public:
    // 2点間にジグザグの稲妻を描画する内部ヘルパー（他クラスからも呼べるようpublic）
    void DrawLightningBolt(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color);
    void DrawLightningBoltInternal(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color, bool drawBranches, int seedOffset = 0);

    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    PlayerState GetState() const { return m_State; }
    Enemy* GetGrabbedEnemy() const { return m_GrabbedEnemy; }
    void   SetGrabbedEnemy(Enemy* enemy);
    float  GetAngularVelocity() const { return m_AngularVelocity; }
    
    bool   HasVacuumItem() const { return m_HasVacuumItem; }
    void   SetHasVacuumItem(bool enable) { m_HasVacuumItem = enable; }
    
    bool   HasGigantItem() const { return m_HasGigantItem; }
    void   SetHasGigantItem(bool enable) { m_HasGigantItem = enable; }
    
    bool   HasLightningItem() const { return m_HasLightningItem; }
    void   SetHasLightningItem(bool enable) { m_HasLightningItem = enable; }

    // 稲妻エフェクトの追加
    void AddLightningEffect(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end);

    void   Throw();

    // 戦闘システム用
    void ApplyDamage(int damage, const DirectX::XMFLOAT3& enemyPos);
    int  GetHP() const { return m_HP; }
    int  GetMaxHP() const { return m_MaxHP; }
    bool IsInvincible() const { return m_InvincibleTimer > 0; }
    bool IsStunned() const { return m_DamageTimer > 0; }
    bool IsGuardActive() const { return m_GuardTimer > 0; }
    bool IsParryActive() const { return m_GuardTimer > 0 && m_GuardTimer <= 12; }
    DirectX::XMFLOAT3 GetForwardVector() const;
    DirectX::XMFLOAT3 GetEmissive() const override;
    ObjectType GetObjectType() const override { return ObjectType::Player; }
};

