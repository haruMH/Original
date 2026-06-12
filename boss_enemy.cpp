#include "boss_enemy.h"
#include "renderer.h"
#include "resource_manager.h"
#include "math_helper.h"
#include "player.h"
#include "manager.h"
#include "camera.h"
#include "enemy_bullet.h"
#include "score_popup.h"
#include "game_rule.h"
#include "shockwave.h"

// ─────────────────────────────────────────────
// 初期化
// ─────────────────────────────────────────────
void BossEnemy::Init()
{
    Enemy::Init();
    
    m_EnemyState = EnemyState::NORMAL;
    m_Scale = XMFLOAT3(5.0f, 5.0f, 5.0f); // 壁と同じ大きさ (5x5x5)
    m_Size  = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_HP    = 15;
    m_MaxHP = 15;
    SetScoreValue(1500); // 巨大ボスは15倍スコア (1500点)
    m_DamageFlashTimer = 0;
    m_AttackTimer      = 0;
    m_AttackPattern    = 0;

    m_RenderComponent = RenderComponent("enemy.png", MeshType::Cube, true);
}

// ─────────────────────────────────────────────
// 更新処理
// ─────────────────────────────────────────────
void BossEnemy::Update()
{
    if (m_DamageFlashTimer > 0) {
        m_DamageFlashTimer--;
    }

    if (m_EnemyState == EnemyState::DEFEATED || m_EnemyState == EnemyState::BLOWN_AWAY) {
        Enemy::Update();
        return;
    }

    UpdateBossAI();
}

