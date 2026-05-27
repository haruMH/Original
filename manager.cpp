#include "manager.h"
#include "camera.h"
#include "game_scene.h"
#include "input.h"
#include "main.h"
#include "player.h"
#include "renderer.h"
#include "resource_manager.h"
#include "collision.h"
#include "enemy.h"


Scene *Manager::m_Scene = nullptr;
Camera *g_Camera = nullptr;
int Manager::m_HitStopFrames = 0;

void Manager::Init() {
  Renderer::Init();
  Input::Init();

  // 光源（原神風：暖かい太陽光 × 冷たい空の光）
  LIGHT light;
  ZeroMemory(&light, sizeof(light));
  light.Enable = TRUE;
  light.Direction = XMFLOAT4(0.5f, -1.0f, 0.5f, 0.0f); // 斜め上方から（太陽風）
  light.Diffuse = XMFLOAT4(1.0f, 0.95f, 0.9f, 1.0f); // ベースの光を抑えて白飛びを防ぐ
  light.Ambient = XMFLOAT4(0.4f, 0.5f, 0.7f, 1.0f); // 環境光も少し落ち着かせる
  light.FogColor = XMFLOAT4(0.6f, 0.8f, 1.0f, 1.0f); // 空色
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
  ResourceManager::Uninit();
  Renderer::Uninit();
}

void Manager::Update() {
    Input::Update();

    // ヒットストップ中はカメラのみ更新し、シーンの更新はスキップする
    if (m_HitStopFrames > 0) {
        m_HitStopFrames--;
        g_Camera->Update();
        return;
    }

    // 1. 各オブジェクトの通常の移動処理（位置の先読みや入力移動など）
    if (m_Scene) {
        m_Scene->Update();

        // 掴んでいるエネミーの位置を確定させる後処理（LateUpdate思想）
        Player* player = dynamic_cast<Player*>(GetPlayer());
        if (player) {
            // プレイヤーの状態が「掴み中」か「回転中」のとき
            if (player->GetState() == PlayerState::GRABBED || player->GetState() == PlayerState::SPINNING) {
                Enemy* grabbedEnemy = player->GetGrabbedEnemy();
                if (grabbedEnemy) {
                    Collision::ResolveGrabPhysics(player, grabbedEnemy, 0.8f);
                }
            }
        }
        // 今後「中ボスがプレイヤーを掴む」などの物理拘束が増えた場合も、このエリアに追記していけます
    }

    g_Camera->Update();
}

void Manager::Draw() {
    if (m_Scene) {
        // 1. フレームの開始宣言（描画ターゲットのクリアと設定）
        Renderer::Begin();

        // 2. シーン内の全オブジェクト（石壁、プレイヤー、箱など）の描画
        m_Scene->Draw();

        // 3. フレームの終了宣言（ブルーム処理の実行 ＆ SwapChain->Present）
        Renderer::End();
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
