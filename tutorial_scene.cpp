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
#include <vector>

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
    device->CreateBuffer(&bd, &sd, &m_QuadVB);

    Renderer::CreateVertexShader(
        &m_VS, &m_IL, Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(
        &m_PS, Renderer::ResolveShaderPath("ui_ps.cso").c_str());

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = FALSE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    device->CreateDepthStencilState(&dsd, &m_DepthState);
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

  if (m_GuideTexture) {
    m_GuideTexture->Release();
    m_GuideTexture = nullptr;
  }
  if (m_SkipTexture) {
    m_SkipTexture->Release();
    m_SkipTexture = nullptr;
  }
  if (m_DepthState) {
    m_DepthState->Release();
    m_DepthState = nullptr;
  }
  if (m_IL) {
    m_IL->Release();
    m_IL = nullptr;
  }
  if (m_PS) {
    m_PS->Release();
    m_PS = nullptr;
  }
  if (m_VS) {
    m_VS->Release();
    m_VS = nullptr;
  }
  if (m_QuadVB) {
    m_QuadVB->Release();
    m_QuadVB = nullptr;
  }
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
    CreateGuideTexture(
        L"STEP 1: 移動とダッシュ",
        L"[W, A, S, D] キーで移動  /  [Shift] キーで高速ダッシュ");
    break;

  case TutorialStep::GRAB_AND_THROW: {
    CreateGuideTexture(
        L"STEP 2: 敵の捕獲と投擲",
        L"敵に接近して [左クリック] で掴む  /  もう一度 [左クリック] で投擲！");
    // サンドバッグ敵をプレイヤー正面に配置
    Enemy *dummy = Manager::AddGameObject<Enemy>();
    dummy->SetPosition(XMFLOAT3(0.0f, -0.5f, 2.0f));
    dummy->SetSandbag(true, INT_MAX); // チュートリアル用：自動消滅なし
    break;
  }

  case TutorialStep::SPIN_ATTACK: {
    CreateGuideTexture(L"STEP 3: 豪快スピン攻撃",
                       L"敵を掴んだ状態で [右クリック] "
                       L"を押すと高速自動スピン！ 周囲の敵を薙ぎ払え");
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
    CreateGuideTexture(
        L"STEP 4: ガード・パリィ・ジャスト回避",
        L"[右クリック長押し] でガード / 着弾直前にガードでパリィ反射！ / "
        L"弾の直前に [Shift] でジャスト回避！");
    // 遠距離攻撃してくる敵を配置
    AttackingEnemy *attacker = Manager::AddGameObject<AttackingEnemy>();
    attacker->SetPosition(XMFLOAT3(0.0f, -0.5f, 8.0f));
    break;
  }

  case TutorialStep::WARP_SLASH: {
    CreateGuideTexture(
        L"STEP 5: ワープスラッシュ",
        L"ジャスト回避（弾の直前に [Shift] ダッシュ）でスロー発動！ "
        L"ロックオン枠の敵に [左クリック] で一閃！");
    // 反撃の練習用標的（パリィ後のサンドバッグや敵がいなければ再配置）
    bool hasEnemy = false;
    for (GameObject *obj : Manager::GetGameObjectList()) {
      if (obj && obj->GetObjectType() == ObjectType::Enemy &&
          !obj->IsDestroy()) {
        hasEnemy = true;
        break;
      }
    }
    if (!hasEnemy) {
      AttackingEnemy *attacker = Manager::AddGameObject<AttackingEnemy>();
      attacker->SetPosition(XMFLOAT3(0.0f, -0.5f, 8.0f));
    }
    break;
  }

  case TutorialStep::COMPLETE:
    CreateGuideTexture(L"TUTORIAL COMPLETE!",
                       L"基本操作をマスターした！ 実戦ステージへ突入します...");
    SoundManager::PlaySE(SoundID::SE_DECIDE);
    m_TransitionDelay = 120; // 2秒後に本編へ
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
  m_TransitionDelay = 50; // 完了演出・少し待って次へ
  SoundManager::PlaySE(SoundID::SE_DECIDE);
  Manager::TriggerFlash(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f), 0.05f);
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
          Manager::ChangeScene(Scene::GAMEPLAY, 1.0f, 1.0f,
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
    // 開始直後の誤判定防止：最低60フレーム待つ
    if (m_StepTimer < 60)
      break;
    // 敵を撃破、またはワープスラッシュ回数が1回以上
    if (player->GetWarpSlashCount() > 0) {
      CompleteCurrentStep();
    } else {
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
    }
    break;
  }

  case TutorialStep::COMPLETE:
    break;
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

  // スキップ入力（ESC でいつでも本編へ直行）
  if (Input::GetKeyTrigger(VK_ESCAPE)) {
    SoundManager::PlaySE(SoundID::SE_DECIDE);
    Manager::ChangeScene(Scene::GAMEPLAY, 0.8f, 0.8f, XMFLOAT4(0, 0, 0, 1));
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
  // 1. 画面上部中央にチュートリアル案内ボードを描画
  if (m_GuideTexture) {
    float posX = (float)SCREEN_WIDTH * 0.5f;
    float posY = 85.0f;
    float w = 680.0f;
    float h = 110.0f;

    XMFLOAT4 emission = m_StepCleared ? XMFLOAT4(0.2f, 1.8f, 0.4f, 0.0f)
                                      : XMFLOAT4(0.0f, 1.2f, 1.8f, 0.0f);
    DrawUIQuad(m_GuideTexture, posX, posY, w, h, emission);
  }

  // 2. 画面右下にスキップ案内を描画
  if (m_SkipTexture) {
    float posX = (float)SCREEN_WIDTH - 150.0f;
    float posY = (float)SCREEN_HEIGHT - 35.0f;
    float w = 260.0f;
    float h = 40.0f;
    DrawUIQuad(m_SkipTexture, posX, posY, w, h,
               XMFLOAT4(0.8f, 0.8f, 0.8f, 0.0f));
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
      0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);

  ctx->OMSetDepthStencilState(m_DepthState, 0);
  ctx->VSSetShader(m_VS, nullptr, 0);
  ctx->PSSetShader(m_PS, nullptr, 0);
  ctx->IASetInputLayout(m_IL);
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  UINT stride = 60u;
  UINT offset = 0;
  ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &stride, &offset);

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

