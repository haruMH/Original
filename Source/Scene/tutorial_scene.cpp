#include "tutorial_scene.h"
#include "attacking_enemy.h"
#include "camera.h"
#include "collision.h"
#include "collision_system.h"
#include "enemy.h"
#include "event_system.h"
#include "event_types.h"
#include "field.h"
#include "game_rule.h"
#include "input.h"
#include "manager.h"
#include "math_helper.h"
#include "player.h"
#include "player_controller.h"
#include "renderer.h"
#include "shockwave.h"
#include "sound_manager.h"
#include "wall.h"
#include "main.h"
#include <vector>
#include <functional>

using namespace DirectX;

// =================================================================
// コンストラクタ / デストラクタ
// =================================================================
TutorialScene::TutorialScene() {}

TutorialScene::~TutorialScene() {}

// =================================================================
// 初期化
// =================================================================
void TutorialScene::Init() {
  // スコアやゲーム状態のリセット
  GameRule::Init();
  Manager::SetHitStopFrames(0);
  Manager::SetSlowMotionTimer(0);
  Manager::SetSlowMotionDuration(0);
  Manager::SetIsBossStage(false);

  // イベント購読（パリィや被弾などの演出）
  EventSystem::Subscribe<PlayerHitEvent>([](const PlayerHitEvent &ev) {
    if (g_Camera)
      g_Camera->Shake(0.35f, 12);
    Manager::AddHitStop(5);
    Manager::TriggerFlash(XMFLOAT4(1.0f, 0.0f, 0.0f, 0.4f), 0.04f);
  });

  EventSystem::Subscribe<PlayerParriedEvent>([](const PlayerParriedEvent &ev) {
    Manager::TriggerFlash(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.02f);
    Manager::StartSlowMotion(120); // チュートリアルでは長めにスロー
    Manager::AddHitStop(15);
  });

  // 1. 地面フィールド生成
  Manager::AddGameObject<Field>();
  Manager::AddGameObject<Skybox>();

  // 2. プレイヤー生成
  Player *player = Manager::AddGameObject<Player>();
  player->SetPosition(XMFLOAT3(0.0f, -0.5f, -5.0f));
  player->SetRotation(XMFLOAT3(0.0f, 0.0f, 0.0f));

  // 3. 練習用の壁を前方に配置
  Wall *wall = Manager::AddGameObject<Wall>();
  wall->SetPosition(XMFLOAT3(0.0f, 2.5f, 16.0f));
  wall->SetScale(XMFLOAT3(24.0f, 6.0f, 1.0f));

  // カメラをプレイヤー後方にセット
  if (g_Camera) {
    g_Camera->SetAngles(0.25f, 0.0f);
  }

  // UI描画用リソースの初期化
  ID3D11Device *device = Renderer::GetDevice();
  if (device) {
    struct VERTEX_3D_LOCAL {
      XMFLOAT3 Position;
      XMFLOAT3 Normal;
      XMFLOAT4 Diffuse;
      XMFLOAT2 TexCoord;
      XMFLOAT3 Tangent;
    } v[4];

    v[0].Position = XMFLOAT3(-0.5f, -0.5f, 0.0f);
    v[0].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[0].Diffuse = XMFLOAT4(1, 1, 1, 1);
    v[0].TexCoord = XMFLOAT2(0.0f, 0.0f);
    v[0].Tangent = XMFLOAT3(1, 0, 0);

    v[1].Position = XMFLOAT3(0.5f, -0.5f, 0.0f);
    v[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[1].Diffuse = XMFLOAT4(1, 1, 1, 1);
    v[1].TexCoord = XMFLOAT2(1.0f, 0.0f);
    v[1].Tangent = XMFLOAT3(1, 0, 0);

    v[2].Position = XMFLOAT3(-0.5f, 0.5f, 0.0f);
    v[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[2].Diffuse = XMFLOAT4(1, 1, 1, 1);
    v[2].TexCoord = XMFLOAT2(0.0f, 1.0f);
    v[2].Tangent = XMFLOAT3(1, 0, 0);

    v[3].Position = XMFLOAT3(0.5f, 0.5f, 0.0f);
    v[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[3].Diffuse = XMFLOAT4(1, 1, 1, 1);
    v[3].TexCoord = XMFLOAT2(1.0f, 1.0f);
    v[3].Tangent = XMFLOAT3(1, 0, 0);

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth = sizeof(v);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = v;
    device->CreateBuffer(&bd, &sd, m_QuadVB.ReleaseAndGetAddressOf());

    Renderer::CreateVertexShader(
        m_VS.ReleaseAndGetAddressOf(), m_IL.ReleaseAndGetAddressOf(),
        Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(
        m_PS.ReleaseAndGetAddressOf(), Renderer::ResolveShaderPath("ui_ps.cso").c_str());

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = FALSE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    device->CreateDepthStencilState(&dsd, m_DepthState.ReleaseAndGetAddressOf());
  }

  CreateSkipTexture();

  // 最初のステップへ
  SetupStep(TutorialStep::MOVE_AND_DASH);
}

// =================================================================
// 終了処理
// =================================================================
void TutorialScene::Uninit() {
  EventSystem::Clear();

  m_GuideTexture.Reset();
  m_SkipTexture.Reset();
  m_DepthState.Reset();
  m_IL.Reset();
  m_PS.Reset();
  m_VS.Reset();
  m_QuadVB.Reset();
}

// =================================================================
// 次のステップの切り替え予約
// =================================================================
void TutorialScene::RequestNextStep(TutorialStep nextStep) {
  m_HasPendingStep = true;
  m_PendingStep = nextStep;
}

// =================================================================
// ステップごとのセットアップ
// =================================================================
void TutorialScene::SetupStep(TutorialStep step) {
  m_CurrentStep = step;
  m_StepTimer = 0;
  m_TransitionDelay = 0;
  m_StepCleared = false;
  m_HasPendingStep = false;

  Player *player = Manager::GetGameObject<Player>();
  if (player) {
    m_StartPlayerPos = player->GetPosition();
    m_InitialPlayerDistance = 0.0f;
  }

  // 既存の敵を安全に破棄マーク（WARP_SLASHステップ以外）
  // WARP_SLASHではパリィで生成されたサンドバッグ敵をそのまま反撃目標として利用する
  if (step != TutorialStep::WARP_SLASH) {
    for (GameObject *obj : Manager::GetGameObjectList()) {
      if (obj && obj->GetObjectType() == ObjectType::Enemy) {
        obj->SetDestroy();
      }
    }
  }

  switch (step) {
  case TutorialStep::MOVE_AND_DASH:
    CreateGuideTexture(1, 5,
        L"移動とダッシュ",
        L"[W, A, S, D] キーで移動  /  [Shift] キーで高速ダッシュ");
    break;

  case TutorialStep::GRAB_AND_THROW: {
    CreateGuideTexture(2, 5,
        L"敵の捕獲と投擲",
        L"敵に接近して [左クリック] で掴む  /  もう一度 [左クリック] で投擲！");
    // サンドバッグ敵をプレイヤー正面に配置
    Enemy *dummy = Manager::AddGameObject<Enemy>();
    dummy->SetPosition(XMFLOAT3(0.0f, -0.5f, 2.0f));
    dummy->SetSandbag(true, INT_MAX); // チュートリアル用：自動消滅なし
    break;
  }

  case TutorialStep::SPIN_ATTACK: {
    CreateGuideTexture(3, 5,
        L"豪快スピン攻撃",
        L"敵を掴んだ状態で [右クリック] を押すと高速自動スピン！ 周囲の敵を薙ぎ払え");
    // 掴む用のサンドバッグ敵
    Enemy *center = Manager::AddGameObject<Enemy>();
    center->SetPosition(XMFLOAT3(0.0f, -0.5f, 2.0f));
    center->SetSandbag(true, INT_MAX); // チュートリアル用：自動消滅なし

    // 周囲に薙ぎ払われる通常敵を数体配置
    for (int i = 0; i < 4; ++i) {
      float angle = (XM_2PI / 4.0f) * i;
      Enemy *minion = Manager::AddGameObject<Enemy>();
      minion->SetPosition(
          XMFLOAT3(sinf(angle) * 4.5f, -0.5f, 2.0f + cosf(angle) * 4.5f));
      minion->SetSandbag(true, INT_MAX); // チュートリアル用：自動消滅なし
    }
    break;
  }

  case TutorialStep::PARRY: {
    CreateGuideTexture(4, 5,
        L"ガード・パリィ・ジャスト回避",
        L"[右クリック長押し] でガード / 着弾直前にガードでパリィ反射！ / 弾の直前に [Shift] でジャスト回避！");
    // 遠距離攻撃してくる敵を配置（移動しない固定砲台）
    AttackingEnemy *attacker = Manager::AddGameObject<AttackingEnemy>();
    if (attacker) {
      attacker->SetPosition(XMFLOAT3(0.0f, -0.5f, 8.0f));
      attacker->SetTurretMode(true);
    }
    break;
  }

  case TutorialStep::WARP_SLASH: {
    CreateGuideTexture(5, 5,
        L"ワープスラッシュ",
        L"ジャスト回避（弾の直前に [Shift] ダッシュ）でスロー発動！ ロックオン枠の敵に [左クリック] で一閃！");
    // リスポーン用の AttackingEnemy を配置（生存中のものがなければ）
    bool hasAttacker = false;
    for (GameObject *obj : Manager::GetGameObjectList()) {
      if (obj && obj->GetObjectType() == ObjectType::Enemy &&
          !obj->IsDestroy()) {
        Enemy *e = static_cast<Enemy *>(obj);
        if (e->GetEnemyState() != EnemyState::DEFEATED && e->IsAttackingEnemy()) {
          hasAttacker = true;
          break;
        }
      }
    }
    if (!hasAttacker) {
      AttackingEnemy *attacker = Manager::AddGameObject<AttackingEnemy>();
      if (attacker) {
        attacker->SetPosition(XMFLOAT3(0.0f, -0.5f, 8.0f));
        attacker->SetTurretMode(true);
      }
    }
    // ステップ開始時にプレイヤーフラグとシーン専用フラグをリセット
    m_WarpSlashDone = false;
    if (player) {
      player->SetWarpSlashCount(0);
      player->SetCanWarpSlash(true);
      player->ResetWarpSlashSuccess();
    }
    break;
  }

  case TutorialStep::COMPLETE:
    CreateGuideTexture(5, 5,
        L"TUTORIAL COMPLETE!",
        L"基本操作をマスターした！ 実戦ステージへ突入します...", true);
    SoundManager::PlaySE(SoundID::SE_DECIDE);
    m_TransitionDelay = 125; // 完了演出55f + 125f = 合計180f (ちょうど3秒) で本編へ
    break;
  }
}

// =================================================================
// ステップ完了判定
// =================================================================
void TutorialScene::CompleteCurrentStep() {
  if (m_StepCleared)
    return;
  m_StepCleared = true;
  m_TransitionDelay = 55; // 完了演出・少し待って次へ
  SoundManager::PlaySE(SoundID::SE_DECIDE);
  Manager::TriggerFlash(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f), 0.05f);

  // 達成表示へUIテクスチャを即時リフレッシュ
  int stepIdx = static_cast<int>(m_CurrentStep) + 1;
  CreateGuideTexture(stepIdx, 5, m_LastMainText, m_LastSubText, true);
}

// =================================================================
// 毎フレームのステップ進行ロジック
// =================================================================
void TutorialScene::UpdateStepLogic() {
  Player *player = Manager::GetGameObject<Player>();
  if (!player)
    return;

  // ステップ完了後の次ステップ遷移処理
  if (m_StepCleared) {
    if (m_TransitionDelay > 0) {
      m_TransitionDelay--;
      if (m_TransitionDelay <= 0) {
        switch (m_CurrentStep) {
        case TutorialStep::MOVE_AND_DASH:
          RequestNextStep(TutorialStep::GRAB_AND_THROW);
          break;
        case TutorialStep::GRAB_AND_THROW:
          RequestNextStep(TutorialStep::SPIN_ATTACK);
          break;
        case TutorialStep::SPIN_ATTACK:
          RequestNextStep(TutorialStep::PARRY);
          break;
        case TutorialStep::PARRY:
          RequestNextStep(TutorialStep::WARP_SLASH);
          break;
        case TutorialStep::WARP_SLASH:
          RequestNextStep(TutorialStep::COMPLETE);
          break;
        case TutorialStep::COMPLETE:
          Manager::ChangeScene(Scene::HUB, 1.0f, 1.0f,
                               XMFLOAT4(0, 0, 0, 1));
          break;
        }
      }
    }
    return;
  }

  switch (m_CurrentStep) {
  case TutorialStep::MOVE_AND_DASH: {
    // 開始直後の誤判定防止：最低60フレーム待つ
    if (m_StepTimer < 60)
      break;
    // プレイヤーが開始位置から一定以上移動、またはダッシュを使用したらクリア
    XMFLOAT3 pPos = player->GetPosition();
    float dist = MathHelper::Length(pPos - m_StartPlayerPos);
    if (dist > 6.0f || player->IsDashing()) {
      CompleteCurrentStep();
    }
    break;
  }

  case TutorialStep::GRAB_AND_THROW: {
    // 開始直後の誤判定防止：最低60フレーム待つ
    if (m_StepTimer < 60)
      break;
    // 敵が撃破されたか判定
    int aliveCount = 0;
    for (GameObject *obj : Manager::GetGameObjectList()) {
      if (obj && obj->GetObjectType() == ObjectType::Enemy) {
        Enemy *e = static_cast<Enemy *>(obj);
        if (e->GetEnemyState() != EnemyState::DEFEATED) {
          aliveCount++;
        }
      }
    }
    if (aliveCount == 0) {
      CompleteCurrentStep();
    }
    break;
  }

  case TutorialStep::SPIN_ATTACK: {
    // 開始直後の誤判定防止：最低60フレーム待つ
    if (m_StepTimer < 60)
      break;
    // 周囲の敵を含めて全滅させたか判定
    int aliveCount = 0;
    for (GameObject *obj : Manager::GetGameObjectList()) {
      if (obj && obj->GetObjectType() == ObjectType::Enemy) {
        Enemy *e = static_cast<Enemy *>(obj);
        if (e->GetEnemyState() != EnemyState::DEFEATED) {
          aliveCount++;
        }
      }
    }
    if (aliveCount == 0) {
      CompleteCurrentStep();
    }
    break;
  }

  case TutorialStep::PARRY: {
    // 敵を倒してしまった場合の無限リスポーン処理
    if (!m_StepCleared) {
      int aliveAttackerCount = 0;
      for (GameObject *obj : Manager::GetGameObjectList()) {
        if (obj && obj->GetObjectType() == ObjectType::Enemy && !obj->IsDestroy()) {
          Enemy *e = static_cast<Enemy *>(obj);
          if (e->GetEnemyState() != EnemyState::DEFEATED) {
            aliveAttackerCount++;
          }
        }
      }
      if (aliveAttackerCount == 0) {
        AttackingEnemy *attacker = Manager::AddGameObject<AttackingEnemy>();
        if (attacker) {
          attacker->SetPosition(XMFLOAT3(0.0f, -0.5f, 8.0f));
          attacker->SetTurretMode(true);
        }
      }
    }

    // 開始直後の誤判定防止：最低90フレーム待つ（スローモーション残留を確実に消す）
    if (m_StepTimer < 90)
      break;
    // パリィが成功したか（スローモーションが発動したか）
    if (Manager::IsSlowMotionActive()) {
      CompleteCurrentStep();
    }
    break;
  }

  case TutorialStep::WARP_SLASH: {
    // ワープスラッシュ成功をシーン内フラグで確認（プレイヤーフラグのリセットに影響されない）
    if (!m_WarpSlashDone &&
        (player->GetWarpSlashSuccess() || player->GetWarpSlashCount() > 0)) {
      m_WarpSlashDone = true;
    }

    if (m_WarpSlashDone) {
      CompleteCurrentStep();
      break;
    }

    // ワープスラッシュ成功前に敵を倒してしまった場合の無限リスポーン処理
    if (!m_StepCleared) {
      int aliveAttackerCount = 0;
      for (GameObject *obj : Manager::GetGameObjectList()) {
        if (obj && obj->GetObjectType() == ObjectType::Enemy && !obj->IsDestroy()) {
          Enemy *e = static_cast<Enemy *>(obj);
          if (e->GetEnemyState() != EnemyState::DEFEATED && e->IsAttackingEnemy()) {
            aliveAttackerCount++;
          }
        }
      }
      if (aliveAttackerCount == 0) {
        AttackingEnemy *attacker = Manager::AddGameObject<AttackingEnemy>();
        if (attacker) {
          attacker->SetPosition(XMFLOAT3(0.0f, -0.5f, 8.0f));
          attacker->SetTurretMode(true);
        }
      }
    }
    break;
  }

  case TutorialStep::COMPLETE: {
    // 3秒経過後に自動で戦闘モード（実戦ステージ）へ遷移
    if (m_TransitionDelay > 0) {
      m_TransitionDelay--;
      if (m_TransitionDelay <= 0) {
        Manager::ChangeScene(Scene::HUB, 1.0f, 1.0f, XMFLOAT4(0, 0, 0, 1));
      }
    }
    break;
  }
  }
}

// =================================================================
// 更新処理
// =================================================================
void TutorialScene::Update() {
  m_StepTimer++;

  // 保留されたステップ遷移があればフレーム冒頭で安全にセットアップ
  if (m_HasPendingStep) {
    m_HasPendingStep = false;
    SetupStep(m_PendingStep);
  }

  // アニメーション・発光パルス用タイマーの進行
  m_PulseTimer += 0.05f;

  // スキップ入力（ESC でいつでも本編へ直行）
  if (Input::GetKeyTrigger(VK_ESCAPE)) {
    SoundManager::PlaySE(SoundID::SE_DECIDE);
    Manager::ChangeScene(Scene::HUB, 0.8f, 0.8f, XMFLOAT4(0, 0, 0, 1));
    return;
  }

  // スローモーションタイマーの更新
  if (Manager::GetSlowMotionTimer() > 0) {
    Manager::SetSlowMotionTimer(Manager::GetSlowMotionTimer() - 1);
  }

  bool updateOthers = true;
  if (Manager::GetSlowMotionTimer() > 0) {
    updateOthers = (Manager::GetSlowMotionTimer() % 5 == 0);
  }

  if (updateOthers) {
    ShockwaveSystem::Update();
  }

  // ─────────────────────────────────────────────────────────────────
  // WARP_SLASH 成功判定はヒットストップに関係なく毎フレーム実行する
  // （ヒットストップ中はプレイヤーが更新されないため、
  //   WarpSlashCount / WarpSlashSuccess が次フレーム以降もリセットされず残存する）
  // ─────────────────────────────────────────────────────────────────
  if (m_CurrentStep == TutorialStep::WARP_SLASH && !m_StepCleared && !m_WarpSlashDone) {
    Player *pCheck = Manager::GetGameObject<Player>();
    if (pCheck && (pCheck->GetWarpSlashSuccess() || pCheck->GetWarpSlashCount() > 0)) {
      LOG_INFO("[TutorialScene] ★★★ WARP_SLASH 検知成功! Success=%d, Count=%d -> ステップ完了へ ★★★\n",
               pCheck->GetWarpSlashSuccess() ? 1 : 0, pCheck->GetWarpSlashCount());
      m_WarpSlashDone = true;
      CompleteCurrentStep();
    }
  }

  if (Manager::GetHitStopFrames() > 0) {
    Manager::SetHitStopFrames(Manager::GetHitStopFrames() - 1);
    return;
  }

  Player *player = Manager::GetGameObject<Player>();

  // オブジェクトの更新
  for (size_t i = 0; i < Manager::GetUpdateObjectList().size(); i++) {
    GameObject *obj = Manager::GetUpdateObjectList()[i];
    if (!obj)
      continue;

    bool isGrabbedEnemy = (player && player->GetGrabbedEnemy() == obj);
    bool shouldUpdate =
        (obj->GetObjectType() == ObjectType::Player || isGrabbedEnemy)
            ? true
            : updateOthers;

    if (shouldUpdate) {
      obj->Update();
    }
  }

  Manager::DestroyObjectsIf();

  if (updateOthers) {
    CollisionSystem::Update();
  }

  // 掴み同期
  if (player) {
    if (player->GetState() == PlayerState::GRABBED ||
        player->GetState() == PlayerState::SPINNING) {
      Enemy *grabbedEnemy = player->GetGrabbedEnemy();
      if (grabbedEnemy) {
        Collision::ResolveGrabPhysics(player, grabbedEnemy, 0.8f);
      }
    }
  }

  // ステップロジックの更新
  UpdateStepLogic();
}

// =================================================================
// 描画処理
// =================================================================
void TutorialScene::Draw() {
  const float screenW = (float)Renderer::GetScreenWidth();
  const float screenH = (float)Renderer::GetScreenHeight();

  // 1. 画面上部中央にチュートリアル案内ボードを描画
  if (m_GuideTexture) {
    float posX = screenW * 0.5f;
    float posY = 88.0f;
    float w = 780.0f;
    float h = 135.0f;

    float pulse = 0.5f + 0.5f * sinf(m_PulseTimer);
    XMFLOAT4 emission = m_StepCleared
        ? XMFLOAT4(0.25f + 0.35f * pulse, 1.8f + 0.6f * pulse, 0.45f + 0.2f * pulse, 0.0f)
        : XMFLOAT4(0.1f * pulse, 0.7f + 0.4f * pulse, 1.2f + 0.5f * pulse, 0.0f);
    DrawUIQuad(m_GuideTexture.Get(), posX, posY, w, h, emission);
  }

  // 2. 画面右下にスキップ案内を描画
  if (m_SkipTexture) {
    float posX = screenW - 160.0f;
    float posY = screenH - 38.0f;
    float w = 280.0f;
    float h = 44.0f;
    float pulse = 0.5f + 0.5f * sinf(m_PulseTimer * 0.8f);
    DrawUIQuad(m_SkipTexture.Get(), posX, posY, w, h,
               XMFLOAT4(0.6f + 0.3f * pulse, 0.8f + 0.4f * pulse, 1.1f + 0.5f * pulse, 0.0f));
  }
}

// =================================================================
// UIクワッド描画ヘルパー
// =================================================================
void TutorialScene::DrawUIQuad(ID3D11ShaderResourceView *srv, float posX,
                               float posY, float width, float height,
                               XMFLOAT4 emission) {
  if (!srv || !m_QuadVB || !m_VS || !m_PS || !m_IL || !m_DepthState)
    return;

  ID3D11DeviceContext *ctx = Renderer::GetDeviceContext();

  XMMATRIX oldView = Renderer::GetViewMatrix();
  XMMATRIX oldProj = Renderer::GetProjectionMatrix();

  XMMATRIX view2D = XMMatrixIdentity();
  XMMATRIX proj2D = XMMatrixOrthographicOffCenterLH(
      0.0f, (float)Renderer::GetScreenWidth(), (float)Renderer::GetScreenHeight(), 0.0f, 0.0f, 1.0f);

  ctx->OMSetDepthStencilState(m_DepthState.Get(), 0);
  ctx->VSSetShader(m_VS.Get(), nullptr, 0);
  ctx->PSSetShader(m_PS.Get(), nullptr, 0);
  ctx->IASetInputLayout(m_IL.Get());
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  UINT stride = 60u;
  UINT offset = 0;
  ID3D11Buffer *vb = m_QuadVB.Get();
  ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);

  Renderer::SetViewMatrix(view2D);
  Renderer::SetProjectionMatrix(proj2D);

  XMMATRIX world = XMMatrixScaling(width, height, 1.0f) *
                   XMMatrixTranslation(posX, posY, 0.0f);
  Renderer::SetWorldMatrix(world);

  MATERIAL mat = {};
  mat.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
  mat.Emission = emission;
  mat.TextureEnable = TRUE;
  Renderer::SetMaterial(mat);

  Renderer::SetTexture(srv);
  ctx->Draw(4, 0);

  Renderer::SetViewMatrix(oldView);
  Renderer::SetProjectionMatrix(oldProj);
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

namespace {
// ポリゴンリージョン付きのGDI DIBからD3D11 SRVを生成する共通ヘルパー
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateRegionGdiTexture(
    int W, int H, const POINT *pts, int ptCount, int lumThreshold, BYTE baseAlpha,
    const std::function<void(HDC, HRGN)> &drawCallback) {
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = W;
  bmi.bmiHeader.biHeight = -H;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  BYTE *bits = nullptr;
  HDC hdc = CreateCompatibleDC(nullptr);
  HBITMAP hBmp =
      CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void **)&bits, nullptr, 0);
  HGDIOBJ oldBmp = SelectObject(hdc, hBmp);

  HRGN hRgn = CreatePolygonRgn(pts, ptCount, WINDING);
  if (drawCallback) {
    drawCallback(hdc, hRgn);
  }
  GdiFlush();

  std::vector<BYTE> rgba(W * H * 4);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      int idx = (y * W + x) * 4;
      BYTE b = bits[idx + 0];
      BYTE g = bits[idx + 1];
      BYTE r = bits[idx + 2];
      BYTE a = 0;
      if (PtInRegion(hRgn, x, y)) {
        a = (r > lumThreshold || g > lumThreshold || b > lumThreshold) ? 255 : baseAlpha;
      }
      rgba[idx + 0] = r;
      rgba[idx + 1] = g;
      rgba[idx + 2] = b;
      rgba[idx + 3] = a;
    }
  }

  SelectObject(hdc, oldBmp);
  DeleteObject(hRgn);
  DeleteObject(hBmp);
  DeleteDC(hdc);

  ID3D11Device *device = Renderer::GetDevice();
  if (!device)
    return nullptr;

  D3D11_TEXTURE2D_DESC td = {};
  td.Width = W;
  td.Height = H;
  td.MipLevels = 1;
  td.ArraySize = 1;
  td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  td.SampleDesc.Count = 1;
  td.Usage = D3D11_USAGE_IMMUTABLE;
  td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  D3D11_SUBRESOURCE_DATA initData = {};
  initData.pSysMem = rgba.data();
  initData.SysMemPitch = W * 4;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
  if (SUCCEEDED(device->CreateTexture2D(&td, &initData, tex.GetAddressOf())) && tex) {
    device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
  }
  return srv;
}
} // namespace

