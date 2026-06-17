#include "player.h"
#include "renderer.h"
#include "input.h"
#include "manager.h"
#include "field.h"
#include "enemy.h"
#include "camera.h"
#include "collision.h"
#include "player_controller.h"
#include "resource_manager.h"
#include "math_helper.h"
#include "enemy_bullet.h"
#include "shockwave.h"
#include "game_rule.h"
#include "score_popup.h"
#include "boss_enemy.h"
void Player::Init()
{
    m_Position = XMFLOAT3(0.0f, -0.5f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_IsAutoSpinning   = false;
    m_CurrentSpinSpeed = 0.0f;
    m_HasVacuumItem    = false;
    m_HasGigantItem    = false;
    m_HasLightningItem = false;
    m_MarkerTimer      = 0;
    m_LightningEffects.clear();

    m_HP = 5;
    m_MaxHP = 5;
    m_DamageTimer = 0;
    m_InvincibleTimer = 0;
    m_KnockbackVelocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_GuardTimer = 0;
    m_LockOnTarget = nullptr;
    m_LockOnFrame = 0;
    m_WarpSlashCount = 0;
    m_MoveAnimation = 0.0f;

    m_VelocityY = 0.0f;
    m_IsJumping = false;
    m_JumpCount = 0;
    m_ScaleVelocityX = 0.0f;
    m_ScaleVelocityY = 0.0f;
    m_ScaleVelocityZ = 0.0f;

    m_DashTimer = 0;
    m_DashCooldown = 0;
    m_DashDirection = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_IsDashing = false;
    m_DashGhosts.clear();

    m_Texture = ResourceManager::GetTexture("player.png");

    // コンポーネント指向での描画パラメータ初期化
    m_RenderComponent = RenderComponent("player.png", MeshType::Cube, true);
}

void Player::SetGrabbedEnemy(Enemy* enemy)
{
    m_GrabbedEnemy = enemy;
    m_State = PlayerState::GRABBED;
    
    // 巨大化アイテムを持っている場合は、掴んだ敵を巨大化させる
    if (m_HasGigantItem && enemy) {
        enemy->SetScale(XMFLOAT3(5.0f, 5.0f, 5.0f));
        // Sizeは通常(1.0f)のままにして、Scaleのみを5.0fにすることで判定サイズが5.0f倍になります
        // (SetSizeまで5.0fにすると 5.0f * 5.0f = 25.0f 倍の超巨大判定になり、即衝突消滅バグが発生します)
        
        // 地面に埋まらないようにY座標を調整
        XMFLOAT3 pos = enemy->GetPosition();
        pos.y = -0.5f + 5.0f * 0.5f; 
        enemy->SetPosition(pos);
    }
}

void Player::Uninit()
{
}

void Player::Update()
{
    XMFLOAT3 oldPos = m_Position; // 更新前の位置を記録

    // HP0 死亡判定
    if (m_HP <= 0) {
        Manager::ChangeScene(Scene::GAMEOVER);
        return;
    }

    // ─── ダッシュ・回避の更新とクールダウン ───
    if (m_DashCooldown > 0) {
        m_DashCooldown--;
    }

    // 残像（ゴースト）の寿命更新
    for (auto it = m_DashGhosts.begin(); it != m_DashGhosts.end(); ) {
        it->Alpha -= 0.08f; // 毎フレーム不透明度を減少
        if (it->Alpha <= 0.0f) {
            it = m_DashGhosts.erase(it);
        } else {
            it++;
        }
    }

    // ダッシュ発動判定 (スタン中でない、かつダッシュ中でない、かつクールダウンでない)
    if (m_DamageTimer <= 0 && m_DashTimer <= 0 && m_DashCooldown <= 0) {
        // Shiftキー(0x10)が押されたか判定
        if (Input::GetKeyTrigger(0x10)) {
            float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;
            XMFLOAT3 moveDir = PlayerController::GetMoveDirection(camYaw);
            
            // 移動キーが押されていればその方向へ、押されていなければ正面方向へダッシュ
            if (MathHelper::LengthSq(moveDir) < 0.001f) {
                moveDir = XMFLOAT3(sinf(m_Rotation.y), 0.0f, cosf(m_Rotation.y));
            }
            
            // 正規化
            float len = sqrtf(moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z);
            if (len > 0.001f) {
                moveDir.x /= len;
                moveDir.y /= len;
                moveDir.z /= len;
            }

            m_DashDirection = moveDir;
            m_DashTimer = 15;        // ダッシュ時間は 15 フレーム
            m_DashCooldown = 45;     // クールダウンは 45 フレーム (約0.75秒)
            m_IsDashing = true;

            // もちもち変形演出：ダッシュ進行方向（XZ面）に長く伸びるように
            m_Scale.y = 0.5f;
            m_Scale.x = 1.8f;
            m_Scale.z = 1.8f;

            m_ScaleVelocityY = -0.1f;
            m_ScaleVelocityX = 0.08f;
            m_ScaleVelocityZ = 0.08f;
            
            // ダッシュ中の無敵時間の設定（ダッシュ15フレーム + マージン5フレーム）
            m_InvincibleTimer = 20; 
        }
    }

    // ダッシュ中の移動処理
    if (m_DashTimer > 0) {
        m_DashTimer--;
        
        // 通常の重力や移動を無視して高速移動
        float dashSpeed = 0.35f; // 通常移動(0.1f)の3.5倍の超高速！
        XMFLOAT3 nextPos = m_Position;
        nextPos.x += m_DashDirection.x * dashSpeed;
        nextPos.z += m_DashDirection.z * dashSpeed;

        // 壁や敵にぶつかる場合は衝突解決
        if (!Collision::CheckAABBCollision(this, nextPos, Manager::GetGameObjectList())) {
            m_Position = nextPos;
        }

        // 残像（ゴーストトレイル）の生成（3フレームおきに生成）
        if (m_DashTimer % 3 == 0) {
            DashGhost ghost;
            ghost.Position = m_Position;
            ghost.Rotation = m_Rotation;
            ghost.Scale = m_Scale;
            ghost.Alpha = 0.8f; // 初期不透明度
            m_DashGhosts.push_back(ghost);
        }

        // ダッシュ終了時の挙動
        if (m_DashTimer == 0) {
            m_IsDashing = false;
            // 終了時のもちもちバウンド
            m_Scale.y = 1.5f;
            m_Scale.x = 0.7f;
            m_Scale.z = 0.7f;
            m_ScaleVelocityY = 0.08f;
            m_ScaleVelocityX = -0.04f;
            m_ScaleVelocityZ = -0.04f;
        }

        // 掴んでいるエネミーの位置同期（ダッシュ中も敵を掴みながら移動できるため）
        if ((m_State == PlayerState::GRABBED || m_State == PlayerState::SPINNING) && m_GrabbedEnemy) {
            XMFLOAT3 grabbedPos = m_Position;
            grabbedPos.y += 0.5f; // プレイヤーの少し上に持ち上げる
            m_GrabbedEnemy->SetPosition(grabbedPos);
        }

        // もちもちの減衰振動（スプリング物理）をここでも回す
        float springK = 0.18f;
        float damping = 0.78f;
        m_ScaleVelocityX += (1.0f - m_Scale.x) * springK;
        m_ScaleVelocityX *= damping;
        m_Scale.x += m_ScaleVelocityX;

        m_ScaleVelocityY += (1.0f - m_Scale.y) * springK;
        m_ScaleVelocityY *= damping;
        m_Scale.y += m_ScaleVelocityY;

        m_ScaleVelocityZ += (1.0f - m_Scale.z) * springK;
        m_ScaleVelocityZ *= damping;
        m_Scale.z += m_ScaleVelocityZ;

        return; // 通常の移動・ジャンプ・入力更新をバイパス
    }

    // ガード状態の更新（しびれスタン中でなく、かつ敵を掴んでいない通常状態のときのみ可能）
    if (m_DamageTimer <= 0 && m_State == PlayerState::IDLE && !m_GrabbedEnemy) {
        // 右クリック(VK_RBUTTON = 0x02) が押されている間ガード
        if (Input::GetKeyPress(0x02)) {
            m_GuardTimer++;
        } else {
            m_GuardTimer = 0;
        }
    } else {
        m_GuardTimer = 0;
    }

    // ロックオンの更新（スローモーション中のみ有効）
    if (Manager::IsSlowMotionActive()) {
        FindLockOnTarget();
    } else {
        m_LockOnTarget = nullptr;
        m_LockOnFrame = 0;
        m_WarpSlashCount = 0; // スローが終了したら回数をリセット
    }

    m_MarkerTimer++;
    float oldRotY = m_Rotation.y;

    // 被弾ダメージタイマーの更新（しびれスタン）
    if (m_DamageTimer > 0) {
        m_DamageTimer--;
        
        // ノックバック物理
        XMFLOAT3 nextPos = m_Position;
        nextPos.x += m_KnockbackVelocity.x;
        nextPos.z += m_KnockbackVelocity.z;
        
        // ノックバックの減衰
        m_KnockbackVelocity.x *= 0.9f;
        m_KnockbackVelocity.z *= 0.9f;
        
        // 壁・敵との衝突判定を行って移動
        if (!Collision::CheckAABBCollision(this, nextPos, Manager::GetGameObjectList())) {
            m_Position = nextPos;
        }
    }

    // 無敵タイマーの更新と点滅処理
    if (m_InvincibleTimer > 0) {
        m_InvincibleTimer--;
        // 8フレーム周期で点滅（4F表示、4F非表示）
        GetRenderComponent().visible = (m_InvincibleTimer % 8 < 4);
    } else {
        GetRenderComponent().visible = true;
    }

    // イナズマエフェクトのタイマー更新
    for (auto it = m_LightningEffects.begin(); it != m_LightningEffects.end(); ) {
        it->Timer--;
        if (it->Timer <= 0) {
            it = m_LightningEffects.erase(it);
        } else {
            it++;
        }
    }

    // しびれスタン中でなければ、キー入力を受け付ける通常ステートの更新を行う
    if (m_DamageTimer <= 0) {
        // スペースキーによるジャンプ入力（二段ジャンプ対応）
        if (Input::GetKeyTrigger(0x20) && m_JumpCount < 2) {
            m_VelocityY = 0.16f; // ジャンプ初速
            m_IsJumping = true;
            m_JumpCount++;

            // 踏み切り時のもちもち演出（縦に縮み、横に広がる）
            m_Scale.y = 0.6f;
            m_Scale.x = 1.3f;
            m_Scale.z = 1.3f;

            // スプリングの初期速度を設定
            m_ScaleVelocityY = 0.1f;
            m_ScaleVelocityX = -0.05f;
            m_ScaleVelocityZ = -0.05f;
        }

        switch (m_State) {
        case PlayerState::IDLE:
            UpdateIdle();
            break;
        case PlayerState::GRABBED:
            UpdateGrabbed();
            break;
        case PlayerState::SPINNING:
            UpdateSpinning();
            break;
        }
    }

    // 角速度（旋回速度）の計算（境界のラッピングを考慮）
    float diff = m_Rotation.y - oldRotY;
    while (diff < -XM_PI) diff += XM_2PI;
    while (diff > XM_PI)  diff -= XM_2PI;
    m_AngularVelocity = diff;

    // ─────────────────────────────────────────────
    // 物理：重力と接地判定の適用
    // ─────────────────────────────────────────────
    if (m_Position.y > -0.5f || m_VelocityY != 0.0f) {
        m_VelocityY -= 0.008f; // 重力加速度
        m_Position.y += m_VelocityY;

        // 空中でのストレッチ演出（Y方向の速度に合わせて縦に伸び、横に潰れる）
        if (m_VelocityY > 0.01f) {
            m_Scale.y += (1.0f + m_VelocityY * 1.5f - m_Scale.y) * 0.2f;
            m_Scale.x += (1.0f - m_VelocityY * 0.75f - m_Scale.x) * 0.2f;
            m_Scale.z += (1.0f - m_VelocityY * 0.75f - m_Scale.z) * 0.2f;
        }

        // 接地判定
        if (m_Position.y <= -0.5f) {
            m_Position.y = -0.5f;
            m_VelocityY = 0.0f;
            m_IsJumping = false;
            m_JumpCount = 0;

            // 着地時のもちもち演出（落下の勢いで縦に強く潰れ、横に広がる）
            m_Scale.y = 0.5f;
            m_Scale.x = 1.3f;
            m_Scale.z = 1.3f;

            // スプリング速度もリセットして初期衝撃を与える
            m_ScaleVelocityY = -0.1f;
            m_ScaleVelocityX = 0.05f;
            m_ScaleVelocityZ = 0.05f;
        }
    }

    // ─────────────────────────────────────────────
    // 移動による伸縮（ぷにぷに）アニメーションの適用
    // ─────────────────────────────────────────────
    // 1フレームの移動量を擬似的な速度（Velocity）として算出
    XMFLOAT3 velocity = XMFLOAT3(m_Position.x - oldPos.x, m_Position.y - oldPos.y, m_Position.z - oldPos.z);
    float speed = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);

    // 目標スケール (1.0f) に向けてスプリング物理を計算して減衰振動（もちもち復元）させる
    float springK = 0.18f; // バネの硬さ
    float damping = 0.78f; // 減衰比

    float forceX = (1.0f - m_Scale.x) * springK;
    m_ScaleVelocityX += forceX;
    m_ScaleVelocityX *= damping;
    m_Scale.x += m_ScaleVelocityX;

    float forceY = (1.0f - m_Scale.y) * springK;
    m_ScaleVelocityY += forceY;
    m_ScaleVelocityY *= damping;
    m_Scale.y += m_ScaleVelocityY;

    float forceZ = (1.0f - m_Scale.z) * springK;
    m_ScaleVelocityZ += forceZ;
    m_ScaleVelocityZ *= damping;
    m_Scale.z += m_ScaleVelocityZ;

    // 移動中のとき、sin波による弾む揺れを加算（1フレーム約16.6ms）
    float dt = 1.0f / 60.0f;
    if (speed > 0.005f) {
        m_MoveAnimation += speed * dt * 30.0f; // スピードに合わせて進行速度を調整
        m_Scale.y += sinf(m_MoveAnimation * 3.0f) * 0.03f;
        m_Scale.x -= sinf(m_MoveAnimation * 3.0f) * 0.015f;
        m_Scale.z -= sinf(m_MoveAnimation * 3.0f) * 0.015f;
    }
}

