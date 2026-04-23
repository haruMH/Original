#pragma once
#include <list>
#include "gameobject.h"

class Manager
{
private:
	static std::list<GameObject*> m_GameObjectList;

public:
	static void Init();
	static void Uninit();
	static void Update();
	static void Draw();

	static GameObject* GetPlayer();
	static std::list<GameObject*>& GetGameObjectList() { return m_GameObjectList; }
};
