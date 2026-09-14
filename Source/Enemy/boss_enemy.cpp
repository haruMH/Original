#include "boss_enemy.h"
#include "camera.h"
#include "enemy_bullet.h"
#include "event_system.h"
#include "event_types.h"
#include "game_constants.h"
#include "game_rule.h"
#include "manager.h"
#include "math_helper.h"
#include "player.h"
#include "renderer.h"
#include "resource_manager.h"
#include "score_popup.h"
#include "shockwave.h"
#include "sound_manager.h"
#include <algorithm> // std::remove_if 使用のため

// namespace BossLightningConfig は削除され、Constants::Boss (game_constants.h)
// に統合されました。

// ─────────────────────────────────────────────
// 初期化
// ─────────────────────────────────────────────
void BossEnemy::Init() {
  Enemy::Init();

  m_EnemyState = EnemyState::NORMAL;
  m_Scale = XMFLOAT3(5.0f, 5.0f, 5.0f); // 壁と同じ大きさ (5x5x5)
  m_Size = XMFLOAT3(1.0f, 1.0f, 1.0f);
  m_HP = Constants::Boss::HP;
  m_MaxHP = Constants::Boss::HP;
  SetScoreValue(Constants::Boss::SCORE); // 巨大ボスは15倍スコア
  m_DamageFlashTimer = 0;
  m_AttackTimer = 0;
  m_AttackPattern = 0;

  m_BossState = BossState::NORMAL;
  m_PhaseAttackTimer = 0;
  m_PhaseIndex = 0;
  m_IsInvincible = false;
  m_Phases = {{Constants::Boss::PHASE1_HP_THRESHOLD, 1, false},
              {Constants::Boss::PHASE2_HP_THRESHOLD, 2, false},
              {Constants::Boss::PHASE3_HP_THRESHOLD, 3, false}};
  m_PhaseTargetPos = XMFLOAT3(0.0f, 0.0f, 0.0f);
  m_LightningVisualTimer = 0;

  m_RenderComponent = RenderComponent("enemy.png", MeshType::Cube, true);
  m_Rotation = XMFLOAT3(0.0f, DirectX::XM_PI, 0.0f); // 初期状態でプレイヤー側（南）を向く
}

// ─────────────────────────────────────────────
// 更新処理
// ─────────────────────────────────────────────
void BossEnemy::Update() {
  if (Manager::IsCutsceneActive()) {
    // カットシーン登場演出中も最初からプレイヤー方向を正面から見据える
    Player* player = Manager::GetGameObject<Player>();
    if (player) {
      XMFLOAT3 pPos = player->GetPosition();
      float dx = pPos.x - m_Position.x;
      float dz = pPos.z - m_Position.z;
      if (dx * dx + dz * dz > 0.001f) {
        m_Rotation.y = atan2f(dx, dz);
        m_Rotation.x = 0.0f;
        m_Rotation.z = 0.0f;
      }
    }
    return;
  }

  if (m_DamageFlashTimer > 0) {
    m_DamageFlashTimer--;
  }

  if (m_EnemyState == EnemyState::DEFEATED ||
      m_EnemyState == EnemyState::BLOWN_AWAY) {
    Enemy::Update();
    return;
  }

  // 1. 定位置復帰ジャンプ中
  if (m_BossState == BossState::RETURN_TO_CENTER) {
    UpdateReturnJump();
    return;
  }

  // 2. フェーズ特別攻撃演出中
  if (m_BossState == BossState::PHASE_TRANSITION) {
    PerformPhaseAttack();
    return;
  }

  // 3. 通常行動中（突進中・大ジャンプ中含む）のHP監視によるフェーズ移行
  if (m_BossState == BossState::NORMAL ||
      m_BossState == BossState::CHARGE_WINDUP ||
      m_BossState == BossState::CHARGE_RUSH ||
      m_BossState == BossState::CHARGE_STUN ||
      m_BossState == BossState::SLAM_WINDUP ||
      m_BossState == BossState::SLAM_AIR ||
      m_BossState == BossState::SLAM_RECOVERY) {
    for (auto &phase : m_Phases) {
      if (m_HP <= phase.hpThreshold && !phase.triggered) {
        m_PhaseIndex = phase.phaseIndex;
        m_IsInvincible = true;
        phase.triggered = true;
        m_PhaseAttackTimer = 0;
        m_LightningVisualTimer = 0;
        m_Scale = XMFLOAT3(5.0f, 5.0f, 5.0f); // スケールをリセット

        // 定位置（ステージ北側中央 Z=BOSS_SPAWN_OFFSET_Z）への復帰大ジャンプを開始
        m_JumpStartPos = m_Position;
        m_JumpTargetPos = XMFLOAT3(0.0f, Constants::Boss::GROUND_Y, Constants::Stage::BOSS_SPAWN_OFFSET_Z);
        m_JumpTimer = 0;
        m_JumpDuration = 45; // 約0.75秒の豪快な放物線跳躍

        m_BossState = BossState::RETURN_TO_CENTER;

        char dbg[128];
        sprintf_s(dbg, "[BossEnemy] Phase %d Triggered! Returning to center via jump.\n", phase.phaseIndex);
        OutputDebugStringA(dbg);
        return;
      }
    }
  }

  // 4. 突進タックル各フェーズの更新
  if (m_BossState == BossState::CHARGE_WINDUP) {
    UpdateChargeWindup();
    return;
  }
  if (m_BossState == BossState::CHARGE_RUSH) {
    UpdateChargeRush();
    return;
  }
  if (m_BossState == BossState::CHARGE_STUN) {
    UpdateChargeStun();
    return;
  }

  // 5. 大ジャンプ・グラウンドスラム各フェーズの更新
  if (m_BossState == BossState::SLAM_WINDUP) {
    UpdateSlamWindup();
    return;
  }
  if (m_BossState == BossState::SLAM_AIR) {
    UpdateSlamAir();
    return;
  }
  if (m_BossState == BossState::SLAM_RECOVERY) {
    UpdateSlamRecovery();
    return;
  }

  // 6. 通常AI更新
  UpdateBossAI();
}

// ─────────────────────────────────────────────
// 特殊攻撃開始前の定位置復帰ジャンプ処理
// ─────────────────────────────────────────────
void BossEnemy::UpdateReturnJump() {
  m_JumpTimer++;
  float t = (float)m_JumpTimer / (float)m_JumpDuration;
  if (t > 1.0f) t = 1.0f;

  // XZ平面の線形補間
  m_Position.x = m_JumpStartPos.x + (m_JumpTargetPos.x - m_JumpStartPos.x) * t;
  m_Position.z = m_JumpStartPos.z + (m_JumpTargetPos.z - m_JumpStartPos.z) * t;

  // Y軸の放物線跳躍（頂点高度 +14.0f）
  float arcY = sinf(t * DirectX::XM_PI) * 14.0f;
  m_Position.y = Constants::Boss::GROUND_Y + arcY;

  // 跳躍中のスカッシュ＆ストレッチ演出
  float stretch = sinf(t * DirectX::XM_PI);
  m_Scale.y = 5.0f + 1.5f * stretch;
  m_Scale.x = m_Scale.z = 5.0f - 0.5f * stretch;

  // プレイヤーの方向へ滑らかに向き直す
  Player *player = Manager::GetGameObject<Player>();
  if (player) {
    XMFLOAT3 toPlayer = player->GetPosition() - m_Position;
    toPlayer.y = 0.0f;
    if (MathHelper::Length(toPlayer) > 0.001f) {
      XMFLOAT3 dir = MathHelper::Normalize(toPlayer);
      m_Rotation.y = atan2f(dir.x, dir.z);
    }
  }

  // 着地（ジャンプ完了）
  if (m_JumpTimer >= m_JumpDuration) {
    m_Position = m_JumpTargetPos;
    m_Scale = XMFLOAT3(5.0f, 5.0f, 5.0f);

    // 着地時の巨大衝撃波エフェクト（ダメージなし・迫力ある波紋）
    XMFLOAT3 slamPos = m_Position;
    slamPos.y = -0.95f;
    ShockwaveSystem::AddShockwave(slamPos, 22.0f, 3.0f, 0.5f, 0.0f, 35, 0.0f, 0, false);
    ShockwaveSystem::AddShockwave(slamPos, 14.0f, 3.0f, 0.8f, 0.0f, 25, 0.0f, 0, false);

    if (g_Camera) {
      g_Camera->Shake(0.5f, 15);
    }

    // 着地完了！定位置での特殊攻撃フェーズを開始
    m_BossState = BossState::PHASE_TRANSITION;
    m_PhaseAttackTimer = 0;
  }
}

