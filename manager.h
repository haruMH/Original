#pragma once
#include "scene.h"

class Manager
{
private:
    static Scene* m_Scene;
    static int m_HitStopFrames;

public:
    static void Init();
    static void Uninit();
    static void Update();
    static void Draw();

    static Scene* GetScene() { return m_Scene; }
    static void SetScene(Scene* scene);
    static GameObject* GetPlayer();

    static void AddHitStop(int frames) { m_HitStopFrames = frames; }
};
