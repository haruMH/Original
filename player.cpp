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
    // HP0 死亡判定
    if (m_HP <= 0) {
        Restart();
        return;
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

    //// 掴んでいる敵の位置をプレイヤーの前方に固定（sinf/cosf で前方ベクトルを直接計算）
    //if ((m_State == PlayerState::GRABBED || m_State == PlayerState::SPINNING) && m_GrabbedEnemy) {
    //    // 前方ベクトルを sinf/cosf で計算、敵を前方2ユニットの位置に配置（Y は地面高さに固定）
    //    XMFLOAT3 fwdF = XMFLOAT3(sinf(m_Rotation.y), 0.0f, cosf(m_Rotation.y));
    //    XMFLOAT3 grabbedPos = m_Position + fwdF * 2.0f;
    //    grabbedPos.y = 0.0f;
    //    m_GrabbedEnemy->SetPosition(grabbedPos);
    //}
}

void Player::UpdateIdle()
{
    float camYaw = g_Camera ? g_Camera->GetAngleY() : 0.0f;
    float mouseMoveX = (float)Input::GetMouseMoveX();

    // プレイヤーコントローラーからカメラ角度を考慮した移動ベクトルを取得
    XMFLOAT3 moveDir = PlayerController::GetMoveDirection(camYaw);

    XMFLOAT3 nextPos = m_Position;
    if (MathHelper::LengthSq(moveDir) > 0.001f) {
        nextPos += moveDir * 0.1f;

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
