#include "player_movement.h"
#include "player.h"
#include "player_controller.h"
#include "math_helper.h"
#include "input.h"
#include "collision.h"
#include "manager.h"
#include "camera.h"
#include "game_constants.h"
#include "enemy.h"
#include "boss_enemy.h"

using namespace DirectX;

// =================================================================
// コンストラクタ / デストラクタ
// =================================================================
PlayerMovement::PlayerMovement(Player* owner)
    : m_Owner(owner)
{
}

PlayerMovement::~PlayerMovement()
{
}

// =================================================================
// 初期化
// =================================================================
void PlayerMovement::Init()
{
    m_VelocityY = 0.0f;
    m_IsJumping = false;
    m_JumpCount = 0;
    
    m_ScaleVelocityX = 0.0f;
    m_ScaleVelocityY = 0.0f;
    m_ScaleVelocityZ = 0.0f;
    
    m_DashTimer = 0;
    m_DashCooldown = 0;
    m_DashDirection = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_IsDashing = false;
    m_DashGhosts.clear();
    
    m_KnockbackVelocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_MoveAnimation = 0.0f;
}

// =================================================================
// 毎フレーム更新処理
// =================================================================
void PlayerMovement::Update()
{
    XMFLOAT3 oldPos = m_Owner->m_Position;
    float oldRotY = m_Owner->m_Rotation.y;

    // ダッシュのクールダウン更新
    if (m_DashCooldown > 0) {
        m_DashCooldown--;
    }

    // 残像（ゴースト）の更新
    UpdateGhosts();

    // ダッシュ発動判定
    // (スタン中でない、かつダッシュ中でない、かつクールダウンでない)
    if (m_Owner->m_DamageTimer <= 0 && m_DashTimer <= 0 && m_DashCooldown <= 0) {
        if (PlayerController::IsDashAction()) {
            float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;
            XMFLOAT3 moveDir = PlayerController::GetMoveDirection(camYaw);
            
            // 移動入力がない場合はプレイヤーの正面方向にダッシュ
            if (MathHelper::LengthSq(moveDir) < 0.001f) {
                moveDir = XMFLOAT3(sinf(m_Owner->m_Rotation.y), 0.0f, cosf(m_Owner->m_Rotation.y));
            }
            
            // 正規化
            float len = sqrtf(moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z);
            if (len > 0.001f) {
                moveDir.x /= len;
                moveDir.y /= len;
                moveDir.z /= len;
            }

            m_DashDirection = moveDir;
            m_DashTimer = Constants::Player::DASH_DURATION;
            m_DashCooldown = Constants::Player::DASH_COOLDOWN;
            m_IsDashing = true;

            // もちもち変形演出：進行方向に長く伸びるように設定
            m_Owner->m_Scale.y = 0.5f;
            m_Owner->m_Scale.x = 1.8f;
            m_Owner->m_Scale.z = 1.8f;

            m_ScaleVelocityY = -0.1f;
            m_ScaleVelocityX = 0.08f;
            m_ScaleVelocityZ = 0.08f;
            
            // ダッシュ中の無敵時間設定
            m_Owner->m_InvincibleTimer = Constants::Player::DASH_INVINCIBLE_TIME;
        }
    }

    // ─────────────────────────────────────────────
    // ダッシュ中の挙動処理
    // ─────────────────────────────────────────────
    if (m_DashTimer > 0) {
        m_DashTimer--;
        
        // 高速移動
        float dashSpeed = Constants::Player::DASH_SPEED;
        XMFLOAT3 nextPos = m_Owner->m_Position;
        nextPos.x += m_DashDirection.x * dashSpeed;
        nextPos.z += m_DashDirection.z * dashSpeed;

        // タックル有効時は衝突判定と攻撃処理を行う
        if (m_Owner->m_TackleTimer > 0) {
            Enemy* hitTarget = nullptr;
            
            // 1. ロックオン対象を優先
            if (m_Owner->m_LockOnTarget && !m_Owner->m_LockOnTarget->IsDestroy()) {
                XMFLOAT3 diff = m_Owner->m_Position - m_Owner->m_LockOnTarget->GetPosition();
                diff.y = 0.0f;
                float dist = MathHelper::Length(diff);
                if (dist < 1.0f + m_Owner->m_LockOnTarget->GetRadius()) {
                    hitTarget = m_Owner->m_LockOnTarget;
                }
            }
            
            // 2. 周囲の敵を走査
            if (!hitTarget) {
                for (GameObject* obj : Manager::GetGameObjectList()) {
                    if (!obj || obj->IsDestroy() || obj == m_Owner) continue;
                    if (obj->GetObjectType() != ObjectType::Enemy && obj->GetObjectType() != ObjectType::Boss) continue;
                    Enemy* e = static_cast<Enemy*>(obj);
                    if (e->GetEnemyState() == EnemyState::DEFEATED) continue;
                    
                    XMFLOAT3 diff = m_Owner->m_Position - e->GetPosition();
                    diff.y = 0.0f;
                    float dist = MathHelper::Length(diff);
                    if (dist < 1.0f + e->GetRadius()) {
                        hitTarget = e;
                        break;
                    }
                }
            }
            
            // ダメージ・吹き飛ばし適用
            if (hitTarget) {
                if (hitTarget->GetObjectType() == ObjectType::Boss) {
                    BossEnemy* bossE = static_cast<BossEnemy*>(hitTarget);
                    bossE->ApplyBossDamage(3, m_Owner->m_Position);
                } else {
                    XMFLOAT3 pushDir = hitTarget->GetPosition() - m_Owner->m_Position;
                    pushDir.y = 0.0f;
                    float len = MathHelper::Length(pushDir);
                    if (len > 0.001f) { pushDir.x /= len; pushDir.z /= len; }
                    else { pushDir = XMFLOAT3(0.0f, 0.0f, 1.0f); }
                    
                    XMFLOAT3 pushVel = XMFLOAT3(pushDir.x * 1.8f, 0.6f, pushDir.z * 1.8f);
                    hitTarget->SetVelocity(pushVel);
                    hitTarget->SetEnemyState(EnemyState::BLOWN_AWAY);
                    hitTarget->Defeat(0.0f, 1.0f, 2.0f);
                }
                
                // ヒット演出
                Manager::AddHitStop(10);
                if (g_Camera) {
                    g_Camera->Shake(0.25f, 10);
                }
                
                m_Owner->m_TackleTimer = 0; // タックル消費
            }
        }

        // 壁との衝突解決
        if (!Collision::CheckAABBCollision(m_Owner, nextPos, Manager::GetGameObjectList())) {
            m_Owner->m_Position = nextPos;
        }

        // 残像生成（3フレームごと）
        if (m_DashTimer % 3 == 0) {
            DashGhost ghost;
            ghost.Position = m_Owner->m_Position;
            ghost.Rotation = m_Owner->m_Rotation;
            ghost.Scale = m_Owner->m_Scale;
            ghost.Alpha = 0.8f;
            m_DashGhosts.push_back(ghost);
        }

        // ダッシュ終了時のバウンド演出
        if (m_DashTimer == 0) {
            m_IsDashing = false;
            m_Owner->m_Scale.y = 1.5f;
            m_Owner->m_Scale.x = 0.7f;
            m_Owner->m_Scale.z = 0.7f;
            m_ScaleVelocityY = 0.08f;
            m_ScaleVelocityX = -0.04f;
            m_ScaleVelocityZ = -0.04f;
        }

        // 掴んでいる敵の位置同期（ダッシュ中も正面位置を正しく維持し、バウンドを防止する）
        if ((m_Owner->m_State == PlayerState::GRABBED || m_Owner->m_State == PlayerState::SPINNING) && m_Owner->m_GrabbedEnemy) {
            Collision::ResolveGrabPhysics(m_Owner, m_Owner->m_GrabbedEnemy, 0.0f);
        }

        // ダッシュ中ももちもちの減衰振動を更新
        UpdateSpringPhysics();
        return;
    }

    // ─────────────────────────────────────────────
    // 被弾気絶（スタン）中のノックバック物理
    // ─────────────────────────────────────────────
    if (m_Owner->m_DamageTimer > 0) {
        XMFLOAT3 nextPos = m_Owner->m_Position;
        nextPos.x += m_KnockbackVelocity.x;
        nextPos.z += m_KnockbackVelocity.z;
        
        m_KnockbackVelocity.x *= 0.9f;
        m_KnockbackVelocity.z *= 0.9f;
        
        if (!Collision::CheckAABBCollision(m_Owner, nextPos, Manager::GetGameObjectList())) {
            m_Owner->m_Position = nextPos;
        }
    }

    // ─────────────────────────────────────────────
    // 通常移動とジャンプ入力の更新
    // ─────────────────────────────────────────────
    if (m_Owner->m_DamageTimer <= 0) {
        // スペースキーでジャンプ（2段ジャンプ対応）
        if (Input::GetKeyTrigger(0x20) && m_JumpCount < Constants::Player::MAX_JUMP_COUNT) {
            m_VelocityY = Constants::Player::JUMP_VELOCITY;
            m_IsJumping = true;
            m_JumpCount++;

            // 踏み切り時のもちもち演出
            m_Owner->m_Scale.y = 0.6f;
            m_Owner->m_Scale.x = 1.3f;
            m_Owner->m_Scale.z = 1.3f;

            m_ScaleVelocityY = 0.1f;
            m_ScaleVelocityX = -0.05f;
            m_ScaleVelocityZ = -0.05f;
        }

        // 通常移動処理の呼び出し
        UpdateNormalMovement();
    }

    // ─────────────────────────────────────────────
    // 重力と接地判定の適用
    // ─────────────────────────────────────────────
    if (m_Owner->m_Position.y > -0.5f || m_VelocityY != 0.0f) {
        m_VelocityY -= Constants::Player::GRAVITY;
        m_Owner->m_Position.y += m_VelocityY;

        // 空中での引き伸ばし演出（縦長）
        if (m_VelocityY > 0.01f) {
            m_Owner->m_Scale.y += (1.0f + m_VelocityY * 1.5f - m_Owner->m_Scale.y) * 0.2f;
            m_Owner->m_Scale.x += (1.0f - m_VelocityY * 0.75f - m_Owner->m_Scale.x) * 0.2f;
            m_Owner->m_Scale.z += (1.0f - m_VelocityY * 0.75f - m_Owner->m_Scale.z) * 0.2f;
        }

        // 着地判定
        if (m_Owner->m_Position.y <= -0.5f) {
            m_Owner->m_Position.y = -0.5f;
            m_VelocityY = 0.0f;
            m_IsJumping = false;
            m_JumpCount = 0;

            // 着地時の潰れ演出
            m_Owner->m_Scale.y = 0.5f;
            m_Owner->m_Scale.x = 1.3f;
            m_Owner->m_Scale.z = 1.3f;

            m_ScaleVelocityY = -0.1f;
            m_ScaleVelocityX = 0.05f;
            m_ScaleVelocityZ = 0.05f;
        }
    }

    // 角速度（旋回速度）の計算
    float diff = m_Owner->m_Rotation.y - oldRotY;
    while (diff < -XM_PI) diff += XM_2PI;
    while (diff > XM_PI)  diff -= XM_2PI;
    m_Owner->m_AngularVelocity = diff;

    // ─────────────────────────────────────────────
    // スプリング物理によるもちもち復元と歩行揺れ
    // ─────────────────────────────────────────────
    UpdateSpringPhysics();

    // 1フレームのXZ移動量から移動速度を算出
    XMFLOAT3 actualVel = XMFLOAT3(m_Owner->m_Position.x - oldPos.x, m_Owner->m_Position.y - oldPos.y, m_Owner->m_Position.z - oldPos.z);
    float speed = sqrtf(actualVel.x * actualVel.x + actualVel.y * actualVel.y + actualVel.z * actualVel.z);
    float dt = 1.0f / 60.0f;

    if (speed > 0.005f) {
        m_MoveAnimation += speed * dt * 30.0f;
        m_Owner->m_Scale.y += sinf(m_MoveAnimation * 3.0f) * 0.03f;
        m_Owner->m_Scale.x -= sinf(m_MoveAnimation * 3.0f) * 0.015f;
        m_Owner->m_Scale.z -= sinf(m_MoveAnimation * 3.0f) * 0.015f;
    }

    // 衝突判定によるめり込みの押し戻し解決
    Collision::ResolveAABBCollision(m_Owner, Manager::GetGameObjectList());
}

