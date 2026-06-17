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
    Enemy*      m_LockOnTarget     = nullptr;   // ロックオンしている敵へのポインタ
    int         m_LockOnFrame      = 0;         // ロックオン対象が確定してからの経過フレーム数
    int         m_WarpSlashCount   = 0;         // スローモーション中のテレポートスラッシュ回数
    float       m_MoveAnimation    = 0.0f;      // 移動アニメーション進捗フェーズ

    // ジャンプ・空中挙動
    float       m_VelocityY        = 0.0f;      // 垂直方向の速度
    bool        m_IsJumping        = false;     // ジャンプ中フラグ
    int         m_JumpCount        = 0;         // 二段ジャンプカウンター

    // 弾性（もちもち）アニメーション用
    float       m_ScaleVelocityX   = 0.0f;      // スケールXの変形速度
    float       m_ScaleVelocityY   = 0.0f;      // スケールYの変形速度
    float       m_ScaleVelocityZ   = 0.0f;      // スケールZの変形速度

    // ダッシュ・回避関連
    struct DashGhost {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT3 Rotation;
        DirectX::XMFLOAT3 Scale;
        float Alpha;
    };
    std::vector<DashGhost> m_DashGhosts;

    int         m_DashTimer        = 0;         // ダッシュ中タイマー（>0でダッシュ中）
    int         m_DashCooldown     = 0;         // ダッシュクールダウン
    DirectX::XMFLOAT3 m_DashDirection = DirectX::XMFLOAT3(0, 0, 0); // ダッシュ方向
    bool        m_IsDashing        = false;     // ダッシュ中フラグ

    void UpdateIdle();
    void UpdateGrabbed();
    void UpdateSpinning();
    void Restart();
    void FindLockOnTarget();

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
    bool   IsDashing() const { return m_IsDashing; }
    bool   IsJustDodgeActive() const { return m_IsDashing && (m_DashTimer >= 10); }

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
    Enemy* GetLockOnTarget() const { return m_LockOnTarget; }
    int    GetLockOnFrame() const { return m_LockOnFrame; }
    ObjectType GetObjectType() const override { return ObjectType::Player; }
};

