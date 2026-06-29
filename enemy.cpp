#include "enemy.h"
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
    m_IsExplosive  = false;
    m_IsLightning  = false;
    m_IsDefeatedCounted = false;
    m_ScoreValue   = Constants::Enemy::DEFAULT_SCORE;

    // リリースビルド時は Assets/texture/ サブフォルダから読み込む
#ifdef NDEBUG
    m_Texture = ResourceManager::GetTexture("Assets/texture/enemy.png");
    
    // コンポーネント指向での描画パラメータ初期化
    m_RenderComponent = RenderComponent("Assets/texture/enemy.png", MeshType::Cube, true);
#else
    m_Texture = ResourceManager::GetTexture("enemy.png");
    
    // コンポーネント指向での描画パラメータ初期化
    m_RenderComponent = RenderComponent("enemy.png", MeshType::Cube, true);
#endif
}

void Enemy::Uninit()
{
}

void Enemy::Update()
{
    if (m_EnemyState == EnemyState::FLYING) {
        // 摩擦（空気抵抗）で徐々に減速させる（摩擦を強くして飛びすぎを防止）
        MathHelper::ScaleXZ(m_Velocity, Constants::Enemy::FLYING_AIR_RESISTANCE);
        // 重力の適用（Y軸の落下）
        m_VelocityY -= Constants::Enemy::FLYING_GRAVITY; 

        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;
        m_Position.y += m_VelocityY;

        // スケールに応じた床面の高さを計算（めり込み防止）
        float baseOffset = (m_Size.y * m_Scale.y - 1.0f) * 0.5f;
        float flightFloorY = -0.3f + baseOffset;

        // 地面（床）への着地クランプ
        if (m_Position.y < flightFloorY) {
            // 接地した瞬間の処理（下向きの十分な速度がある場合）
            if (m_VelocityY < -0.05f) {
                // 巨大化エネミーが叩きつけられた際のカメラシェイクとヒットストップ
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

        // 速度が十分に落ち、かつ浮かせた地面に着地しているならNORMALに戻す（巨大エネミーは消滅）
        // ただし爆発属性がある敵は game_scene 側で爆発処理するため、ここでは遷移しない
        if (m_Position.y <= flightFloorY && abs(m_Velocity.x) < 0.05f && abs(m_Velocity.z) < 0.05f) {
            if (!m_IsExplosive) {
                if (m_Scale.x > 2.0f) {
                    // 巨大化エネミーは飛び終わってスピードが0になったら消滅させる
                    m_EnemyState = EnemyState::DEFEATED;
                    m_Velocity   = XMFLOAT3(0.0f, 0.0f, 0.0f);
                    m_VelocityY  = 0.0f;

                    // スピードが0になり、消滅する瞬間に多重波紋衝撃波を発生させる（Y座標を地面の高さに強制固定し、規模とディレイを拡大）
                    XMFLOAT3 shockPos = m_Position;
                    shockPos.y = -0.95f; // 地面の高さに完全クランプ

                    ShockwaveSystem::AddShockwave(shockPos, 15.0f, 1.8f, 0.9f, 0.0f, 32, 1.4f, 0);
                    ShockwaveSystem::AddShockwave(shockPos, 11.0f, 1.8f, 0.9f, 0.0f, 26, 0.9f, 7);
                    ShockwaveSystem::AddShockwave(shockPos, 7.0f, 1.8f, 0.9f, 0.0f, 20, 0.5f, 14);
                } else {
                    m_Position.y = -0.5f + baseOffset; // 静止時に本来の地面の高さに密着させる
                    m_Velocity   = XMFLOAT3(0, 0, 0);
                    m_VelocityY  = 0.0f;
                    m_EnemyState = EnemyState::NORMAL;
                    m_UprightTimer = 60;  // 1秒後にゆっくり起き上がるようにタイマーをセット
                }
            }
            // 爆発属性の場合は FLYING のまま → game_scene の着地+速度0チェックで爆発させる
        }

        m_Rotation.x += 0.2f;
        m_Rotation.z += 0.15f;
    }
    else if (m_EnemyState == EnemyState::DEFEATED) {
        m_Scale *= 0.85f;                                  // 3軸同時に縮小！
        m_Rotation += DirectX::XMFLOAT3(0.5f, 0.5f, 0.5f); // 3軸同時に高速回転！
        MathHelper::ScaleXZ(m_Velocity, 0.9f);             // 摩擦による減速

        // 吹っ飛ぶ慣性を少しだけ維持してスライドさせる
        m_Velocity.x *= 0.9f;
        m_Velocity.z *= 0.9f;
        
        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;

        if (m_Scale.x < 0.05f) {
            SetDestroy(); // 完全に消滅
        }
    }
    else if (m_EnemyState == EnemyState::NORMAL) {
        if (m_IsSandbag) {
            m_Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
            m_SandbagLife--;
            if (m_SandbagLife <= 0) {
                Defeat();
                m_EnemyState = EnemyState::DEFEATED;
                return;
            }
        }
        else {
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
    }
    else if (m_EnemyState == EnemyState::VACUUMED) {
        // 掴んでいる敵を中心にブラックホールのように公転する挙動
        Player* player = Manager::GetGameObject<Player>();
        if (player) {
                // プレイヤーがスピン状態を終えていたら通常に戻す
                if (player->GetState() != PlayerState::SPINNING) {
                    m_EnemyState = EnemyState::NORMAL;
                    return;
                }

                // ブラックホールの中心 = 掴んでいる敵の位置（なければプレイヤー位置）
                Enemy* grabbed = player->GetGrabbedEnemy();
                DirectX::XMFLOAT3 pivotPos = grabbed
                    ? grabbed->GetPosition()
                    : player->GetPosition();

                DirectX::XMFLOAT3 diff = m_Position - pivotPos;
                float dist = MathHelper::Length(diff);

                // 徐々に掴まれている敵に引き寄せる（半径 2.5 に収束）
                float targetDist = 2.5f;
                dist = MathHelper::Lerp(dist, targetDist, 0.06f);

                // 掴んでいる敵のスピン速度で公転させる
                float angle = atan2f(diff.x, diff.z);
                angle += player->GetAngularVelocity();

                m_Position.x = pivotPos.x + sinf(angle) * dist;
                m_Position.z = pivotPos.z + cosf(angle) * dist;
                m_Position.y = pivotPos.y + 0.3f; // 掴んでいる敵の高さに合わせて浮かす

                m_Velocity = DirectX::XMFLOAT3(0, 0, 0); // 吸い込まれている間は速度をゼロにする

                // 吸引されている演出で自転させる
                m_Rotation.y += 0.15f;
            }
        }
    else if (m_EnemyState == EnemyState::BLOWN_AWAY) {
        // 爆風で吹き飛ばされる物理挙動
        MathHelper::ScaleXZ(m_Velocity, 0.95f); // 空気抵抗
        m_VelocityY -= 0.02f; // 重力
        
        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;
        m_Position.y += m_VelocityY;
        
        // スケールに応じた床面の高さを計算（めり込み防止）
        float baseOffset = (m_Size.y * m_Scale.y - 1.0f) * 0.5f;
        float floorY = -0.5f + baseOffset;
        if (m_Position.y < floorY) {
            m_Position.y = floorY;
            m_VelocityY = 0.0f;
        }
        
        // 激しく回転する
        m_Rotation.x += 0.3f;
        m_Rotation.y += 0.2f;
        m_Rotation.z += 0.25f;
        
        // 地面に激突して、速度が十分に遅くなったら撃破状態（DEFEATED）にして消滅させる
        if (m_Position.y <= floorY && abs(m_Velocity.x) < 0.05f && abs(m_Velocity.z) < 0.05f) {
            m_EnemyState = EnemyState::DEFEATED;
            m_Velocity = DirectX::XMFLOAT3(0, 0, 0);
        }
    }
}

void Enemy::Draw()
{
    // 通常パスの場合は RenderSystem 側で一括描画するため本体は描画しないが、
    // 電撃属性（m_IsLightning）がある場合は周囲にスパークエフェクトを描画する
    if (!Renderer::IsShadowMode() && !Renderer::IsOutlineMode())
    {
        if (m_IsLightning)
        {
            Player* player = Manager::GetGameObject<Player>();
            if (player)
            {
                // 敵の中心から周囲の空中へ 2〜3本 ほどビリビリと火花を散らす
                XMFLOAT3 start = m_Position;
                start.y += 0.3f; // 中心付近

                int sparks = 2 + (rand() % 2); 
                for (int i = 0; i < sparks; i++)
                {
                    float angle = ((float)rand() / RAND_MAX) * XM_2PI;
                    float pitch = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * XM_PIDIV4; // 上下45度
                    float dist = 0.8f + ((float)rand() / RAND_MAX) * 1.2f; // 長さ0.8〜2.0m

                    XMFLOAT3 end = XMFLOAT3(
                        start.x + sinf(angle) * cosf(pitch) * dist,
                        start.y + sinf(pitch) * dist,
                        start.z + cosf(angle) * cosf(pitch) * dist
                    );

                    // パチパチ明滅するシアンのイナズマを描画
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
// 撃破処理（二重カウント防止機能付き）
// ─────────────────────────────────────────────
void Enemy::Defeat(float colorR, float colorG, float colorB)
{
    if (m_IsDefeatedCounted) return;
    m_IsDefeatedCounted = true;

    // スコア加算と撃破数インクリメント
    GameRule::OnEnemyDefeated(m_ScoreValue);

    // スコアポップアップ表示
    ScorePopupSystem::AddPopup(m_Position.x, m_Position.y + 1.0f, m_Position.z, m_ScoreValue, colorR, colorG, colorB);
}

XMFLOAT3 Enemy::GetEmissive() const
{
    if (m_IsSandbag) {
        return XMFLOAT3(3.0f, 2.0f, 0.0f);
    }
    return GameObject::GetEmissive();
}