void Player::UpdateIdle()
{
    // スローモーション中かつロックオン対象が存在し、スラッシュ攻撃回数が3回未満の場合、左クリックで雷電テレポートスラッシュを発動
    if (Manager::IsSlowMotionActive() && m_LockOnTarget && m_WarpSlashCount < 3) {
        if (PlayerController::IsGrabOrThrowAction()) {
            m_WarpSlashCount++; // 攻撃回数をインクリメント
            Enemy* target = m_LockOnTarget;
            XMFLOAT3 targetPos = target->GetPosition();
            XMFLOAT3 startPos = m_Position;
            
            // XZ平面での移動方向ベクトルを計算
            using namespace DirectX;
            XMVECTOR vStart = XMLoadFloat3(&startPos);
            XMVECTOR vTarget = XMLoadFloat3(&targetPos);
            XMVECTOR vDiff = vTarget - vStart;
            vDiff = XMVectorSetY(vDiff, 0.0f); // 高さ方向はカット
            float dist = XMVectorGetX(XMVector3Length(vDiff));
            
            XMVECTOR vDir;
            if (dist > 0.001f) {
                vDir = vDiff / dist;
            } else {
                vDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
            }
            
            // ターゲットのサイズに応じたテレポート距離の計算（めり込み防止）
            float halfWidth = target->GetSize().x * target->GetScale().x * 0.5f;
            float warpDistance = halfWidth + 1.0f; // 表面から 1.0f（プレイヤー半径0.5m + マージン0.5m）離す
            
            // ターゲットの手前へテレポート
            XMVECTOR vWarpPos = vTarget - vDir * warpDistance;
            XMFLOAT3 warpPos;
            XMStoreFloat3(&warpPos, vWarpPos);
            warpPos.y = m_Position.y; // プレイヤーの接地高さを維持
            
            // 座標と回転の更新
            m_Position = warpPos;
            
            // テレポートの瞬間の伸縮演出（一瞬縦に潰れることで接地感を強調）
            m_Scale.y = 0.4f;
            m_Scale.x = 1.8f;
            m_Scale.z = 1.8f;
            
            XMFLOAT3 dir;
            XMStoreFloat3(&dir, vDir);
            m_Rotation.y = atan2f(dir.x, dir.z);
            
            // ビジュアルエフェクト: 雷電軌跡をプレイヤーに登録
            XMFLOAT3 boltStart = startPos;
            XMFLOAT3 boltEnd = targetPos;
            boltStart.y += 0.3f;
            boltEnd.y += 0.3f;
            
            // 3本の強力な重ね稲妻エフェクトで一閃を表現
            AddLightningEffect(boltStart, boltEnd);
            AddLightningEffect(XMFLOAT3(boltStart.x + 0.1f, boltStart.y, boltStart.z), XMFLOAT3(boltEnd.x + 0.1f, boltEnd.y, boltEnd.z));
            AddLightningEffect(XMFLOAT3(boltStart.x - 0.1f, boltStart.y, boltStart.z), XMFLOAT3(boltEnd.x - 0.1f, boltEnd.y, boltEnd.z));
            
            // 足元に青白い衝撃波を発生させ、周囲の敵をなぎ払う
            ShockwaveSystem::AddShockwave(warpPos, 4.0f, 0.0f, 2.5f, 4.0f, 20, 1.5f, 0);
            
            // ターゲット敵がボスの場合はボスにダメージ、それ以外は吹き飛ばし
            if (target->GetObjectType() == ObjectType::Boss) {
                BossEnemy* boss = static_cast<BossEnemy*>(target);
                boss->ApplyBossDamage(4, m_Position); // テレポートスラッシュはボスに4ダメージ！
            } else {
                // ターゲット敵を強烈に吹き飛ばし(BLOWN_AWAY)電撃を纏わせる
                XMFLOAT3 pushVel = XMFLOAT3(dir.x * 1.6f, 0.6f, dir.z * 1.6f);
                target->SetVelocity(pushVel);
                target->SetEnemyState(EnemyState::BLOWN_AWAY);
                target->SetLightning(true);
                
                // 撃破処理（テレポートスラッシュ・シアン色ポップアップ）
                target->Defeat(0.0f, 2.0f, 3.0f);
            }
            
            // ヒットインパクト演出 (強いヒットストップとカメラシェイク)
            Manager::AddHitStop(15);
            if (g_Camera) {
                g_Camera->Shake(0.45f, 15);
            }
            
            // ウィッチタイムを90F（1.5秒）に延長し、連続テレポートチェインを支援
            Manager::StartSlowMotion(90);
            
            // テレポート攻撃成立後は、このフレームの通常の更新処理をバイパスする
            return;
        }
    }

    float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;
    float mouseMoveX = (float)Input::GetMouseMoveX();

    // プレイヤーコントローラーからカメラ角度を考慮した移動ベクトルを取得
    XMFLOAT3 moveDir = PlayerController::GetMoveDirection(camYaw);

    XMFLOAT3 nextPos = m_Position;
    if (MathHelper::LengthSq(moveDir) > 0.001f) {
        // ガード中は移動速度を通常の25%に制限する（通常: 0.1f -> ガード中: 0.025f）
        float moveSpeed = IsGuardActive() ? 0.025f : 0.1f;
        nextPos += moveDir * moveSpeed;

        if (abs(mouseMoveX) <= 0.1f) {
            m_Rotation.y = atan2f(moveDir.x, moveDir.z);
        }
    }

    if (abs(mouseMoveX) > 0.1f) {
        m_Rotation.y = camYaw;
    }

    // 壁・敵への衝突で移動ブロック（掴みは行わない）
    if (!Collision::CheckAABBCollision(this, nextPos, Manager::GetGameObjectList())) {
        m_Position = nextPos;
    }

    // 左クリック：正面付近にいる NORMAL 状態の敵を掴む
    if (PlayerController::IsGrabOrThrowAction()) {
        XMFLOAT3 fwd = XMFLOAT3(sinf(m_Rotation.y), 0.0f, cosf(m_Rotation.y));
        float grabRange = 4.0f;  // 掴み有効距離
        Enemy* nearest  = nullptr;
        float  nearestDist = grabRange;

        for (GameObject* obj : Manager::GetGameObjectList()) {
            if (!obj || obj == this || obj->IsDestroy()) continue;
            if (obj->GetObjectType() != ObjectType::Enemy) continue;
            Enemy* e = static_cast<Enemy*>(obj);
            if (e->GetEnemyState() != EnemyState::NORMAL) continue;

            XMFLOAT3 toEnemy = e->GetPosition() - m_Position;
            float dist = MathHelper::Length(toEnemy);
            if (dist < nearestDist) {
                nearestDist = dist;
                nearest = e;
            }
        }

        if (nearest) {
            SetGrabbedEnemy(nearest);
        }
    }
}

