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

            // プレイヤーに向かってゆっくりと接近
            float speed = ATTACK_SPEED;
            XMFLOAT3 vel = GetVelocity();
            vel.x = toPlayer.x * speed;
            vel.z = toPlayer.z * speed;
            SetVelocity(vel);

            // 移動方向（プレイヤー方向）を向かせる
            float angle = atan2f(toPlayer.x, toPlayer.z);
            m_Rotation.y = angle;

            // 移動先座標を計算して衝突判定
            XMFLOAT3 nextPos = m_Position;
            nextPos.x += vel.x;
            nextPos.z += vel.z;

            // 衝突しなければ座標を更新
            if (!Collision::CheckAABBCollision(this, nextPos, Manager::GetGameObjectList()))
            {
                m_Position = nextPos;
                SetEnemyState(EnemyState::CHASING);
            }
            else
            {
                // 壁などに衝突した場合は移動を止める
                XMFLOAT3 stopVel = GetVelocity();
                stopVel.x = 0.0f;
                stopVel.z = 0.0f;
                SetVelocity(stopVel);
                SetEnemyState(EnemyState::NORMAL);
            }

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
