#include "enemy_bullet.h"
#include "renderer.h"
#include "resource_manager.h"
#include "player.h"
#include "manager.h"
#include "math_helper.h"
#include "collision.h"
#include "enemy.h"
#include "wall.h"

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
    m_Position.z += m_Direction.z * m_Speed;

    // プレイヤーとの衝突判定
    Player* player = Manager::GetGameObject<Player>();
    if (player)
    {
        // 簡易的な球判定（半径を考慮）
        XMFLOAT3 diff = m_Position - player->GetPosition();
        diff.y = 0.0f; // 高さのブレを無視してXZ平面上で判定
        float dist = MathHelper::Length(diff);
        float limitDist = GetRadius() + player->GetRadius() * 0.7f; // プレイヤーの当たり判定を少し狭めにしてゲーム性向上

        if (dist < limitDist)
        {
            // プレイヤーに1ダメージを与える
            player->ApplyDamage(1, m_Position);
            // 弾は消滅
            SetDestroy();
            return;
        }
    }

    // 壁との衝突判定（壁に当たったら消滅）
    for (GameObject* obj : Manager::GetGameObjectList())
    {
        if (!obj || obj->IsDestroy() || obj == this || obj == player) continue;
        
        // エネミー（他のエネミーや自分自身）とは衝突させない
        if (obj->GetObjectType() == ObjectType::Enemy) continue;

        // 壁などのオブジェクトと衝突したら弾を消す
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