void Player::UpdateGrabbed()
{
    float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;

    // プレイヤーコントローラーからカメラ角度を考慮した移動ベクトルを取得
    XMFLOAT3 moveDir = PlayerController::GetMoveDirection(camYaw);

    XMFLOAT3 nextPos = m_Position;
    if (MathHelper::LengthSq(moveDir) > 0.001f) {
        nextPos += moveDir * 0.1f;
    }

    m_Rotation.y = camYaw;

    // 壁への衝突で移動ブロック（掴んでいる敵は通り抜け許可、新たな掴みは行わない）
    GameObject* ignoreGrabbed = m_GrabbedEnemy;
    if (!Collision::CheckAABBCollision(this, nextPos, Manager::GetGameObjectList(), &ignoreGrabbed)) {
        m_Position = nextPos;
    }

    // 右クリック：スピン（回転投げ）開始
    if (m_GrabbedEnemy && PlayerController::IsSpinToggleAction()) {
        m_State = PlayerState::SPINNING;
        m_CurrentSpinSpeed = 0.0f;
    }

    // 左クリック：掴んだまま（スピンなし）で直接発射
    if (m_GrabbedEnemy && PlayerController::IsGrabOrThrowAction()) {
        Throw();
        return; // Throw()内でIDLEに遷移するためここで終了
    }

    // 掴んでいる敵の位置をプレイヤーの正面に固定追従させる
    if (m_GrabbedEnemy) {
        Collision::ResolveGrabPhysics(this, m_GrabbedEnemy, 0.0f);
    }

    if (!m_GrabbedEnemy) {
        m_State = PlayerState::IDLE;
    }
}

