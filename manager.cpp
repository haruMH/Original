#include "manager.h"
#include "camera.h"
#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "input.h"
#include "camera.h"
#include "player.h"
#include "field.h"
#include "enemy.h"

std::list<GameObject*> Manager::m_GameObjectList;
Camera *g_Camera;

void Manager::Init() {
  Renderer::Init();
  Input::Init();

  // 光源（平行光源）の設定
  LIGHT light;
  ZeroMemory(&light, sizeof(light));
  light.Enable    = TRUE;
  light.Direction = XMFLOAT4(0.5f, -1.0f, 0.5f, 0.0f); // 真下に近い角度から少し斜めに
  light.Diffuse   = XMFLOAT4(1.2f, 1.2f, 1.2f, 1.0f);  // 光を強くする
  light.Ambient   = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);  // 環境光を抑えて影を際立たせる
  Renderer::SetLight(light);

  g_Camera = new Camera();
  g_Camera->Init();

  Player* player = new Player();
  player->Init();
  m_GameObjectList.push_back(player);

  Field* field = new Field();
  field->Init();
  m_GameObjectList.push_back(field);

  // エネミー（障害物）を数体配置
  for (int i = 0; i < 5; i++) {
    Enemy* enemy = new Enemy();
    enemy->Init();
    enemy->SetPosition(XMFLOAT3((float)(i * 4 - 8), -0.5f, 10.0f));
    m_GameObjectList.push_back(enemy);
  }
}

void Manager::Uninit() {
  for (GameObject* obj : m_GameObjectList) {
    obj->Uninit();
    delete obj;
  }
  m_GameObjectList.clear();

  g_Camera->Uninit();
  delete g_Camera;

  Input::Uninit();
  Renderer::Uninit();
}

void Manager::Update() {
  Input::Update();

  for (auto it = m_GameObjectList.begin(); it != m_GameObjectList.end(); ) {
    (*it)->Update();
    if ((*it)->IsDestroy()) {
      (*it)->Uninit();
      delete (*it);
      it = m_GameObjectList.erase(it);
    } else {
      ++it;
    }
  }

  g_Camera->Update();
}

void Manager::Draw() {
  Renderer::Begin();

  // --- 1. 陜ｨ・ｰ鬮ｱ・｢邵ｺ・ｮ隰蜀怜愛 ---
  // --- 1. 背景・地面の描画 ---
  for (GameObject* obj : m_GameObjectList) {
      if (dynamic_cast<Field*>(obj)) {
          obj->Draw();
      }
  }

  // --- 2. 影の描画 ---
  // 平面投影行列の作成（地面 Y = -0.999f への投影）
  XMVECTOR plane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.999f); 
  XMVECTOR light = XMVectorSet(1.0f, -1.0f, 1.0f, 0.0f); // Manager::Initで設定した光源と合わせる
  XMMATRIX shadowMatrix = XMMatrixShadow(plane, light);

  Renderer::SetShadowMatrix(shadowMatrix);
  Renderer::SetShadowMode(true);
  for (GameObject* obj : m_GameObjectList) {
      if (dynamic_cast<Player*>(obj) || dynamic_cast<Enemy*>(obj)) {
          obj->Draw(); // Renderer::SetWorldMatrix が内部で影行列を合成する
      }
  }
  Renderer::SetShadowMode(false);

  // --- 3. 本体の描画 ---
  for (GameObject* obj : m_GameObjectList) {
      if (!dynamic_cast<Field*>(obj)) {
          obj->Draw();
      }
  }

  g_Camera->Draw();

  Renderer::End();
}

GameObject* Manager::GetPlayer() {
  // プレイヤーは最初に登録されている前提
  if (!m_GameObjectList.empty()) {
      return m_GameObjectList.front();
  }
  return nullptr;
}
