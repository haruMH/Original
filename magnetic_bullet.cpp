#include "magnetic_bullet.h"
#include "renderer.h"
#include "resource_manager.h"
#include "player.h"
#include "manager.h"
#include "math_helper.h"
#include "collision.h"
#include "camera.h"

using namespace DirectX;

void MagneticBullet::Init()
{
    m_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(0.4f, 0.4f, 0.4f);
    m_Size     = XMFLOAT3(0.4f, 0.4f, 0.4f);
    m_Destroy  = false;

    // 紫色の魔力（磁力）発光色を設定
    m_EmissiveColor = XMFLOAT3(1.5f, 0.0f, 2.5f);

    // 既存の enemy.png を流用し、インスタンス描画で描画
    m_RenderComponent = RenderComponent("enemy.png", MeshType::Cube, true);
}

void MagneticBullet::Uninit() {}

void MagneticBullet::Draw() {}

void MagneticBullet::Setup(GameObject* boss, float startAngle, float startRadius, float targetRadius, float rotSpeed, float attractFactor, float yOffset)
{
    m_BossTarget = boss;
    m_Angle = startAngle;
    m_CurrentRadius = startRadius;
    m_TargetRadius = targetRadius;
    m_RotSpeed = rotSpeed;
    m_AttractFactor = attractFactor;
    m_YOffset = yOffset;
    m_CurrentYOffset = startRadius * 0.3f + yOffset; // 頭上螺旋降下のための初期高さ設定（半径の30%上から開始）
}

void MagneticBullet::Launch(XMFLOAT3 targetPos)
{
    if (m_IsLaunched) return;
    m_IsLaunched = true;

    // ターゲット（プレイヤー）への方向ベクトルを算出
    XMVECTOR vTarget = XMLoadFloat3(&targetPos);
    vTarget = XMVectorSetY(vTarget, -0.5f); // 地面すれすれを狙う
    XMVECTOR vPos = XMLoadFloat3(&m_Position);
    XMVECTOR vDir = vTarget - vPos;

    float dist = XMVectorGetX(XMVector3Length(vDir));
    if (dist > 0.001f) {
        vDir = XMVector3Normalize(vDir);
        XMStoreFloat3(&m_Direction, vDir);
    } else {
        m_Direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
    }
}

void MagneticBullet::Update()
{
    if (!m_IsLaunched) {
        // === 未射出状態：ボスの周囲を螺旋周回 ===
        if (!m_BossTarget || m_BossTarget->IsDestroy()) {
            SetDestroy();
            return;
        }

        // 1. 角度の更新
        m_Angle += m_RotSpeed;
        if (m_Angle > XM_2PI) m_Angle -= XM_2PI;

        // 2. 半径と高さオフセットの更新（螺旋状に徐々に目標値へ収束）
        m_CurrentRadius += (m_TargetRadius - m_CurrentRadius) * m_AttractFactor;
        // Y方向は半径より5倍速く収束させることでカットシーン中にボスの中心高さに落ち着かせる
        m_CurrentYOffset += (m_YOffset - m_CurrentYOffset) * (m_AttractFactor * 5.0f);

        // 3. 奇妙な軌道（うねり・緩急）の適用
        // 半径に高速な脈動（うねり）を加える
        float wavyRadius = m_CurrentRadius + 1.2f * sinf(m_Angle * 3.0f);
        // 高さ（YOffset）にも独自の上下ウェーブを加える
        float wavyY = m_CurrentYOffset + 0.8f * sinf(m_Angle * 2.0f);
        // 角度を少し非線形に変調させて、回転速度に緩急をつける
        float skewedAngle = m_Angle + 0.3f * cosf(m_Angle * 2.0f);

        // 4. 座標の決定
        XMFLOAT3 bossPos = m_BossTarget->GetPosition();
        m_Position.x = bossPos.x + wavyRadius * cosf(skewedAngle);
        m_Position.y = bossPos.y + wavyY;
        m_Position.z = bossPos.z + wavyRadius * sinf(skewedAngle);
    }
    else {
        // === 射出状態：プレイヤーに向かって直線的に高速移動 ===
        m_Position.x += m_Direction.x * m_LaunchSpeed;
        m_Position.y += m_Direction.y * m_LaunchSpeed;
        m_Position.z += m_Direction.z * m_LaunchSpeed;

        // 寿命更新・消滅
        m_Life--;
        if (m_Life <= 0) {
            SetDestroy();
            return;
        }

        // 壁との衝突による消滅処理
        for (GameObject* obj : Manager::GetGameObjectList()) {
            if (!obj || obj->IsDestroy()) continue;
            if (obj->GetObjectType() == ObjectType::Wall) {
                if (Collision::CheckAABB(this, obj)) {
                    SetDestroy();
                    return;
                }
            }
        }
    }
}