void Player::UpdateSpinning()
{
    float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;

    // プレイヤーコントローラーからカメラ角度を考慮した移動ベクトルを取得
    XMFLOAT3 moveDir = PlayerController::GetMoveDirection(camYaw);

    XMFLOAT3 nextPos = m_Position;
    if (MathHelper::LengthSq(moveDir) > 0.001f) {
        nextPos += moveDir * 0.1f;
    }

    // 徐々にスピン加速
    float targetSpinSpeed = 0.80f;
    m_CurrentSpinSpeed += (targetSpinSpeed - m_CurrentSpinSpeed) * 0.001f;

    m_Rotation.y += m_CurrentSpinSpeed;
    if (m_Rotation.y > XM_2PI) m_Rotation.y -= XM_2PI;

    if (g_Camera) {
        float dynamicShake = 0.01f + m_CurrentSpinSpeed * 0.15f;
        g_Camera->Shake(dynamicShake, 2);
    }

    // 壁への衝突で移動ブロック（掴んでいる敵は通り抜け許可）
    GameObject* ignoreGrabbed = m_GrabbedEnemy;
    if (!Collision::CheckAABBCollision(this, nextPos, Manager::GetGameObjectList(), &ignoreGrabbed)) {
        m_Position = nextPos;
    }

    // 吸引アイテムを持っている場合、周囲の敵を吸い寄せる
    if (m_HasVacuumItem) {
        float vacuumRadius = 8.0f; // 吸引判定半径
        float vacuumForce = 0.03f; // 引力スピード

        for (GameObject* obj : Manager::GetGameObjectList()) {
            if (!obj || obj == this || obj == m_GrabbedEnemy) continue;

            if (obj->GetObjectType() == ObjectType::Enemy) {
                Enemy* enemy = static_cast<Enemy*>(obj);
                if (enemy->GetEnemyState() == EnemyState::NORMAL || enemy->GetEnemyState() == EnemyState::CHASING) {
                    XMFLOAT3 enemyPos = enemy->GetPosition();
                    XMFLOAT3 diff = m_Position - enemyPos;
                    float distSq = MathHelper::LengthSq(diff);

                    if (distSq < vacuumRadius * vacuumRadius && distSq > 0.01f) {
                        float dist = sqrtf(distSq);
                        XMFLOAT3 dir = diff / dist;

                        float spinDirSign = (m_AngularVelocity >= 0.0f) ? 1.0f : -1.0f;
                        XMFLOAT3 tangent = XMFLOAT3(-dir.z, 0.0f, dir.x) * spinDirSign * 0.3f;
                        XMFLOAT3 force = (dir + tangent) * vacuumForce;

                        XMFLOAT3 vel = enemy->GetVelocity();
                        vel.x += force.x;
                        vel.z += force.z;
                        enemy->SetVelocity(vel);

                        enemy->SetEnemyState(EnemyState::VACUUMED);
                    }
                }
            }
        }
    }

    // 左クリック：スピンしながら投げる（メイン発射操作）
    if (PlayerController::IsGrabOrThrowAction()) {
        Throw();
        return; // Throw()内でIDLEに遷移するためここで終了
    }

    // 右クリック：スピンをキャンセルして掴み状態に戻る
    if (PlayerController::IsSpinToggleAction()) {
        m_State = PlayerState::GRABBED;
        m_CurrentSpinSpeed = 0.0f;
    }

    // 掴んでいる敵の位置をプレイヤーの正面に固定追従させる
    if (m_GrabbedEnemy) {
        Collision::ResolveGrabPhysics(this, m_GrabbedEnemy, 0.0f);
    }

    if (!m_GrabbedEnemy) {
        m_State = PlayerState::IDLE;
        m_CurrentSpinSpeed = 0.0f;
    }
}

void Player::Throw()
{
    if (m_GrabbedEnemy) {
        float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;

        // 投擲の瞬間、プレイヤーの向きをカメラの正面（エイム方向）に強制同期させる
        m_Rotation.y = camYaw;

        // sinf/cosf で前方ベクトルを直接計算（XMVECTOR 変換不要）
        XMFLOAT3 fwdF = XMFLOAT3(sinf(m_Rotation.y), 0.0f, cosf(m_Rotation.y));

        // 基本の投擲速度（前方）
        float baseThrowSpeed = 0.8f;
        
        // スピン（遠心力）による速度ボーナス
        // 回転が速いほど、カメラ正面にすっ飛んでいくスピードが加速する！
        float speedBoost = abs(m_AngularVelocity) * 6.0f; 
        float totalSpeed = baseThrowSpeed + speedBoost;

        // カメラ正面へのベクトルを算出（MathHelper の演算子オーバーロードで記述）
        XMFLOAT3 throwVelocity = fwdF * totalSpeed;

        // 巨大化アイテムを持っている場合は、正面の地面に向けて叩きつけるように下方向への初期速度を与える
        if (m_HasGigantItem) {
            throwVelocity.y = -0.3f - speedBoost * 0.6f;
        }

        m_GrabbedEnemy->SetVelocity(throwVelocity);
        m_GrabbedEnemy->SetEnemyState(EnemyState::FLYING);

        // もし吸い込みアイテムを持っていたら、投げた敵を爆発属性にする
        if (m_HasVacuumItem) {
            m_GrabbedEnemy->SetExplosive(true);
        }

        // もし雷電アイテムを持っていたら、投げた敵を電撃属性にする
        if (m_HasLightningItem) {
            m_GrabbedEnemy->SetLightning(true);
        }

        // 吸い込まれて公転していた敵も一斉射出する
        // ─── 挙動：プレイヤー正面方向へ全員まとめて直線発射し、
        //          爆発時に TriggerExplosion で四方八方へ吹き飛ばす ───
        for (GameObject* obj : Manager::GetGameObjectList()) {
            if (!obj) continue;
            if (obj->GetObjectType() != ObjectType::Enemy) continue;
            Enemy* enemy = static_cast<Enemy*>(obj);
            if (enemy->GetEnemyState() == EnemyState::VACUUMED) {
                // プレイヤーの向いている正面方向（fwdF）へ直線発射
                // 掴んでいた敵と同じ方向に全員まとめてぶっ放す！
                float finalSpeed = totalSpeed * 0.9f;
                XMFLOAT3 vacuumThrowVel = fwdF * finalSpeed;

                enemy->SetVelocity(vacuumThrowVel);
                enemy->SetEnemyState(EnemyState::FLYING);
                // 爆発属性を付与 → 着地・壁衝突時に TriggerExplosion が呼ばれ四方八方に爆散
                enemy->SetExplosive(true);
            }
        }
        // アイテム効果を消費
        m_HasVacuumItem = false;
        m_HasGigantItem = false;
        m_HasLightningItem = false;

        m_GrabbedEnemy = nullptr;
        m_State = PlayerState::IDLE;
        m_IsAutoSpinning = false; // 投げたら自動回転をストップ

        // 投擲の瞬間の伸縮演出（横にグッと潰れてからバウンド）
        m_Scale.y = 0.5f;
        m_Scale.x = 1.6f;
        m_Scale.z = 1.6f;

        // 投擲時の爽快なカメラシェイク（遠心力に応じてインパクトを極大化）
        float throwShake = 0.08f + abs(m_AngularVelocity) * 1.8f;
        if (throwShake > 0.40f) throwShake = 0.40f;
        g_Camera->Shake(throwShake, 12);
    }
}