// =================================================================
// ガイドUIテクスチャ生成
// =================================================================
void TutorialScene::CreateGuideTexture(const std::wstring &mainText,
                                       const std::wstring &subText) {
  if (m_GuideTexture) {
    m_GuideTexture->Release();
    m_GuideTexture = nullptr;
  }

  const int W = 680;
  const int H = 110;

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

  // 背景（ダーク半透明ボックス）
  HBRUSH bgBrush = CreateSolidBrush(RGB(18, 22, 32));
  RECT bgRect = {0, 0, W, H};
  FillRect(hdc, &bgRect, bgBrush);
  DeleteObject(bgBrush);

  // 外枠（スタイリッシュなシアン/ゴールドの境界線）
  HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(0, 210, 255));
  HGDIOBJ oldPen = SelectObject(hdc, borderPen);
  HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
  HGDIOBJ oldBrush = SelectObject(hdc, nullBrush);
  Rectangle(hdc, 1, 1, W - 1, H - 1);
  SelectObject(hdc, oldPen);
  SelectObject(hdc, oldBrush);
  DeleteObject(borderPen);

  // フォント作成
  HFONT hFontMain =
      CreateFontW(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                  DEFAULT_PITCH | FF_DONTCARE, L"Arial");
  HFONT hFontSub =
      CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                  DEFAULT_PITCH | FF_DONTCARE, L"Arial");

  SetBkMode(hdc, TRANSPARENT);

  // メインタイトル
  HGDIOBJ oldFont = SelectObject(hdc, hFontMain);
  SetTextColor(hdc, RGB(255, 230, 80)); // 黄金色
  RECT rcMain = {20, 10, W - 20, 50};
  DrawTextW(hdc, mainText.c_str(), -1, &rcMain, DT_CENTER | DT_SINGLELINE);

  // サブ説明テキスト
  SelectObject(hdc, hFontSub);
  SetTextColor(hdc, RGB(230, 240, 255)); // 明るい白青
  RECT rcSub = {20, 56, W - 20, H - 10};
  DrawTextW(hdc, subText.c_str(), -1, &rcSub, DT_CENTER | DT_WORDBREAK);

  GdiFlush();

  // 透過度設定（背景は半透明 0.85、文字と境界線は 1.0）
  std::vector<BYTE> rgba(W * H * 4);
  for (int i = 0; i < W * H; i++) {
    BYTE b = bits[i * 4 + 0];
    BYTE g = bits[i * 4 + 1];
    BYTE r = bits[i * 4 + 2];
    BYTE a = 210;
    if (r > 60 || g > 60 || b > 60) {
      a = 255;
    }
    rgba[i * 4 + 0] = r;
    rgba[i * 4 + 1] = g;
    rgba[i * 4 + 2] = b;
    rgba[i * 4 + 3] = a;
  }

  SelectObject(hdc, oldFont);
  SelectObject(hdc, oldBmp);
  DeleteObject(hFontMain);
  DeleteObject(hFontSub);
  DeleteObject(hBmp);
  DeleteDC(hdc);

  ID3D11Device *device = Renderer::GetDevice();
  if (!device)
    return;

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

  ID3D11Texture2D *tex = nullptr;
  if (SUCCEEDED(device->CreateTexture2D(&td, &initData, &tex))) {
    device->CreateShaderResourceView(tex, nullptr, &m_GuideTexture);
    tex->Release();
  }
}

