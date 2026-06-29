#include "player_combat.h"
#include "player.h"
#include "player_movement.h"
#include "math_helper.h"
#include "player_controller.h"
#include "collision.h"
#include "manager.h"
#include "camera.h"
#include "game_constants.h"
#include "enemy.h"
#include "boss_enemy.h"
#include "enemy_bullet.h"
#include "shockwave.h"

using namespace DirectX;

// =================================================================
// コンストラクタ / デストラクタ
// =================================================================
PlayerCombat::PlayerCombat(Player* owner)
    : m_Owner(owner)
{
}

PlayerCombat::~PlayerCombat()
{
}

// =================================================================
// 初期化
// =================================================================
void PlayerCombat::Init()
{
}

// =================================================================
// 毎フレーム更新処理
// =================================================================
void PlayerCombat::Update()
{
    // ガード状態の更新（スタン中でなく、かつ敵を掴んでいない通常状態のときのみ可能）
    if (m_Owner->m_DamageTimer <= 0 && m_Owner->m_State == PlayerState::IDLE && !m_Owner->m_GrabbedEnemy) {
        if (PlayerController::IsGuardAction()) {
            m_Owner->m_GuardTimer++;
        } else {
            m_Owner->m_GuardTimer = 0;
        }
    } else {
        m_Owner->m_GuardTimer = 0;
    }

    // ロックオンの更新（スローモーション中のみ有効）
    if (Manager::IsSlowMotionActive()) {
        FindLockOnTarget();
    } else {
        m_Owner->m_LockOnTarget = nullptr;
        m_Owner->m_LockOnFrame = 0;
        m_Owner->m_WarpSlashCount = 0;
        m_Owner->m_CanWarpSlash = true;
    }

    m_Owner->m_MarkerTimer++;

    // しびれスタン中でなければ、各状態の更新を行う
    if (m_Owner->m_DamageTimer <= 0) {
        switch (m_Owner->m_State) {
        case PlayerState::IDLE:
            UpdateIdle();
            break;
        case PlayerState::GRABBED:
            UpdateGrabbed();
            break;
        case PlayerState::SPINNING:
            UpdateSpinning();
            break;
        }
    }
}

