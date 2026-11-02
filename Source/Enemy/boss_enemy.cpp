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
#include "player_visual.h"
#include "renderer.h"
#include "resource_manager.h"
#include "score_popup.h"
#include "shockwave.h"
#include "sound_manager.h"
#include "material_drop.h"
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
  m_SpinHitCooldownTimer = 0;
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
  if (m_SpinHitCooldownTimer > 0) {
    m_SpinHitCooldownTimer--;
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

        LOG_INFO("[BossEnemy] Phase %d Triggered! Returning to center via jump.\n", phase.phaseIndex);
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
        LOG_INFO("[BossEnemy] Close-range counter: Slam Jump Attack.\n");
        StartSlamAttack();
      } else if (r < 80) {
        LOG_INFO("[BossEnemy] Close-range counter: Charge Tackle Attack.\n");
        StartChargeAttack();
      } else {
        LOG_INFO("[BossEnemy] Close-range counter: 3-Way Spread Bullet.\n");
        Fire3WaySpread();
      }
    } else {
      // 中・遠距離の場合: 順繰りに多彩な攻撃パターンを実行
      m_AttackPattern = (m_AttackPattern + 1) % 4;
      if (m_AttackPattern == 0) {
        LOG_INFO("[BossEnemy] Fire 3-Way Spread Bullet.\n");
        Fire3WaySpread();
      } else if (m_AttackPattern == 1) {
        LOG_INFO("[BossEnemy] Fire Rapid Bullet.\n");
        FireRapidShot();
      } else if (m_AttackPattern == 2) {
        LOG_INFO("[BossEnemy] Start Charge Tackle Attack.\n");
        StartChargeAttack();
      } else {
        LOG_INFO("[BossEnemy] Start Slam Jump Attack.\n");
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
  if (player) {
    if (!m_HasDamagedPlayerInCharge) {
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

    // 突進中、プレイヤーが壁の外へ押し出されないよう安全にクランプ
    float safeLimit = Constants::Stage::BOSS_ROOM_SIZE - 0.8f;
    XMFLOAT3 pPos = player->GetPosition();
    if (fabsf(pPos.x) > safeLimit || fabsf(pPos.z) > safeLimit) {
      pPos.x = MathHelper::Clamp(pPos.x, -safeLimit, safeLimit);
      pPos.z = MathHelper::Clamp(pPos.z, -safeLimit, safeLimit);
      player->SetPosition(pPos);
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

    // 壁激突スタン時：ぶつかった壁と反対側（アリーナ手前側）へ投げられるオブジェクトを3個ドロップ
    XMFLOAT3 awayFromWall = XMFLOAT3(-m_ChargeDir.x, 0.0f, -m_ChargeDir.z);
    SpawnDroppingObjects(3, awayFromWall, XM_PI * 0.5f);

    // 壁とボスの間にプレイヤーが挟まれていた場合、ボスの手前へ安全に救出
    if (player) {
      XMFLOAT3 pPos = player->GetPosition();
      float safeLimit = Constants::Stage::BOSS_ROOM_SIZE - 1.2f;
      pPos.x = MathHelper::Clamp(pPos.x, -safeLimit, safeLimit);
      pPos.z = MathHelper::Clamp(pPos.z, -safeLimit, safeLimit);
      player->SetPosition(pPos);
    }

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

    // スタンプ着地時：周囲（壁から離れる方向基準）へ投げられるオブジェクトを3個ドロップ
    XMFLOAT3 centerDir;
    float distFromCenter = sqrtf(m_Position.x * m_Position.x + m_Position.z * m_Position.z);
    if (distFromCenter > 3.0f) {
      centerDir = XMFLOAT3(-m_Position.x / distFromCenter, 0.0f, -m_Position.z / distFromCenter);
    } else {
      centerDir = XMFLOAT3(0.0f, 0.0f, -1.0f);
    }
    SpawnDroppingObjects(3, centerDir, XM_PI * 0.8f);

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
    XMFLOAT3 hitPos = hitSourcePos;
    for (int i = 0; i < 6; i++) {
      float rx = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
      float ry = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
      float rz = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
      XMFLOAT3 sparkEnd = XMFLOAT3(hitPos.x + rx, hitPos.y + ry, hitPos.z + rz);
      PlayerVisual::DrawLightningBolt(hitPos, sparkEnd, 0.02f, XMFLOAT4(0.0f, 1.8f, 2.5f, 1.0f));
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

    // ボス撃破演出：スローモーション発動＆強烈なホワイトインパクトフラッシュ
    Manager::StartSlowMotion(Constants::Boss::DEFEAT_SLOW_MOTION_DURATION);
    Manager::TriggerFlash(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.95f), Constants::Boss::DEFEAT_FLASH_FADE_SPEED);
    Manager::TriggerRadialBlur(0.85f);
    Manager::TriggerChromaticAberration(0.7f);

    // ボス撃破処理（赤色ポップアップ）
    Defeat(2.5f, 0.0f, 0.0f);

    // ボス撃破の瞬間にプレイヤーの低HP警告（画面の赤点滅）を即座に解除
    Manager::SetLowHPWarning(false);
  }
}

// ─────────────────────────────────────────────
// 投げられるオブジェクト（サンドバッグブロック）の転がりドロップ処理
// ─────────────────────────────────────────────
void BossEnemy::SpawnDroppingObjects(int count, const DirectX::XMFLOAT3& baseDir, float spreadAngle)
{
  float len = sqrtf(baseDir.x * baseDir.x + baseDir.z * baseDir.z);
  XMFLOAT3 dir = (len > 0.001f) ? XMFLOAT3(baseDir.x / len, 0.0f, baseDir.z / len) : XMFLOAT3(0.0f, 0.0f, -1.0f);
  float baseAngle = atan2f(dir.x, dir.z);

  for (int i = 0; i < count; ++i) {
    float angleOffset = 0.0f;
    if (count > 1) {
      float t = (float)i / (float)(count - 1); // 0.0f 〜 1.0f
      angleOffset = (t - 0.5f) * spreadAngle;  // -spreadAngle/2 〜 +spreadAngle/2
    }
    float currentAngle = baseAngle + angleOffset;
    XMFLOAT3 shootDir = XMFLOAT3(sinf(currentAngle), 0.0f, cosf(currentAngle));

    Enemy* obj = Manager::AddGameObject<Enemy>();
    if (obj) {
      // ボス本体の体表より少し外側・上空から放り出される
      float spawnDist = 2.8f;
      XMFLOAT3 spawnPos;
      spawnPos.x = m_Position.x + shootDir.x * spawnDist;
      spawnPos.y = m_Position.y + 1.2f;
      spawnPos.z = m_Position.z + shootDir.z * spawnDist;

      // アリーナ外へ飛び出さないよう安全クランプ
      float arenaLimit = Constants::Stage::BOSS_ROOM_SIZE - 2.0f;
      spawnPos.x = MathHelper::Clamp(spawnPos.x, -arenaLimit, arenaLimit);
      spawnPos.z = MathHelper::Clamp(spawnPos.z, -arenaLimit, arenaLimit);

      obj->SetPosition(spawnPos);
      obj->SetScale(XMFLOAT3(1.1f, 1.1f, 1.1f));
      obj->SetEnemyState(EnemyState::FLYING);
      obj->SetThrownByPlayer(false); // 転がり中（攻撃衝突判定OFF）

      // 寿命3秒（180フレーム）のサンドバッグ（非生物ブロック）
      obj->SetSandbag(true, 180);

      // 水平初速と上向き初速（放物線を描いてポーンと転がる）
      float speedH = 0.15f + (float)(i % 2) * 0.03f;
      float speedY = 0.22f + (float)(i % 2) * 0.04f;
      obj->SetVelocity(XMFLOAT3(shootDir.x * speedH, speedY, shootDir.z * speedH));

      // ランダムな初期回転
      obj->SetRotation(XMFLOAT3(
          (float)(rand() % 100) / 20.0f,
          (float)(rand() % 100) / 20.0f,
          (float)(rand() % 100) / 20.0f
      ));
    }
  }
}

void BossEnemy::SpawnMaterialDrops() {
  // ボス撃破時：巨人の装甲片×3、巨人の動力核×1、エネルギーセル×2を放射状に放出
  std::vector<MaterialType> bossDrops = {
    MaterialType::TITAN_ARMOR,
    MaterialType::TITAN_ARMOR,
    MaterialType::TITAN_ARMOR,
    MaterialType::TITAN_CORE,
    MaterialType::ENERGY_CELL,
    MaterialType::ENERGY_CELL
  };

  int count = static_cast<int>(bossDrops.size());
  for (int i = 0; i < count; ++i) {
    MaterialDrop* drop = Manager::AddGameObject<MaterialDrop>();
    if (drop) {
      // 全方位へ大きく広がる初速
      float angle = (XM_2PI / count) * i + (((float)rand() / RAND_MAX) * 0.4f - 0.2f);
      float speed = 0.12f + ((float)rand() / RAND_MAX) * 0.08f;
      float speedY = 0.25f + ((float)rand() / RAND_MAX) * 0.10f; // 高く打ち上げる
      XMFLOAT3 vel(cosf(angle) * speed, speedY, sinf(angle) * speed);

      XMFLOAT3 spawnPos = m_Position;
      spawnPos.y += 2.0f; // ボスの胸元から放出
      drop->Setup(bossDrops[i], spawnPos, vel);
    }
  }
}