// ─────────────────────────────────────────────
// ボスAI制御
// ─────────────────────────────────────────────
void BossEnemy::UpdateBossAI() {
  Player *player = Manager::GetGameObject<Player>();
  if (!player)
    return;

  XMFLOAT3 playerPos = player->GetPosition();
  XMFLOAT3 bossPos = m_Position;

  XMFLOAT3 toPlayer = playerPos - bossPos;
  toPlayer.y = 0.0f;
  float dist = MathHelper::Length(toPlayer);

  // ボスはその場から動かないが、常にプレイヤーの方を向く
  if (dist > 0.001f) {
    XMFLOAT3 dir = MathHelper::Normalize(toPlayer);
    m_Rotation.y = atan2f(dir.x, dir.z);
  }
  m_Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

  // HP/フェーズに応じた動的攻撃間隔の算出
  int requiredInterval = Constants::Boss::ATTACK_INTERVAL_BASE;
  if (m_PhaseIndex >= 3 || m_HP <= Constants::Boss::PHASE3_HP_THRESHOLD) {
    requiredInterval = Constants::Boss::ATTACK_INTERVAL_PHASE3;
  } else if (m_PhaseIndex >= 2 || m_HP <= Constants::Boss::PHASE2_HP_THRESHOLD) {
    requiredInterval = Constants::Boss::ATTACK_INTERVAL_PHASE2;
  } else if (m_PhaseIndex >= 1 || m_HP <= Constants::Boss::PHASE1_HP_THRESHOLD) {
    requiredInterval = Constants::Boss::ATTACK_INTERVAL_PHASE1;
  }

  m_AttackTimer++;
  if (m_AttackTimer >= requiredInterval) {
    m_AttackTimer = 0;

    // プレイヤーが近距離にいる場合: 迎撃として大ジャンプスラムまたは突進を優先
    if (dist < Constants::Boss::CLOSE_RANGE_THRESHOLD) {
      int r = rand() % 100;
      if (r < 50) {
        OutputDebugStringA("[BossEnemy] Close-range counter: Slam Jump Attack.\n");
        StartSlamAttack();
      } else if (r < 80) {
        OutputDebugStringA("[BossEnemy] Close-range counter: Charge Tackle Attack.\n");
        StartChargeAttack();
      } else {
        OutputDebugStringA("[BossEnemy] Close-range counter: 3-Way Spread Bullet.\n");
        Fire3WaySpread();
      }
    } else {
      // 中・遠距離の場合: 順繰りに多彩な攻撃パターンを実行
      m_AttackPattern = (m_AttackPattern + 1) % 4;
      if (m_AttackPattern == 0) {
        OutputDebugStringA("[BossEnemy] Fire 3-Way Spread Bullet.\n");
        Fire3WaySpread();
      } else if (m_AttackPattern == 1) {
        OutputDebugStringA("[BossEnemy] Fire Rapid Bullet.\n");
        FireRapidShot();
      } else if (m_AttackPattern == 2) {
        OutputDebugStringA("[BossEnemy] Start Charge Tackle Attack.\n");
        StartChargeAttack();
      } else {
        OutputDebugStringA("[BossEnemy] Start Slam Jump Attack.\n");
        StartSlamAttack();
      }
    }
  }
}

// ─────────────────────────────────────────────
// 突進タックルの開始
// ─────────────────────────────────────────────
void BossEnemy::StartChargeAttack() {
  m_BossState = BossState::CHARGE_WINDUP;
  m_ChargeTimer = 0;
  m_ChargeTraveled = 0.0f;
  m_HasDamagedPlayerInCharge = false;

  Player *player = Manager::GetGameObject<Player>();
  if (player) {
    XMFLOAT3 toPlayer = player->GetPosition() - m_Position;
    toPlayer.y = 0.0f;
    if (MathHelper::Length(toPlayer) > 0.001f) {
      m_ChargeDir = MathHelper::Normalize(toPlayer);
      m_Rotation.y = atan2f(m_ChargeDir.x, m_ChargeDir.z);
    }
  }
}

// ─────────────────────────────────────────────
// 突進タメ予兆処理（地面を擦るスクワット姿勢と方向ロック）
// ─────────────────────────────────────────────
void BossEnemy::UpdateChargeWindup() {
  m_ChargeTimer++;
  float progress = (float)m_ChargeTimer / (float)Constants::Boss::CHARGE_WINDUP_FRAMES;
  if (progress > 1.0f) progress = 1.0f;

  Player *player = Manager::GetGameObject<Player>();

  // タメ前半（進捗80%まで）はプレイヤーを追尾して向き直る（残り20%で突進方向をロック！）
  if (progress < 0.8f && player) {
    XMFLOAT3 toPlayer = player->GetPosition() - m_Position;
    toPlayer.y = 0.0f;
    if (MathHelper::Length(toPlayer) > 0.001f) {
      m_ChargeDir = MathHelper::Normalize(toPlayer);
      m_Rotation.y = atan2f(m_ChargeDir.x, m_ChargeDir.z);
    }
  }

  // 体を低く沈み込ませて力を溜めるスクワット演出（Yを縮め、XZを太らせる）
  float squat = sinf(progress * DirectX::XM_PIDIV2);
  m_Scale.y = 5.0f - 1.6f * squat; // 5.0 -> 3.4
  m_Scale.x = m_Scale.z = 5.0f + 0.6f * squat; // 5.0 -> 5.6

  // 足元から地面を擦る摩擦エフェクト（10フレームごとに小さな収縮波紋）
  if (m_ChargeTimer % 10 == 0) {
    XMFLOAT3 footPos = m_Position;
    footPos.y = -0.95f;
    ShockwaveSystem::AddShockwave(footPos, 6.0f, 2.0f, 0.2f, 0.0f, 15, 0.0f, 0, true);
  }

  // タメ完了 → 猛突進発進！
  if (m_ChargeTimer >= Constants::Boss::CHARGE_WINDUP_FRAMES) {
    m_BossState = BossState::CHARGE_RUSH;
    m_ChargeTimer = 0;
    m_ChargeTraveled = 0.0f;
    m_HasDamagedPlayerInCharge = false;

    // 発進時の土煙大衝撃波
    XMFLOAT3 dashStartPos = m_Position;
    dashStartPos.y = -0.95f;
    ShockwaveSystem::AddShockwave(dashStartPos, 8.0f, 2.5f, 0.0f, 0.0f, 20, 1.0f, 0, false);

    if (g_Camera) {
      g_Camera->Shake(0.3f, 10);
    }
  }
}

// ─────────────────────────────────────────────
// 突進中処理（高速移動・プレイヤー直撃判定・壁激突判定）
// ─────────────────────────────────────────────
void BossEnemy::UpdateChargeRush() {
  m_ChargeTimer++;

  // 前傾姿勢で突進（勢いのある形状）
  m_Scale = XMFLOAT3(4.6f, 4.4f, 5.8f);

  // 突進移動（フェーズが進むごとに突進スピードが向上）
  float speed = Constants::Boss::CHARGE_SPEED * (1.0f + 0.1f * (float)m_PhaseIndex);
  m_Position.x += m_ChargeDir.x * speed;
  m_Position.z += m_ChargeDir.z * speed;
  m_ChargeTraveled += speed;

  // 突進の土煙軌跡エフェクト（6フレームごと）
  if (m_ChargeTimer % 6 == 0) {
    XMFLOAT3 trailPos = m_Position;
    trailPos.y = -0.95f;
    ShockwaveSystem::AddShockwave(trailPos, 4.5f, 1.8f, 0.2f, 0.0f, 12, 0.0f, 0, false);
  }

  // プレイヤー衝突判定
  Player *player = Manager::GetGameObject<Player>();
  if (player && !m_HasDamagedPlayerInCharge) {
    XMFLOAT3 pPos = player->GetPosition();
    float dx = pPos.x - m_Position.x;
    float dz = pPos.z - m_Position.z;
    float dist = sqrtf(dx * dx + dz * dz);

    if (dist <= Constants::Boss::CHARGE_HIT_RADIUS && !player->IsInvincible()) {
      HitInfo hitInfo;
      hitInfo.damage = Constants::Boss::CHARGE_DAMAGE;
      hitInfo.hitSourcePos = m_Position;
      hitInfo.knockbackVel = XMFLOAT3(m_ChargeDir.x * 0.45f, 0.0f, m_ChargeDir.z * 0.45f);
      player->OnHit(hitInfo);

      SoundManager::PlaySE(SoundID::SE_PLAYER_HIT);
      if (g_Camera) g_Camera->Shake(0.45f, 12);

      m_HasDamagedPlayerInCharge = true;
    }
  }

  // 壁衝突判定（部屋サイズ 28.0f、ボスの半幅 2.5f）
  float wallLimit = Constants::Stage::BOSS_ROOM_SIZE - 2.5f;
  bool hitWall = false;

  if (m_Position.x >= wallLimit)  { m_Position.x = wallLimit;  hitWall = true; }
  if (m_Position.x <= -wallLimit) { m_Position.x = -wallLimit; hitWall = true; }
  if (m_Position.z >= wallLimit)  { m_Position.z = wallLimit;  hitWall = true; }
  if (m_Position.z <= -wallLimit) { m_Position.z = -wallLimit; hitWall = true; }

  // 壁激突！大衝撃波＆スタン移行
  if (hitWall) {
    XMFLOAT3 crashPos = m_Position;
    crashPos.y = 0.5f;
    ShockwaveSystem::AddShockwave(crashPos, 16.0f, 3.0f, 0.0f, 0.0f, 30, 2.5f, 0, false);

    SoundManager::PlaySE(SoundID::SE_HIT_WALL);
    if (g_Camera) g_Camera->Shake(0.6f, 20);

    // 壁から少し跳ね返る
    m_Position.x -= m_ChargeDir.x * 1.5f;
    m_Position.z -= m_ChargeDir.z * 1.5f;

    // スタン状態へ移行（攻撃チャンス！）
    m_BossState = BossState::CHARGE_STUN;
    m_ChargeTimer = 0;
    m_Scale = XMFLOAT3(5.5f, 3.8f, 5.5f); // 激突で潰れる
    return;
  }

  // 最大突進距離に達したら自然減速・通常復帰
  if (m_ChargeTraveled >= Constants::Boss::CHARGE_MAX_DISTANCE) {
    m_BossState = BossState::NORMAL;
    m_Scale = XMFLOAT3(5.0f, 5.0f, 5.0f);
    m_AttackTimer = 0;
  }
}