// =================================================================
// ガイドUIテクスチャ生成
// =================================================================
void TutorialScene::CreateGuideTexture(int stepNumber, int totalSteps,
                                       const std::wstring &mainText,
                                       const std::wstring &subText,
                                       bool isCleared) {
  m_LastMainText = mainText;
  m_LastSubText  = subText;

  const int W = 780;
  const int H = 135;
  const int C = 16; // 四隅のサイバー斜めカット幅
  POINT pts[8] = {
      { C, 0 }, { W - C, 0 }, { W, C }, { W, H - C },
      { W - C, H }, { C, H }, { 0, H - C }, { 0, C }
  };

  m_GuideTexture = CreateRegionGdiTexture(W, H, pts, 8, 45, 215, [&](HDC hdc, HRGN hRgn) {
    SelectClipRgn(hdc, hRgn);

    // 背景グラデーション（上部: インディゴスレート、下部: 漆黒）
    for (int y = 0; y < H; ++y) {
      float t = (float)y / (float)H;
      BYTE r = (BYTE)(16.0f * (1.0f - t) + 8.0f * t);
      BYTE g = (BYTE)(24.0f * (1.0f - t) + 12.0f * t);
      BYTE b = (BYTE)(42.0f * (1.0f - t) + 20.0f * t);
      HBRUSH lineBrush = CreateSolidBrush(RGB(r, g, b));
      RECT lineRect = { 0, y, W, y + 1 };
      FillRect(hdc, &lineRect, lineBrush);
      DeleteObject(lineBrush);
    }

    SelectClipRgn(hdc, nullptr);

    // サイバー外枠（通常: ネオンシアン、クリア時: ネオングリーン）
    COLORREF neonColor = isCleared ? RGB(50, 255, 120) : RGB(0, 220, 255);
    HPEN borderPen = CreatePen(PS_SOLID, 2, neonColor);
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Polygon(hdc, pts, 8);

    // 内側デュアルアクセントライン（オフセット 3px）
    const int C2 = C - 2;
    POINT ptsInner[8] = {
        { C2 + 3, 3 }, { W - C2 - 3, 3 }, { W - 3, C2 + 3 }, { W - 3, H - C2 - 3 },
        { W - C2 - 3, H - 3 }, { C2 + 3, H - 3 }, { 3, H - C2 - 3 }, { 3, C2 + 3 }
    };
    HPEN innerPen = CreatePen(PS_SOLID, 1, isCleared ? RGB(30, 180, 80) : RGB(0, 140, 200));
    SelectObject(hdc, innerPen);
    Polygon(hdc, ptsInner, 8);
    DeleteObject(innerPen);

    SetBkMode(hdc, TRANSPARENT);

    // ステッププログレスインジケーター（上部 Y = 8〜32）
    const int badgeW = 230;
    const int badgeH = 22;
    const int badgeX = (W - badgeW) / 2;
    const int badgeY = 8;
    RECT rcBadge = { badgeX, badgeY, badgeX + badgeW, badgeY + badgeH };
    HBRUSH badgeBrush = CreateSolidBrush(isCleared ? RGB(20, 80, 45) : RGB(20, 45, 75));
    HPEN badgePen = CreatePen(PS_SOLID, 1, neonColor);
    SelectObject(hdc, badgeBrush);
    SelectObject(hdc, badgePen);
    RoundRect(hdc, badgeX, badgeY, badgeX + badgeW, badgeY + badgeH, 6, 6);
    DeleteObject(badgeBrush);
    DeleteObject(badgePen);

    HFONT hFontBadge = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                   DEFAULT_PITCH | FF_DONTCARE, L"Meiryo UI");
    HGDIOBJ oldFont = SelectObject(hdc, hFontBadge);
    SetTextColor(hdc, isCleared ? RGB(80, 255, 140) : RGB(255, 220, 75));
    std::wstring badgeText = isCleared ? L"★ STEP CLEARED! ★"
                                       : (L"◆ STEP " + std::to_wstring(stepNumber) + L" / " + std::to_wstring(totalSteps) + L" ◆");
    DrawTextW(hdc, badgeText.c_str(), -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // プログレスノード（5つのドットノードを左右に配置）
    const int dotY = badgeY + badgeH / 2;
    for (int i = 1; i <= totalSteps; ++i) {
      int dotX = (i <= 2) ? (badgeX - 70 + (i - 1) * 32) : (badgeX + badgeW + 20 + (i - 3) * 32);
      COLORREF nodeColor;
      if (i < stepNumber || (i == stepNumber && isCleared)) {
        nodeColor = RGB(50, 255, 120); // 達成済み: ネオングリーン
      } else if (i == stepNumber) {
        nodeColor = RGB(255, 220, 70);  // 現在ステップ: エナジーゴールド
      } else {
        nodeColor = RGB(65, 80, 100);   // 未到達: ダークグレー
      }
      HBRUSH dotBrush = CreateSolidBrush(nodeColor);
      HPEN dotPen = CreatePen(PS_SOLID, 1, nodeColor);
      SelectObject(hdc, dotBrush);
      SelectObject(hdc, dotPen);
      Ellipse(hdc, dotX - 5, dotY - 5, dotX + 5, dotY + 5);
      DeleteObject(dotBrush);
      DeleteObject(dotPen);
    }

    // メインタイトル（Y = 36〜76）
    HFONT hFontMain = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, L"Meiryo UI");
    SelectObject(hdc, hFontMain);
    RECT rcMainShadow = { 22, 38, W - 18, 76 };
    SetTextColor(hdc, RGB(0, 0, 0));
    DrawTextW(hdc, mainText.c_str(), -1, &rcMainShadow, DT_CENTER | DT_SINGLELINE);

    RECT rcMain = { 20, 36, W - 20, 74 };
    SetTextColor(hdc, isCleared ? RGB(100, 255, 160) : RGB(255, 235, 130));
    DrawTextW(hdc, mainText.c_str(), -1, &rcMain, DT_CENTER | DT_SINGLELINE);

    // サブ説明文（Y = 78〜126）
    HFONT hFontSub = CreateFontW(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Meiryo UI");
    SelectObject(hdc, hFontSub);
    RECT rcSubShadow = { 26, 80, W - 24, H - 6 };
    SetTextColor(hdc, RGB(0, 0, 0));
    DrawTextW(hdc, subText.c_str(), -1, &rcSubShadow, DT_CENTER | DT_WORDBREAK);

    RECT rcSub = { 25, 78, W - 25, H - 8 };
    SetTextColor(hdc, RGB(225, 240, 255));
    DrawTextW(hdc, subText.c_str(), -1, &rcSub, DT_CENTER | DT_WORDBREAK);

    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    DeleteObject(hFontBadge);
    DeleteObject(hFontMain);
    DeleteObject(hFontSub);
  });
}

