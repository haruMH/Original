#include "enemy_bullet.h"
#include "renderer.h"
#include "resource_manager.h"
#include "player.h"
#include "manager.h"
#include "math_helper.h"
#include "collision.h"
#include "enemy.h"
#include "wall.h"
#include "camera.h"
#include "shockwave.h"
#include "boss_enemy.h"
#include "score_popup.h"
#include "game_rule.h"

// ─────────────────────────────────────────────
// 初期化
// ─────────────────────────────────────────────
void EnemyBullet::Init()
{
    m_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(0.4f, 0.4f, 0.4f); // 弾は小さめ
    m_Size     = XMFLOAT3(0.4f, 0.4f, 0.4f);
    m_Life     = BULLET_LIFE;
    m_Speed    = BULLET_SPEED;
    m_Destroy  = false;
    m_IsPlayerOwned = false;
    m_EmissiveColor = XMFLOAT3(2.5f, 0.5f, 0.0f);

    // 描画には enemy.png を流用し、インスタンス描画で描く
    m_RenderComponent = RenderComponent("enemy.png", MeshType::Cube, true);
}

// ─────────────────────────────────────────────
// 終了処理
// ─────────────────────────────────────────────
void EnemyBullet::Uninit()
{
}

// ─────────────────────────────────────────────
// 更新処理
// ─────────────────────────────────────────────
void EnemyBullet::Update()
{
    // 直線移動
    m_Position.x += m_Direction.x * m_Speed;
    m_Position.y += m_Direction.y * m_Speed;
    m_Position.z += m_Direction.z * m_Speed;

    Player* player = Manager::GetGameObject<Player>();

    // ─── ① プレイヤー反射状態（味方弾）かどうかの分岐 ───
    if (!m_IsPlayerOwned)
    {
        // === 敵の弾：プレイヤーとの衝突判定 ===
        if (player)
        {
            XMFLOAT3 diff = m_Position - player->GetPosition();
            diff.y = 0.0f;
            float dist = MathHelper::Length(diff);
            float limitDist = GetRadius() + player->GetRadius() * 0.7f;

            if (dist < limitDist)
            {
                if (player->IsParryActive())
                {
                    // ─── パリィ成功 ───
                    m_IsPlayerOwned = true; // 所有権をプレイヤー側に
                    m_Speed *= 1.8f;        // 速度アップ
                    m_EmissiveColor = XMFLOAT3(4.0f, 3.0f, 0.0f); // 眩しいゴールド色に変更
                    
                    // 軌道をプレイヤーの正面方向に反射
                    m_Direction = player->GetForwardVector();

                    // 残り寿命リセット
                    m_Life = BULLET_LIFE;

                    // ウィッチタイム（周囲のスローモーション）を発動
                    Manager::StartSlowMotion(120);

                    // 強烈なフィードバック
                    Manager::AddHitStop(12); // ヒットストップ
                    if (g_Camera)
                    {
                        g_Camera->Shake(0.45f, 15); // カメラシェイク
                    }

                    // プレイヤーの周囲に衝撃波エフェクト（敵をひるませる）
                    DirectX::XMFLOAT3 shockPos = player->GetPosition();
                    shockPos.y = -0.95f; // 床面に完全クランプ
                    // 小規模な衝撃波を発生
                    ShockwaveSystem::AddShockwave(shockPos, 6.0f, 1.2f, 0.8f, 0.0f, 16, 0.6f, 0);
                }
                else if (player->IsGuardActive())
                {
                    // ─── 通常ガード成功 ───
                    // ダメージを無効化し、弾は消滅
                    Manager::AddHitStop(2);
                    SetDestroy();
                    return;
                }
                else
                {
                    // ─── 被弾（通常ダメージ） ───
                    player->ApplyDamage(1, m_Position);
                    SetDestroy();
                    return;
                }
            }
        }
    }
    else
    {
        // === 反射された味方の弾：エネミー（Enemy）との衝突判定 ===
        for (GameObject* obj : Manager::GetGameObjectList())
        {
            if (!obj || obj->IsDestroy() || obj == this || obj == player) continue;
            if (obj->GetObjectType() != ObjectType::Enemy && obj->GetObjectType() != ObjectType::Boss) continue;

            Enemy* enemy = static_cast<Enemy*>(obj);

            // すでに倒されているエネミーは対象外
            EnemyState eState = enemy->GetEnemyState();
            if (eState == EnemyState::DEFEATED || eState == EnemyState::BLOWN_AWAY || eState == EnemyState::VACUUMED) continue;

            // 弾と敵の距離判定
            XMFLOAT3 diff = m_Position - enemy->GetPosition();
            diff.y = 0.0f;
            float dist = MathHelper::Length(diff);
            float limitDist = GetRadius() + enemy->GetRadius();

            if (dist < limitDist)
            {
                if (enemy->GetObjectType() == ObjectType::Boss)
                {
                    BossEnemy* boss = static_cast<BossEnemy*>(enemy);
                    boss->ApplyBossDamage(2, m_Position); // 反射弾はボスに2ダメージ
                }
                else
                {
                    // 敵に衝突：敵を吹き飛ばす
                    float force = 0.35f;
                    XMFLOAT3 pushVel = XMFLOAT3(m_Direction.x * force, 0.18f, m_Direction.z * force);
                    enemy->SetVelocity(pushVel);
                    enemy->SetEnemyState(EnemyState::BLOWN_AWAY);

                    // 撃破処理
                    enemy->Defeat();
                }

                // 弾は消滅
                SetDestroy();
                return;
            }
        }
    }

    // 壁との衝突判定（壁に当たったら消滅。味方弾でも同様）
    for (GameObject* obj : Manager::GetGameObjectList())
    {
        if (!obj || obj->IsDestroy() || obj == this || obj == player) continue;
        
        // 敵、ボス、地面、他の弾丸は除外（壁などの障害物のみと衝突させる）
        ObjectType type = obj->GetObjectType();
        if (type == ObjectType::Enemy || 
            type == ObjectType::Boss || 
            type == ObjectType::Field || 
            type == ObjectType::Bullet) 
        {
            continue;
        }

        if (Collision::CheckAABB(this, obj))
        {
            SetDestroy();
            return;
        }
    }

    // 寿命のカウントダウン
    m_Life--;
    if (m_Life <= 0)
    {
        SetDestroy();
    }
}

// ─────────────────────────────────────────────
// 描画処理（シャドウ/アウトラインパス用）
// ─────────────────────────────────────────────
void EnemyBullet::Draw()
{
    // 通常描画は RenderSystem 側で一括で行われるため、ここでは何もしない
    if (!Renderer::IsShadowMode() && !Renderer::IsOutlineMode()) return;

    XMMATRIX worldMatrix =
        XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
        XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z) *
        XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    Renderer::SetWorldMatrix(worldMatrix);

    ID3D11ShaderResourceView* tex = ResourceManager::GetTexture("enemy.png");
    Renderer::DrawCube(worldMatrix, tex);
}
