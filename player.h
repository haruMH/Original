#pragma once
#include "gameobject.h"
#include <vector>
#include <list>

// 前方宣言
class Enemy;
class PlayerMovement;
class PlayerCombat;
class PlayerVisual;

// 稲妻エフェクト用のデータ構造
struct LightningSegment {
    DirectX::XMFLOAT3 Start;
    DirectX::XMFLOAT3 End;
};

struct LightningEffect {
    std::vector<LightningSegment> Segments;
    int Timer; // 残り表示フレーム数
};

// プレイヤーの状態定義
enum class PlayerState {
    IDLE,       // 何も掴んでいない通常状態
    GRABBED,    // 敵を掴んで狙いを定めている状態
    SPINNING    // 掴んだ敵を自動スピンで振り回している状態
};

// =================================================================
// プレイヤーオブジェクトクラス (Player)
// =================================================================
// プレイヤーの本体コアクラス。状態管理やHPの保持、および
// 役割ごとに分割されたサブモジュール（移動・戦闘・描画）を統合します。
class Player : public GameObject
{
private:
    ID3D11ShaderResourceView* m_Texture = nullptr;

    // サブモジュールへのポインタ
    PlayerMovement* m_Movement = nullptr;
    PlayerCombat*   m_Combat   = nullptr;
    PlayerVisual*   m_Visual   = nullptr;

    // プレイヤーの現在の状態
    PlayerState m_State = PlayerState::IDLE;
    Enemy*      m_GrabbedEnemy = nullptr;       // 現在つかんでいる敵
    float       m_AngularVelocity = 0.0f;       // 旋回速度（遠心力計算用）
    bool        m_IsAutoSpinning = false;        // 自動スピンフラグ
    float       m_CurrentSpinSpeed = 0.0f;       // 現在の自動スピン速度
    
    // アイテム取得フラグ
    bool        m_HasVacuumItem = false;
    bool        m_HasGigantItem = false;
    bool        m_HasLightningItem = false;
    
    int         m_MarkerTimer = 0;              // ターゲットマークのアニメーション用タイマー
    std::list<LightningEffect> m_LightningEffects; // 放電・連鎖雷電エフェクトのリスト

    // ステータスとタイマー
    int         m_HP               = 5;
    int         m_MaxHP            = 5;
    int         m_DamageTimer      = 0;         // スタン（被弾硬直）タイマー
    int         m_InvincibleTimer  = 0;         // 無敵タイマー
    int         m_GuardTimer       = 0;         // ガード継続時間
    Enemy*      m_LockOnTarget     = nullptr;   // ロックオン中の敵
    int         m_LockOnFrame      = 0;         // ロックオン確定からの経過フレーム数
    int         m_WarpSlashCount   = 0;         // テレポートスラッシュのチェイン回数
    bool        m_CanWarpSlash     = true;      // テレポートスラッシュの実行可否
    int         m_TackleTimer      = 0;         // タックル有効時間

private:
    // 復活処理
    void Restart();

public:
    // イナズマ描画（他クラス・既存呼び出しへの互換性のためにラッパーとして残す）
    void DrawLightningBolt(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color);
    void DrawLightningBoltInternal(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color, bool drawBranches, int seedOffset = 0);

    // GameObjectとしてのライフサイクルメソッド
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    // ゲッターとセッター
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
    
    bool   IsDashing() const;
    bool   IsJustDodgeActive() const;
    void   ResetDashCooldown();

    // タックル攻撃関連
    void   EnableTackle(int frames = 90) { m_TackleTimer = frames; }
    bool   IsInTackle() const { return m_TackleTimer > 0; }

    // 放電エフェクトの追加
    void AddLightningEffect(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end);

    // 投擲
    void   Throw();

    void OnHit(const HitInfo& info) override;

    // ステータス取得
    int  GetHP() const { return m_HP; }
    void SetHP(int hp) { m_HP = hp; }
    int  GetMaxHP() const { return m_MaxHP; }
    bool IsInvincible() const { return m_InvincibleTimer > 0; }
    bool IsStunned() const { return m_DamageTimer > 0; }
    bool IsGuardActive() const { return m_GuardTimer > 0; }
    bool IsParryActive() const;
    
    DirectX::XMFLOAT3 GetForwardVector() const;
    DirectX::XMFLOAT3 GetEmissive() const override;
    Enemy* GetLockOnTarget() const { return m_LockOnTarget; }
    int    GetLockOnFrame() const { return m_LockOnFrame; }
    
    static ObjectType GetStaticType() { return ObjectType::Player; }
    ObjectType GetObjectType() const override { return GetStaticType(); }
    
    void NotifyObjectDestroyed(GameObject* obj);
    void ExecuteParryCounter(DirectX::XMFLOAT3 bulletPos);
    void DisableWarpSlash() { m_CanWarpSlash = false; }

    // ゲッター・セッター（カプセル化対応）
    void        SetState(PlayerState state) { m_State = state; }
    void        SetAngularVelocity(float omega) { m_AngularVelocity = omega; }
    bool        IsAutoSpinning() const { return m_IsAutoSpinning; }
    void        SetAutoSpinning(bool enable) { m_IsAutoSpinning = enable; }
    float       GetCurrentSpinSpeed() const { return m_CurrentSpinSpeed; }
    void        SetCurrentSpinSpeed(float speed) { m_CurrentSpinSpeed = speed; }
    int         GetMarkerTimer() const { return m_MarkerTimer; }
    void        IncrementMarkerTimer() { m_MarkerTimer++; }
    std::list<LightningEffect>& GetLightningEffects() { return m_LightningEffects; }
    const std::list<LightningEffect>& GetLightningEffects() const { return m_LightningEffects; }
    int         GetDamageTimer() const { return m_DamageTimer; }
    void        SetDamageTimer(int timer) { m_DamageTimer = timer; }
    int         GetInvincibleTimer() const { return m_InvincibleTimer; }
    void        SetInvincibleTimer(int timer) { m_InvincibleTimer = timer; }
    int         GetGuardTimer() const { return m_GuardTimer; }
    void        SetGuardTimer(int timer) { m_GuardTimer = timer; }
    void        IncrementGuardTimer() { m_GuardTimer++; }
    void        SetLockOnTarget(Enemy* target) { m_LockOnTarget = target; }
    void        SetLockOnFrame(int frame) { m_LockOnFrame = frame; }
    void        IncrementLockOnFrame() { m_LockOnFrame++; }
    int         GetWarpSlashCount() const { return m_WarpSlashCount; }
    void        SetWarpSlashCount(int count) { m_WarpSlashCount = count; }
    void        IncrementWarpSlashCount() { m_WarpSlashCount++; }
    bool        CanWarpSlash() const { return m_CanWarpSlash; }
    void        SetCanWarpSlash(bool enable) { m_CanWarpSlash = enable; }
    int         GetTackleTimer() const { return m_TackleTimer; }
    void        SetTackleTimer(int timer) { m_TackleTimer = timer; }
    void        DecrementTackleTimer() { if (m_TackleTimer > 0) m_TackleTimer--; }

    PlayerMovement* GetMovementModule() { return m_Movement; }
    PlayerCombat*   GetCombatModule() { return m_Combat; }
    PlayerVisual*   GetVisualModule() { return m_Visual; }
};
