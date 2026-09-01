#include "enemy.h"
#include "event_system.h"
#include "event_types.h"
#include "enemy_affix.h"
#include "renderer.h"
#include "resource_manager.h"
#include "math_helper.h"
#include "player.h"
#include "manager.h"
#include "camera.h"
#include "shockwave.h"
#include "game_rule.h"
#include "score_popup.h"
#include "game_constants.h"

void Enemy::Init()
{
    m_Position = XMFLOAT3(0.0f, -0.5f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_Size     = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_UprightTimer = 0;
    m_Affix        = nullptr;
    m_IsDefeatedCounted = false;
    m_ScoreValue   = Constants::Enemy::DEFAULT_SCORE;

    m_Texture = ResourceManager::GetTexture("enemy.png");
    
    // レンダーコンポーネントでの描画パラメータ設定
    m_RenderComponent = RenderComponent("enemy.png", MeshType::Cube, true);
}

void Enemy::Uninit()
{
}

void Enemy::Update()
{
    if (m_Affix) m_Affix->Update(this);

    if (m_EnemyState == EnemyState::FLYING) {
        // 投げ飛ばされた敵の空気抵抗（XZ減速率）の適用
        MathHelper::ScaleXZ(m_Velocity, Constants::Enemy::FLYING_AIR_RESISTANCE);
        // 重力の適用（Y軸速度の減算）
        m_VelocityY -= Constants::Enemy::FLYING_GRAVITY; 

        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;
        m_Position.y += m_VelocityY;

        // スケール値に応じた着地時の床高さ計算（めり込み防止）
        float baseOffset = (m_Size.y * m_Scale.y - 1.0f) * 0.5f;
        float flightFloorY = -0.3f + baseOffset;

        // 地面（XZ平面）への着地判定
        if (m_Position.y < flightFloorY) {
            // 着地時のバウンドまたは衝撃波の発生（十分な下向き速度がある場合のみ）
            if (m_VelocityY < -0.05f) {
                // 巨大化エネミーが地面にぶつかった時のカメラシェイクとヒットストップ
                if (m_Scale.x > 2.0f) {
                    if (g_Camera) {
                        float impactShake = abs(m_VelocityY) * 1.5f;
                        if (impactShake > 0.6f) impactShake = 0.6f;
                        g_Camera->Shake(impactShake, 15);
                    }
                    Manager::AddHitStop(10);
                }
            }

            m_Position.y = flightFloorY;
            m_VelocityY = 0.0f;
        }

        // 速度がほぼ0になり、かつ地面に接地しているならNORMALに戻す（巨大エネミーは除く）
        // 爆発属性エネミーは gameplay_scene 側で処理するため、ここでは状態遷移しない
        if (m_Position.y <= flightFloorY && abs(m_Velocity.x) < 0.05f && abs(m_Velocity.z) < 0.05f) {
            if (!IsExplosive()) {
                if (m_Scale.x > 2.0f) {
                    // 巨大化エネミーは飛行終了時にスピードが0になった時点で撃破とする
                    m_EnemyState = EnemyState::DEFEATED;
                    m_Velocity   = XMFLOAT3(0.0f, 0.0f, 0.0f);
                    m_VelocityY  = 0.0f;

                    // スピードが0になった時点で、周囲の敵を巻き込む衝撃波を発生（高さは地面位置に固定）
                    XMFLOAT3 shockPos = m_Position;
                    shockPos.y = -0.95f; // 地面の高さに強制設定

                    ShockwaveSystem::AddShockwave(shockPos, 15.0f, 1.8f, 0.9f, 0.0f, 32, 1.4f, 0);
                    ShockwaveSystem::AddShockwave(shockPos, 11.0f, 1.8f, 0.9f, 0.0f, 26, 0.9f, 7);
                    ShockwaveSystem::AddShockwave(shockPos, 7.0f, 1.8f, 0.9f, 0.0f, 20, 0.5f, 14);
                } else {
                    m_Position.y = -0.5f + baseOffset; // 通常時の接地高さに戻す
                    m_Velocity   = XMFLOAT3(0, 0, 0);
                    m_VelocityY  = 0.0f;
                    m_EnemyState = EnemyState::NORMAL;
                    m_UprightTimer = 60;  // 1秒間起き上がれないようにタイマーを設定
                }
            }
        }

        m_Rotation.x += 0.2f;
        m_Rotation.z += 0.15f;
    }
    else if (m_EnemyState == EnemyState::DEFEATED) {
        m_Scale *= 0.85f;                                  // 徐々に縮小
        m_Rotation += DirectX::XMFLOAT3(0.5f, 0.5f, 0.5f); // 徐々に回転
        MathHelper::ScaleXZ(m_Velocity, 0.9f);             // 空気抵抗による減速

        // 撃破されて消滅中のスライド移動
        m_Velocity.x *= 0.9f;
        m_Velocity.z *= 0.9f;
        
        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;

        if (m_Scale.x < 0.05f) {
            SetDestroy(); // 完全に消滅
        }
    }
    else if (m_EnemyState == EnemyState::NORMAL) {
        if (m_UprightTimer > 0) {
            m_UprightTimer--;
        }
        else {
            m_Rotation.x = MathHelper::Lerp(m_Rotation.x, 0.0f, 0.1f);
            m_Rotation.z = MathHelper::Lerp(m_Rotation.z, 0.0f, 0.1f);

            MathHelper::ClearIfNearZero(m_Rotation.x);
            MathHelper::ClearIfNearZero(m_Rotation.z);
        }
    }
    else if (m_EnemyState == EnemyState::VACUUMED) {
        // 掴まれている敵を中心にブラックホールのように回転する挙動
        Player* player = Manager::GetGameObject<Player>();
        if (player) {
                // プレイヤーのスピン状態が終了した場合は通常状態に戻す
                if (player->GetState() != PlayerState::SPINNING) {
                    m_EnemyState = EnemyState::NORMAL;
                    return;
                }

                // 吸い込みの中心 = 掴まれている敵の位置（なければプレイヤー位置）
                Enemy* grabbed = player->GetGrabbedEnemy();
                DirectX::XMFLOAT3 pivotPos = grabbed
                    ? grabbed->GetPosition()
                    : player->GetPosition();

                DirectX::XMFLOAT3 diff = m_Position - pivotPos;
                float dist = MathHelper::Length(diff);

                // スピンに巻き込まれている敵を引き寄せる（半径 2.5m に収束）
                float targetDist = 2.5f;
                dist = MathHelper::Lerp(dist, targetDist, 0.06f);

                // 掴まれている敵のスピン角速度で回転させる
                float angle = atan2f(diff.x, diff.z);
                angle += player->GetAngularVelocity();

                m_Position.x = pivotPos.x + sinf(angle) * dist;
                m_Position.z = pivotPos.z + cosf(angle) * dist;
                m_Position.y = pivotPos.y + 0.3f; // 掴んでいる敵の高さに合わせて浮かせる

                m_Velocity = DirectX::XMFLOAT3(0, 0, 0); // 吸い込まれている間は速度をゼロにする

                // 吸い込まれている間の回転
                m_Rotation.y += 0.15f;
            }
        }
    else if (m_EnemyState == EnemyState::BLOWN_AWAY) {
        // 爆風で吹き飛ばされている物理処理
        MathHelper::ScaleXZ(m_Velocity, 0.95f); // 空気抵抗
        m_VelocityY -= 0.02f; // 重力
        
        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;
        m_Position.y += m_VelocityY;
        
        // めめり込み防止の接地チェック
        float baseOffset = (m_Size.y * m_Scale.y - 1.0f) * 0.5f;
        float floorY = -0.5f + baseOffset;
        if (m_Position.y < floorY) {
            m_Position.y = floorY;
            m_VelocityY = 0.0f;
        }
        
        // 吹き飛び中の回転
        m_Rotation.x += 0.3f;
        m_Rotation.y += 0.2f;
        m_Rotation.z += 0.25f;
        
        // 地面に接地し、移動速度が十分に遅くなったら撃破状態に変更して消滅へ
        if (m_Position.y <= floorY && abs(m_Velocity.x) < 0.05f && abs(m_Velocity.z) < 0.05f) {
            m_EnemyState = EnemyState::DEFEATED;
            m_Velocity = DirectX::XMFLOAT3(0, 0, 0);
        }
    }
}

void Enemy::Draw()
{
    // 通常パスの場合、RenderSystem で一括描画するためここでは描画しない
    // 電撃属性（m_IsLightning）の場合は自主的にスパークエフェクトを描画する
    if (!Renderer::IsShadowMode() && !Renderer::IsOutlineMode())
    {
        if (IsLightning())
        {
            Player* player = Manager::GetGameObject<Player>();
            if (player)
            {
                // 敵の中心から周囲の空間に2〜3本のパチパチする稲妻を描画
                XMFLOAT3 start = m_Position;
                start.y += 0.3f; // 敵の高さの中心

                int sparks = 2 + (rand() % 2); 
                for (int i = 0; i < sparks; i++)
                {
                    float angle = ((float)rand() / RAND_MAX) * XM_2PI;
                    float pitch = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * XM_PIDIV4; // 上下45度
                    float dist = 0.8f + ((float)rand() / RAND_MAX) * 1.2f; // 放電範囲 0.8〜2.0m

                    XMFLOAT3 end = XMFLOAT3(
                        start.x + sinf(angle) * cosf(pitch) * dist,
                        start.y + sinf(pitch) * dist,
                        start.z + cosf(angle) * cosf(pitch) * dist
                    );

                    // パチパチする水色電撃の描画
                    player->DrawLightningBolt(start, end, 0.02f, XMFLOAT4(0.0f, 2.0f, 2.8f, 1.0f));
                }
            }
        }
        return;
    }

    XMMATRIX worldMatrix =
        XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
        XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z) *
        XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    Renderer::SetWorldMatrix(worldMatrix);

    Renderer::DrawCube(worldMatrix, m_Texture);
}

