#pragma once
#include <list>
#include "gameobject.h"

class Scene {
protected:
    std::list<GameObject*> m_GameObjectList;

public:
    Scene() {}
    virtual ~Scene() {}

    virtual void Init() = 0;
    virtual void Uninit();
    virtual void Update();
    virtual void Draw();

    void AddGameObject(GameObject* obj) { m_GameObjectList.push_back(obj); }
    std::list<GameObject*>& GetGameObjectList() { return m_GameObjectList; }
};