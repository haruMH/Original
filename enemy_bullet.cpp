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
    // リリースビルド時は Assets/texture/ サブフォルダから読み込む
#ifdef NDEBUG
    m_RenderComponent = RenderComponent("Assets/texture/enemy.png", MeshType::Cube, true);
#else
    m_RenderComponent = RenderComponent("enemy.png", MeshType::Cube, true);
#endif
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
            float dist = MathHelper::Length(diff);
            float limitDist = GetRadius() + player->GetRadius() * 0.7f;

            if (dist < limitDist)
            {
                if (player->IsParryActive())
                {
                    player->ExecuteParryCounter(m_Position);
                    SetDestroy();
                    return;
                }
                else if (player->IsJustDodgeActive())
                {
                    m_IsPlayerOwned = true;
                    m_Speed *= 1.2f;
                    SetDamage(2);
                    m_EmissiveColor = XMFLOAT3(0.0f, 1.8f, 2.5f);
                    m_Direction = player->GetForwardVector();
                    m_Life = BULLET_LIFE;

                    Manager::StartSlowMotion(180);

                    Manager::AddHitStop(12);
                    if (g_Camera)
                    {
                        g_Camera->Shake(0.3f, 15);
                    }

                    ShockwaveSystem::AddShockwave(player->GetPosition(), 10.0f, 0.0f, 3.5f, 5.0f, 30, 0.0f, 0);

                    player->ResetDashCooldown();
                    player->DisableWarpSlash();
                    // ジャスト回避成功時にタックルを有効化（モブ弾・ボス弾問わず）
                    player->EnableTackle(90);

                    GameRule::AddScore(150);
                    ScorePopupSystem::AddPopup(player->GetPosition().x, player->GetPosition().y + 1.0f, player->GetPosition().z, 150, 0.0f, 0.8f, 1.5f);

                    return;
                }
                else if (player->IsDashing())
                {
                    if (m_IsBossBullet)
                    {
                        // ボスの弾をダッシュ回避：タックル攻撃を有効化
                        player->EnableTackle(90); // 1.5秒間タックル有効
                        ShockwaveSystem::AddShockwave(player->GetPosition(), 5.0f, 0.0f, 2.0f, 3.0f, 20, 0.0f, 0);
                        if (g_Camera) g_Camera->Shake(0.2f, 8);
                    }
                    // 通常敵の弾はそのまますり抜け（タックル無効）
                    return;
                }
                else if (player->IsGuardActive())
                {
                    if (m_IsBossBullet)
                    {
                        // ボスの弾を通常ガードで受けたら高ダメージで反射発射
                        m_IsPlayerOwned = true;
                        m_Speed *= 1.5f;
                        SetDamage(4); // ジャストドッジの2より高い4ダメージ
                        m_EmissiveColor = XMFLOAT3(1.5f, 0.0f, 2.5f); // 紫色で反射弾を識別しやすく
                        m_Direction = player->GetForwardVector();
                        m_Life = BULLET_LIFE;

                        Manager::AddHitStop(3);
                        if (g_Camera) g_Camera->Shake(0.2f, 8);
                        ShockwaveSystem::AddShockwave(player->GetPosition(), 6.0f, 0.0f, 2.5f, 4.0f, 20, 0.0f, 0);
                        return;
                    }
                    else
                    {
                        // 通常ガード：ダメージ無効、弾消滅
                        Manager::AddHitStop(2);
                        SetDestroy();
                        return;
                    }
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
            float dist = MathHelper::Length(diff);
            float limitDist = GetRadius() + enemy->GetRadius();

            if (dist < limitDist)
            {
                if (enemy->GetObjectType() == ObjectType::Boss)
                {
                    BossEnemy* boss = static_cast<BossEnemy*>(enemy);
                    boss->ApplyBossDamage(GetDamage(), m_Position);
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