// ─────────────────────────────────────────────
// ボスAI制御
// ─────────────────────────────────────────────
void BossEnemy::UpdateBossAI()
{
    Player* player = Manager::GetGameObject<Player>();
    if (!player) return;

    XMFLOAT3 playerPos = player->GetPosition();
    XMFLOAT3 bossPos = m_Position;

    XMFLOAT3 toPlayer = playerPos - bossPos;
    toPlayer.y = 0.0f;
    float dist = MathHelper::Length(toPlayer);

    if (dist > 4.0f) {
        XMFLOAT3 dir = MathHelper::Normalize(toPlayer);
        m_Velocity = dir * 0.02f; // ゆっくり移動
        m_Position.x += m_Velocity.x;
        m_Position.z += m_Velocity.z;

        m_Rotation.y = atan2f(dir.x, dir.z);
    } else {
        m_Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    m_AttackTimer++;
    if (m_AttackTimer >= 120) { // 攻撃間隔
        m_AttackTimer = 0;
        OutputDebugStringA("[BossEnemy] Attack timer reached 120. Performing attack.\n");
        
        m_AttackPattern = (m_AttackPattern + 1) % 2;
        if (m_AttackPattern == 0) {
            OutputDebugStringA("[BossEnemy] Fire 3-Way Spread Bullet.\n");
            Fire3WaySpread();
        } else {
            OutputDebugStringA("[BossEnemy] Fire Rapid Bullet.\n");
            FireRapidShot();
        }
    }
}

// ─────────────────────────────────────────────
// 3方向拡散弾の発射
// ─────────────────────────────────────────────
void BossEnemy::Fire3WaySpread()
{
    Player* player = Manager::GetGameObject<Player>();
    if (!player) return;

    XMFLOAT3 bulletPos = m_Position;
    bulletPos.y += 1.5f; // 巨大ボスの中心付近から

    XMFLOAT3 toPlayer = player->GetPosition() - bulletPos;
    float dist = MathHelper::Length(toPlayer);
    if (dist < 0.01f) return;
    XMFLOAT3 baseDir = MathHelper::Normalize(toPlayer);

    float angles[3] = { 0.0f, -0.26f, 0.26f }; // 中央、左右約15度
    for (int i = 0; i < 3; i++) {
        EnemyBullet* bullet = Manager::AddGameObject<EnemyBullet>();
        bullet->SetPosition(bulletPos);

        float angle = angles[i];
        float rotX = baseDir.x * cosf(angle) - baseDir.z * sinf(angle);
        float rotZ = baseDir.x * sinf(angle) + baseDir.z * cosf(angle);

        bullet->SetDirection(XMFLOAT3(rotX, baseDir.y, rotZ));
        bullet->SetSpeed(0.18f);
        bullet->SetScale(XMFLOAT3(1.5f, 1.5f, 1.5f)); // 巨大な弾
    }

    if (g_Camera) g_Camera->Shake(0.15f, 8);
}

// ─────────────────────────────────────────────
// 高速連射弾（擬似連射）の発射
// ─────────────────────────────────────────────
void BossEnemy::FireRapidShot()
{
    Player* player = Manager::GetGameObject<Player>();
    if (!player) return;

    XMFLOAT3 bulletPos = m_Position;
    bulletPos.y += 1.5f;

    XMFLOAT3 toPlayer = player->GetPosition() - bulletPos;
    XMFLOAT3 dir = MathHelper::Normalize(toPlayer);

    // 速度差で擬似的な3連射を表現
    float speeds[3] = { 0.12f, 0.16f, 0.20f };
    for (int i = 0; i < 3; i++) {
        EnemyBullet* bullet = Manager::AddGameObject<EnemyBullet>();
        bullet->SetPosition(bulletPos);
        bullet->SetDirection(dir);
        bullet->SetSpeed(speeds[i]);
        bullet->SetScale(XMFLOAT3(1.2f, 1.2f, 1.2f));
    }

    if (g_Camera) g_Camera->Shake(0.12f, 8);
}

// ─────────────────────────────────────────────
// ボス被弾ダメージ処理
// ─────────────────────────────────────────────
void BossEnemy::ApplyBossDamage(int damage, const DirectX::XMFLOAT3& hitSourcePos)
{
    if (m_EnemyState == EnemyState::DEFEATED || m_EnemyState == EnemyState::BLOWN_AWAY) return;

    m_HP -= damage;
    m_DamageFlashTimer = 15;

    Manager::AddHitStop(12);
    if (g_Camera) g_Camera->Shake(0.40f, 12);

    // 巨大なので少しだけノックバック
    XMFLOAT3 diff = m_Position - hitSourcePos;
    diff.y = 0.0f;
    float dist = MathHelper::Length(diff);
    if (dist > 0.001f) {
        XMFLOAT3 pushDir = diff / dist;
        m_Position.x += pushDir.x * 0.3f;
        m_Position.z += pushDir.z * 0.3f;
    }

    // 残りHPポップアップ (赤文字)
    ScorePopupSystem::AddPopup(m_Position.x, m_Position.y + 3.0f, m_Position.z, m_HP, 2.5f, 0.0f, 0.0f);

    if (m_HP <= 0) {
        m_HP = 0;
        m_EnemyState = EnemyState::DEFEATED;
        m_Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

        // 撃破時に多重衝撃波を発生
        XMFLOAT3 deathPos = m_Position;
        deathPos.y = -0.95f;
        ShockwaveSystem::AddShockwave(deathPos, 18.0f, 2.5f, 0.0f, 0.0f, 40, 3.0f, 0);
        ShockwaveSystem::AddShockwave(deathPos, 12.0f, 2.5f, 1.5f, 0.0f, 30, 2.0f, 10);
        ShockwaveSystem::AddShockwave(deathPos, 6.0f, 1.8f, 2.2f, 0.0f, 20, 1.0f, 20);

        if (g_Camera) g_Camera->Shake(0.85f, 30);

        // ボス撃破処理（赤色ポップアップ）
        Defeat(2.5f, 0.0f, 0.0f);
    }
}

// ─────────────────────────────────────────────
// 描画処理
// ─────────────────────────────────────────────
void BossEnemy::Draw()
{
}

// ─────────────────────────────────────────────
// 自発光の定義
// ─────────────────────────────────────────────
DirectX::XMFLOAT3 BossEnemy::GetEmissive() const
{
    if (m_DamageFlashTimer > 0) {
        return XMFLOAT3(5.0f, 0.0f, 0.0f); // 被弾時点滅は赤
    }
    return XMFLOAT3(2.5f, 0.0f, 1.5f); // 通常時は赤紫色発光
}
