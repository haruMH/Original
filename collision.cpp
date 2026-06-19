#include "collision.h"
#include "field.h"
#include "enemy.h"
#include "math_helper.h"
#include <cmath>

bool Collision::CheckAABB(const GameObject* a, const GameObject* b)
{
    if (!a || !b) return false;
    return CheckAABB(a, a->GetPosition(), b);
}

bool Collision::CheckAABB(const GameObject* a, const DirectX::XMFLOAT3& nextPosA, const GameObject* b)
{
    if (!a || !b) return false;

    DirectX::XMFLOAT3 posB = b->GetPosition();
    DirectX::XMFLOAT3 sizeA = a->GetSize();
    DirectX::XMFLOAT3 scaleA = a->GetScale();
    DirectX::XMFLOAT3 sizeB = b->GetSize();
    DirectX::XMFLOAT3 scaleB = b->GetScale();

    // プレイヤーのもちもち伸縮（アニメーションスケール）が衝突判定のバウンディングボックスを
    // 変動させないよう、衝突判定時はプレイヤーのスケールを固定値 (1.0f, 1.0f, 1.0f) にリセットします。
    if (a->GetObjectType() == ObjectType::Player) {
        scaleA = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
    }
    if (b->GetObjectType() == ObjectType::Player) {
        scaleB = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
    }

    // X, Y, Z 各軸でお互いに重なっているかを判定
    bool collisionX = std::abs(nextPosA.x - posB.x) < (sizeA.x * scaleA.x + sizeB.x * scaleB.x) * 0.5f;
    bool collisionY = std::abs(nextPosA.y - posB.y) < (sizeA.y * scaleA.y + sizeB.y * scaleB.y) * 0.5f;
    bool collisionZ = std::abs(nextPosA.z - posB.z) < (sizeA.z * scaleA.z + sizeB.z * scaleB.z) * 0.5f;

    return collisionX && collisionY && collisionZ;
}

bool Collision::CheckSphere(const GameObject* a, const GameObject* b)
{
    if (!a || !b) return false;

    // MathHelperを使用して2点間の距離の2乗を計算し、半径の合計の2乗と比較
    float distSq = MathHelper::DistanceSq(a->GetPosition(), b->GetPosition());
    float sumRad = a->GetRadius() + b->GetRadius();

    return distSq < (sumRad * sumRad);
}

bool Collision::CheckAABBCollision(const GameObject* self, const DirectX::XMFLOAT3& nextPos, const std::list<GameObject*>& objList, GameObject** ignoreObj)
{
    if (!self) return false;

    for (GameObject* obj : objList) {
        if (obj == self) continue;
        if (ignoreObj && obj == *ignoreObj) continue;       // 既に掴んでいる敵は衝突除外
        if (obj->GetObjectType() == ObjectType::Field) continue; // 地面は除外
        if (obj->IsDestroy()) continue;

        if (CheckAABB(self, nextPos, obj)) {
            // 掴み対象のセットは「まだ何も掴んでいない（*ignoreObj == nullptr）」
            // かつ「NORMAL状態の敵」のときのみ行う
            // → GRABBED / SPINNING 中は絶対に上書きしない（持ち替わりバグ防止）
            // → FLYING / VACUUMED などの非NORMAL敵は掴まない（連鎖衝突バグ防止）
            if (ignoreObj && !(*ignoreObj)) {
                if (obj->GetObjectType() == ObjectType::Enemy) {
                    Enemy* e = static_cast<Enemy*>(obj);
                    if (e->GetEnemyState() == EnemyState::NORMAL) {
                        *ignoreObj = obj;
                    }
                }
            }
            return true;
        }
    }
    return false;
}

void Collision::ResolveGrabPhysics(GameObject* parent, GameObject* child, float offset)
{
    if (!parent || !child) return;

    DirectX::XMFLOAT3 parentPos = parent->GetPosition();
    DirectX::XMFLOAT3 parentRot = parent->GetRotation();

    // お互いの AABB のサイズとスケールを取得
    DirectX::XMFLOAT3 sizeP = parent->GetSize();
    DirectX::XMFLOAT3 scaleP = parent->GetScale();
    DirectX::XMFLOAT3 sizeC = child->GetSize();
    DirectX::XMFLOAT3 scaleC = child->GetScale();

    // parent の正面方向（Z軸方向）の半径と、child の背面方向（Z軸方向）の半径を計算
    // ※もし立方体なら size.z * scale.z * 0.5f が「中心から外界までの距離」になります
    float parentRadiusZ = (sizeP.z * scaleP.z) * 0.5f;
    float childRadiusZ = (sizeC.z * scaleC.z) * 0.5f;

    // 2つの箱がピッタリ接触する理想の距離 ＋ 微調整用のオフセット
    float finalDistance = parentRadiusZ + childRadiusZ + offset;

    // 前方ベクトルを計算
    DirectX::XMFLOAT3 fwdF = DirectX::XMFLOAT3(std::sinf(parentRot.y), 0.0f, std::cosf(parentRot.y));

    // 算出した理想の距離で座標を設定
    DirectX::XMFLOAT3 grabbedPos;
    grabbedPos.x = parentPos.x + fwdF.x * finalDistance;
    // 掴んでいる敵の大きさに応じて、プレイヤーの高さを基準に設定してめり込みを防ぐ
    grabbedPos.y = parentPos.y + (sizeC.y * scaleC.y) * 0.5f;
    grabbedPos.z = parentPos.z + fwdF.z * finalDistance;

    child->SetPosition(grabbedPos);
}

void Collision::ResolveAABBCollision(GameObject* self, const std::list<GameObject*>& objList)
{
    if (!self || self->IsDestroy()) return;

    DirectX::XMFLOAT3 posA = self->GetPosition();
    DirectX::XMFLOAT3 sizeA = self->GetSize();
    DirectX::XMFLOAT3 scaleA = self->GetScale();
    if (self->GetObjectType() == ObjectType::Player) {
        scaleA = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
    }

    float radiusA_X = sizeA.x * scaleA.x * 0.5f;
    float radiusA_Z = sizeA.z * scaleA.z * 0.5f;

    for (GameObject* obj : objList) {
        if (!obj || obj == self || obj->IsDestroy()) continue;
        if (obj->GetObjectType() == ObjectType::Field) continue;

        if (obj->GetObjectType() == ObjectType::Enemy) {
            Enemy* enemy = static_cast<Enemy*>(obj);
            if (enemy->GetEnemyState() == EnemyState::GRABBED) continue;
        }

        if (CheckAABB(self, posA, obj)) {
            DirectX::XMFLOAT3 posB = obj->GetPosition();
            DirectX::XMFLOAT3 sizeB = obj->GetSize();
            DirectX::XMFLOAT3 scaleB = obj->GetScale();
            if (obj->GetObjectType() == ObjectType::Player) {
                scaleB = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
            }

            float radiusB_X = sizeB.x * scaleB.x * 0.5f;
            float radiusB_Z = sizeB.z * scaleB.z * 0.5f;

            float overlapX = (radiusA_X + radiusB_X) - std::abs(posA.x - posB.x);
            float overlapZ = (radiusA_Z + radiusB_Z) - std::abs(posA.z - posB.z);

            if (overlapX < overlapZ) {
                if (posA.x > posB.x) {
                    posA.x += overlapX;
                } else {
                    posA.x -= overlapX;
                }
            } else {
                if (posA.z > posB.z) {
                    posA.z += overlapZ;
                } else {
                    posA.z -= overlapZ;
                }
            }
            self->SetPosition(posA);
        }
    }
}