// ─────────────────────────────────────────────
// 壁激突後のスタン（気絶・無防備隙）処理
// ─────────────────────────────────────────────
void BossEnemy::UpdateChargeStun() {
  m_ChargeTimer++;

  // 気絶中の小刻みなピヨピヨ揺れ演出
  float wobble = sinf((float)m_ChargeTimer * 0.4f) * 0.15f;
  m_Rotation.z = wobble;
  m_Scale.y = 4.0f + 0.3f * sinf((float)m_ChargeTimer * 0.2f);
  m_Scale.x = m_Scale.z = 5.2f - 0.2f * sinf((float)m_ChargeTimer * 0.2f);

  // 15フレームごとに気絶の星/電気火花エフェクト
  if (m_ChargeTimer % 15 == 0) {
    XMFLOAT3 sparkPos = m_Position;
    sparkPos.y += 2.5f;
    ShockwaveSystem::AddShockwave(sparkPos, 3.5f, 2.0f, 2.0f, 0.0f, 15, 0.0f, 0, false);
  }

  // スタン終了 → 体勢を立て直して通常AIへ復帰！
  if (m_ChargeTimer >= Constants::Boss::CHARGE_STUN_FRAMES) {
    m_BossState = BossState::NORMAL;
    m_Rotation.z = 0.0f;
    m_Scale = XMFLOAT3(5.0f, 5.0f, 5.0f);
    m_AttackTimer = 0; // 次の行動へのクールダウン開始
  }
}

// ─────────────────────────────────────────────
// 大ジャンプ・グラウンドスラムの開始
// ─────────────────────────────────────────────
void BossEnemy::StartSlamAttack() {
  m_BossState = BossState::SLAM_WINDUP;
  m_SlamTimer = 0;
  m_SlamStartPos = m_Position;

  Player *player = Manager::GetGameObject<Player>();
  if (player) {
    m_SlamTargetPos = player->GetPosition();
    m_SlamTargetPos.y = Constants::Boss::GROUND_Y;

    // 壁の外に出ないように安全マージンでクランプ
    float limit = Constants::Stage::BOSS_ROOM_SIZE - 4.0f;
    m_SlamTargetPos.x = std::clamp(m_SlamTargetPos.x, -limit, limit);
    m_SlamTargetPos.z = std::clamp(m_SlamTargetPos.z, -limit, limit);
  } else {
    m_SlamTargetPos = m_Position;
  }
}

// ─────────────────────────────────────────────
// 大ジャンプ前のタメ予兆処理（膝を深く曲げる圧縮姿勢）
// ─────────────────────────────────────────────
void BossEnemy::UpdateSlamWindup() {
  m_SlamTimer++;
  float progress = (float)m_SlamTimer / (float)Constants::Boss::SLAM_WINDUP_FRAMES;
  if (progress > 1.0f) progress = 1.0f;

  // 体を強く沈み込ませる（5.0 -> 2.8）
  float squat = sinf(progress * DirectX::XM_PIDIV2);
  m_Scale.y = 5.0f - 2.2f * squat;
  m_Scale.x = m_Scale.z = 5.0f + 1.0f * squat;

  // 足元に収縮衝撃波（エネルギー吸引演出）
  if (m_SlamTimer % 8 == 0) {
    XMFLOAT3 footPos = m_Position;
    footPos.y = -0.95f;
    ShockwaveSystem::AddShockwave(footPos, 7.0f, 2.0f, 0.3f, 0.0f, 15, 0.0f, 0, true);
  }

  // タメ完了 → 上空へ大ジャンプ発進！
  if (m_SlamTimer >= Constants::Boss::SLAM_WINDUP_FRAMES) {
    m_BossState = BossState::SLAM_AIR;
    m_SlamTimer = 0;
    m_SlamStartPos = m_Position;

    // 踏み切り大土煙
    XMFLOAT3 launchPos = m_Position;
    launchPos.y = -0.95f;
    ShockwaveSystem::AddShockwave(launchPos, 9.0f, 3.0f, 0.0f, 0.0f, 25, 0.5f, 0, false);
    SoundManager::PlaySE(SoundID::SE_THROW, 1.2f);
  }
}

// ─────────────────────────────────────────────
// 上空への滞空・着地地点狙い・急降下スラム処理
// ─────────────────────────────────────────────
void BossEnemy::UpdateSlamAir() {
  m_SlamTimer++;

  Player *player = Manager::GetGameObject<Player>();

  // フェーズA: 急上昇（指定フレームで滞空高度へ打ち上げ）
  if (m_SlamTimer <= Constants::Boss::SLAM_RISE_FRAMES) {
    float t = (float)m_SlamTimer / (float)Constants::Boss::SLAM_RISE_FRAMES;
    m_Position.y = Constants::Boss::GROUND_Y + sinf(t * DirectX::XM_PIDIV2) * (Constants::Boss::SLAM_JUMP_HEIGHT - Constants::Boss::GROUND_Y);
    m_Scale.y = 6.5f; // 縦長に伸びる
    m_Scale.x = m_Scale.z = 4.0f;
    return;
  }

  // フェーズB: 滞空・追尾と着地予兆サークル表示（落下前まで）
  if (m_SlamTimer <= 45) {
    m_Position.y = Constants::Boss::SLAM_JUMP_HEIGHT;
    m_Scale = XMFLOAT3(5.0f, 5.0f, 5.0f);

    // プレイヤーの位置を追従（着地予測地点の更新、一定フレームでターゲット位置を確定）
    if (player && m_SlamTimer <= Constants::Boss::SLAM_LOCK_TARGET_FRAMES) {
      XMFLOAT3 pPos = player->GetPosition();
      float limit = Constants::Stage::BOSS_ROOM_SIZE - 4.0f;
      pPos.x = std::clamp(pPos.x, -limit, limit);
      pPos.z = std::clamp(pPos.z, -limit, limit);

      // 徐々にプレイヤー位置へターゲットを移動
      m_SlamTargetPos.x += (pPos.x - m_SlamTargetPos.x) * 0.15f;
      m_SlamTargetPos.z += (pPos.z - m_SlamTargetPos.z) * 0.15f;
      m_SlamTargetPos.y = Constants::Boss::GROUND_Y;
    }

    // ボス本体のXZ座標も目標地点へ空中移動
    m_Position.x += (m_SlamTargetPos.x - m_Position.x) * 0.2f;
    m_Position.z += (m_SlamTargetPos.z - m_Position.z) * 0.2f;

    // 着地目標地点の床面に赤い警告サークル（落下地点インジケータ）
    if (m_SlamTimer % 5 == 0) {
      XMFLOAT3 warnPos = m_SlamTargetPos;
      warnPos.y = -0.95f;
      ShockwaveSystem::AddShockwave(warnPos, Constants::Boss::SLAM_SHOCKWAVE_RADIUS, 2.0f, 0.0f, 0.0f, 15, 0.0f, 0, false);
    }
    return;
  }

  // フェーズC: 急降下（46フレーム〜着地まで）
  m_Position.x = m_SlamTargetPos.x;
  m_Position.z = m_SlamTargetPos.z;
  m_Position.y -= Constants::Boss::SLAM_FALL_SPEED * 1.5f;

  // 落下時の高速ストレッチ演出
  m_Scale.y = 7.5f;
  m_Scale.x = m_Scale.z = 3.8f;

  // 地面着地判定（グラウンドスラム発動！）
  if (m_Position.y <= Constants::Boss::GROUND_Y) {
    m_Position.y = Constants::Boss::GROUND_Y;

    // 1. 巨大地響きリング衝撃波（ジャンプで回避可能な全方位波）
    XMFLOAT3 slamCenter = m_Position;
    slamCenter.y = -0.95f;
    ShockwaveSystem::AddShockwave(
        slamCenter,
        Constants::Boss::SLAM_SHOCKWAVE_RADIUS,
        2.5f,
        0.35f,
        0.0f,
        25,
        1.5f,
        0,
        false
    );

    // 2. 着地激突音＆カメラシェイク
    SoundManager::PlaySE(SoundID::SE_HIT_WALL, 1.3f);
    if (g_Camera) {
      g_Camera->Shake(0.55f, 22);
    }

    // 3. プレイヤーへの着地衝撃ダメージ判定（プレイヤーがジャンプ中なら回避成功！）
    if (player) {
      XMFLOAT3 pPos = player->GetPosition();
      XMFLOAT3 diff = pPos - m_Position;
      diff.y = 0.0f;
      float dist = MathHelper::Length(diff);

      // 半径以内 かつ プレイヤーが地上（ジャンプでかわせるよう Y < 0.6f）
      if (dist <= Constants::Boss::SLAM_SHOCKWAVE_RADIUS && pPos.y < 0.6f) {
        HitInfo hit;
        hit.damage = Constants::Boss::SLAM_SHOCKWAVE_DAMAGE;
        hit.hitSourcePos = m_Position;
        XMFLOAT3 kbDir = (dist > 0.001f) ? MathHelper::Normalize(diff) : XMFLOAT3(0.0f, 0.0f, 1.0f);
        hit.knockbackVel = XMFLOAT3(kbDir.x * 0.45f, 0.35f, kbDir.z * 0.45f);
        player->OnHit(hit);
        SoundManager::PlaySE(SoundID::SE_PLAYER_HIT);
      }
    }

    // 4. 着地衝撃で体が大きく潰れる（スカッシュ）
    m_Scale.y = 2.6f;
    m_Scale.x = m_Scale.z = 6.6f;

    // 5. スラム後硬直（リカバリー）へ移行
    m_BossState = BossState::SLAM_RECOVERY;
    m_SlamTimer = 0;
  }
}