// ─────────────────────────────────────────────
// 撃破処理（二重カウント防止機能・スコア・ポップアップ演出）
// ─────────────────────────────────────────────
void Enemy::Defeat(float colorR, float colorG, float colorB)
{
    if (m_IsDefeatedCounted) return;
    m_IsDefeatedCounted = true;

    // イベントの発行
    EnemyDefeatedEvent defEvent;
    defEvent.scoreValue = m_ScoreValue;
    defEvent.position = m_Position;
    defEvent.position.y += 1.0f; // ポップアップの高さオフセット
    defEvent.popupColor = DirectX::XMFLOAT3(colorR, colorG, colorB);
    EventSystem::Publish<EnemyDefeatedEvent>(defEvent);
}

XMFLOAT3 Enemy::GetEmissive() const
{
    if (m_Affix) {
        return m_Affix->GetEmissive();
    }
    return GameObject::GetEmissive();
}

void Enemy::OnHit(const HitInfo& info)
{
    if (m_EnemyState == EnemyState::DEFEATED || m_EnemyState == EnemyState::BLOWN_AWAY) return;

    SetVelocity(info.knockbackVel);
    SetEnemyState(EnemyState::BLOWN_AWAY);

    if (info.setLightning) {
        SetLightning(true);
    }

    Defeat(info.popupColor.x, info.popupColor.y, info.popupColor.z);
}

bool Enemy::IsExplosive() const
{
    return m_Affix ? m_Affix->IsExplosive() : false;
}

void Enemy::SetExplosive(bool explosive)
{
    if (explosive) {
        m_Affix = std::make_shared<ExplosiveAffix>();
    } else if (IsExplosive()) {
        m_Affix = nullptr;
    }
}

bool Enemy::IsLightning() const
{
    return m_Affix ? m_Affix->IsLightning() : false;
}

void Enemy::SetLightning(bool lightning)
{
    if (lightning) {
        m_Affix = std::make_shared<LightningAffix>();
    } else if (IsLightning()) {
        m_Affix = nullptr;
    }
}

bool Enemy::IsSandbag() const
{
    return m_Affix ? m_Affix->IsSandbag() : false;
}

void Enemy::SetSandbag(bool enable)
{
    if (enable) {
        m_Affix = std::make_shared<SandbagAffix>();
    } else if (IsSandbag()) {
        m_Affix = nullptr;
    }
}