void Player::Draw()
{
    // 通常・シャドウ・アウトラインの本体描画は RenderSystem 側で一括描画するため、
    // ここでの個別描画（36頂点）は行いません。

    // ─── 通常パス時のみ UI や ガイドラインを描画 ───
    if (!Renderer::IsShadowMode() && !Renderer::IsOutlineMode()) {
        // -2. ダッシュ残像（ゴーストトレイル）の描画
        if (!m_DashGhosts.empty()) {
            Renderer::SetupCubeDraw(); // キューブ共通アセットのバインド

            for (const auto& ghost : m_DashGhosts) {
                XMMATRIX world = XMMatrixScaling(ghost.Scale.x, ghost.Scale.y, ghost.Scale.z) * 
                                 XMMatrixRotationRollPitchYaw(ghost.Rotation.x, ghost.Rotation.y, ghost.Rotation.z) * 
                                 XMMatrixTranslation(ghost.Position.x, ghost.Position.y, ghost.Position.z);
                Renderer::SetWorldMatrix(world);

                // 青白く発光する半透明マテリアル
                MATERIAL ghostMat;
                ZeroMemory(&ghostMat, sizeof(ghostMat));
                ghostMat.Diffuse        = XMFLOAT4(0.0f, 0.5f, 1.0f, ghost.Alpha);
                ghostMat.Ambient        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
                ghostMat.Specular       = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
                ghostMat.Emission       = XMFLOAT4(0.0f, 0.6f * ghost.Alpha, 1.5f * ghost.Alpha, 1.0f);
                ghostMat.Shininess      = 0.0f;
                ghostMat.TextureEnable  = FALSE; // 単色発光
                ghostMat.RimPower       = 0.0f;
                Renderer::SetMaterial(ghostMat);

                Renderer::GetDeviceContext()->Draw(36, 0);
            }
        }

        // -1. ガード/パリィ中のプラズマシールドエフェクト描画
        if (IsGuardActive()) {
            XMFLOAT3 center = m_Position;
            center.y += 0.3f; // 中心付近

            // パリィ中はシアン、通常ガードは青
            XMFLOAT4 sparkColor = IsParryActive() 
                ? XMFLOAT4(0.0f, 2.5f, 4.0f, 1.0f) 
                : XMFLOAT4(0.0f, 1.0f, 2.5f, 1.0f);

            int sparks = IsParryActive() ? 4 : 2;
            for (int i = 0; i < sparks; i++) {
                float angle = ((float)rand() / RAND_MAX) * XM_2PI;
                float pitch = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * XM_PIDIV4;
                float radius = 1.0f; // プレイヤーより少し広い球状

                XMFLOAT3 start = XMFLOAT3(
                    center.x + sinf(angle) * cosf(pitch) * radius,
                    center.y + sinf(pitch) * radius,
                    center.z + cosf(angle) * cosf(pitch) * radius
                );

                float angle2 = angle + XM_PIDIV4 + (((float)rand() / RAND_MAX) * 0.1f);
                XMFLOAT3 end = XMFLOAT3(
                    center.x + sinf(angle2) * cosf(pitch) * radius,
                    center.y + sinf(pitch) * radius,
                    center.z + cosf(angle2) * cosf(pitch) * radius
                );

                DrawLightningBolt(start, end, 0.02f, sparkColor);
            }
        }

        // 0. しびれスタン中の放電演出
        if (m_DamageTimer > 0) {
            XMFLOAT3 start = m_Position;
            start.y += 0.3f; // 中心付近

            int sparks = 2 + (rand() % 2); 
            for (int i = 0; i < sparks; i++) {
                float angle = ((float)rand() / RAND_MAX) * XM_2PI;
                float pitch = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * XM_PIDIV4;
                float dist = 0.8f + ((float)rand() / RAND_MAX) * 1.2f;

                XMFLOAT3 end = XMFLOAT3(
                    start.x + sinf(angle) * cosf(pitch) * dist,
                    start.y + sinf(pitch) * dist,
                    start.z + cosf(angle) * cosf(pitch) * dist
                );

                // パチパチ明滅するマゼンタ（ピンク）のイナズマを描画
                DrawLightningBolt(start, end, 0.02f, XMFLOAT4(2.8f, 0.0f, 2.0f, 1.0f));
            }
        }

        // 1. 敵を掴んでいる時にエイムガイド（3Dレーザーライン）を描画
        if (m_GrabbedEnemy) {
            float camYaw = 0.0f;
            if (g_Camera) {
                camYaw = g_Camera->GetAngleY();
            }

            // カメラ正面を指す極細のレーザービームのワールド行列を作成
            XMMATRIX guideWorld = XMMatrixScaling(0.04f, 0.04f, 8.0f) * 
                                  XMMatrixTranslation(0.0f, 0.0f, 4.0f) * // プレイヤーの目の前から前方8ユニット分に伸ばす
                                  XMMatrixRotationRollPitchYaw(0.0f, camYaw, 0.0f) *
                                  XMMatrixTranslation(m_Position.x, m_Position.y + 0.3f, m_Position.z); // ウエストの高さから射出

            Renderer::SetWorldMatrix(guideWorld);

            // 鮮やかなネオングリーンの自発光（エミッシブ）マテリアルを設定
            MATERIAL guideMaterial;
            ZeroMemory(&guideMaterial, sizeof(guideMaterial));
            guideMaterial.Diffuse        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            guideMaterial.Ambient        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            guideMaterial.Specular       = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
            guideMaterial.Emission       = XMFLOAT4(0.0f, 1.8f, 0.5f, 1.0f); // 輝度を1.0以上に設定してBloomを強く発光させる！
            guideMaterial.Shininess      = 0.0f;
            guideMaterial.TextureEnable  = FALSE; // 単色ネオン光
            guideMaterial.RimPower       = 0.0f;
            Renderer::SetMaterial(guideMaterial);

            Renderer::SetupCubeDraw();
            Renderer::GetDeviceContext()->Draw(36, 0);
        }

        // 2. 掴んでいない時（IDLE）に、掴み有効範囲内にいる最も近い敵の頭上にマーカー（インジケータ）を描画
        if (m_State == PlayerState::IDLE) {
            float grabRange = 4.0f;  // 掴み有効距離
            Enemy* nearest  = nullptr;
            float  nearestDist = grabRange;

            for (GameObject* obj : Manager::GetGameObjectList()) {
                if (!obj || obj == this || obj->IsDestroy()) continue;
                if (obj->GetObjectType() != ObjectType::Enemy) continue;
                Enemy* e = static_cast<Enemy*>(obj);
                if (e->GetEnemyState() != EnemyState::NORMAL) continue;

                XMFLOAT3 toEnemy = e->GetPosition() - m_Position;
                float dist = MathHelper::Length(toEnemy);
                if (dist < nearestDist) {
                    nearestDist = dist;
                    nearest = e;
                }
            }

            if (nearest) {
                XMFLOAT3 enemyPos = nearest->GetPosition();
                
                // サイン波によるふわふわ上下浮遊アニメーション
                float hover = sinf((float)m_MarkerTimer * 0.08f) * 0.12f;
                // くるくる自転させる角度
                float rotY = (float)m_MarkerTimer * 0.05f;

                // 45度傾けたクリスタル状の小さなキューブを敵の頭上に描画
                XMMATRIX markerWorld = XMMatrixScaling(0.22f, 0.22f, 0.22f) * 
                                       XMMatrixRotationRollPitchYaw(0.5f, rotY, 0.5f) * 
                                       XMMatrixTranslation(enemyPos.x, enemyPos.y + 1.5f + hover, enemyPos.z);

                Renderer::SetWorldMatrix(markerWorld);

                // 鮮やかなネオンイエローの自発光（エミッシブ）マテリアルを設定
                MATERIAL markerMaterial;
                ZeroMemory(&markerMaterial, sizeof(markerMaterial));
                markerMaterial.Diffuse        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
                markerMaterial.Ambient        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
                markerMaterial.Specular       = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
                markerMaterial.Emission       = XMFLOAT4(1.8f, 1.8f, 0.0f, 1.0f); // 黄色発光
                markerMaterial.Shininess      = 0.0f;
                markerMaterial.TextureEnable  = FALSE;
                markerMaterial.RimPower       = 0.0f;
                Renderer::SetMaterial(markerMaterial);

                Renderer::SetupCubeDraw();
                Renderer::GetDeviceContext()->Draw(36, 0);
            }
        }

        // 3. 雷電アイテムを取得しており、かつ敵を掴んでいる場合、プレイヤーと敵の間にイナズマを描画
        if (m_HasLightningItem && m_GrabbedEnemy) {
            XMFLOAT3 pPos = m_Position;
            pPos.y += 0.3f; // ウエストの高さ
            XMFLOAT3 ePos = m_GrabbedEnemy->GetPosition();
            ePos.y += 0.3f;
            DrawLightningBolt(pPos, ePos, 0.05f, XMFLOAT4(0.0f, 1.8f, 2.5f, 1.0f));
        }

        // 3.5. 雷電アイテム所持中の追加放電エフェクト（プレイヤー周囲＆スピン中の敵からの放電）
        if (m_HasLightningItem) {
            // (A) プレイヤー自身の周囲への常時放電（40%の確率で地面へ落雷）
            if (((float)rand() / RAND_MAX) < 0.4f) {
                float angle = ((float)rand() / RAND_MAX) * XM_2PI;
                float dist = 0.5f + ((float)rand() / RAND_MAX) * 1.5f; // 半径0.5〜2.0m
                XMFLOAT3 start = m_Position;
                start.y += 0.5f; // 腰から少し上の高さ
                XMFLOAT3 end = XMFLOAT3(
                    m_Position.x + sinf(angle) * dist,
                    -0.5f, // 地面
                    m_Position.z + cosf(angle) * dist
                );
                DrawLightningBolt(start, end, 0.02f, XMFLOAT4(0.0f, 1.8f, 2.5f, 1.0f));
            }

            // (B) スピン中の掴んでいる敵からの激しい放電（毎フレーム2本の空中放電）
            if (m_State == PlayerState::SPINNING && m_GrabbedEnemy) {
                XMFLOAT3 enemyPos = m_GrabbedEnemy->GetPosition();
                enemyPos.y += 0.3f;
                for (int i = 0; i < 2; i++) {
                    float angle = ((float)rand() / RAND_MAX) * XM_2PI;
                    float pitch = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * XM_PIDIV4; // 上下45度
                    float dist = 1.5f + ((float)rand() / RAND_MAX) * 2.0f; // 長さ1.5〜3.5m
                    XMFLOAT3 end = XMFLOAT3(
                        enemyPos.x + sinf(angle) * cosf(pitch) * dist,
                        enemyPos.y + sinf(pitch) * dist,
                        enemyPos.z + cosf(angle) * cosf(pitch) * dist
                    );
                    DrawLightningBolt(enemyPos, end, 0.025f, XMFLOAT4(0.0f, 2.0f, 2.8f, 1.0f));
                }
            }
        }

        // 4. 連鎖イナズマエフェクトの描画
        for (const auto& effect : m_LightningEffects) {
            for (const auto& segment : effect.Segments) {
                // パチパチ明滅するシアン/水色のイナズマ
                DrawLightningBolt(segment.Start, segment.End, 0.04f, XMFLOAT4(0.0f, 2.0f, 2.8f, 1.0f));
            }
        }

        // 5. ロックオンマーカーの描画（スローモーション中のターゲットロックオン演出）
        if (m_LockOnTarget && Manager::IsSlowMotionActive()) {
            using namespace DirectX;
            
            XMFLOAT3 enemyPos = m_LockOnTarget->GetPosition();
            // 敵の中心位置を基準にする
            XMFLOAT3 center = enemyPos;
            
            // カメラの正面ベクトルと直交ベクトルを取得
            XMFLOAT3 camFwd = g_Camera->GetForward();
            XMVECTOR vCamFwd = XMVector3Normalize(XMLoadFloat3(&camFwd));
            
            XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            XMVECTOR tangentX = XMVector3Cross(vCamFwd, up);
            if (XMVectorGetX(XMVector3Length(tangentX)) < 0.01f) {
                tangentX = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
            }
            tangentX = XMVector3Normalize(tangentX);
            XMVECTOR tangentY = XMVector3Normalize(XMVector3Cross(vCamFwd, tangentX));
            
            // フォーカスイン縮小アニメーションのスケール計算 (0〜12Fで1.8倍から1.0倍に収縮)
            float t = (float)m_LockOnFrame / 12.0f;
            if (t > 1.0f) t = 1.0f;
            float scale = 1.8f - 0.8f * t;
            
            // 敵のスケールも加味する（巨大化エネミーにもフィットするように）
            float enemyScale = m_LockOnTarget->GetScale().x;
            scale *= enemyScale;
            
            XMVECTOR vCenter = XMLoadFloat3(&center);
            
            // 電撃の色 (シアン)
            XMFLOAT4 lockOnColor = XMFLOAT4(0.0f, 2.0f, 3.0f, 1.0f);
            XMFLOAT4 innerColor = XMFLOAT4(0.5f, 2.2f, 3.0f, 1.0f);
            
            // (A) 外円の描画 (半径1.0f * scale、時計回り回転)
            float outerRadius = 1.0f * scale;
            float outerRot = (float)m_MarkerTimer * 0.02f;
            const int circleSegments = 16;
            XMFLOAT3 prevPoint;
            
            for (int i = 0; i <= circleSegments; i++) {
                float angle = outerRot + ((float)i / circleSegments) * XM_2PI;
                XMVECTOR vPt = vCenter + tangentX * cosf(angle) * outerRadius + tangentY * sinf(angle) * outerRadius;
                XMFLOAT3 currPoint;
                XMStoreFloat3(&currPoint, vPt);
                
                if (i > 0) {
                    DrawLightningBolt(prevPoint, currPoint, 0.015f, lockOnColor);
                }
                prevPoint = currPoint;
            }
            
            // (B) 内円の描画 (半径0.6f * scale、反時計回り回転)
            float innerRadius = 0.6f * scale;
            float innerRot = -(float)m_MarkerTimer * 0.03f;
            for (int i = 0; i <= circleSegments; i++) {
                float angle = innerRot + ((float)i / circleSegments) * XM_2PI;
                XMVECTOR vPt = vCenter + tangentX * cosf(angle) * innerRadius + tangentY * sinf(angle) * innerRadius;
                XMFLOAT3 currPoint;
                XMStoreFloat3(&currPoint, vPt);
                
                if (i > 0) {
                    DrawLightningBolt(prevPoint, currPoint, 0.012f, innerColor);
                }
                prevPoint = currPoint;
            }
            
            // (C) 4隅のL字型コーナーブラケットの描画
            float boxSize = 1.2f * scale;
            float edgeLen = 0.3f * scale;
            
            // 各コーナーの位置ベクトル
            XMVECTOR corners[4] = {
                vCenter + (tangentX + tangentY) * boxSize,  // 右上
                vCenter + (tangentX - tangentY) * boxSize,  // 右下
                vCenter - (tangentX + tangentY) * boxSize,  // 左下
                vCenter - (tangentX - tangentY) * boxSize   // 左上
            };
            
            // 各コーナーからのL字の向き（tangentX, tangentY の正負）
            XMVECTOR dirX[4] = { -tangentX, -tangentX, tangentX, tangentX };
            XMVECTOR dirY[4] = { -tangentY, tangentY, tangentY, -tangentY };
            
            for (int c = 0; c < 4; c++) {
                XMFLOAT3 ptCorner, ptX, ptY;
                XMStoreFloat3(&ptCorner, corners[c]);
                XMStoreFloat3(&ptX, corners[c] + dirX[c] * edgeLen);
                XMStoreFloat3(&ptY, corners[c] + dirY[c] * edgeLen);
                
                DrawLightningBolt(ptCorner, ptX, 0.015f, lockOnColor);
                DrawLightningBolt(ptCorner, ptY, 0.015f, lockOnColor);
            }
        }
    }
}