// ─────────────────────────────────────────────
// 着地グラウンドスラム後の隙（硬直リカバリー）処理
// ─────────────────────────────────────────────
void BossEnemy::UpdateSlamRecovery() {
  m_SlamTimer++;

  // 潰れた体勢から徐々に本来の 5.0x5.0x5.0 へ復元
  float progress = (float)m_SlamTimer / (float)Constants::Boss::SLAM_RECOVERY_FRAMES;
  if (progress > 1.0f) progress = 1.0f;

  m_Scale.y = 2.6f + 2.4f * progress;
  m_Scale.x = m_Scale.z = 6.6f - 1.6f * progress;

  // 硬直終了 → 通常AIへ復帰
  if (m_SlamTimer >= Constants::Boss::SLAM_RECOVERY_FRAMES) {
    m_BossState = BossState::NORMAL;
    m_Scale = XMFLOAT3(5.0f, 5.0f, 5.0f);
    m_AttackTimer = 0;
  }
}

// ─────────────────────────────────────────────
// 3方向拡散弾の発射
// ─────────────────────────────────────────────
void BossEnemy::Fire3WaySpread() {
  Player *player = Manager::GetGameObject<Player>();
  if (!player)
    return;

  XMFLOAT3 bulletPos = m_Position;
  bulletPos.y += 1.5f; // 巨大ボスの中心付近から

  XMFLOAT3 toPlayer = player->GetPosition() - bulletPos;
  float dist = MathHelper::Length(toPlayer);
  if (dist < 0.01f)
    return;
  XMFLOAT3 baseDir = MathHelper::Normalize(toPlayer);

  float angles[3] = {0.0f, -0.26f, 0.26f}; // 中央、左右約15度
  for (int i = 0; i < 3; i++) {
    EnemyBullet *bullet = Manager::AddGameObject<EnemyBullet>();
    bullet->SetPosition(bulletPos);

    float angle = angles[i];
    float rotX = baseDir.x * cosf(angle) - baseDir.z * sinf(angle);
    float rotZ = baseDir.x * sinf(angle) + baseDir.z * cosf(angle);

    bullet->SetDirection(XMFLOAT3(rotX, baseDir.y, rotZ));
    bullet->SetSpeed(0.18f);
    bullet->SetScale(XMFLOAT3(1.5f, 1.5f, 1.5f));
    bullet->SetIsBossBullet(true); // ボスフラグを設定
  }

  if (g_Camera)
    g_Camera->Shake(0.15f, 8);
}

// ─────────────────────────────────────────────
// 高速連射弾（擬似連射）の発射
// ─────────────────────────────────────────────
void BossEnemy::FireRapidShot() {
  Player *player = Manager::GetGameObject<Player>();
  if (!player)
    return;

  XMFLOAT3 bulletPos = m_Position;
  bulletPos.y += 1.5f;

  XMFLOAT3 toPlayer = player->GetPosition() - bulletPos;
  XMFLOAT3 dir = MathHelper::Normalize(toPlayer);

  // 速度差で擬似的な3連射を表現
  float speeds[3] = {0.12f, 0.16f, 0.20f};
  for (int i = 0; i < 3; i++) {
    EnemyBullet *bullet = Manager::AddGameObject<EnemyBullet>();
    bullet->SetPosition(bulletPos);
    bullet->SetDirection(dir);
    bullet->SetSpeed(speeds[i]);
    bullet->SetScale(XMFLOAT3(1.2f, 1.2f, 1.2f));
    bullet->SetIsBossBullet(true); // ボスフラグを設定
  }

  if (g_Camera)
    g_Camera->Shake(0.12f, 8);
}

void BossEnemy::OnHit(const HitInfo &info) {
  ApplyBossDamage(info.damage, info.hitSourcePos);
}

// ボス被弾ダメージ処理
// ─────────────────────────────────────────────
void BossEnemy::ApplyBossDamage(int damage,
                                const DirectX::XMFLOAT3 &hitSourcePos) {
  if (m_EnemyState == EnemyState::DEFEATED ||
      m_EnemyState == EnemyState::BLOWN_AWAY)
    return;

  if (m_IsInvincible) {
    // 無敵中はダメージを無効化し、バリアに弾かれた電気火花エフェクトを発生
    Player *player = Manager::GetGameObject<Player>();
    if (player) {
      XMFLOAT3 hitPos = hitSourcePos;
      // 被弾位置からランダムな方向に6本の短いイナズマを飛ばす
      for (int i = 0; i < 6; i++) {
        float rx = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
        float ry = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
        float rz = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
        XMFLOAT3 sparkEnd =
            XMFLOAT3(hitPos.x + rx, hitPos.y + ry, hitPos.z + rz);
        player->DrawLightningBolt(hitPos, sparkEnd, 0.02f,
                                  XMFLOAT4(0.0f, 1.8f, 2.5f, 1.0f));
      }
    }

    if (g_Camera)
      g_Camera->Shake(0.12f, 6);

    // ダメージ数値 "0" のポップアップ（フェーズごとのバリア色に合わせる）
    float r = 0.0f, g = 1.5f, b = 2.5f; // デフォルト：青
    if (m_PhaseIndex == 2) {
      r = 2.5f;
      g = 2.0f;
      b = 0.0f; // 黄色
    } else if (m_PhaseIndex == 3) {
      r = 2.5f;
      g = 0.0f;
      b = 0.0f; // 赤色
    }
    ScorePopupSystem::AddPopup(m_Position.x, m_Position.y + 3.0f, m_Position.z,
                               0, r, g, b);
    return;
  }

  m_HP -= damage;
  m_DamageFlashTimer = 15;

  BossHitEvent hitEvent;
  hitEvent.damage = damage;
  hitEvent.hitSourcePos = hitSourcePos;
  EventSystem::Publish<BossHitEvent>(hitEvent);

  // 巨大なので少しだけノックバック
  XMFLOAT3 diff = m_Position - hitSourcePos;
  diff.y = 0.0f;
  float dist = MathHelper::Length(diff);
  if (dist > 0.001f) {
    XMFLOAT3 pushDir = diff / dist;
    m_Position.x += pushDir.x * 0.3f;
    m_Position.z += pushDir.z * 0.3f;
  }

  // ── オーバーダメージ保護：未発動フェーズがある場合はHPをその閾値でクランプ
  // ──
  if (m_BossState == BossState::NORMAL) {
    for (auto &phase : m_Phases) {
      if (!phase.triggered && m_HP < phase.hpThreshold) {
        m_HP = phase.hpThreshold;
        break;
      }
    }
  }

  // 残りHPポップアップ (赤文字)
  ScorePopupSystem::AddPopup(m_Position.x, m_Position.y + 3.0f, m_Position.z,
                             m_HP, 2.5f, 0.0f, 0.0f);

  if (m_HP <= 0) {
    m_HP = 0;
    m_EnemyState = EnemyState::DEFEATED;
    m_Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

    // 撃破時に多重衝撃波を発生
    XMFLOAT3 deathPos = m_Position;
    deathPos.y = -0.95f;
    ShockwaveSystem::AddShockwave(deathPos, 18.0f, 2.5f, 0.0f, 0.0f, 40, 3.0f,
                                  0);
    ShockwaveSystem::AddShockwave(deathPos, 12.0f, 2.5f, 1.5f, 0.0f, 30, 2.0f,
                                  10);
    ShockwaveSystem::AddShockwave(deathPos, 6.0f, 1.8f, 2.2f, 0.0f, 20, 1.0f,
                                  20);

    if (g_Camera)
      g_Camera->Shake(0.85f, 30);

    // ボス撃破処理（赤色ポップアップ）
    Defeat(2.5f, 0.0f, 0.0f);

    // ボス撃破の瞬間にプレイヤーの低HP警告（画面の赤点滅）を即座に解除
    Manager::SetLowHPWarning(false);
  }
}

