#include "scene.h"
#include "renderer.h"
#include "field.h"

void Scene::Uninit() {
    for (GameObject* obj : m_GameObjectList) {
        obj->Uninit();
        delete obj;
    }
    m_GameObjectList.clear();
}

void Scene::Update() {
    for (auto it = m_GameObjectList.begin(); it != m_GameObjectList.end(); ) {
        (*it)->Update();
        if ((*it)->IsDestroy()) {
            (*it)->Uninit();
            delete *it;
            it = m_GameObjectList.erase(it);
        } else {
            it++;
        }
    }
}

void Scene::Draw() {
    for (GameObject* obj : m_GameObjectList) {
        obj->Draw();
    }
}