// =================================================================
// IDLE（通常）状態の戦闘更新
// =================================================================
void PlayerCombat::UpdateIdle()
{
    // タックル有効中に左クリック → 敵の目の前にテレポートしてタックル！
    if (m_Owner->m_TackleTimer > 0 && PlayerController::IsGrabOrThrowAction()) {
        Enemy* target = nullptr;
        
        // ロックオン対象を優先
        if (m_Owner->m_LockOnTarget && !m_Owner->m_LockOnTarget->IsDestroy()) {
            target = m_Owner->m_LockOnTarget;
        } else {
            // 近くの敵を検索
            float nearestDist = 15.0f;
            for (GameObject* obj : Manager::GetGameObjectList()) {
                if (!obj || obj == m_Owner || obj->IsDestroy()) continue;
                if (obj->GetObjectType() != ObjectType::Enemy && obj->GetObjectType() != ObjectType::Boss) continue;
                Enemy* e = static_cast<Enemy*>(obj);
                if (e->GetEnemyState() == EnemyState::DEFEATED) continue;
                
                float dist = MathHelper::Length(e->GetPosition() - m_Owner->m_Position);
                if (dist < nearestDist) {
                    nearestDist = dist;
                    target = e;
                }
            }
        }
        
        // タックルテレポート実行
        if (target) {
            XMFLOAT3 targetPos = target->GetPosition();
            XMFLOAT3 startPos = m_Owner->m_Position;
            
            XMVECTOR vStart = XMLoadFloat3(&startPos);
            XMVECTOR vTarget = XMLoadFloat3(&targetPos);
            XMVECTOR vDiff = vTarget - vStart;
            vDiff = XMVectorSetY(vDiff, 0.0f);
            float dist = XMVectorGetX(XMVector3Length(vDiff));
            
            XMVECTOR vDir = (dist > 0.001f) ? (vDiff / dist) : XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
            
            float targetRadius = target->GetRadius();
            float warpDistance = targetRadius + 0.8f;
            
            XMVECTOR vWarpPos = vTarget - vDir * warpDistance;
            XMFLOAT3 warpPos;
            XMStoreFloat3(&warpPos, vWarpPos);
            warpPos.y = m_Owner->m_Position.y; // 接地高さを維持
            
            m_Owner->m_Position = warpPos;
            XMFLOAT3 dir;
            XMStoreFloat3(&dir, vDir);
            m_Owner->m_Rotation.y = atan2f(dir.x, dir.z);
            
            // もちもち変形演出
            m_Owner->m_Scale.y = 0.4f;
            m_Owner->m_Scale.x = 1.8f;
            m_Owner->m_Scale.z = 1.8f;
            m_Owner->m_Movement->SetScaleVelocity(0.08f, -0.1f, 0.08f);
            
            // 衝撃波発生
            ShockwaveSystem::AddShockwave(warpPos, 5.0f, 0.0f, 2.5f, 4.0f, 20, 1.5f, 0);
            
            // ダメージ適用
            if (target->GetObjectType() == ObjectType::Boss) {
                BossEnemy* boss = static_cast<BossEnemy*>(target);
                boss->ApplyBossDamage(3, m_Owner->m_Position);
            } else {
                XMFLOAT3 pushVel = XMFLOAT3(dir.x * 1.8f, 0.6f, dir.z * 1.8f);
                target->SetVelocity(pushVel);
                target->SetEnemyState(EnemyState::BLOWN_AWAY);
                target->Defeat(0.0f, 1.0f, 2.0f);
            }
            
            // インパクト演出
            Manager::AddHitStop(12);
            if (g_Camera) {
                g_Camera->Shake(0.35f, 12);
            }
            
            m_Owner->m_TackleTimer = 0;
            return;
        }
    }

    // 雷電テレポートスラッシュ発動判定（スロー中かつロックオン中かつ攻撃回数3回未満）
    if (Manager::IsSlowMotionActive() && m_Owner->m_CanWarpSlash && m_Owner->m_LockOnTarget && m_Owner->m_WarpSlashCount < 3) {
        if (PlayerController::IsGrabOrThrowAction()) {
            bool hasGrabTarget = false;
            float grabRange = Constants::Player::GRAB_RANGE;
            for (GameObject* obj : Manager::GetGameObjectList()) {
                if (!obj || obj == m_Owner || obj->IsDestroy()) continue;
                if (obj->GetObjectType() != ObjectType::Enemy) continue;
                Enemy* e = static_cast<Enemy*>(obj);
                if (e->GetEnemyState() != EnemyState::NORMAL) continue;

                float dist = MathHelper::Length(e->GetPosition() - m_Owner->m_Position);
                if (dist < grabRange) {
                    hasGrabTarget = true;
                    break;
                }
            }

            // 掴み有効範囲内に敵がいない場合のみ、テレポートスラッシュ発動
            if (!hasGrabTarget) {
                m_Owner->m_WarpSlashCount++;
                Enemy* target = m_Owner->m_LockOnTarget;
                XMFLOAT3 targetPos = target->GetPosition();
                XMFLOAT3 startPos = m_Owner->m_Position;
            
                XMVECTOR vStart = XMLoadFloat3(&startPos);
                XMVECTOR vTarget = XMLoadFloat3(&targetPos);
                XMVECTOR vDiff = vTarget - vStart;
                vDiff = XMVectorSetY(vDiff, 0.0f);
                float dist = XMVectorGetX(XMVector3Length(vDiff));
                
                XMVECTOR vDir = (dist > 0.001f) ? (vDiff / dist) : XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
                
                float halfWidth = target->GetSize().x * target->GetScale().x * 0.5f;
                float warpDistance = halfWidth + 1.0f;
                
                XMVECTOR vWarpPos = vTarget - vDir * warpDistance;
                XMFLOAT3 warpPos;
                XMStoreFloat3(&warpPos, vWarpPos);
                warpPos.y = m_Owner->m_Position.y;
                
                m_Owner->m_Position = warpPos;
                m_Owner->m_Scale.y = 0.4f;
                m_Owner->m_Scale.x = 1.8f;
                m_Owner->m_Scale.z = 1.8f;
                
                XMFLOAT3 dir;
                XMStoreFloat3(&dir, vDir);
                m_Owner->m_Rotation.y = atan2f(dir.x, dir.z);
                
                // 雷電エフェクトの追加
                XMFLOAT3 boltStart = startPos;
                XMFLOAT3 boltEnd = targetPos;
                boltStart.y += 0.3f;
                boltEnd.y += 0.3f;
                m_Owner->AddLightningEffect(boltStart, boltEnd);
                m_Owner->AddLightningEffect(XMFLOAT3(boltStart.x + 0.1f, boltStart.y, boltStart.z), XMFLOAT3(boltEnd.x + 0.1f, boltEnd.y, boltEnd.z));
                m_Owner->AddLightningEffect(XMFLOAT3(boltStart.x - 0.1f, boltStart.y, boltStart.z), XMFLOAT3(boltEnd.x - 0.1f, boltEnd.y, boltEnd.z));
                
                // 衝撃波
                ShockwaveSystem::AddShockwave(warpPos, 4.0f, 0.0f, 2.5f, 4.0f, 20, 1.5f, 0);
                
                // ダメージ適用
                if (target->GetObjectType() == ObjectType::Boss) {
                    BossEnemy* boss = static_cast<BossEnemy*>(target);
                    boss->ApplyBossDamage(4, m_Owner->m_Position);
                } else {
                    XMFLOAT3 pushVel = XMFLOAT3(dir.x * 1.6f, 0.6f, dir.z * 1.6f);
                    target->SetVelocity(pushVel);
                    target->SetEnemyState(EnemyState::BLOWN_AWAY);
                    target->SetLightning(true);
                    target->Defeat(0.0f, 2.0f, 3.0f);
                }
                
                // インパクト
                Manager::AddHitStop(15);
                if (g_Camera) {
                    g_Camera->Shake(0.45f, 15);
                }
                
                Manager::StartSlowMotion(90);
                return;
            }
        }
    }

    // 通常の敵の掴み処理
    if (PlayerController::IsGrabOrThrowAction()) {
        float grabRange = Constants::Player::GRAB_RANGE;
        Enemy* nearest  = nullptr;
        float  nearestDist = grabRange;

        for (GameObject* obj : Manager::GetGameObjectList()) {
            if (!obj || obj == m_Owner || obj->IsDestroy()) continue;
            if (obj->GetObjectType() != ObjectType::Enemy) continue;
            Enemy* e = static_cast<Enemy*>(obj);
            if (e->GetEnemyState() != EnemyState::NORMAL) continue;

            XMFLOAT3 toEnemy = e->GetPosition() - m_Owner->m_Position;
            float dist = MathHelper::Length(toEnemy);
            if (dist < nearestDist) {
                nearestDist = dist;
                nearest = e;
            }
        }

        if (nearest) {
            m_Owner->SetGrabbedEnemy(nearest);
        }
    }
}