// ─────────────────────────────────────────────
// 特別攻撃パターンの制御
// ─────────────────────────────────────────────
void BossEnemy::PerformPhaseAttack() {
  m_PhaseAttackTimer++;

  // フェーズ攻撃関数のテーブル (データ駆動)
  typedef void (BossEnemy::*PhaseAttackFunc)();
  static const PhaseAttackFunc PHASE_ATTACK_TABLE[] = {
      nullptr, // 0番目は未使用 (m_PhaseIndex が 1-indexed のため)
      &BossEnemy::PerformPhase1Attack, &BossEnemy::PerformPhase2Attack,
      &BossEnemy::PerformPhase3Attack};

  const int numPhases =
      sizeof(PHASE_ATTACK_TABLE) / sizeof(PHASE_ATTACK_TABLE[0]);
  if (m_PhaseIndex >= 1 && m_PhaseIndex < numPhases) {
    PhaseAttackFunc func = PHASE_ATTACK_TABLE[m_PhaseIndex];
    if (func) {
      (this->*func)();
    }
  }
}

// ─────────────────────────────────────────────
// フェーズ1特別攻撃: 360度サークル弾幕 (持続時間: 400フレーム)
// ─────────────────────────────────────────────
BossPhase1ChargeInfo BossEnemy::GetPhase1ChargeInfo() const {
  BossPhase1ChargeInfo info = {};
  info.isCharging = false;
  info.relativeTimer = 0;
  info.angleOffset = 0.0f;

  if (m_PhaseAttackTimer >= 45 && m_PhaseAttackTimer < 60) {
    info.isCharging = true;
    info.relativeTimer = m_PhaseAttackTimer - 45;
    info.angleOffset = 0.0f;
  } else if (m_PhaseAttackTimer >= 125 && m_PhaseAttackTimer < 140) {
    info.isCharging = true;
    info.relativeTimer = m_PhaseAttackTimer - 125;
    info.angleOffset = (DirectX::XM_2PI / 24) * 0.3f;
  } else if (m_PhaseAttackTimer >= 205 && m_PhaseAttackTimer < 220) {
    info.isCharging = true;
    info.relativeTimer = m_PhaseAttackTimer - 205;
    info.angleOffset = (DirectX::XM_2PI / 24) * 0.6f;
  } else if (m_PhaseAttackTimer >= 285 && m_PhaseAttackTimer < 300) {
    info.isCharging = true;
    info.relativeTimer = m_PhaseAttackTimer - 285;
    info.angleOffset = (DirectX::XM_2PI / 24) * 0.9f;
  }

  return info;
}

// ─────────────────────────────────────────────
// フェーズ1特別攻撃: 360度サークル弾幕 (持続時間: 400フレーム)
// ─────────────────────────────────────────────
void BossEnemy::PerformPhase1Attack() {
  // プレイヤーの高さに合わせる（頭上通過防止・チャージ波紋の発生高さにも使用）
  float bulletY = -0.2f;
  Player *player = Manager::GetGameObject<Player>();
  if (player) {
    bulletY = player->GetPosition().y + 0.3f;
  }
  XMFLOAT3 bulletPos = m_Position;
  bulletPos.y = bulletY;

  // ヘルパーから一括してチャージ情報を取得（コードのコピペ・二重定義を防止）
  BossPhase1ChargeInfo chargeInfo = GetPhase1ChargeInfo();

  if (chargeInfo.isCharging) {
    float chargeT = chargeInfo.relativeTimer / 15.0f; // 0.0 -> 1.0
    // ボスをタメで少し縮ませる（Yを縮め、XZを太らせる）
    m_Scale.y = 5.0f - 1.0f * sinf(chargeT * DirectX::XM_PIDIV2); // 5.0 -> 4.0
    m_Scale.x = m_Scale.z =
        5.0f + 0.3f * sinf(chargeT * DirectX::XM_PIDIV2); // 5.0 -> 5.3

    // 吸引開始フレーム（relativeTimer == 0）でエフェクトを追加
    if (chargeInfo.relativeTimer == 0) {
      // 半径を 18.0f / 12.0f に拡大し、高さを弾幕に合わせる
      ShockwaveSystem::AddShockwave(bulletPos, 18.0f, 1.5f, 0.0f, 2.0f, 15,
                                    0.0f, 0, true);
      ShockwaveSystem::AddShockwave(bulletPos, 12.0f, 1.5f, 0.0f, 2.0f, 15,
                                    0.0f, 0, true);
    }
  } else {
    // 通常時（発射時含む）は元のサイズに戻す
    m_Scale = XMFLOAT3(5.0f, 5.0f, 5.0f);
  }

  // 60, 140, 220, 300f で発射 (計4回)
  if (m_PhaseAttackTimer == 60 || m_PhaseAttackTimer == 140 ||
      m_PhaseAttackTimer == 220 || m_PhaseAttackTimer == 300) {
    const int numBullets = 24;

    // 掃射回数ごとに弾幕の角度をずらして、安全地帯が毎回変わるようにする
    float startAngleOffset = 0.0f;
    if (m_PhaseAttackTimer == 140) {
      startAngleOffset = (XM_2PI / numBullets) * 0.3f; // 隙間の1/4ずらす
    } else if (m_PhaseAttackTimer == 220) {
      startAngleOffset = (XM_2PI / numBullets) * 0.6f; // 隙間の1/2ずらす
    } else if (m_PhaseAttackTimer == 300) {
      startAngleOffset = (XM_2PI / numBullets) * 0.9f; // 隙間の3/4ずらす
    }

    for (int i = 0; i < numBullets; i++) {
      EnemyBullet *bullet = Manager::AddGameObject<EnemyBullet>();
      if (bullet) {
        bullet->SetPosition(bulletPos);
        float angle = i * (XM_2PI / numBullets) + startAngleOffset;
        bullet->SetDirection(XMFLOAT3(cosf(angle), 0.0f, sinf(angle)));
        bullet->SetSpeed(0.12f);
        bullet->SetScale(XMFLOAT3(1.2f, 1.2f, 1.2f));
        bullet->SetLife(300); // 寿命を長く（5秒）設定して消えにくくする
      }
    }
    if (g_Camera)
      g_Camera->Shake(0.2f, 10);
  }

  if (m_PhaseAttackTimer >= 400) {
    m_BossState = BossState::NORMAL;
    m_IsInvincible = false;
    m_PhaseAttackTimer = 0;
    OutputDebugStringA("[BossEnemy] Phase 1 Attack Finished.\n");
  }
}

