#include "enemy.h"
#include "renderer.h"
#include "resource_manager.h"
#include "math_helper.h"
#include "player.h"
#include "manager.h"

void Enemy::Init()
{
    m_Position = XMFLOAT3(0.0f, -0.5f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_Size     = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_UprightTimer = 0;
    m_IsExplosive  = false;

    m_Texture = ResourceManager::GetTexture("enemy.png");
    
    // コンポーネント指向での描画パラメータ初期化
    m_RenderComponent = RenderComponent("enemy.png", MeshType::Cube, true);
}

void Enemy::Uninit()
{
}

void Enemy::Update()
{
    if (m_EnemyState == EnemyState::FLYING) {
        // 摩擦（空気抵抗）で徐々に減速させる（摩擦を強くして飛びすぎを防止）
        MathHelper::ScaleXZ(m_Velocity, 0.94f);
        // 重力の適用（Y軸の落下）
        m_VelocityY -= 0.015f; 

        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;
        m_Position.y += m_VelocityY;

        // 回転中のめり込みを防ぐため、飛行中のクランプ床面を少し浮かせた高さ(-0.3f)にする
        float flightFloorY = -0.3f;

        // 地面（床）への着地クランプ
        if (m_Position.y < flightFloorY) {
            m_Position.y = flightFloorY;
            m_VelocityY = 0.0f;
        }

        // 速度が十分に落ち、かつ浮かせた地面に着地しているならNORMALに戻す
        // ただし爆発属性がある敵は game_scene 側で爆発処理するため、ここでは遷移しない
        if (m_Position.y <= flightFloorY && abs(m_Velocity.x) < 0.05f && abs(m_Velocity.z) < 0.05f) {
            if (!m_IsExplosive) {
                m_Position.y = -0.5f; // 静止時に本来の地面の高さ(-0.5f)に密着させる
                m_Velocity   = XMFLOAT3(0, 0, 0);
                m_VelocityY  = 0.0f;
                m_EnemyState = EnemyState::NORMAL;
                m_UprightTimer = 60;  // 1秒後にゆっくり起き上がるようにタイマーをセット
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
        // 静止後、1秒間（60フレーム）待機してからゆっくり直立に戻す
        if (m_UprightTimer > 0) {
            m_UprightTimer--;
        }
        else {
            // X軸とZ軸の回転を徐々に0（直立）に近づける（LERP）
            m_Rotation.x = MathHelper::Lerp(m_Rotation.x, 0.0f, 0.1f);
            m_Rotation.z = MathHelper::Lerp(m_Rotation.z, 0.0f, 0.1f);


            MathHelper::ClearIfNearZero(m_Rotation.x);
            MathHelper::ClearIfNearZero(m_Rotation.z);
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
        
        float floorY = -0.5f;
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
    // 通常パスの場合は RenderSystem 側で一括描画するため、ここでは描画しない
    if (!Renderer::IsShadowMode() && !Renderer::IsOutlineMode())
    {
        return;
    }

    XMMATRIX worldMatrix =
        XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
        XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z) *
        XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    Renderer::SetWorldMatrix(worldMatrix);

    Renderer::DrawCube(worldMatrix, m_Texture);
}