// =================================================================
// 通常移動処理
// =================================================================
void PlayerMovement::UpdateNormalMovement()
{
    float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;
    XMFLOAT3 moveDir = PlayerController::GetMoveDirection(camYaw);

    // 移動入力がある場合のみ座標と回転を更新
    if (MathHelper::LengthSq(moveDir) > 0.001f) {
        float speed = Constants::Player::MOVE_SPEED;
        XMFLOAT3 nextPos = m_Owner->m_Position;
        nextPos.x += moveDir.x * speed;
        nextPos.z += moveDir.z * speed;

        // 壁との衝突がなければ移動する
        if (!Collision::CheckAABBCollision(m_Owner, nextPos, Manager::GetGameObjectList())) {
            m_Owner->m_Position = nextPos;
        }

        // スピン（自動・手動）中でない場合のみ、移動方向へ向きを変える
        if (m_Owner->m_State != PlayerState::SPINNING) {
            float targetYaw = atan2f(moveDir.x, moveDir.z);
            m_Owner->m_Rotation.y = MathHelper::LerpAngle(m_Owner->m_Rotation.y, targetYaw, 0.15f);
        }
    }
}

// =================================================================
// もちもち復元のスプリング物理演算
// =================================================================
void PlayerMovement::UpdateSpringPhysics()
{
    float springK = Constants::Player::SPRING_K;
    float damping = Constants::Player::DAMPING;

    // X軸
    float forceX = (1.0f - m_Owner->m_Scale.x) * springK;
    m_ScaleVelocityX += forceX;
    m_ScaleVelocityX *= damping;
    m_Owner->m_Scale.x += m_ScaleVelocityX;

    // Y軸
    float forceY = (1.0f - m_Owner->m_Scale.y) * springK;
    m_ScaleVelocityY += forceY;
    m_ScaleVelocityY *= damping;
    m_Owner->m_Scale.y += m_ScaleVelocityY;

    // Z軸
    float forceZ = (1.0f - m_Owner->m_Scale.z) * springK;
    m_ScaleVelocityZ += forceZ;
    m_ScaleVelocityZ *= damping;
    m_Owner->m_Scale.z += m_ScaleVelocityZ;
}

// =================================================================
// ダッシュ残像の寿命管理
// =================================================================
void PlayerMovement::UpdateGhosts()
{
    for (auto it = m_DashGhosts.begin(); it != m_DashGhosts.end(); ) {
        it->Alpha -= 0.08f; // 毎フレーム不透明度を減少
        if (it->Alpha <= 0.0f) {
            it = m_DashGhosts.erase(it);
        } else {
            it++;
        }
    }
}
