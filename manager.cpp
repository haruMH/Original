#include "manager.h"
#include "camera.h"
#include "game_scene.h"
#include "input.h"
#include "main.h"
#include "player.h"
#include "renderer.h"


Scene *Manager::m_Scene = nullptr;
Camera *g_Camera = nullptr;

void Manager::Init() {
  Renderer::Init();
  Input::Init();

  // 光源（原神風：暖かい太陽光 × 冷たい空の光）
  LIGHT light;
  ZeroMemory(&light, sizeof(light));
  light.Enable = TRUE;
  light.Direction = XMFLOAT4(0.6f, -1.0f, 0.4f, 0.0f); // 斜め上方から（太陽風）
  light.Diffuse =
      XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f); // ベースの光を抑えて白飛びを防ぐ
  light.Ambient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f); // 環境光も少し落ち着かせる
  light.FogColor = XMFLOAT4(0.55f, 0.72f, 0.95f, 1.0f); // 空色
  light.FogStart = 30.0f; // 自分の周りがよく見えるようにフォグを遠ざける
  light.FogEnd = 60.0f;   // フォグ終了距離も遠くへ
  Renderer::SetLight(light);

  g_Camera = new Camera();
  g_Camera->Init();

  // 最初のシーンをセット
  SetScene(new GameScene());
}

void Manager::Uninit() {
  if (m_Scene) {
    m_Scene->Uninit();
    delete m_Scene;
    m_Scene = nullptr;
  }

  g_Camera->Uninit();
  delete g_Camera;

  Input::Uninit();
  Renderer::Uninit();
}

void Manager::Update() {
  Input::Update();

  if (m_Scene) {
    m_Scene->Update();
  }

  g_Camera->Update();
}

void Manager::Draw() {
  if (m_Scene) {
    m_Scene->Draw();
  }
}

void Manager::SetScene(Scene *scene) {
  if (m_Scene) {
    m_Scene->Uninit();
    delete m_Scene;
  }
  m_Scene = scene;
  m_Scene->Init();
}

GameObject *Manager::GetPlayer() {
  if (!m_Scene)
    return nullptr;
  for (GameObject *obj : m_Scene->GetGameObjectList()) {
    if (dynamic_cast<Player *>(obj)) {
      return obj;
    }
  }
  return nullptr;
}
