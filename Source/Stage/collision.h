#pragma once
#include "gameobject.h"
#include <vector>

class Collision
{
public:
    static bool CheckAABB(const GameObject* a, const GameObject* b);
    static bool CheckAABB(const GameObject* a, const DirectX::XMFLOAT3& nextPosA, const GameObject* b);
    static bool CheckSphere(const GameObject* a, const GameObject* b);
    static bool CheckAABBCollision(const GameObject* self, const DirectX::XMFLOAT3& nextPos, const std::vector<GameObject*>& objList, GameObject** ignoreObj = nullptr);
    static void ResolveGrabPhysics(GameObject* parent, GameObject* child, float offset = 0.0f);
    static void ResolveAABBCollision(GameObject* self, const std::vector<GameObject*>& objList);
};