// =================================================================
// GRABBED（敵掴み）状態の戦闘更新
// =================================================================
void PlayerCombat::UpdateGrabbed()
{
    // 右クリック：スピン開始
    if (m_Owner->m_GrabbedEnemy && PlayerController::IsSpinToggleAction()) {
        m_Owner->m_State = PlayerState::SPINNING;
        m_Owner->m_CurrentSpinSpeed = Constants::Player::MIN_SPIN_SPEED;
    }

    // 左クリック：直接投げる
    if (m_Owner->m_GrabbedEnemy && PlayerController::IsGrabOrThrowAction()) {
        Throw();
        return;
    }

    // 敵の位置同期
    if (m_Owner->m_GrabbedEnemy) {
        Collision::ResolveGrabPhysics(m_Owner, m_Owner->m_GrabbedEnemy, 0.0f);
    }

    if (!m_Owner->m_GrabbedEnemy) {
        m_Owner->m_State = PlayerState::IDLE;
    }
}

// =================================================================
// SPINNING（振り回し）状態の戦闘更新
// =================================================================
void PlayerCombat::UpdateSpinning()
{
    // スピン加速
    float targetSpinSpeed = Constants::Player::MAX_SPIN_SPEED;
    m_Owner->m_CurrentSpinSpeed += (targetSpinSpeed - m_Owner->m_CurrentSpinSpeed) * Constants::Player::SPIN_ACCELERATION;

    m_Owner->m_Rotation.y += m_Owner->m_CurrentSpinSpeed;
    if (m_Owner->m_Rotation.y > XM_2PI) m_Owner->m_Rotation.y -= XM_2PI;

    if (g_Camera) {
        float dynamicShake = 0.01f + m_Owner->m_CurrentSpinSpeed * 0.15f;
        g_Camera->Shake(dynamicShake, 2);
    }

    // 吸引効果
    if (m_Owner->m_HasVacuumItem) {
        float vacuumRadius = 8.0f;
        float vacuumForce = 0.03f;

        for (GameObject* obj : Manager::GetGameObjectList()) {
            if (!obj || obj == m_Owner || obj == m_Owner->m_GrabbedEnemy) continue;

            if (obj->GetObjectType() == ObjectType::Enemy) {
                Enemy* enemy = static_cast<Enemy*>(obj);
                if (enemy->GetEnemyState() == EnemyState::NORMAL || enemy->GetEnemyState() == EnemyState::CHASING) {
                    XMFLOAT3 enemyPos = enemy->GetPosition();
                    XMFLOAT3 diff = m_Owner->m_Position - enemyPos;
                    float distSq = MathHelper::LengthSq(diff);

                    if (distSq < vacuumRadius * vacuumRadius && distSq > 0.01f) {
                        float dist = sqrtf(distSq);
                        XMFLOAT3 dir = diff / dist;

                        float spinDirSign = (m_Owner->m_AngularVelocity >= 0.0f) ? 1.0f : -1.0f;
                        XMFLOAT3 tangent = XMFLOAT3(-dir.z, 0.0f, dir.x) * spinDirSign * 0.3f;
                        XMFLOAT3 force = (dir + tangent) * vacuumForce;

                        XMFLOAT3 vel = enemy->GetVelocity();
                        vel.x += force.x;
                        vel.z += force.z;
                        enemy->SetVelocity(vel);

                        enemy->SetEnemyState(EnemyState::VACUUMED);
                    }
                }
            }
        }
    }

    // 左クリック：投げる
    if (PlayerController::IsGrabOrThrowAction()) {
        Throw();
        return;
    }

    // 右クリック：掴み状態に戻る（キャンセル）
    if (PlayerController::IsSpinToggleAction()) {
        m_Owner->m_State = PlayerState::GRABBED;
        m_Owner->m_CurrentSpinSpeed = Constants::Player::MIN_SPIN_SPEED;
    }

    // 敵の位置同期
    if (m_Owner->m_GrabbedEnemy) {
        Collision::ResolveGrabPhysics(m_Owner, m_Owner->m_GrabbedEnemy, 0.0f);
    }

    if (!m_Owner->m_GrabbedEnemy) {
        m_Owner->m_State = PlayerState::IDLE;
        m_Owner->m_CurrentSpinSpeed = Constants::Player::MIN_SPIN_SPEED;
    }
}