// ─────────────────────────────────────────────
// フェーズ2特別攻撃: 縄跳び回転ラインマーカー (持続時間: 720フレーム = 12.0秒)
// ─────────────────────────────────────────────
void BossEnemy::PerformPhase2Attack() {
  Player *player = Manager::GetGameObject<Player>();
  if (!player)
    return;

  m_PhaseAttackTimer++;

  // 予兆時間（最初の60フレーム）は回転させず、判定もスキップ
  if (m_PhaseAttackTimer <= 60) {
    return;
  }

  // 60フレーム以降から回転を開始
  float angle = (m_PhaseAttackTimer - 60) * 0.015f;

  // プレイヤーへの衝突判定
  XMFLOAT3 pPos = player->GetPosition();
  float playerAngle = atan2f(pPos.z - m_Position.z, pPos.x - m_Position.x);
  float dist = MathHelper::Length(
      XMFLOAT3(pPos.x - m_Position.x, 0.0f, pPos.z - m_Position.z));

  // ─────────────────────────────────────────────────────────────────────────
  // 衝突判定: 「プレイヤー座標 → 棒の直線」への垂直距離で判定する
  //
  // 描画コードの変換順:
  //   Scale(0.5, 0.5, 30) → Translation(0,0,15) → RotationY(angle) →
  //   Translation(boss) RotationY(angle) でのZ軸基底: (sin(angle), 0,
  //   cos(angle)) → 棒の延びる方向は (sin(angle), 0, cos(angle)) ←
  //   XM_PIDIV2補正は不要
  //
  // 棒のビジュアル幅: Scale.x = 0.5f → 半径 = 0.25f → HIT_THRESHOLD = 0.25f
  // ─────────────────────────────────────────────────────────────────────────

  // 棒の向き単位ベクトル: 描画の RotationY(angle) と同じ向き
  float barDirX = sinf(angle); // RotationY によるZ→X成分
  float barDirZ = cosf(angle); // RotationY によるZ→Z成分

  // プレイヤーのXZ平面でのボスからの相対ベクトル
  float relX = pPos.x - m_Position.x;
  float relZ = pPos.z - m_Position.z;

  // 棒の直線へのスカラー射影（-BAR_LENGTH ≦ proj ≦ BAR_LENGTH の範囲が棒の存在域）
  float proj = relX * barDirX + relZ * barDirZ;
  const float BAR_LENGTH = Constants::Boss::PHASE2_BAR_LENGTH;

  // 棒の直線への垂直距離（点と直線の最短距離）
  float perpX = relX - proj * barDirX;
  float perpZ = relZ - proj * barDirZ;
  float perpDist = sqrtf(perpX * perpX + perpZ * perpZ);

  // 判定しきい値: ビジュアルのX幅 = 0.5f → 半径 = 0.25f
  const float HIT_THRESHOLD = 0.25f;

  // 棒の長さ範囲内 かつ 垂直距離がしきい値以内 かつ 地上 かつ 無敵でない
  if (fabsf(proj) <= BAR_LENGTH && perpDist < HIT_THRESHOLD &&
      player->GetPosition().y <= 0.0f && !player->IsInvincible()) {
    // ─── デバッグ出力（ASCII のみ・バッファ512バイト）───
    char dbgBuf[512];
    sprintf_s(dbgBuf, sizeof(dbgBuf),
              "[BAR HIT] Player=(%.2f, %.2f, %.2f)  Boss=(%.2f, %.2f, %.2f)\n"
              "  barDir=(%.3f, %.3f)  proj=%.2f  perpDist=%.4f  dist2D=%.2f\n",
              pPos.x, pPos.y, pPos.z, m_Position.x, m_Position.y, m_Position.z,
              barDirX, barDirZ, proj, perpDist, dist);
    OutputDebugStringA(dbgBuf);

    HitInfo hitInfo;
    hitInfo.damage = 1;
    hitInfo.hitSourcePos = m_Position;
    player->OnHit(hitInfo);
    if (g_Camera)
      g_Camera->Shake(0.25f, 8);
  }

  if (m_PhaseAttackTimer >= 720) {
    m_BossState = BossState::NORMAL;
    m_IsInvincible = false;
    m_PhaseAttackTimer = 0;
    m_ActiveShockwaves.clear(); // リストのクリア
    OutputDebugStringA("[BossEnemy] Phase 2 Attack Finished.\n");
  }
}

