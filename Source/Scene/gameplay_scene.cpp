#include "gameplay_scene.h"
#include "event_system.h"
#include "event_types.h"
#include "score_popup.h"
#include "manager.h"
#include "player.h"
#include "enemy.h"
#include "boss_enemy.h"
#include "wall.h"
#include "item.h"
#include "field.h"
#include "collision.h"
#include "collision_system.h"
#include "game_rule.h"
#include "game_constants.h"
#include "shockwave.h"
#include "camera.h"
#include "input.h"
#include "math_helper.h"
#include "attacking_enemy.h"
#include "magnetic_bullet.h"
#include "progression_manager.h"

using namespace DirectX;

// =================================================================
// コンストラクタ / デストラクタ
// =================================================================
GameplayScene::GameplayScene()
{
}

GameplayScene::~GameplayScene()
{
}

// =================================================================
// 初期化処理
// =================================================================
void GameplayScene::Init()
{
    // タイムアタック計測の開始
    GameRule::Init();
    ProgressionManager::StartSession(); // クエスト素材セッション開始

    // イベント購読の登録
    EventSystem::Subscribe<PlayerHitEvent>([](const PlayerHitEvent& ev) {
        if (g_Camera) g_Camera->Shake(0.35f, 12);
        Manager::AddHitStop(5);
        Manager::TriggerFlash(XMFLOAT4(1.0f, 0.0f, 0.0f, 0.4f), 0.04f); // 被弾時に赤いフラッシュ
        Manager::TriggerChromaticAberration(0.55f); // 被弾ショックによる色収差
    });

    EventSystem::Subscribe<EnemyDefeatedEvent>([this](const EnemyDefeatedEvent& ev) {
        m_ComboTimer = Constants::TimeAttack::COMBO_DURATION_FRAMES; // 2秒間コンボ維持
        m_ComboCount++;

        int finalScore = ev.scoreValue;
        if (m_ComboCount >= Constants::TimeAttack::COMBO_BONUS_MIN_COUNT) {
            // 3コンボ以上でスコア倍率を適用 (x1.3, x1.4...)
            float multiplier = 1.0f + (m_ComboCount * Constants::TimeAttack::COMBO_SCORE_STEP);
            finalScore = static_cast<int>(ev.scoreValue * multiplier);
        }

        // タイムアタック：雑魚敵撃破で -1.0秒短縮！
        GameRule::ReduceTime(Constants::TimeAttack::ENEMY_DEFEAT_TIME_REDUCE);
        GameRule::OnEnemyDefeated(finalScore);
        ScorePopupSystem::AddTextPopup(ev.position.x, ev.position.y + 0.5f, ev.position.z, L"-1.0s", 0.3f, 2.2f, 0.8f);

        if (m_ComboCount >= Constants::TimeAttack::COMBO_BONUS_MIN_COUNT) {
            // コンボ数を金色ポップアップで通知
            ScorePopupSystem::AddPopup(ev.position.x, ev.position.y + 1.2f, ev.position.z, m_ComboCount, 2.5f, 1.8f, 0.0f);
            Manager::AddHitStop(3); // コンボ時の手応えヒットストップ
            if (g_Camera) g_Camera->Shake(0.15f, 6);
            Manager::TriggerRadialBlur(0.20f);
        }
    });

    EventSystem::Subscribe<BossHitEvent>([](const BossHitEvent& ev) {
        Manager::AddHitStop(12);
        if (g_Camera) g_Camera->Shake(0.40f, 12);
        Manager::TriggerFlash(XMFLOAT4(1.0f, 0.8f, 0.0f, 0.3f), 0.05f); // ボス被弾時に黄色閃光
        Manager::TriggerRadialBlur(0.50f);
        Manager::TriggerChromaticAberration(0.35f);
    });

    EventSystem::Subscribe<PlayerParriedEvent>([](const PlayerParriedEvent& ev) {
        // タイムアタック：パリィ成功で -2.0秒短縮！
        GameRule::ReduceTime(Constants::TimeAttack::PARRY_TIME_REDUCE);
        ScorePopupSystem::AddTextPopup(ev.position.x, ev.position.y + 1.2f, ev.position.z, L"-2.0s", 0.0f, 2.5f, 2.5f);

        Manager::TriggerFlash(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.02f); // パリィ成功時に真っ白閃光

        // パリィタイム延長強化
        int slowDuration = ProgressionManager::HasParryExtension()
            ? Constants::TimeAttack::PARRY_SLOW_EXTENDED_FRAMES
            : Constants::TimeAttack::PARRY_SLOW_NORMAL_FRAMES;
        Manager::StartSlowMotion(slowDuration);
        Manager::AddHitStop(15);      // 手応えヒットストップ
        Manager::TriggerRadialBlur(0.75f);
        Manager::TriggerChromaticAberration(0.60f);

        // カウンターショック強化：周囲の敵を吹き飛ばす衝撃波
        if (ProgressionManager::HasCounterShock()) {
            XMFLOAT3 shockPos = ev.position;
            shockPos.y = -0.95f;
            ShockwaveSystem::AddShockwave(shockPos, 8.0f, 1.5f, 0.0f, 0.0f, 24, 1.2f, 0);

            for (GameObject* obj : Manager::GetCategoryList(ObjectType::Enemy)) {
                if (!obj || obj->IsDestroy()) continue;
                Enemy* enemy = static_cast<Enemy*>(obj);
                if (enemy->GetEnemyState() == EnemyState::DEFEATED || enemy->IsSandbag()) continue;

                XMFLOAT3 diff = enemy->GetPosition() - ev.position;
                diff.y = 0.0f;
                float d = MathHelper::Length(diff);
                if (d < Constants::TimeAttack::COUNTER_SHOCK_RADIUS && d > 0.01f) {
                    diff.x /= d;
                    diff.z /= d;
                    HitInfo hInfo;
                    hInfo.damage = 1;
                    hInfo.knockbackVel = XMFLOAT3(diff.x * 0.25f, 0.18f, diff.z * 0.25f);
                    enemy->OnHit(hInfo);
                }
            }
        }
    });

    EventSystem::Subscribe<BossSpawnEvent>([](const BossSpawnEvent& ev) {
        if (g_Camera) {
            XMFLOAT3 eye = ev.bossPosition;
            eye.y += 7.0f;  // ボスの中心(Y=4.0f)の上方7mにカメラを配置 (高さ11m)
            eye.z -= 22.0f; // ボスの巨大スケールに合わせて十分引き離す (22m)
            XMFLOAT3 target = ev.bossPosition; // ボスの中心(Y=4.0f)を注視
            g_Camera->SetCutsceneMode(true, eye, target);
        }

        // 磁力旋回弾の生成 (等間隔に8個)
        BossEnemy* boss = Manager::GetGameObject<BossEnemy>();
        if (boss) {
            const int BULLET_COUNT = Constants::TimeAttack::MAGNETIC_BULLET_COUNT;
            for (int i = 0; i < BULLET_COUNT; ++i) {
                float startAngle = (XM_2PI / BULLET_COUNT) * i;
                MagneticBullet* bullet = Manager::AddGameObject<MagneticBullet>();
                if (bullet) {
                    bullet->Setup(
                        boss,
                        startAngle,
                        Constants::TimeAttack::MAGNETIC_BULLET_INIT_RADIUS,  // 初期半径 (ボスの外側から螺旋で吸い寄せられる)
                        Constants::TimeAttack::MAGNETIC_BULLET_ORBIT_RADIUS, // 目標周回半径 (ボスScale=5.0f、うねり幅±1.2fを考慮してボス外周をクリアに周回)
                        Constants::TimeAttack::MAGNETIC_BULLET_ROT_SPEED,    // 回転速度
                        Constants::TimeAttack::MAGNETIC_BULLET_ATTRACT,      // 吸引力
                        0.0f                                                 // ボスの中心(胸元)を周回するため高さオフセットは0.0f
                    );
                }
            }
        }
    });

    EventSystem::Subscribe<BossBattleStartEvent>([](const BossBattleStartEvent& ev) {
        if (g_Camera) {
            g_Camera->SetCutsceneMode(false);
            g_Camera->Shake(0.5f, 15); // 戦闘開始時の咆哮の余韻シェイク
        }

        // カットシーン終了時にプレイヤーをボス方向（北）へ向かせる
        Player* player = Manager::GetGameObject<Player>();
        if (player) {
            XMFLOAT3 rot = player->GetRotation();
            rot.y = 0.0f; // Z+方向（ボスのいる北向き）にリセット
            player->SetRotation(rot);
        }

        // 生存している磁力弾をすべてプレイヤーに向かって一斉射出
        player = Manager::GetGameObject<Player>();
        if (player) {
            XMFLOAT3 playerPos = player->GetPosition();
            for (MagneticBullet* bullet : Manager::GetGameObjects<MagneticBullet>()) {
                if (bullet && !bullet->IsLaunched()) {
                    bullet->Launch(playerPos);
                }
            }
        }
    });

    // ゲームプレイ本編の初期化
    GameRule::Init(); // スコアやゲームクリア状態のリセット
    
    Manager::SetHitStopFrames(0);
    Manager::SetSlowMotionTimer(0);
    Manager::SetSlowMotionDuration(0);
    m_ClearDelayTimer = 0;

    // 1. 地面・空オブジェクトの生成
    Manager::AddGameObject<Field>();
    Manager::AddGameObject<Skybox>();

    // 2. プレイヤーオブジェクトの生成
    Player* player = Manager::AddGameObject<Player>();
    player->SetPosition(XMFLOAT3(0.0f, -0.5f, 0.0f));

    // ボス開始か通常開始かの分岐 (クエスト選択による動的制御)
    bool startBoss = Manager::GetStartFromBoss();
    if (startBoss) {
        Manager::SetIsBossStage(true);
        
        // ボス部屋の四方壁を生成（高く・厚くしてプレイヤーの場外飛び出しを防止）
        Wall::CreateBoundaryWalls(
            Constants::Stage::BOSS_ROOM_SIZE,
            Constants::Stage::BOSS_WALL_HEIGHT,
            Constants::Stage::BOSS_WALL_Y,
            Constants::Stage::BOSS_WALL_THICKNESS);

        // ボスエネミーの生成
        BossEnemy* boss = Manager::AddGameObject<BossEnemy>();
        boss->SetPosition(XMFLOAT3(0.0f, Constants::Boss::GROUND_Y, Constants::Stage::BOSS_SPAWN_OFFSET_Z));
        boss->SetRotation(XMFLOAT3(0.0f, XM_PI, 0.0f));

        GameRule::SetTotalEnemies(1);
    } else {
        Manager::SetIsBossStage(false);
        
        // 通常ステージ外周壁の生成（60m×60m バトルアリーナ）
        Wall::CreateBoundaryWalls(
            Constants::Stage::NORMAL_ROOM_SIZE,
            Constants::Stage::NORMAL_WALL_HEIGHT,
            Constants::Stage::NORMAL_WALL_Y,
            Constants::Stage::NORMAL_WALL_THICKNESS);

        // 通常ステージ障害物：左右のピラー（柱）
        Wall* pillarL = Manager::AddGameObject<Wall>();
        pillarL->SetPosition(XMFLOAT3(-Constants::Stage::PILLAR_POS_X, Constants::Stage::PILLAR_POS_Y, Constants::Stage::PILLAR_POS_Z));
        pillarL->SetScale(XMFLOAT3(Constants::Stage::PILLAR_SCALE_XZ, Constants::Stage::PILLAR_SCALE_Y, Constants::Stage::PILLAR_SCALE_XZ));

        Wall* pillarR = Manager::AddGameObject<Wall>();
        pillarR->SetPosition(XMFLOAT3(Constants::Stage::PILLAR_POS_X, Constants::Stage::PILLAR_POS_Y, Constants::Stage::PILLAR_POS_Z));
        pillarR->SetScale(XMFLOAT3(Constants::Stage::PILLAR_SCALE_XZ, Constants::Stage::PILLAR_SCALE_Y, Constants::Stage::PILLAR_SCALE_XZ));

        // 通常ステージ障害物：左右のバリケード（低壁・飛び越え可能）
        Wall* barricadeL = Manager::AddGameObject<Wall>();
        barricadeL->SetPosition(XMFLOAT3(-Constants::Stage::BARRICADE_POS_X, Constants::Stage::BARRICADE_POS_Y, Constants::Stage::BARRICADE_POS_Z));
        barricadeL->SetScale(XMFLOAT3(Constants::Stage::BARRICADE_WIDTH, Constants::Stage::BARRICADE_HEIGHT, Constants::Stage::BARRICADE_THICKNESS));

        Wall* barricadeR = Manager::AddGameObject<Wall>();
        barricadeR->SetPosition(XMFLOAT3(Constants::Stage::BARRICADE_POS_X, Constants::Stage::BARRICADE_POS_Y, Constants::Stage::BARRICADE_POS_Z));
        barricadeR->SetScale(XMFLOAT3(Constants::Stage::BARRICADE_WIDTH, Constants::Stage::BARRICADE_HEIGHT, Constants::Stage::BARRICADE_THICKNESS));

        // プレイヤー初期位置と向き（南から北向き +Z 方向を見据える）
        player->SetPosition(XMFLOAT3(0.0f, -0.5f, Constants::Stage::PLAYER_START_POS_Z));
        player->SetRotation(XMFLOAT3(0.0f, 0.0f, 0.0f));

        // カメラもプレイヤーの背後から敵陣を見据える初期アングル（北向き）に設定
        if (g_Camera) {
            g_Camera->SetAngles(0.2f, 0.0f);
        }

        // タクティカル敵陣形のスポーン
        int totalEnemies = 0;

        // 【前衛】プレイヤー正面・掴みやすいモブ & サンドバッグ
        // 1. サンドバッグ（中央手前：開幕で掴んで振り回す用）
        Enemy* sandbag = Manager::AddGameObject<Enemy>();
        sandbag->SetPosition(XMFLOAT3(0.0f, -0.5f, -8.0f));
        sandbag->SetSandbag(true, 300);
        totalEnemies++;

        // 2. 前衛通常モブ（左右）
        Enemy* frontL = Manager::AddGameObject<Enemy>();
        frontL->SetPosition(XMFLOAT3(-5.0f, -0.5f, -7.0f));
        totalEnemies++;

        Enemy* frontR = Manager::AddGameObject<Enemy>();
        frontR->SetPosition(XMFLOAT3(5.0f, -0.5f, -7.0f));
        totalEnemies++;

        // 【中衛】属性部隊（爆発敵 ＆ 電撃敵 ＆ 護衛）
        // 3. 爆発Affix（赤く点滅：投げると12m大爆発）
        Enemy* exp1 = Manager::AddGameObject<Enemy>();
        exp1->SetPosition(XMFLOAT3(-4.0f, -0.5f, 2.0f));
        exp1->SetExplosive(true);
        totalEnemies++;

        Enemy* exp2 = Manager::AddGameObject<Enemy>();
        exp2->SetPosition(XMFLOAT3(4.0f, -0.5f, 2.0f));
        exp2->SetExplosive(true);
        totalEnemies++;

        // 4. 電撃Affix（水色グロー：投げると最大5体連鎖電撃）
        Enemy* lig1 = Manager::AddGameObject<Enemy>();
        lig1->SetPosition(XMFLOAT3(-9.0f, -0.5f, 4.0f));
        lig1->SetLightning(true);
        totalEnemies++;

        Enemy* lig2 = Manager::AddGameObject<Enemy>();
        lig2->SetPosition(XMFLOAT3(9.0f, -0.5f, 4.0f));
        lig2->SetLightning(true);
        totalEnemies++;

        // 5. 中衛護衛モブ
        Enemy* midGuard1 = Manager::AddGameObject<Enemy>();
        midGuard1->SetPosition(XMFLOAT3(-2.0f, -0.5f, 0.0f));
        totalEnemies++;

        Enemy* midGuard2 = Manager::AddGameObject<Enemy>();
        midGuard2->SetPosition(XMFLOAT3(2.0f, -0.5f, 0.0f));
        totalEnemies++;

        // 【後衛】遠距離シューター部隊（AttackingEnemy 4体 ＆ 奥護衛モブ）
        // 6. 柱奥のシューター
        AttackingEnemy* shooterL = Manager::AddGameObject<AttackingEnemy>();
        shooterL->SetPosition(XMFLOAT3(-12.0f, -0.5f, 15.0f));
        totalEnemies++;

        AttackingEnemy* shooterR = Manager::AddGameObject<AttackingEnemy>();
        shooterR->SetPosition(XMFLOAT3(12.0f, -0.5f, 15.0f));
        totalEnemies++;

        // 7. 中央奥のシューター
        AttackingEnemy* shooterC1 = Manager::AddGameObject<AttackingEnemy>();
        shooterC1->SetPosition(XMFLOAT3(-5.0f, -0.5f, 18.0f));
        totalEnemies++;

        AttackingEnemy* shooterC2 = Manager::AddGameObject<AttackingEnemy>();
        shooterC2->SetPosition(XMFLOAT3(5.0f, -0.5f, 18.0f));
        totalEnemies++;

        // 8. 最奥の護衛モブ
        Enemy* backGuard1 = Manager::AddGameObject<Enemy>();
        backGuard1->SetPosition(XMFLOAT3(0.0f, -0.5f, 14.0f));
        totalEnemies++;

        Enemy* backGuard2 = Manager::AddGameObject<Enemy>();
        backGuard2->SetPosition(XMFLOAT3(0.0f, -0.5f, 22.0f));
        totalEnemies++;

        GameRule::SetTotalEnemies(totalEnemies);
    }

    // 各種アイテムを生成
    Item* itemVacuum = Manager::AddGameObject<Item>();
    itemVacuum->SetItemType(ItemType::VACUUM);

    Item* itemGigant = Manager::AddGameObject<Item>();
    itemGigant->SetItemType(ItemType::GIGANT);

    Item* itemLightning = Manager::AddGameObject<Item>();
    itemLightning->SetItemType(ItemType::LIGHTNING);

    if (Manager::IsBossStage()) {
        itemVacuum->SetPosition(XMFLOAT3(0.0f, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_BOSS_VACUUM_Z));
        itemGigant->SetPosition(XMFLOAT3(Constants::Stage::ITEM_BOSS_GIGANT_X, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_BOSS_GIGANT_Z));
        itemLightning->SetPosition(XMFLOAT3(Constants::Stage::ITEM_BOSS_LIGHTNING_X, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_BOSS_LIGHTNING_Z));
    } else {
        itemVacuum->SetPosition(XMFLOAT3(0.0f, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_NORMAL_VACUUM_Z));
        itemGigant->SetPosition(XMFLOAT3(Constants::Stage::ITEM_NORMAL_GIGANT_X, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_NORMAL_GIGANT_Z));
        itemLightning->SetPosition(XMFLOAT3(Constants::Stage::ITEM_NORMAL_LIGHTNING_X, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_NORMAL_LIGHTNING_Z));
    }

    if (startBoss) {
        Manager::TriggerBossSpawnCutscene();
    }
}