// ─────────────────────────────────────────────
// 2点間にジグザグのイナズマを描画する内部ヘルパー
// ─────────────────────────────────────────────
void Player::DrawLightningBolt(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color)
{
    // 通常描画時以外は無視（シャドウやアウトラインパスでは描画しない）
    if (Renderer::IsShadowMode() || Renderer::IsOutlineMode()) return;

    using namespace DirectX;
    
    // 3本の極細プラズマの束として描画する
    // それぞれ微妙に異なるノイズで高速パチパチ明滅させることで、本物のプラズマ放電に見せる
    // 1本目：青（太さ極細）
    DrawLightningBoltInternal(start, end, 0.02f, XMFLOAT4(0.0f, 1.0f, 2.5f, 1.0f), true, 0);
    // 2本目：シアン（太さ極細、わずかに座標をずらす）
    DrawLightningBoltInternal(start, end, 0.022f, XMFLOAT4(0.5f, 1.8f, 2.5f, 1.0f), true, 1);
    // 3本目：白コア（太さ超極細、わずかに座標をずらす）
    DrawLightningBoltInternal(start, end, 0.015f, XMFLOAT4(1.8f, 2.2f, 2.5f, 1.0f), false, 2);
}

void Player::DrawLightningBoltInternal(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness, const DirectX::XMFLOAT4& color, bool drawBranches, int seedOffset)
{
    using namespace DirectX;
    
    XMVECTOR pA = XMLoadFloat3(&start);
    XMVECTOR pB = XMLoadFloat3(&end);
    XMVECTOR dir = pB - pA;
    float len = XMVectorGetX(XMVector3Length(dir));
    if (len < 0.01f) return;

    // 分割数
    const int segmentsCount = 5;
    std::vector<XMVECTOR> points;
    points.push_back(pA);

    // 直交ベクトルを求める
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR tangentX = XMVector3Cross(XMVector3Normalize(dir), up);
    if (XMVectorGetX(XMVector3Length(tangentX)) < 0.01f) {
        tangentX = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }
    tangentX = XMVector3Normalize(tangentX);
    XMVECTOR tangentY = XMVector3Normalize(XMVector3Cross(XMVector3Normalize(dir), tangentX));

    // 中継点の生成
    for (int i = 1; i < segmentsCount; i++) {
        float ratio = (float)i / segmentsCount;
        XMVECTOR pt = pA + dir * ratio;

        // ジグザグノイズ（明滅感を出すため、毎フレーム変わる）
        float noiseScale = len * 0.07f; // 長さに応じたノイズ幅
        if (noiseScale > 0.35f) noiseScale = 0.35f;

        // seedOffsetによって、乱数生成器から取得する値を変え、ライン形状を分離する
        for (int k = 0; k < seedOffset; k++) {
            rand();
        }

        float offsetX = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * noiseScale;
        float offsetY = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * noiseScale;

        pt += tangentX * offsetX + tangentY * offsetY;
        points.push_back(pt);
    }
    points.push_back(pB);

    // セグメントの描画
    for (size_t i = 0; i < points.size() - 1; i++) {
        XMVECTOR segmentDir = points[i+1] - points[i];
        float segmentLen = XMVectorGetX(XMVector3Length(segmentDir));
        if (segmentLen > 0.001f) {
            XMVECTOR midPoint = (points[i] + points[i+1]) * 0.5f;

            XMVECTOR zAxis = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
            XMVECTOR targetDir = XMVector3Normalize(segmentDir);
            XMVECTOR rotAxis = XMVector3Cross(zAxis, targetDir);
            float rotAxisLen = XMVectorGetX(XMVector3Length(rotAxis));

            XMMATRIX rotation;
            if (rotAxisLen < 0.001f) {
                float dot = XMVectorGetX(XMVector3Dot(zAxis, targetDir));
                if (dot < 0.0f) {
                    rotation = XMMatrixRotationY(XM_PI);
                } else {
                    rotation = XMMatrixIdentity();
                }
            } else {
                float dot = XMVectorGetX(XMVector3Dot(zAxis, targetDir));
                float angle = acosf(dot);
                rotation = XMMatrixRotationAxis(rotAxis, angle);
            }

            // 毎フレーム少し太さにゆらぎ（明滅ノイズ）を与える
            float flicker = 0.8f + ((float)rand() / RAND_MAX) * 0.4f; 
            XMMATRIX scale = XMMatrixScaling(thickness * flicker, thickness * flicker, segmentLen);
            XMMATRIX translation = XMMatrixTranslationFromVector(midPoint);
            XMMATRIX world = scale * rotation * translation;

            Renderer::SetWorldMatrix(world);

            MATERIAL segmentMaterial;
            ZeroMemory(&segmentMaterial, sizeof(segmentMaterial));
            segmentMaterial.Diffuse        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            segmentMaterial.Ambient        = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            segmentMaterial.Specular       = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
            segmentMaterial.Emission       = color;
            segmentMaterial.Shininess      = 0.0f;
            segmentMaterial.TextureEnable  = FALSE;
            segmentMaterial.RimPower       = 0.0f;
            Renderer::SetMaterial(segmentMaterial);

            Renderer::SetupCubeDraw();
            Renderer::GetDeviceContext()->Draw(36, 0);

            // 枝分かれの描画（アウターの描画時のみ、一定確率で分岐させる）
            if (drawBranches && i > 0 && i < points.size() - 2) {
                if (((float)rand() / RAND_MAX) < 0.25f) { // 25%の確率で枝分かれ
                    // 枝の長さをランダム決定
                    float branchLen = len * (0.15f + ((float)rand() / RAND_MAX) * 0.15f);
                    
                    // 枝の方向（外側へのランダムな方向）
                    float angle = ((float)rand() / RAND_MAX) * XM_2PI;
                    XMVECTOR branchDir = XMVector3Normalize(tangentX * cosf(angle) + tangentY * sinf(angle) + XMVector3Normalize(dir) * 0.3f);
                    
                    XMVECTOR branchEnd = points[i+1] + branchDir * branchLen;
                    XMFLOAT3 bStart, bEnd;
                    XMStoreFloat3(&bStart, points[i+1]);
                    XMStoreFloat3(&bEnd, branchEnd);

                    // 枝分かれは再帰的に描画するが、枝の枝は作らない（drawBranches=false）
                    DrawLightningBoltInternal(bStart, bEnd, thickness * 0.4f, color, false, seedOffset + 10);
                }
            }
        }
    }
}