// ─────────────────────────────────────────────
// フェーズ3特別攻撃: メテオストリーム ＆ 全方位狂乱乱射 (持続時間: 500フレーム)
// ─────────────────────────────────────────────
void BossEnemy::PerformPhase3Attack() {
  Player *player = Manager::GetGameObject<Player>();
  if (!player)
    return;

  // プレイヤーの高さに合わせる（頭上通過防止・チャージ波紋の発生高さにも使用）
  float bulletY = player->GetPosition().y + 0.3f;
  XMFLOAT3 bulletPos = m_Position;
  bulletPos.y = bulletY;

  // ─── 極大魔力チャージ（最初の60フレーム） ───
  if (m_PhaseAttackTimer < 60) {
    float t = m_PhaseAttackTimer / 60.0f; // 0.0 -> 1.0
    // ボスの体を押しつぶすタメ演出（Yを縮め、XZを太らせる）
    m_Scale.y = 5.0f - 1.8f * sinf(t * XM_PIDIV2);             // 5.0f -> 3.2f
    m_Scale.x = m_Scale.z = 5.0f + 0.6f * sinf(t * XM_PIDIV2); // 5.0f -> 5.6f

    // 10フレーム目に超巨大吸引エフェクトを発生（持続50f、弾幕発射位置
    // bulletPos）
    if (m_PhaseAttackTimer == 10) {
      // 超巨大な収縮サークル（半径28.0f / 18.0f）
      ShockwaveSystem::AddShockwave(bulletPos, 28.0f, 2.5f, 0.5f, 0.0f, 50,
                                    0.0f, 0, true);
      ShockwaveSystem::AddShockwave(bulletPos, 18.0f, 2.5f, 0.5f, 0.0f, 50,
                                    0.0f, 0, true);
    }
    return; // チャージ中は射撃処理を行わない
  }

  // 60フレーム目に突入した瞬間、力を開放してカメラをシェイク＆ボスサイズ復元
  if (m_PhaseAttackTimer == 60) {
    m_Scale = XMFLOAT3(5.0f, 5.0f, 5.0f);
    if (g_Camera)
      g_Camera->Shake(0.4f, 15);
  }

  // ─────────────────────────────────────────────────────────────────────────
  // 全方位弾幕（前半: 60〜330f） ─ 螺旋 ＋ 狙い撃ち 二層構造
  // (チャージ期間追加のため、元の30〜300fからそれぞれ30fシフト)
  // ─────────────────────────────────────────────────────────────────────────
  if (m_PhaseAttackTimer >= 60 && m_PhaseAttackTimer <= 330) {

    // ── [層1] スパイラル弾幕（6fに1回・8方向） ──
    if ((m_PhaseAttackTimer - 60) % 10 == 0) {
      int shotIndex = (m_PhaseAttackTimer - 60) / 6;
      float baseOffset = shotIndex * (XM_PI / 9.0f); // 毎ショット20度回転

      const int BULLET_COUNT = 8; // 8方向（45度刻み）
      const float ANGLE_STEP = XM_2PI / BULLET_COUNT;

      for (int i = 0; i < BULLET_COUNT; i++) {
        float fireAngle = baseOffset + ANGLE_STEP * i;
        EnemyBullet *bullet = Manager::AddGameObject<EnemyBullet>();
        if (bullet) {
          bullet->SetPosition(bulletPos);
          bullet->SetDirection(
              XMFLOAT3(cosf(fireAngle), 0.0f, sinf(fireAngle)));
          bullet->SetSpeed(0.24f);
          bullet->SetScale(XMFLOAT3(1.0f, 1.0f, 1.0f));
          bullet->SetLife(180);
        }
      }
    }

    // ── [層2] 狙い撃ち扇形3連弾（20fに1回） ──
    if ((m_PhaseAttackTimer - 60) % 20 == 0) {
      float aimAngle = atan2f(player->GetPosition().z - m_Position.z,
                              player->GetPosition().x - m_Position.x);

      float offsets[3] = {-0.087f, 0.0f, 0.087f}; // 中央 ＋ 左右5度
      for (int i = 0; i < 3; i++) {
        float a = aimAngle + offsets[i];
        EnemyBullet *bullet = Manager::AddGameObject<EnemyBullet>();
        if (bullet) {
          bullet->SetPosition(bulletPos);
          bullet->SetDirection(XMFLOAT3(cosf(a), 0.0f, sinf(a)));
          bullet->SetSpeed(0.32f); // 狙い弾は速め
          bullet->SetScale(XMFLOAT3(1.2f, 1.2f, 1.2f));
          bullet->SetLife(200);
        }
      }
    }
  }

  // ─────────────────────────────────────────────────────────────────────────
  // 落雷フェーズ（後半: 320〜550f） ─ 2種類の落雷を組み合わせ
  //
  // [落雷A] プレイヤー狙い撃ち: 90フレームに1回（猶予50f）
  // [落雷B] マップランダム多発: 40フレームに1回、3〜5箇所に同時に落雷警告
  // ─────────────────────────────────────────────────────────────────────────

  // ── [落雷A] プレイヤー狙い撃ち警告 ──
  int start = Constants::Boss::ATTACK_START_FRAME;
  int end = start + Constants::Boss::ATTACK_DURATION_FRAME;
  int warnFrames = Constants::Boss::STRIKE_WARNING_FRAMES;
  int intervalFrames = Constants::Boss::STRIKE_INTERVAL_FRAMES;

  if (m_PhaseAttackTimer >= start && m_PhaseAttackTimer <= end &&
      (m_PhaseAttackTimer - start) % intervalFrames == 0) {
    m_PhaseTargetPos = player->GetPosition();
    m_PhaseTargetPos.y = -0.95f;

    // 収縮する赤い警告サークルエフェクト（着弾猶予時間に同期）
    ShockwaveSystem::AddShockwave(m_PhaseTargetPos, 4.5f, 2.5f, 0.0f, 0.0f, warnFrames,
                                  0.0f, 0, true);
    ShockwaveSystem::AddShockwave(m_PhaseTargetPos, 3.0f, 2.5f, 0.0f, 0.0f, warnFrames - 15,
                                  0.0f, 15, true);
  }

  // 落雷A 発生（警告のwarnFrames後）
  if (m_PhaseAttackTimer >= start + warnFrames && m_PhaseAttackTimer <= end + warnFrames &&
      (m_PhaseAttackTimer - (start + warnFrames)) % intervalFrames == 0) {
    ShockwaveSystem::AddShockwave(m_PhaseTargetPos, 3.5f, 3.0f, 0.0f, 0.0f, 20,
                                  1.5f, 0);
    m_LightningVisualTimer = 15;

    XMFLOAT3 pPos = player->GetPosition();
    float dx = pPos.x - m_PhaseTargetPos.x;
    float dz = pPos.z - m_PhaseTargetPos.z;
    float dst = sqrtf(dx * dx + dz * dz);

    // 範囲内（STRIKE_DAMAGE_RADIUS以内）にプレイヤーがいる場合のみダメージを与える
    if (dst <= Constants::Boss::STRIKE_DAMAGE_RADIUS && !player->IsInvincible()) {
      HitInfo hitInfo;
      hitInfo.damage = Constants::Boss::STRIKE_DAMAGE;
      hitInfo.hitSourcePos = m_PhaseTargetPos;
      player->OnHit(hitInfo);
    }
    if (g_Camera)
      g_Camera->Shake(0.3f, 8);
  }

  // ── [落雷B] マップランダム多発警告（40fに1回・3〜5箇所） ──
  //   ステージ範囲: 壁(roomSize=18.0f)の内側に雷が落ちるように調整
  const float STAGE_HALF = Constants::Boss::STAGE_HALF_WIDTH;
  if (m_PhaseAttackTimer >= start + 10 && m_PhaseAttackTimer <= end &&
      (m_PhaseAttackTimer - (start - 20)) % 35 == 0) {
    int strikeCount = Constants::Boss::RANDOM_STRIKE_MIN +
                      (rand() % Constants::Boss::RANDOM_STRIKE_EXTRA);
    for (int s = 0; s < strikeCount; s++) {
      XMFLOAT3 randPos;
      randPos.x = ((float)rand() / RAND_MAX) * STAGE_HALF * 2.0f - STAGE_HALF;
      randPos.y = -0.95f;
      randPos.z = ((float)rand() / RAND_MAX) * STAGE_HALF * 2.0f - STAGE_HALF;

      // 警告エフェクト（40f猶予・収縮する赤いサークル）
      ShockwaveSystem::AddShockwave(randPos, 3.0f, 2.0f, 0.0f, 0.0f, 40, 0.0f,
                                    0, true);

      // 落雷B 発生（40f後に展開する衝撃波 + ビジュアル登録）
      ShockwaveSystem::AddShockwave(randPos, 3.0f, 2.0f, 0.0f, 0.0f, 15, 1.2f,
                                    40);

      // 40f後の着弾時に稲妻ビジュアルを登録（timer=40で展開）
      m_RandomLightnings.push_back({randPos, 40});
    }
  }

  // 落雷B の着弾と同タイミングでプレイヤーへのダメージ確認（40fごと）
  if (m_PhaseAttackTimer >= start + 50 && m_PhaseAttackTimer <= end + 50 &&
      (m_PhaseAttackTimer - (start + 50)) % 35 == 0) {
    // ランダム落雷の着弾フレームでカメラシェイク
    if (g_Camera)
      g_Camera->Shake(0.2f, 5);
    if (m_LightningVisualTimer < 8) {
      m_LightningVisualTimer = 8;
    }
  }

  if (m_LightningVisualTimer > 0) {
    m_LightningVisualTimer--;
  }

  if (m_PhaseAttackTimer >= end + Constants::Boss::STRIKE_WARNING_FRAMES + 10) {
    m_BossState = BossState::NORMAL;
    m_IsInvincible = false;
    m_PhaseAttackTimer = 0;
    m_RandomLightnings.clear(); // ランダム落雷リストをリセット
    OutputDebugStringA("[BossEnemy] Phase 3 Attack Finished.\n");
  }
}