// =================================================================
// 掴んでいる敵を投げ飛ばす
// =================================================================
void PlayerCombat::Throw()
{
    if (m_Owner->m_GrabbedEnemy) {
        float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;
        m_Owner->m_Rotation.y = camYaw;

        XMFLOAT3 fwdF = XMFLOAT3(sinf(m_Owner->m_Rotation.y), 0.0f, cosf(m_Owner->m_Rotation.y));

        float baseThrowSpeed = Constants::Player::THROW_FORCE;
        float speedBoost = abs(m_Owner->m_AngularVelocity) * 6.0f; 
        float totalSpeed = baseThrowSpeed + speedBoost;

        XMFLOAT3 throwVelocity = fwdF * totalSpeed;

        if (m_Owner->m_HasGigantItem) {
            throwVelocity.y = -0.3f - speedBoost * 0.6f;
        }

        m_Owner->m_GrabbedEnemy->SetVelocity(throwVelocity);
        m_Owner->m_GrabbedEnemy->SetEnemyState(EnemyState::FLYING);

        // アイテム効果の適用
        if (m_Owner->m_HasVacuumItem) {
            m_Owner->m_GrabbedEnemy->SetExplosive(true);
        }
        if (m_Owner->m_HasLightningItem) {
            m_Owner->m_GrabbedEnemy->SetLightning(true);
        }

        // 公転中の敵も一斉射出
        for (GameObject* obj : Manager::GetGameObjectList()) {
            if (!obj) continue;
            if (obj->GetObjectType() != ObjectType::Enemy) continue;
            Enemy* enemy = static_cast<Enemy*>(obj);
            if (enemy->GetEnemyState() == EnemyState::VACUUMED) {
                float finalSpeed = totalSpeed * 0.9f;
                XMFLOAT3 vacuumThrowVel = fwdF * finalSpeed;

                enemy->SetVelocity(vacuumThrowVel);
                enemy->SetEnemyState(EnemyState::FLYING);
                enemy->SetExplosive(true);
            }
        }

        // フラグ消費
        m_Owner->m_HasVacuumItem = false;
        m_Owner->m_HasGigantItem = false;
        m_Owner->m_HasLightningItem = false;

        m_Owner->m_GrabbedEnemy = nullptr;
        m_Owner->m_State = PlayerState::IDLE;
        m_Owner->m_IsAutoSpinning = false;

        // 伸縮演出
        m_Owner->m_Scale.y = 0.5f;
        m_Owner->m_Scale.x = 1.6f;
        m_Owner->m_Scale.z = 1.6f;

        // カメラシェイク
        float throwShake = 0.08f + abs(m_Owner->m_AngularVelocity) * 1.8f;
        if (throwShake > 0.40f) throwShake = 0.40f;
        g_Camera->Shake(throwShake, 12);
    }
}

