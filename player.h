#pragma once
#include "gameobject.h"

class Enemy;

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
    int         m_MarkerTimer = 0;         // マーカー用アニメーションタイマー

    void UpdateIdle();
    void UpdateGrabbed();
    void UpdateSpinning();

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    PlayerState GetState() const { return m_State; }
    Enemy* GetGrabbedEnemy() const { return m_GrabbedEnemy; }
    void   SetGrabbedEnemy(Enemy* enemy) { m_GrabbedEnemy = enemy; m_State = PlayerState::GRABBED; }
    float  GetAngularVelocity() const { return m_AngularVelocity; }
    bool   HasVacuumItem() const { return m_HasVacuumItem; }
    void   SetHasVacuumItem(bool enable) { m_HasVacuumItem = enable; }
    void   Throw();
};