// ─────────────────────────────────────────────
// バリアエフェクト（および落雷・縄跳びビームビジュアル）の描画
// ─────────────────────────────────────────────
void BossEnemy::DrawBarrierEffect() {
  Player *player = Manager::GetGameObject<Player>();
  if (!player)
    return;

  // 1. バリアエフェクト
  if (m_BossState == BossState::PHASE_TRANSITION) {
    XMFLOAT4 barrierColor;
    if (m_PhaseIndex == 1) {
      barrierColor = XMFLOAT4(0.0f, 1.5f, 3.0f, 1.0f); // 青
    } else if (m_PhaseIndex == 2) {
      barrierColor = XMFLOAT4(3.0f, 2.5f, 0.0f, 1.0f); // 黄
    } else {
      barrierColor = XMFLOAT4(3.0f, 0.0f, 0.0f, 1.0f); // 赤
    }

    XMFLOAT3 center = m_Position;
    center.y += 1.5f;

    float radius = 5.0f;

    int numBolts = 12;
    for (int i = 0; i < numBolts; ++i) {
      float theta1 = ((float)rand() / RAND_MAX) * XM_PI;
      float phi1 = ((float)rand() / RAND_MAX) * XM_2PI;
      float theta2 = ((float)rand() / RAND_MAX) * XM_PI;
      float phi2 = ((float)rand() / RAND_MAX) * XM_2PI;

      XMFLOAT3 p1;
      p1.x = center.x + radius * sinf(theta1) * cosf(phi1);
      p1.y = center.y + radius * cosf(theta1);
      p1.z = center.z + radius * sinf(theta1) * sinf(phi1);

      XMFLOAT3 p2;
      p2.x = center.x + radius * sinf(theta2) * cosf(phi2);
      p2.y = center.y + radius * cosf(theta2);
      p2.z = center.z + radius * sinf(theta2) * sinf(phi2);

      player->DrawLightningBoltInternal(p1, p2, 0.03f, barrierColor, false, i);
      player->DrawLightningBoltInternal(
          p1, p2, 0.015f, XMFLOAT4(2.5f, 2.5f, 2.5f, 1.0f), false, i + 10);
    }
  }

  // 2. 縄跳びラインマーカービジュアル (フェーズ2)
  if (m_BossState == BossState::PHASE_TRANSITION && m_PhaseIndex == 2) {
    bool isWarning = (m_PhaseAttackTimer <= 60);
    // 警告フェーズの間は点滅させる（10F周期で前半5Fのみ表示）
    if (!isWarning || (m_PhaseAttackTimer % 10 < 5)) {
      float angle = isWarning ? 0.0f : (m_PhaseAttackTimer - 60) * 0.015f;

      // 鮮やかなネオングリーンの自発光（エミッシブ）マテリアルを設定
      MATERIAL guideMaterial;
      ZeroMemory(&guideMaterial, sizeof(guideMaterial));
      guideMaterial.Diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
      guideMaterial.Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
      guideMaterial.Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
      guideMaterial.Emission = isWarning ? XMFLOAT4(3.0f, 0.0f, 0.0f, 1.0f)
                                         : XMFLOAT4(0.0f, 1.8f, 0.5f, 1.0f);
      guideMaterial.Shininess = 0.0f;
      guideMaterial.TextureEnable = FALSE; // 単色ネオン光
      guideMaterial.RimPower = 0.0f;
      Renderer::SetMaterial(guideMaterial);

      Renderer::SetupCubeDraw();

      float angles[2] = {angle, angle + XM_PI};
      for (int i = 0; i < 2; i++) {
        // ボスの中心から前方へ伸ばすワールド行列を作成
        // 高さ Y = -0.6f (地面は -0.95f、プレイヤーの足元は Y=0 なのでジャンプで超えられる高さ)
        const float barLen = Constants::Boss::PHASE2_BAR_LENGTH;
        XMMATRIX guideWorld =
            XMMatrixScaling(0.5f, 0.5f, barLen) *
            XMMatrixTranslation(0.0f, 0.0f, barLen * 0.5f) *
            XMMatrixRotationRollPitchYaw(0.0f, angles[i], 0.0f) *
            XMMatrixTranslation(m_Position.x, -0.6f, m_Position.z);

        Renderer::SetWorldMatrix(guideWorld);
        Renderer::GetDeviceContext()->Draw(36, 0);
      }
    }
  }

  // 3. 落雷ビジュアル (フェーズ3)
  if (m_BossState == BossState::PHASE_TRANSITION && m_PhaseIndex == 3) {

    // ── [落雷A] プレイヤー狙い撃ち稲妻 ──
    if (m_LightningVisualTimer > 0) {
      XMFLOAT3 strikeStart = m_PhaseTargetPos;
      strikeStart.y = 25.0f;
      XMFLOAT3 strikeEnd = m_PhaseTargetPos;

      player->DrawLightningBoltInternal(strikeStart, strikeEnd, 0.12f,
                                        XMFLOAT4(3.0f, 0.0f, 0.0f, 1.0f), true,
                                        m_LightningVisualTimer);
      player->DrawLightningBoltInternal(strikeStart, strikeEnd, 0.06f,
                                        XMFLOAT4(3.0f, 2.0f, 2.0f, 1.0f), false,
                                        m_LightningVisualTimer + 5);
    }

    // ── [落雷B] ランダム多発稲妻（m_RandomLightningsリストを走査） ──
    for (auto &rl : m_RandomLightnings) {
      if (rl.timer <= 0)
        continue;

      XMFLOAT3 rStart = rl.pos;
      rStart.y = 25.0f;
      XMFLOAT3 rEnd = rl.pos;

      // 落雷Bは少し細め・黄色がかった白色で差別化
      player->DrawLightningBoltInternal(rStart, rEnd, 0.08f,
                                        XMFLOAT4(2.5f, 2.0f, 0.0f, 1.0f), true,
                                        rl.timer);
      player->DrawLightningBoltInternal(rStart, rEnd, 0.04f,
                                        XMFLOAT4(3.0f, 3.0f, 1.5f, 1.0f), false,
                                        rl.timer + 3);
    }

    // ランダム落雷タイマーを毎フレーム減算し、期限切れを削除
    for (auto &rl : m_RandomLightnings) {
      if (rl.timer > 0)
        rl.timer--;
    }
    m_RandomLightnings.erase(
        std::remove_if(m_RandomLightnings.begin(), m_RandomLightnings.end(),
                       [](const RandomLightning &r) { return r.timer <= 0; }),
        m_RandomLightnings.end());
  }

  // 4. 弾幕予測レーザーライン（フェーズ1）
  if (m_BossState == BossState::PHASE_TRANSITION && m_PhaseIndex == 1) {
    BossPhase1ChargeInfo chargeInfo = GetPhase1ChargeInfo();

    if (chargeInfo.isCharging) {
      float bulletY = -0.2f;
      Player *player = Manager::GetGameObject<Player>();
      if (player) {
        bulletY = player->GetPosition().y + 0.3f;
      }
      XMFLOAT3 bulletPos = m_Position;
      bulletPos.y = bulletY;

      // 暗めの赤色の自発光（エミッシブ）マテリアルを設定
      // チャージが進むほど輝度が増す
      float intensity = 0.5f + 1.5f * ((float)chargeInfo.relativeTimer / 15.0f);
      MATERIAL laserMaterial;
      ZeroMemory(&laserMaterial, sizeof(laserMaterial));
      laserMaterial.Diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
      laserMaterial.Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
      laserMaterial.Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
      laserMaterial.Emission =
          XMFLOAT4(intensity * 1.5f, 0.0f, 0.0f, 1.0f); // 赤色ビーム
      laserMaterial.Shininess = 0.0f;
      laserMaterial.TextureEnable = FALSE;
      laserMaterial.RimPower = 0.0f;
      Renderer::SetMaterial(laserMaterial);

      Renderer::SetupCubeDraw();

      const int numLines = 24;
      const float LINE_LENGTH = 25.0f;
      for (int i = 0; i < numLines; i++) {
        float angle = i * (XM_2PI / numLines) + chargeInfo.angleOffset;

        // ボス位置から放射状に極細の棒（X=0.03, Y=0.03, Z=LINE_LENGTH）を配置
        XMMATRIX laserWorld =
            XMMatrixScaling(0.03f, 0.03f, LINE_LENGTH) *
            XMMatrixTranslation(0.0f, 0.0f, LINE_LENGTH * 0.5f) *
            XMMatrixRotationRollPitchYaw(0.0f, angle, 0.0f) *
            XMMatrixTranslation(bulletPos.x, bulletPos.y, bulletPos.z);

        Renderer::SetWorldMatrix(laserWorld);
        Renderer::GetDeviceContext()->Draw(36, 0);
      }
    }
  }
}

// ─────────────────────────────────────────────
// 描画処理
// ─────────────────────────────────────────────
void BossEnemy::Draw() {
  Enemy::Draw();

  if (!Renderer::IsShadowMode() && !Renderer::IsOutlineMode()) {
    DrawBarrierEffect();
  }
}

// ─────────────────────────────────────────────
// 自発光の定義
// ─────────────────────────────────────────────
DirectX::XMFLOAT3 BossEnemy::GetEmissive() const {
  if (m_DamageFlashTimer > 0) {
    return XMFLOAT3(5.0f, 0.0f, 0.0f); // 被弾時点滅は赤
  }

  if (m_BossState == BossState::RETURN_TO_CENTER) {
    // 復帰大ジャンプ中は、次フェーズカラーに応じた強い予兆発光
    if (m_PhaseIndex == 1) {
      return XMFLOAT3(0.5f, 2.0f, 3.5f); // 青白
    } else if (m_PhaseIndex == 2) {
      return XMFLOAT3(3.5f, 3.0f, 0.2f); // 黄金
    } else {
      return XMFLOAT3(3.5f, 0.5f, 0.2f); // 紅蓮
    }
  }

  if (m_BossState == BossState::CHARGE_WINDUP) {
    // 突進タメ中: 危険を知らせる力強い赤橙パルス発光
    float pulse = 0.5f + 0.5f * sinf((float)m_ChargeTimer * 0.35f);
    return XMFLOAT3(3.0f + 2.0f * pulse, 0.6f * pulse, 0.0f);
  }

  if (m_BossState == BossState::CHARGE_RUSH) {
    // 突進中: 灼熱の赤熱発光
    return XMFLOAT3(4.5f, 0.8f, 0.0f);
  }

  if (m_BossState == BossState::CHARGE_STUN) {
    // スタン中: 弱々しい黄色のピヨピヨ点滅
    float blink = (m_ChargeTimer % 10 < 5) ? 1.0f : 0.2f;
    return XMFLOAT3(1.8f * blink, 1.8f * blink, 0.2f * blink);
  }

  if (m_BossState == BossState::SLAM_WINDUP) {
    // スラムタメ中: 橙〜黄色のエネルギー吸引発光
    float pulse = 0.5f + 0.5f * sinf((float)m_SlamTimer * 0.4f);
    return XMFLOAT3(3.2f, 1.8f * pulse, 0.2f);
  }

  if (m_BossState == BossState::SLAM_AIR) {
    // 滞空・急降下中: 黄金の落下一撃発光
    return XMFLOAT3(4.0f, 3.2f, 0.5f);
  }

  if (m_BossState == BossState::SLAM_RECOVERY) {
    // 着地後隙: 余熱が冷めていくグラデーション
    float cool = 1.0f - (float)m_SlamTimer / (float)Constants::Boss::SLAM_RECOVERY_FRAMES;
    return XMFLOAT3(2.5f + 1.5f * cool, 1.0f * cool, 0.2f * cool);
  }

  if (m_BossState == BossState::PHASE_TRANSITION) {
    // バリア明滅演出用の強度
    float intensity =
        2.0f + 3.0f * (0.5f + 0.5f * sinf(m_PhaseAttackTimer * 0.2f));
    if (m_PhaseIndex == 1) {
      return XMFLOAT3(0.0f, intensity * 0.5f, intensity); // 青
    } else if (m_PhaseIndex == 2) {
      return XMFLOAT3(intensity, intensity * 0.8f, 0.0f); // 黄
    } else {
      return XMFLOAT3(intensity, 0.0f, 0.0f); // 赤
    }
  }

  return XMFLOAT3(2.5f, 0.0f, 1.5f); // 通常時は赤紫色発光
}