// =================================================================
// 被弾ダメージ適用
// =================================================================
void PlayerCombat::ApplyDamage(int damage, const DirectX::XMFLOAT3& enemyPos)
{
    if (m_Owner->m_InvincibleTimer > 0 || m_Owner->m_HP <= 0) return;

    m_Owner->m_HP -= damage;
    if (m_Owner->m_HP < 0) m_Owner->m_HP = 0;

    m_Owner->m_Scale.y = 0.5f;
    m_Owner->m_Scale.x = 1.7f;
    m_Owner->m_Scale.z = 1.7f;

    m_Owner->m_DamageTimer = Constants::Player::DAMAGE_STUN_DURATION * 2; // 60フレーム
    m_Owner->m_InvincibleTimer = Constants::Player::INVINCIBLE_DURATION * 3; // 180フレーム

    if (m_Owner->m_GrabbedEnemy) {
        m_Owner->m_GrabbedEnemy->SetEnemyState(EnemyState::NORMAL);
        m_Owner->m_GrabbedEnemy->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
        m_Owner->m_GrabbedEnemy = nullptr;
    }
    m_Owner->m_State = PlayerState::IDLE;

    // ノックバック計算
    DirectX::XMFLOAT3 diff = m_Owner->m_Position - enemyPos;
    diff.y = 0.0f;
    float dist = MathHelper::Length(diff);
    if (dist > 0.001f) {
        diff.x /= dist;
        diff.z /= dist;
    } else {
        diff = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);
    }

    XMFLOAT3 knockback = DirectX::XMFLOAT3(diff.x * 0.35f, 0.0f, diff.z * 0.35f);
    m_Owner->m_Movement->ApplyKnockback(knockback);

    if (g_Camera) {
        g_Camera->Shake(0.35f, 12);
    }
    Manager::AddHitStop(5);
}