// ─────────────────────────────────────────────
// イナズマエフェクトの追加
// ─────────────────────────────────────────────
void Player::AddLightningEffect(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end)
{
    LightningEffect effect;
    effect.Timer = 24; // 24フレーム生存（びりびり時間を延長）
    
    // 稲妻の経路を等分割して登録（パチパチ震えるのは描画時に行うが、始点と終点は固定）
    LightningSegment seg;
    seg.Start = start;
    seg.End = end;
    effect.Segments.push_back(seg);

    m_LightningEffects.push_back(effect);
}

// ─────────────────────────────────────────────
// ダメージ被弾処理
// ─────────────────────────────────────────────
void Player::ApplyDamage(int damage, const DirectX::XMFLOAT3& enemyPos)
{
    // 無敵状態、または既にHP0の場合はダメージを無視
    if (m_InvincibleTimer > 0 || m_HP <= 0) return;

    m_HP -= damage;
    if (m_HP < 0) m_HP = 0;

    // 被弾の衝撃による伸縮演出（ペチャンコになってから元に戻る）
    m_Scale.y = 0.5f;
    m_Scale.x = 1.7f;
    m_Scale.z = 1.7f;

    // 被弾しびれスタンタイマーと無敵タイマーを設定
    m_DamageTimer = 60;        // 1秒間スタン（しびれ操作不能）
    m_InvincibleTimer = 180;   // 3秒間無敵

    // 掴んでいるエネミーの強制解放
    if (m_GrabbedEnemy)
    {
        m_GrabbedEnemy->SetEnemyState(EnemyState::NORMAL);
        m_GrabbedEnemy->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
        m_GrabbedEnemy = nullptr;
    }
    m_State = PlayerState::IDLE;

    // ノックバック物理の計算
    DirectX::XMFLOAT3 diff = m_Position - enemyPos;
    diff.y = 0.0f; // Y軸は無視
    float dist = MathHelper::Length(diff);
    if (dist > 0.001f)
    {
        diff.x /= dist;
        diff.z /= dist;
    }
    else
    {
        // 重なっている場合は真後ろへ吹き飛ばす
        diff = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);
    }

    // ノックバックの初速を設定
    m_KnockbackVelocity = DirectX::XMFLOAT3(diff.x * 0.35f, 0.0f, diff.z * 0.35f);

    // 被弾時のカメラシェイクとヒットストップ
    if (g_Camera)
    {
        g_Camera->Shake(0.35f, 12);
    }
    Manager::AddHitStop(5); // 5フレームのヒットストップで衝撃を表現
}

