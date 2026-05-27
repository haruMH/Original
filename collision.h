#pragma once
#include "gameobject.h"
#include <list>

// 衝突判定を行う静的クラス
class Collision
{
public:
    // 2つのGameObjectの現在の位置でのAABB衝突判定
    static bool CheckAABB(const GameObject* a, const GameObject* b);

    // GameObject a が指定座標 nextPosA にあると仮定したときの、GameObject b とのAABB衝突判定
    static bool CheckAABB(const GameObject* a, const DirectX::XMFLOAT3& nextPosA, const GameObject* b);

    // 2つのGameObjectの球同士の衝突判定
    static bool CheckSphere(const GameObject* a, const GameObject* b);

    // 自オブジェクトが移動先(nextPos)で他のオブジェクト群(objList)と衝突するかの一括判定
    // ignoreObjに指定されたポインタ（掴んでいる敵など）は判定から除外します
    // Enemyと衝突した場合はignoreObjに書き込んで掴み判定として機能します
    static bool CheckAABBCollision(const GameObject* self, const DirectX::XMFLOAT3& nextPos, const std::list<GameObject*>& objList, GameObject** ignoreObj = nullptr);

    // collision.h 内
    static void ResolveGrabPhysics(GameObject* parent, GameObject* child, float offset = 0.0f);
};