// =================================================================
// 終了処理
// =================================================================
void GameplayScene::Uninit()
{
    Manager::SetLowHPWarning(false); // 警告赤パルスを解除
    EventSystem::Clear(); // イベントリスナーのクリア
}

// =================================================================
// フレーム更新
// =================================================================
void GameplayScene::Update()
{
    // 1. コンボタイマーの更新
    if (m_ComboTimer > 0) {
        m_ComboTimer--;
        if (m_ComboTimer <= 0) {
            m_ComboCount = 0; // コンボ切れ
        }
    }

    // 2. プレイヤーのHP監視による瀕死警告（赤パルス）ON/OFF
    Player* player = Manager::GetGameObject<Player>();
    if (player) {
        if (player->GetHP() <= 0) {
            GameRule::StopTimer();
            ProgressionManager::CommitSession(false); // 獲得素材を保存
            Manager::SetLowHPWarning(false);
            Manager::ChangeScene(Scene::GAMEOVER);
            return;
        }
        // ボス撃破後（クリアディレイ中含む）またはゲームクリア後は警告点滅を解除する
        bool isClearOrDefeated = GameRule::IsGameClear() || (m_ClearDelayTimer > 0);
        bool isLowHP = (player->GetHP() == 1) && !isClearOrDefeated;
        Manager::SetLowHPWarning(isLowHP);
    } else {
        Manager::SetLowHPWarning(false);
    }

    // 3. タイムアタックタイマーのフレーム更新（クリア中は停止）
    if (!GameRule::IsGameClear() && m_ClearDelayTimer == 0) {
        GameRule::UpdateTimer();
    }

    // ゲームプレイ本体の更新ロジック実行
    UpdateGameplay();
}