// =================================================================
// スキップ案内テクスチャ生成
// =================================================================
void TutorialScene::CreateSkipTexture() {
  if (m_SkipTexture) {
    m_SkipTexture->Release();
    m_SkipTexture = nullptr;
  }

  const int W = 260;
  const int H = 40;

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

  HBRUSH bgBrush = CreateSolidBrush(RGB(15, 15, 20));
  RECT bgRect = {0, 0, W, H};
  FillRect(hdc, &bgRect, bgBrush);
  DeleteObject(bgBrush);

  HFONT hFont =
      CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                  DEFAULT_PITCH | FF_DONTCARE, L"Arial");

  HGDIOBJ oldFont = SelectObject(hdc, hFont);
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(180, 190, 205));
  RECT rcText = {0, 8, W, H};
  DrawTextW(hdc, L"[ESC] スキップ", -1, &rcText, DT_CENTER | DT_SINGLELINE);

  GdiFlush();

  std::vector<BYTE> rgba(W * H * 4);
  for (int i = 0; i < W * H; i++) {
    BYTE b = bits[i * 4 + 0];
    BYTE g = bits[i * 4 + 1];
    BYTE r = bits[i * 4 + 2];
    rgba[i * 4 + 0] = r;
    rgba[i * 4 + 1] = g;
    rgba[i * 4 + 2] = b;
    rgba[i * 4 + 3] = (r > 40 || g > 40 || b > 40) ? 240 : 160;
  }

  SelectObject(hdc, oldFont);
  SelectObject(hdc, oldBmp);
  DeleteObject(hFont);
  DeleteObject(hBmp);
  DeleteDC(hdc);

  ID3D11Device *device = Renderer::GetDevice();
  if (!device)
    return;

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

  ID3D11Texture2D *tex = nullptr;
  if (SUCCEEDED(device->CreateTexture2D(&td, &initData, &tex))) {
    device->CreateShaderResourceView(tex, nullptr, &m_SkipTexture);
    tex->Release();
  }
}
