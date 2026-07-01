#include "player.h"
#include "player_movement.h"
#include "player_combat.h"
#include "player_visual.h"
#include "renderer.h"
#include "resource_manager.h"
#include "manager.h"
#include "game_constants.h"
#include "camera.h"
#include "shockwave.h"
#include "enemy_bullet.h"
#include "enemy.h"

using namespace DirectX;

// =================================================================
// 初期化処理
// =================================================================
void Player::Init()
{
    // 各サブモジュールのインスタンス生成
    m_Movement = new PlayerMovement(this);
    m_Combat   = new PlayerCombat(this);
    m_Visual   = new PlayerVisual(this);

    // プレイヤーの初期座標と状態設定
    m_Position = XMFLOAT3(0.0f, -0.5f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_Size     = XMFLOAT3(1.0f, 1.0f, 1.0f);

    m_State            = PlayerState::IDLE;
    m_IsAutoSpinning   = false;
    m_CurrentSpinSpeed = Constants::Player::MIN_SPIN_SPEED;
    m_HasVacuumItem    = false;
    m_HasGigantItem    = false;
    m_HasLightningItem = false;
    m_MarkerTimer      = 0;
    m_LightningEffects.clear();

    m_HP               = Constants::Player::MAX_HP;
    m_MaxHP            = Constants::Player::MAX_HP;
    m_DamageTimer      = 0;
    m_InvincibleTimer  = 0;
    m_GuardTimer       = 0;
    m_LockOnTarget     = nullptr;
    m_LockOnFrame      = 0;
    m_WarpSlashCount   = 0;
    m_CanWarpSlash     = true;
    m_TackleTimer      = 0;

    // 各サブモジュールの初期化
    m_Movement->Init();
    m_Combat->Init();
    m_Visual->Init();

    // テクスチャリソースの読み込みと描画コンポーネント設定
#ifdef NDEBUG
    m_Texture = ResourceManager::GetTexture("Assets/texture/player.png");
    m_RenderComponent = RenderComponent("Assets/texture/player.png", MeshType::Cube, true);
#else
    m_Texture = ResourceManager::GetTexture("player.png");
    m_RenderComponent = RenderComponent("player.png", MeshType::Cube, true);
#endif
}

// =================================================================
// 終了処理（解放）
// =================================================================
void Player::Uninit()
{
    // 生成したサブモジュールの破棄
    if (m_Movement) {
        delete m_Movement;
        m_Movement = nullptr;
    }
    if (m_Combat) {
        delete m_Combat;
        m_Combat = nullptr;
    }
    if (m_Visual) {
        delete m_Visual;
        m_Visual = nullptr;
    }
}

// =================================================================
// 毎フレーム更新処理
// =================================================================
void Player::Update()
{
    // HP0でのゲームオーバー判定
    if (m_HP <= 0) {
        Manager::ChangeScene(Scene::GAMEOVER);
        return;
    }

    // タックル有効時間のカウントダウン
    if (m_TackleTimer > 0) {
        m_TackleTimer--;
    }

    // スタン（被弾硬直）タイマーの更新
    if (m_DamageTimer > 0) {
        m_DamageTimer--;
    }

    // 無敵タイマーの更新とモデル点滅演出
    if (m_InvincibleTimer > 0) {
        m_InvincibleTimer--;
        // 8フレーム周期でモデル表示を点滅（4F非表示、4F表示）
        GetRenderComponent().visible = (m_InvincibleTimer % 8 < 4);
    } else {
        GetRenderComponent().visible = true;
    }

    // サブモジュール（移動・戦闘・描画用エフェクト）の更新処理を実行
    m_Movement->Update();
    m_Combat->Update();
    m_Visual->Update();
}

// =================================================================
// 描画処理
// =================================================================
void Player::Draw()
{
    // エフェクト描画をビジュアルモジュールに委譲
    m_Visual->Draw();
}

// =================================================================
// つかんだ敵のセット
// =================================================================
void Player::SetGrabbedEnemy(Enemy* enemy)
{
    m_GrabbedEnemy = enemy;
    m_State = PlayerState::GRABBED;
    
    if (enemy) {
        enemy->SetEnemyState(EnemyState::GRABBED);
        
        // 巨大化アイテムを取得している場合は、つかんだ敵を巨大化させる
        if (m_HasGigantItem) {
            enemy->SetScale(XMFLOAT3(Constants::Enemy::GIGANT_SCALE, Constants::Enemy::GIGANT_SCALE, Constants::Enemy::GIGANT_SCALE));
            
            // 地面に埋まらないようにY座標を補正
            XMFLOAT3 pos = enemy->GetPosition();
            pos.y = -0.5f + Constants::Enemy::GIGANT_SCALE * 0.5f; 
            enemy->SetPosition(pos);
        }
    }
}

// =================================================================
// 稲妻エフェクトの追加
// =================================================================
void Player::AddLightningEffect(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end)
{
    LightningEffect effect;
    effect.Timer = Constants::Player::LIGHTNING_TIMER * 3; // 24フレーム
    
    LightningSegment seg;
    seg.Start = start;
    seg.End = end;
    effect.Segments.push_back(seg);

    m_LightningEffects.push_back(effect);
}

// =================================================================
// 他モジュールへの委譲ゲッター/セッター/ラッパー
// =================================================================
bool Player::IsDashing() const
{
    return m_Movement ? m_Movement->IsDashing() : false;
}

bool Player::IsJustDodgeActive() const
{
    return m_Movement ? m_Movement->IsJustDodgeActive() : false;
}

void Player::ResetDashCooldown()
{
    if (m_Movement) m_Movement->ResetDashCooldown();
}

bool Player::IsParryActive() const
{
    // ガード入力直後の12フレーム以内がパリィ有効時間
    return m_GuardTimer > 0 && m_GuardTimer <= Constants::Player::PARRY_ACCEPT_DURATION;
}

void Player::DrawLightningBolt(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color)
{
    if (m_Visual) {
        m_Visual->DrawLightningBolt(start, end, thickness, color);
    }
}

void Player::DrawLightningBoltInternal(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color, bool drawBranches, int seedOffset)
{
    if (m_Visual) {
        m_Visual->DrawLightningBoltInternal(start, end, thickness, color, drawBranches, seedOffset);
    }
}

void Player::Throw()
{
    if (m_Combat) m_Combat->Throw();
}

void Player::ApplyDamage(int damage, const DirectX::XMFLOAT3& enemyPos)
{
    if (m_Combat) m_Combat->ApplyDamage(damage, enemyPos);
}

void Player::ExecuteParryCounter(DirectX::XMFLOAT3 bulletPos)
{
    if (m_Combat) m_Combat->ExecuteParryCounter(bulletPos);
}

// =================================================================
// 被弾・リスタート（復活）処理
// =================================================================
void Player::Restart()
{
    // 各ステータスのリセット
    m_HP = m_MaxHP;
    m_DamageTimer = Constants::Player::DAMAGE_STUN_DURATION; // 30F
    m_InvincibleTimer = Constants::Player::INVINCIBLE_DURATION * 3; // 180F
    m_State = PlayerState::IDLE;
    m_IsAutoSpinning = false;
    m_CurrentSpinSpeed = Constants::Player::MIN_SPIN_SPEED;

    m_Position = XMFLOAT3(0.0f, -0.5f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    
    // 復活時のバウンド演出
    m_Scale.y = 2.0f;
    m_Scale.x = 0.5f;
    m_Scale.z = 0.5f;

    m_Movement->Init();
    m_Combat->Init();
    m_Visual->Init();

    // つかんでいた敵を強制解放
    if (m_GrabbedEnemy) {
        m_GrabbedEnemy->SetEnemyState(EnemyState::NORMAL);
        m_GrabbedEnemy->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
        m_GrabbedEnemy = nullptr;
    }

    // 画面内の敵の弾をすべて消去
    for (GameObject* obj : Manager::GetGameObjectList()) {
        if (obj && obj->GetObjectType() == ObjectType::Bullet) {
            EnemyBullet* bullet = static_cast<EnemyBullet*>(obj);
            bullet->SetDestroy();
        }
    }
    
    // プレイヤーの足元に強力な多重衝撃波を発生させて、周囲の敵をなぎ払う
    XMFLOAT3 shockPos = m_Position;
    shockPos.y = -0.95f; // 床面
    ShockwaveSystem::AddShockwave(shockPos, 18.0f, 2.5f, 1.8f, 0.0f, 40, 2.2f, 0);

    // カメラの強い揺れ
    if (g_Camera) {
        g_Camera->Shake(0.8f, 25);
    }
}

// =================================================================
// オブジェクト破棄時のクリーンアップ通知（ダングリングポインタ防止）
// =================================================================
void Player::NotifyObjectDestroyed(GameObject* obj)
{
    if (m_LockOnTarget == obj) {
        m_LockOnTarget = nullptr;
        m_LockOnFrame = 0;
        m_WarpSlashCount = 0;
    }
    if (m_GrabbedEnemy == obj) {
        m_GrabbedEnemy = nullptr;
        m_State = PlayerState::IDLE;
    }
}

// =================================================================
// プレイヤーの正面方向ベクトルの取得
// =================================================================
XMFLOAT3 Player::GetForwardVector() const
{
    return XMFLOAT3(sinf(m_Rotation.y), 0.0f, cosf(m_Rotation.y));
}

// =================================================================
// 自発光（Emissive）情報の取得（ガード/パリィ中のエフェクト発光）
// =================================================================
XMFLOAT3 Player::GetEmissive() const
{
    if (IsParryActive()) {
        return XMFLOAT3(0.0f, 2.0f, 5.0f); // パリィ受付中は眩しいシアン
    }
    if (IsGuardActive()) {
        return XMFLOAT3(0.0f, 0.5f, 1.5f); // ガード中は落ち着いた青色
    }
    return XMFLOAT3(0.0f, 0.0f, 0.0f);
}