// ─────────────────────────────────────────────
// リスタート（復活）処理
// ─────────────────────────────────────────────
void Player::Restart()
{
    // ステータスのリセット
    m_HP = m_MaxHP;
    m_DamageTimer = 30;       // リスタート時の硬直は少し短く（0.5秒）
    m_InvincibleTimer = 180;  // 3秒間無敵
    m_KnockbackVelocity = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

    // プレイヤーを初期座標に戻す
    m_Position = DirectX::XMFLOAT3(0.0f, -0.5f, 0.0f);
    m_Rotation = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

    // 復活時のバウンド伸縮演出（縦に引き伸ばされてから着地バウンド）
    m_Scale.y = 2.0f;
    m_Scale.x = 0.5f;
    m_Scale.z = 0.5f;

    // 掴みの解放
    if (m_GrabbedEnemy)
    {
        m_GrabbedEnemy->SetEnemyState(EnemyState::NORMAL);
        m_GrabbedEnemy->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
        m_GrabbedEnemy = nullptr;
    }
    m_State = PlayerState::IDLE;

    // 画面内の敵の弾をすべて消去する
    for (GameObject* obj : Manager::GetGameObjectList())
    {
        if (obj)
        {
            if (obj->GetObjectType() == ObjectType::Bullet)
            {
                EnemyBullet* bullet = static_cast<EnemyBullet*>(obj);
                bullet->SetDestroy();
            }
        }
    }
    
    // プレイヤーの足元に強力な多重衝撃波を発生させて、周囲の敵をなぎ払う
    DirectX::XMFLOAT3 shockPos = m_Position;
    shockPos.y = -0.95f; // 床面に完全クランプ
    ShockwaveSystem::AddShockwave(shockPos, 18.0f, 2.5f, 1.8f, 0.0f, 40, 2.2f, 0);

    // カメラの強いシェイク
    if (g_Camera)
    {
        g_Camera->Shake(0.8f, 25);
    }
}

// ─────────────────────────────────────────────
// プレイヤーの正面方向ベクトルの取得
// ─────────────────────────────────────────────
DirectX::XMFLOAT3 Player::GetForwardVector() const
{
    return DirectX::XMFLOAT3(sinf(m_Rotation.y), 0.0f, cosf(m_Rotation.y));
}

// ─────────────────────────────────────────────
// 自発光（Emissive）情報の取得（ガード/パリィ中のエフェクト発光）
// ─────────────────────────────────────────────
DirectX::XMFLOAT3 Player::GetEmissive() const
{
    if (IsParryActive())
    {
        return DirectX::XMFLOAT3(0.0f, 2.0f, 5.0f); // パリィ受付時間中は眩しいシアン/ブルー
    }
    if (IsGuardActive())
    {
        return DirectX::XMFLOAT3(0.0f, 0.5f, 1.5f); // ガード中は落ち着いた青色
    }
    return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f); // 通常時は無発光
}

// ─────────────────────────────────────────────
// ロックオン対象の探索（スローモーション中のみ）
// ─────────────────────────────────────────────
void Player::FindLockOnTarget()
{
    if (!g_Camera) return;

    XMFLOAT3 camPos = g_Camera->GetPosition();
    XMFLOAT3 camFwd = g_Camera->GetForward();

    using namespace DirectX;
    XMVECTOR vCamFwd = XMVector3Normalize(XMLoadFloat3(&camFwd));
    XMVECTOR vCamPos = XMLoadFloat3(&camPos);

    Enemy* bestTarget = nullptr;
    float maxCos = -1.0f;
    float maxDistance = 30.0f;
    float minCos = 0.7071f; // cos(45度) = 左右45度以内

    for (GameObject* obj : Manager::GetGameObjectList())
    {
        if (!obj || obj == this || obj->IsDestroy()) continue;
        ObjectType type = obj->GetObjectType();
        if (type != ObjectType::Enemy && type != ObjectType::Boss) continue;

        Enemy* enemy = static_cast<Enemy*>(obj);
        EnemyState state = enemy->GetEnemyState();
        if (state == EnemyState::DEFEATED || state == EnemyState::GRABBED || state == EnemyState::VACUUMED) continue;
        if (enemy == m_GrabbedEnemy) continue;

        XMFLOAT3 enemyPos = enemy->GetPosition();
        XMVECTOR vEnemyPos = XMLoadFloat3(&enemyPos);

        // プレイヤーからの距離チェック
        float dist = MathHelper::Length(enemyPos - m_Position);
        if (dist > maxDistance) continue;

        // カメラから敵への方向ベクトルと、カメラ正面ベクトルの内積（コサイン類似度）
        XMVECTOR vToEnemy = XMVector3Normalize(vEnemyPos - vCamPos);
        float cosAngle = XMVectorGetX(XMVector3Dot(vToEnemy, vCamFwd));

        if (cosAngle >= minCos)
        {
            if (cosAngle > maxCos)
            {
                maxCos = cosAngle;
                bestTarget = enemy;
            }
        }
    }

    if (bestTarget != m_LockOnTarget)
    {
        m_LockOnTarget = bestTarget;
        m_LockOnFrame = 0; // ターゲットが変わったらフォーカスアニメーション用にタイマーリセット
    }
    else if (m_LockOnTarget != nullptr)
    {
        m_LockOnFrame++;
    }
}

// ─────────────────────────────────────────────
// オブジェクト破棄時のクリーンアップ処理（ダングリングポインタ防止）
// ─────────────────────────────────────────────
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