// =================================================================
// スキップ案内テクスチャ生成
// =================================================================
void TutorialScene::CreateSkipTexture() {
  const int W = 280;
  const int H = 44;
  const int S = 12; // 平行四辺形のスラッシュ傾斜
  POINT pts[4] = { { S, 0 }, { W, 0 }, { W - S, H }, { 0, H } };

  m_SkipTexture = CreateRegionGdiTexture(W, H, pts, 4, 40, 210, [&](HDC hdc, HRGN hRgn) {
    SelectClipRgn(hdc, hRgn);

    // 背景（ダークスレートグラデーション）
    HBRUSH bgBrush = CreateSolidBrush(RGB(14, 18, 30));
    RECT bgRect = { 0, 0, W, H };
    FillRect(hdc, &bgRect, bgBrush);
    DeleteObject(bgBrush);

    SelectClipRgn(hdc, nullptr);

    // 外枠ネオンライン
    HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(0, 210, 255));
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Polygon(hdc, pts, 4);

    SetBkMode(hdc, TRANSPARENT);

    // キーキャップ [ ESC ]
    const int keyX = 18;
    const int keyY = 8;
    const int keyW = 68;
    const int keyH = 28;
    HBRUSH keyBrush = CreateSolidBrush(RGB(32, 42, 60));
    HPEN keyPen = CreatePen(PS_SOLID, 1, RGB(180, 215, 255));
    SelectObject(hdc, keyBrush);
    SelectObject(hdc, keyPen);
    RoundRect(hdc, keyX, keyY, keyX + keyW, keyY + keyH, 6, 6);
    DeleteObject(keyBrush);
    DeleteObject(keyPen);

    HFONT hFontEsc = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HGDIOBJ oldFont = SelectObject(hdc, hFontEsc);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT rcEsc = { keyX, keyY, keyX + keyW, keyY + keyH };
    DrawTextW(hdc, L"ESC", -1, &rcEsc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // スキップ案内テキスト
    HFONT hFontText = CreateFontW(17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, L"Meiryo UI");
    SelectObject(hdc, hFontText);
    SetTextColor(hdc, RGB(200, 235, 255));
    RECT rcText = { keyX + keyW + 12, 10, W - 16, H - 8 };
    DrawTextW(hdc, L"SKIP TUTORIAL ▶", -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    DeleteObject(hFontEsc);
    DeleteObject(hFontText);
  });
}
