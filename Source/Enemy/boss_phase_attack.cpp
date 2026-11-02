#include "boss_enemy.h"
#include "camera.h"
#include "enemy_bullet.h"
#include "game_constants.h"
#include "manager.h"
#include "math_helper.h"
#include "player.h"
#include "player_visual.h"
#include "renderer.h"
#include "shockwave.h"
#include <algorithm>

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
    LOG_INFO("[BossEnemy] Phase 1 Attack Finished.\n");
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
    LOG_INFO("[BAR HIT] Player=(%.2f, %.2f, %.2f)  Boss=(%.2f, %.2f, %.2f)\n"
             "  barDir=(%.3f, %.3f)  proj=%.2f  perpDist=%.4f  dist2D=%.2f\n",
             pPos.x, pPos.y, pPos.z, m_Position.x, m_Position.y, m_Position.z,
             barDirX, barDirZ, proj, perpDist, dist);

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
    LOG_INFO("[BossEnemy] Phase 2 Attack Finished.\n");
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
    LOG_INFO("[BossEnemy] Phase 3 Attack Finished.\n");
  }
}

// ─────────────────────────────────────────────
// バリアエフェクト（および落雷・縄跳びビームビジュアル）の描画
// ─────────────────────────────────────────────
void BossEnemy::DrawBarrierEffect() {
  if (m_BossState != BossState::PHASE_TRANSITION) {
    return;
  }

  DrawPhaseBarrier();

  if (m_PhaseIndex == 1) {
    DrawPhase1LaserGuides();
  } else if (m_PhaseIndex == 2) {
    DrawPhase2JumpRopeBeams();
  } else if (m_PhaseIndex == 3) {
    DrawPhase3Lightnings();
  }
}

void BossEnemy::DrawPhaseBarrier() {
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
  const float radius = 5.0f;
  const int numBolts = 12;

  for (int i = 0; i < numBolts; ++i) {
    float theta1 = ((float)rand() / RAND_MAX) * XM_PI;
    float phi1 = ((float)rand() / RAND_MAX) * XM_2PI;
    float theta2 = ((float)rand() / RAND_MAX) * XM_PI;
    float phi2 = ((float)rand() / RAND_MAX) * XM_2PI;

    XMFLOAT3 p1(
        center.x + radius * sinf(theta1) * cosf(phi1),
        center.y + radius * cosf(theta1),
        center.z + radius * sinf(theta1) * sinf(phi1));
    XMFLOAT3 p2(
        center.x + radius * sinf(theta2) * cosf(phi2),
        center.y + radius * cosf(theta2),
        center.z + radius * sinf(theta2) * sinf(phi2));

    PlayerVisual::DrawLightningBoltInternal(p1, p2, 0.03f, barrierColor, false, i);
    PlayerVisual::DrawLightningBoltInternal(
        p1, p2, 0.015f, XMFLOAT4(2.5f, 2.5f, 2.5f, 1.0f), false, i + 10);
  }
}

void BossEnemy::DrawPhase1LaserGuides() {
  BossPhase1ChargeInfo chargeInfo = GetPhase1ChargeInfo();
  if (!chargeInfo.isCharging) {
    return;
  }

  float bulletY = -0.2f;
  Player *player = Manager::GetGameObject<Player>();
  if (player) {
    bulletY = player->GetPosition().y + 0.3f;
  }
  XMFLOAT3 bulletPos = m_Position;
  bulletPos.y = bulletY;

  float intensity = 0.5f + 1.5f * ((float)chargeInfo.relativeTimer / 15.0f);
  MATERIAL laserMaterial = {};
  laserMaterial.Diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
  laserMaterial.Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
  laserMaterial.Emission = XMFLOAT4(intensity * 1.5f, 0.0f, 0.0f, 1.0f);
  laserMaterial.TextureEnable = FALSE;
  Renderer::SetMaterial(laserMaterial);
  Renderer::SetupCubeDraw();

  const int numLines = 24;
  const float LINE_LENGTH = 25.0f;
  for (int i = 0; i < numLines; i++) {
    float angle = i * (XM_2PI / numLines) + chargeInfo.angleOffset;
    XMMATRIX laserWorld =
        XMMatrixScaling(0.03f, 0.03f, LINE_LENGTH) *
        XMMatrixTranslation(0.0f, 0.0f, LINE_LENGTH * 0.5f) *
        XMMatrixRotationRollPitchYaw(0.0f, angle, 0.0f) *
        XMMatrixTranslation(bulletPos.x, bulletPos.y, bulletPos.z);

    Renderer::SetWorldMatrix(laserWorld);
    Renderer::GetDeviceContext()->Draw(36, 0);
  }
}

