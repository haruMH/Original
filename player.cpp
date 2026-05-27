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


void Player::Init()
{
    m_Position = XMFLOAT3(0.0f, -0.5f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_IsAutoSpinning   = false;
    m_CurrentSpinSpeed = 0.0f;
    m_HasVacuumItem    = false;
    m_MarkerTimer      = 0;

    m_Texture = ResourceManager::GetTexture("player.png");

    // コンポーネント指向での描画パラメータ初期化
    m_RenderComponent = RenderComponent("player.png", MeshType::Cube, true);
}

void Player::Uninit()
{
}

void Player::Update()
{
    m_MarkerTimer++;
    float oldRotY = m_Rotation.y;

    // 状態別の更新処理
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
    if (!Collision::CheckAABBCollision(this, nextPos, Manager::GetScene()->GetGameObjectList())) {
        m_Position = nextPos;
    }

    // 左クリック：正面付近にいる NORMAL 状態の敵を掴む
    if (PlayerController::IsGrabOrThrowAction()) {
        XMFLOAT3 fwd = XMFLOAT3(sinf(m_Rotation.y), 0.0f, cosf(m_Rotation.y));
        float grabRange = 4.0f;  // 掴み有効距離
        Enemy* nearest  = nullptr;
        float  nearestDist = grabRange;

        for (GameObject* obj : Manager::GetScene()->GetGameObjectList()) {
            if (!obj || obj == this || obj->IsDestroy()) continue;
            Enemy* e = dynamic_cast<Enemy*>(obj);
            if (!e) continue;
            if (e->GetEnemyState() != EnemyState::NORMAL) continue;

            XMFLOAT3 toEnemy = e->GetPosition() - m_Position;
            float dist = MathHelper::Length(toEnemy);
            if (dist < nearestDist) {
                nearestDist = dist;
                nearest = e;
            }
        }

        if (nearest) {
            m_GrabbedEnemy = nearest;
            m_State = PlayerState::GRABBED;
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
    if (!Collision::CheckAABBCollision(this, nextPos, Manager::GetScene()->GetGameObjectList(), &ignoreGrabbed)) {
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
    if (!Collision::CheckAABBCollision(this, nextPos, Manager::GetScene()->GetGameObjectList(), &ignoreGrabbed)) {
        m_Position = nextPos;
    }

    // 吸引アイテムを持っている場合、周囲の敵を吸い寄せる
    if (m_HasVacuumItem) {
        float vacuumRadius = 8.0f; // 吸引判定半径
        float vacuumForce = 0.03f; // 引力スピード

        for (GameObject* obj : Manager::GetScene()->GetGameObjectList()) {
            if (!obj || obj == this || obj == m_GrabbedEnemy) continue;

            Enemy* enemy = dynamic_cast<Enemy*>(obj);
            if (enemy) {
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

        m_GrabbedEnemy->SetVelocity(throwVelocity);
        m_GrabbedEnemy->SetEnemyState(EnemyState::FLYING);

        // もし吸い込みアイテムを持っていたら、投げた敵を爆発属性にする
        if (m_HasVacuumItem) {
            m_GrabbedEnemy->SetExplosive(true);
        }

        // 吸い込まれて公転していた敵も一斉射出する
        // ─── 挙動：プレイヤー正面方向へ全員まとめて直線発射し、
        //          爆発時に TriggerExplosion で四方八方へ吹き飛ばす ───
        for (GameObject* obj : Manager::GetScene()->GetGameObjectList()) {
            if (!obj) continue;
            Enemy* enemy = dynamic_cast<Enemy*>(obj);
            if (enemy && enemy->GetEnemyState() == EnemyState::VACUUMED) {
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

            for (GameObject* obj : Manager::GetScene()->GetGameObjectList()) {
                if (!obj || obj == this || obj->IsDestroy()) continue;
                Enemy* e = dynamic_cast<Enemy*>(obj);
                if (!e) continue;
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
    }
}