// =================================================================
// ゲームプレイの更新処理の実体（旧 Manager::UpdateGameplay）
// =================================================================
void GameplayScene::UpdateGameplay()
{
    // スローモーションタイマーの更新
    if (Manager::GetSlowMotionTimer() > 0) {
        Manager::SetSlowMotionTimer(Manager::GetSlowMotionTimer() - 1);
    }

    // スローモーション中は更新頻度を 1/5 に間引く
    bool updateOthers = true;
    if (Manager::GetSlowMotionTimer() > 0) {
        updateOthers = (Manager::GetSlowMotionTimer() % 5 == 0);
    }

    // 衝撃波システムの更新
    if (updateOthers) {
        ShockwaveSystem::Update();
    }

    // ゲームクリア後は更新を行わない
    if (GameRule::IsGameClear()) return;

    // ヒットストップ中は他の更新をスキップ
    if (Manager::GetHitStopFrames() > 0) {
        Manager::SetHitStopFrames(Manager::GetHitStopFrames() - 1);
        return;
    }

    Player* player = Manager::GetGameObject<Player>();

    // Update list iteration
    const std::vector<GameObject*>& updateList = Manager::GetUpdateObjectList();
    for (size_t i = 0; i < updateList.size(); i++) {
        GameObject* obj = updateList[i];
        if (!obj) continue;

        bool shouldUpdate = true;
        if (!updateOthers) {
            ObjectType type = obj->GetObjectType();
            bool isGrabbedEnemy = (player && player->GetGrabbedEnemy() == obj);
            bool isSandbag = (type == ObjectType::Enemy && static_cast<Enemy*>(obj)->IsSandbag());

            if (type != ObjectType::Player && !isGrabbedEnemy && !isSandbag && !obj->IgnoreSlowMotion()) {
                shouldUpdate = false;
            }
        }

        if (shouldUpdate) {
            obj->Update();
        }
    }

    // 破棄対象オブジェクトを一括クリーンアップ
    Manager::DestroyObjectsIf();

    // 衝突判定システムの実行
    if (updateOthers) {
        CollisionSystem::Update();
    }

    // つかみ位置の同期（LateUpdate）：スローモーション中でも毎フレーム実行してガクつきを防ぐ
    if (player) {
        if (player->GetState() == PlayerState::GRABBED || player->GetState() == PlayerState::SPINNING) {
            Enemy* grabbedEnemy = player->GetGrabbedEnemy();
            if (grabbedEnemy) {
                Collision::ResolveGrabPhysics(player, grabbedEnemy, 0.8f);
            }
        }
    }

    // Stage clear checks
    if (!Manager::IsBossStage()) {
        // Normal stage: monitor all enemies
        bool enemyExists = false; 
        for (GameObject* obj : Manager::GetCategoryList(ObjectType::Enemy)) {
            if (obj && !obj->IsDestroy()) {
                Enemy* enemy = static_cast<Enemy*>(obj);
                if (enemy->GetEnemyState() != EnemyState::DEFEATED) {
                    enemyExists = true;
                    break;
                }
            }
        }

        if (!enemyExists) {
            Manager::TransitionToBossStage();
        }
    } else {
        // Boss stage: monitor boss defeat
        if (!GameRule::IsGameClear()) {
            BossEnemy* boss = Manager::GetGameObject<BossEnemy>();
            bool bossExists = (boss && !boss->IsDestroy() && boss->GetEnemyState() != EnemyState::DEFEATED);

            if (!bossExists) {
                // ボス撃破直後にタイマーを確定停止＆低HP警告解除
                if (m_ClearDelayTimer == 0) {
                    GameRule::StopTimer();
                    Manager::SetLowHPWarning(false);
                    // スローモーション発動（同期保証）
                    if (!Manager::IsSlowMotionActive()) {
                        Manager::StartSlowMotion(Constants::Boss::DEFEAT_SLOW_MOTION_DURATION);
                    }
                }
                // ボス撃破後、ディレイ時間を経てリザルト画面に遷移する
                m_ClearDelayTimer++;

                // 撃破後の画面点滅演出（ディレイ期間中、勝利の余韻パルス点滅）
                if (m_ClearDelayTimer >= 20 && m_ClearDelayTimer < Constants::Boss::CLEAR_DELAY_FRAMES - 15) {
                    // サイン波によるリズミカルな金〜白の明滅
                    float pulse = 0.5f + 0.5f * sinf((float)m_ClearDelayTimer * 0.22f);
                    float pulseAlpha = 0.08f + 0.22f * pulse;
                    // 白〜黄金色の勝利フラッシュパルス
                    XMFLOAT4 pulseColor(1.0f, 0.95f, 0.7f, pulseAlpha);
                    Manager::TriggerFlash(pulseColor, 0.06f);
                }

                if (m_ClearDelayTimer >= Constants::Boss::CLEAR_DELAY_FRAMES) {
                    GameRule::SetGameClear(true);
                    ProgressionManager::CommitSession(true); // クリア確定＆素材・討伐数セーブ
                    Manager::ChangeScene(Scene::CLEAR);
                    LOG_INFO("[GameRule] *** ボス撃破！ゲームクリア！ ***\n");
                }
            } else {
                m_ClearDelayTimer = 0;
            }
        }
    }
}

// =================================================================
// 描画
// =================================================================
void GameplayScene::Draw()
{
}