void BossEnemy::DrawPhase2JumpRopeBeams() {
  bool isWarning = (m_PhaseAttackTimer <= 60);
  if (isWarning && (m_PhaseAttackTimer % 10 >= 5)) {
    return;
  }

  float angle = isWarning ? 0.0f : (m_PhaseAttackTimer - 60) * 0.015f;

  MATERIAL guideMaterial = {};
  guideMaterial.Diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
  guideMaterial.Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
  guideMaterial.Emission = isWarning ? XMFLOAT4(3.0f, 0.0f, 0.0f, 1.0f)
                                     : XMFLOAT4(0.0f, 1.8f, 0.5f, 1.0f);
  guideMaterial.TextureEnable = FALSE;
  Renderer::SetMaterial(guideMaterial);
  Renderer::SetupCubeDraw();

  float angles[2] = {angle, angle + XM_PI};
  const float barLen = Constants::Boss::PHASE2_BAR_LENGTH;
  for (int i = 0; i < 2; i++) {
    XMMATRIX guideWorld =
        XMMatrixScaling(0.5f, 0.5f, barLen) *
        XMMatrixTranslation(0.0f, 0.0f, barLen * 0.5f) *
        XMMatrixRotationRollPitchYaw(0.0f, angles[i], 0.0f) *
        XMMatrixTranslation(m_Position.x, -0.6f, m_Position.z);

    Renderer::SetWorldMatrix(guideWorld);
    Renderer::GetDeviceContext()->Draw(36, 0);
  }
}

void BossEnemy::DrawPhase3Lightnings() {
  if (m_LightningVisualTimer > 0) {
    XMFLOAT3 strikeStart = m_PhaseTargetPos;
    strikeStart.y = 25.0f;
    XMFLOAT3 strikeEnd = m_PhaseTargetPos;

    PlayerVisual::DrawLightningBoltInternal(strikeStart, strikeEnd, 0.12f,
                                            XMFLOAT4(3.0f, 0.0f, 0.0f, 1.0f), true,
                                            m_LightningVisualTimer);
    PlayerVisual::DrawLightningBoltInternal(strikeStart, strikeEnd, 0.06f,
                                            XMFLOAT4(3.0f, 2.0f, 2.0f, 1.0f), false,
                                            m_LightningVisualTimer + 5);
  }

  for (auto &rl : m_RandomLightnings) {
    if (rl.timer <= 0)
      continue;

    XMFLOAT3 rStart = rl.pos;
    rStart.y = 25.0f;
    XMFLOAT3 rEnd = rl.pos;

    PlayerVisual::DrawLightningBoltInternal(rStart, rEnd, 0.08f,
                                            XMFLOAT4(2.5f, 2.0f, 0.0f, 1.0f), true,
                                            rl.timer);
    PlayerVisual::DrawLightningBoltInternal(rStart, rEnd, 0.04f,
                                            XMFLOAT4(3.0f, 3.0f, 1.5f, 1.0f), false,
                                            rl.timer + 3);
  }

  for (auto &rl : m_RandomLightnings) {
    if (rl.timer > 0)
      rl.timer--;
  }
  m_RandomLightnings.erase(
      std::remove_if(m_RandomLightnings.begin(), m_RandomLightnings.end(),
                     [](const RandomLightning &r) { return r.timer <= 0; }),
      m_RandomLightnings.end());
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

