#include "attacking_enemy.h"
#include "player.h"
#include "manager.h"
#include "enemy_bullet.h"
#include "math_helper.h"
#include "collision.h"

// ─────────────────────────────────────────────
// 初期化
// ─────────────────────────────────────────────
void AttackingEnemy::Init()
{
    // 親クラスの初期化
    Enemy::Init();

    // 攻撃する敵は難易度が高いのでスコアを200点に設定
    SetScoreValue(200);

    // 射撃クールダウンタイマーをバラつかせる（初期ディレイ）
    m_ShootCooldown = (BASE_COOLDOWN / 2) + rand() % BASE_COOLDOWN;
}

// ─────────────────────────────────────────────
// 更新処理
// ─────────────────────────────────────────────
void AttackingEnemy::Update()
{
    EnemyState state = GetEnemyState();

    // 通常状態のときのみ、プレイヤーの追尾と射撃を行う
    if (state == EnemyState::NORMAL || state == EnemyState::CHASING)
    {
        Player* player = Manager::GetGameObject<Player>();
        if (player)
        {
            XMFLOAT3 playerPos = player->GetPosition();
            XMFLOAT3 toPlayer  = playerPos - m_Position;
            toPlayer.y = 0.0f; // 高さの差は追尾物理において無視

            float dist = MathHelper::Length(toPlayer);
            if (dist > 0.001f)
            {
                toPlayer.x /= dist;
                toPlayer.z /= dist;
            }

            // ─── ① 周囲の敵を迂回するための反発ベクトル（Steering Avoidance）の計算 ───
            XMFLOAT3 steer = toPlayer; // 基本はプレイヤーへの方向

            for (GameObject* obj : Manager::GetGameObjectList())
            {
                if (!obj || obj == this || obj->IsDestroy()) continue;

                // 他の敵（サンドバックエネミー または 射撃エネミー）に対して反発する
                if (obj->GetObjectType() == ObjectType::Enemy)
                {
                    XMFLOAT3 otherPos = obj->GetPosition();
                    XMFLOAT3 diff = m_Position - otherPos;
                    diff.y = 0.0f; // 水平面のみ考慮

                    float d = MathHelper::Length(diff);
                    if (d < AVOID_RADIUS && d > 0.001f)
                    {
                        // 距離が近いほど、強い反発力を進行方向にブレンドする
                        float strength = (AVOID_RADIUS - d) / AVOID_RADIUS;
                        steer.x += (diff.x / d) * strength * AVOID_FORCE;
                        steer.z += (diff.z / d) * strength * AVOID_FORCE;
                    }
                }
            }

            // 回避ベクトルを正規化
            float steerLen = MathHelper::Length(steer);
            if (steerLen > 0.001f)
            {
                steer.x /= steerLen;
                steer.z /= steerLen;
            }

            // 移動方向（調整後の進行方向）を向かせる
            float angle = atan2f(steer.x, steer.z);
            m_Rotation.y = angle;

            // ─── ② Y軸ジャンプと重力の物理計算 ───
            XMFLOAT3 vel = GetVelocity();
            float speed = ATTACK_SPEED;
            vel.x = steer.x * speed;
            vel.z = steer.z * speed;

            // 接地チェックと重力適用
            float baseOffset = (m_Size.y * m_Scale.y - 1.0f) * 0.5f;
            float floorY = -0.5f + baseOffset;
            bool isGrounded = (m_Position.y <= floorY + 0.001f);

            if (!isGrounded)
            {
                // 空中なら重力を適用
                vel.y -= GRAVITY;
            }
            else
            {
                m_Position.y = floorY;
                vel.y = 0.0f;
            }

            // 移動先座標を計算（X, Z成分のみ）
            XMFLOAT3 nextPos = m_Position;
            nextPos.x += vel.x;
            nextPos.z += vel.z;

            // XZ軸の移動先で衝突判定
            bool hasCollision = Collision::CheckAABBCollision(this, nextPos, Manager::GetGameObjectList());

            if (!hasCollision)
            {
                m_Position.x = nextPos.x;
                m_Position.z = nextPos.z;
                SetEnemyState(EnemyState::CHASING);
            }
            else
            {
                // 進行方向のX, Z軸移動で衝突した場合
                // 衝突した相手が敵であるかチェックし、敵ならジャンプする
                if (isGrounded)
                {
                    // 自身のAABBと衝突する敵を特定する
                    for (GameObject* obj : Manager::GetGameObjectList())
                    {
                        if (!obj || obj == this || obj->IsDestroy()) continue;
                        if (obj->GetObjectType() == ObjectType::Enemy)
                        {
                            // AABB判定
                            if (Collision::CheckAABB(this, nextPos, obj))
                            {
                                // 敵と衝突しているなら、ジャンプ初速を付与！
                                vel.y = JUMP_FORCE;
                                isGrounded = false;
                                break;
                            }
                        }
                    }
                }

                // 壁などの他の障害物でジャンプもできない（または敵以外の衝突）場合
                if (isGrounded)
                {
                    vel.x = 0.0f;
                    vel.z = 0.0f;
                    SetEnemyState(EnemyState::NORMAL);
                }
            }

            // Y軸の移動を反映
            m_Position.y += vel.y;
            if (m_Position.y < floorY)
            {
                m_Position.y = floorY;
                vel.y = 0.0f;
            }

            // 物理速度を保存
            SetVelocity(vel);

            // プレイヤーとの距離が一定（SHOOT_RANGE）以内なら射撃する
            if (dist < SHOOT_RANGE)
            {
                if (m_ShootCooldown > 0)
                {
                    m_ShootCooldown--;
                }
                else
                {
                    // クールダウンリセット
                    m_ShootCooldown = BASE_COOLDOWN + rand() % (BASE_COOLDOWN / 2);

                    // 弾オブジェクトの生成と設定
                    EnemyBullet* bullet = Manager::AddGameObject<EnemyBullet>();
                    if (bullet)
                    {
                        // 敵の少し前方かつ、胸の高さ付近から発射
                        XMFLOAT3 spawnPos = m_Position;
                        spawnPos.x += toPlayer.x * 0.8f;
                        spawnPos.z += toPlayer.z * 0.8f;
                        spawnPos.y += 0.2f;

                        bullet->SetPosition(spawnPos);
                        bullet->SetDirection(toPlayer);
                    }
                }
            }
        }
    }
    else
    {
        // 投げ、吸い込み、吹き飛びなどの状態は、親クラス Enemy の高度な物理に任せる
        Enemy::Update();
    }
}