// =================================================================
// パリィ成功時のカウンター実行
// =================================================================
void PlayerCombat::ExecuteParryCounter(DirectX::XMFLOAT3 bulletPos)
{
    // サンドバッグ敵の生成
    Enemy* sandbag = Manager::AddGameObject<Enemy>();
    if (sandbag) {
        XMFLOAT3 spawnPos = bulletPos;
        spawnPos.y = -0.5f;
        sandbag->SetPosition(spawnPos);
        sandbag->SetScale(XMFLOAT3(1.2f, 1.2f, 1.2f));
        sandbag->SetSandbag(true);
    }

    // もちもち変形
    m_Owner->m_Scale.y = 1.3f;
    m_Owner->m_Scale.x = 0.8f;
    m_Owner->m_Scale.z = 0.8f;
    m_Owner->m_Movement->SetScaleVelocity(-0.04f, 0.08f, -0.04f);

    // 火花イナズマ軌跡の生成
    XMFLOAT3 boltStart = m_Owner->m_Position;
    boltStart.y += 0.3f;
    XMFLOAT3 boltEnd = bulletPos;
    boltEnd.y += 0.3f;
    m_Owner->AddLightningEffect(boltStart, boltEnd);

    // 衝撃波
    XMFLOAT3 shockPos = bulletPos;
    shockPos.y = -0.95f;
    ShockwaveSystem::AddShockwave(shockPos, 3.0f, 1.5f, 0.8f, 0.0f, 16, 0.0f, 0);

    // ヒット演出
    Manager::AddHitStop(8);
    if (g_Camera) {
        g_Camera->Shake(0.2f, 10);
    }

    Manager::StartSlowMotion(90);
    m_Owner->DisableWarpSlash();
}

// =================================================================
// ロックオン対象探索（スロー中のみ）
// =================================================================
void PlayerCombat::FindLockOnTarget()
{
    if (!g_Camera) return;

    XMFLOAT3 camPos = g_Camera->GetPosition();
    XMFLOAT3 camFwd = g_Camera->GetForward();

    XMVECTOR vCamFwd = XMVector3Normalize(XMLoadFloat3(&camFwd));
    XMVECTOR vCamPos = XMLoadFloat3(&camPos);

    Enemy* bestTarget = nullptr;
    float maxCos = -1.0f;
    float maxDistance = 30.0f;
    float minCos = 0.7071f; // cos(45度)

    for (GameObject* obj : Manager::GetGameObjectList()) {
        if (!obj || obj == m_Owner || obj->IsDestroy()) continue;
        ObjectType type = obj->GetObjectType();
        if (type != ObjectType::Enemy && type != ObjectType::Boss) continue;

        Enemy* enemy = static_cast<Enemy*>(obj);
        EnemyState state = enemy->GetEnemyState();
        if (state == EnemyState::DEFEATED || state == EnemyState::GRABBED || state == EnemyState::VACUUMED) continue;
        if (enemy == m_Owner->m_GrabbedEnemy) continue;

        XMFLOAT3 enemyPos = enemy->GetPosition();
        XMVECTOR vEnemyPos = XMLoadFloat3(&enemyPos);

        float dist = MathHelper::Length(enemyPos - m_Owner->m_Position);
        if (dist > maxDistance) continue;

        XMVECTOR vToEnemy = XMVector3Normalize(vEnemyPos - vCamPos);
        float cosAngle = XMVectorGetX(XMVector3Dot(vToEnemy, vCamFwd));

        if (cosAngle >= minCos) {
            if (cosAngle > maxCos) {
                maxCos = cosAngle;
                bestTarget = enemy;
            }
        }
    }

    if (bestTarget != m_Owner->m_LockOnTarget) {
        m_Owner->m_LockOnTarget = bestTarget;
        m_Owner->m_LockOnFrame = 0;
    } else if (m_Owner->m_LockOnTarget != nullptr) {
        m_Owner->m_LockOnFrame++;
    }
}
