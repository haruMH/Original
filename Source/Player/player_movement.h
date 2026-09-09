#pragma once
#include <vector>
#include <directxmath.h>

class Player;

// =================================================================
// プレイヤー移動・物理管理クラス (PlayerMovement)
// =================================================================
// プレイヤーの「歩き」「ジャンプ」「ダッシュ（回避）」および
// もちもち変形演出のためのスプリング物理計算を担当します。
class PlayerMovement
{
public:
    // ダッシュ時の残像（ゴースト）描画用データ構造
    struct DashGhost {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT3 Rotation;
        DirectX::XMFLOAT3 Scale;
        float Alpha;
    };

private:
    Player* m_Owner = nullptr;  // 所有者であるPlayerへのポインタ

    // ジャンプ・空中挙動パラメータ
    float       m_VelocityY        = 0.0f;      // 垂直（Y）方向 of 速度
    bool        m_IsJumping        = false;     // 現在ジャンプ中かどうかのフラグ
    int         m_JumpCount        = 0;         // 空中ジャンプのカウント（2段ジャンプ制御用）

    // もちもち弾性アニメーション用の各軸変形速度
    float       m_ScaleVelocityX   = 0.0f;
    float       m_ScaleVelocityY   = 0.0f;
    float       m_ScaleVelocityZ   = 0.0f;

    // ダッシュ・回避関連パラメータ
    std::vector<DashGhost> m_DashGhosts;        // ダッシュ中の残像リスト
    int         m_DashTimer        = 0;         // ダッシュ状態の残り継続フレーム数
    int         m_DashCooldown     = 0;         // ダッシュ再使用可能までのクールダウンフレーム数
    DirectX::XMFLOAT3 m_DashDirection = DirectX::XMFLOAT3(0, 0, 0); // ダッシュする方向ベクトル
    bool        m_IsDashing        = false;     // 現在ダッシュ中かどうかのフラグ

    // 被弾ノックバックおよび移動アニメーション進捗
    DirectX::XMFLOAT3 m_KnockbackVelocity = DirectX::XMFLOAT3(0, 0, 0); // 被弾時のノックバック速度
    float       m_MoveAnimation    = 0.0f;      // 歩行アニメーションの位相

public:
    // コンストラクタ / デストラクタ
    PlayerMovement(Player* owner);
    ~PlayerMovement();

    // 初期化と毎フレーム更新
    void Init();
    void Update();

    // ゲッターとセッター
    bool IsDashing() const { return m_IsDashing; }
    bool IsJustDodgeActive() const { return m_IsDashing && (m_DashTimer >= 10); }
    void ResetDashCooldown() { m_DashCooldown = 0; }
    const std::vector<DashGhost>& GetDashGhosts() const { return m_DashGhosts; }

    float GetVelocityY() const { return m_VelocityY; }
    void SetVelocityY(float vel) { m_VelocityY = vel; }
    bool IsJumping() const { return m_IsJumping; }
    void SetJumping(bool jumping) { m_IsJumping = jumping; }

    // ノックバックの適用
    void ApplyKnockback(const DirectX::XMFLOAT3& vel) { m_KnockbackVelocity = vel; }
    DirectX::XMFLOAT3 GetKnockbackVelocity() const { return m_KnockbackVelocity; }

    // もちもち変形制御のための変形速度ゲッター・セッター
    float GetScaleVelocityX() const { return m_ScaleVelocityX; }
    float GetScaleVelocityY() const { return m_ScaleVelocityY; }
    float GetScaleVelocityZ() const { return m_ScaleVelocityZ; }
    void SetScaleVelocity(float vx, float vy, float vz) {
        m_ScaleVelocityX = vx;
        m_ScaleVelocityY = vy;
        m_ScaleVelocityZ = vz;
    }

private:
    // 通常移動処理
    void UpdateNormalMovement();

    // もちもち弾性のスプリング物理演算
    void UpdateSpringPhysics();

    // ダッシュ残像の寿命管理
    void UpdateGhosts();
};
