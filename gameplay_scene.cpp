#include "gameplay_scene.h"
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
// 初期化
// =================================================================
void GameplayScene::Init()
{
    // ゲームプレイ本編の初期化
    GameRule::Init(); // スコアや敵数、ゲームクリア状態のリセット
    
    Manager::m_HitStopFrames = 0;
    Manager::m_SlowMotionTimer = 0;
    Manager::m_SlowMotionDuration = 0;
    m_ClearDelayTimer = 0;

    // 1. 地面オブジェクトの生成
    Manager::AddGameObject<Field>();

    // 2. プレイヤーオブジェクトの生成
    Player* player = Manager::AddGameObject<Player>();
    player->SetPosition(XMFLOAT3(0.0f, -0.5f, 0.0f));

    // ボスから開始か通常から開始かの分岐
    if (Constants::Debug::START_FROM_BOSS) {
        Manager::m_IsBossStage = true;
        
        // ボス部屋の壁を生成
        float roomSize = Constants::Stage::BOSS_ROOM_SIZE;
        Wall* wallN = Manager::AddGameObject<Wall>();
        wallN->SetPosition(XMFLOAT3(0.0f, 1.5f, roomSize));
        wallN->SetScale(XMFLOAT3(roomSize * 2.0f, 5.0f, 1.0f));
        
        Wall* wallS = Manager::AddGameObject<Wall>();
        wallS->SetPosition(XMFLOAT3(0.0f, 1.5f, -roomSize));
        wallS->SetScale(XMFLOAT3(roomSize * 2.0f, 5.0f, 1.0f));
        
        Wall* wallE = Manager::AddGameObject<Wall>();
        wallE->SetPosition(XMFLOAT3(roomSize, 1.5f, 0.0f));
        wallE->SetScale(XMFLOAT3(1.0f, 5.0f, roomSize * 2.0f));
        
        Wall* wallW = Manager::AddGameObject<Wall>();
        wallW->SetPosition(XMFLOAT3(-roomSize, 1.5f, 0.0f));
        wallW->SetScale(XMFLOAT3(1.0f, 5.0f, roomSize * 2.0f));

        // ボスエネミーの生成
        BossEnemy* boss = Manager::AddGameObject<BossEnemy>();
        boss->SetPosition(XMFLOAT3(0.0f, 1.5f, Constants::Stage::BOSS_SPAWN_OFFSET_Z));

        GameRule::SetTotalEnemies(1);
    } else {
        Manager::m_IsBossStage = false;
        
        // 通常ステージ：壁を敵エリアの外側に配置
        Wall* wall = Manager::AddGameObject<Wall>();
        wall->SetPosition(XMFLOAT3(-8.0f, 1.5f, -7.0f));
        wall->SetScale(XMFLOAT3(5.0f, 5.0f, 5.0f));

        // プレイヤーを初期位置へ
        player->SetPosition(XMFLOAT3(0.0f, -0.5f, Constants::Stage::PLAYER_START_POS_Z));

        // モブ敵をグリッド配置
        int totalEnemies = 0;
        int minX = -Constants::Stage::ENEMY_GRID_COLS / 2;
        int maxX = minX + Constants::Stage::ENEMY_GRID_COLS - 1;
        int minZ = -Constants::Stage::ENEMY_GRID_ROWS / 2;
        int maxZ = minZ + Constants::Stage::ENEMY_GRID_ROWS - 1;

        for (int x = minX; x <= maxX; x++) {
            for (int z = minZ; z <= maxZ; z++) {
                Enemy* enemy = nullptr;
                if ((x + z) % 2 == 0) {
                    enemy = Manager::AddGameObject<AttackingEnemy>();
                } else {
                    enemy = Manager::AddGameObject<Enemy>();
                }
                float posX = (float)x * 3.2f + 1.0f;
                float posZ = (float)z * 3.2f - 7.0f;
                enemy->SetPosition(XMFLOAT3(posX, -0.5f, posZ));
                totalEnemies++;
            }
        }
        GameRule::SetTotalEnemies(totalEnemies);
    }

    // 各種アイテムを生成
    Item* itemVacuum = Manager::AddGameObject<Item>();
    itemVacuum->SetItemType(ItemType::VACUUM);

    Item* itemGigant = Manager::AddGameObject<Item>();
    itemGigant->SetItemType(ItemType::GIGANT);

    Item* itemLightning = Manager::AddGameObject<Item>();
    itemLightning->SetItemType(ItemType::LIGHTNING);

    if (Manager::m_IsBossStage) {
        itemVacuum->SetPosition(XMFLOAT3(0.0f, 0.5f, -15.0f));
        itemGigant->SetPosition(XMFLOAT3(-4.0f, 0.5f, -15.0f));
        itemLightning->SetPosition(XMFLOAT3(4.0f, 0.5f, -15.0f));
    } else {
        itemVacuum->SetPosition(XMFLOAT3(0.0f, 0.5f, 4.0f));
        itemGigant->SetPosition(XMFLOAT3(-4.0f, 0.5f, 4.0f));
        itemLightning->SetPosition(XMFLOAT3(2.0f, 0.5f, 6.0f));
    }
}

// =================================================================
// 終了処理
// =================================================================
void GameplayScene::Uninit()
{
}

// =================================================================
// 毎フレーム更新処理
// =================================================================
void GameplayScene::Update()
{
    // プレイヤーの生存確認
    Player* player = Manager::GetGameObject<Player>();
    if (player && player->GetHP() <= 0) {
        Manager::ChangeScene(Scene::GAMEOVER);
        return;
    }

    // ゲームプレイ本編の物理・更新ロジックを実行
    UpdateGameplay();
}

// =================================================================
// ゲームプレイの更新処理の実体（旧 Manager::UpdateGameplay）
// =================================================================
void GameplayScene::UpdateGameplay()
{
    // スローモーションタイマーの更新
    if (Manager::m_SlowMotionTimer > 0) {
        Manager::m_SlowMotionTimer--;
    }

    // スローモーション中は更新頻度を 1/5 に間引く
    bool updateOthers = true;
    if (Manager::m_SlowMotionTimer > 0) {
        updateOthers = (Manager::m_SlowMotionTimer % 5 == 0);
    }

    // 衝撃波システムの更新
    if (updateOthers) {
        ShockwaveSystem::Update();
    }

    // ゲームクリア後は更新を行わない
    if (GameRule::IsGameClear()) return;

    // ヒットストップ中は他の更新をスキップ
    if (Manager::m_HitStopFrames > 0) {
        Manager::m_HitStopFrames--;
        return;
    }

    Player* player = Manager::GetGameObject<Player>();

    // 全オブジェクトの更新と破棄の管理
    std::vector<GameObject*> nextUpdateObjects;
    nextUpdateObjects.reserve(Manager::m_UpdateObjects.size());

    for (size_t i = 0; i < Manager::m_UpdateObjects.size(); ) {
        GameObject* obj = Manager::m_UpdateObjects[i];
        bool shouldUpdate = true;

        // プレイヤー、掴まれているエネミー、およびサンドバッグ（元弾）は毎フレーム更新する（スロー中の挙動バグを防ぐ）
        bool isGrabbedEnemy = (player && player->GetGrabbedEnemy() == obj);
        bool isSandbag = false;
        if (obj->GetObjectType() == ObjectType::Enemy) {
            Enemy* enemy = static_cast<Enemy*>(obj);
            if (enemy->IsSandbag()) {
                isSandbag = true;
            }
        }

        if (obj->GetObjectType() != ObjectType::Player && !isGrabbedEnemy && !isSandbag) {
            shouldUpdate = updateOthers;
        }

        if (shouldUpdate) {
            obj->Update();
        }

        if (obj->IsDestroy()) {
            if (player) {
                player->NotifyObjectDestroyed(obj);
            }
            if (obj == Manager::m_CachedPlayer) {
                Manager::m_CachedPlayer = nullptr;
            }
            Manager::UnregisterCategory(obj);
            obj->Uninit();
            delete obj;

            // O(1)スワップ＆ポップ消去法
            if (i != Manager::m_UpdateObjects.size() - 1) {
                Manager::m_UpdateObjects[i] = Manager::m_UpdateObjects.back();
            }
            Manager::m_UpdateObjects.pop_back();

            // 描画用リストからも削除
            for (size_t j = 0; j < Manager::m_GameObjects.size(); j++) {
                if (Manager::m_GameObjects[j] == obj) {
                    if (j != Manager::m_GameObjects.size() - 1) {
                        Manager::m_GameObjects[j] = Manager::m_GameObjects.back();
                    }
                    Manager::m_GameObjects.pop_back();
                    break;
                }
            }
        } else {
            i++;
        }
    }

    // 衝突判定システムの実行
    if (updateOthers) {
        CollisionSystem::Update();
    }

    // つかみ位置の同期（LateUpdate）：スローモーション中でも毎フレーム同期してガクつきを防ぐ
    if (player) {
        if (player->GetState() == PlayerState::GRABBED || player->GetState() == PlayerState::SPINNING) {
            Enemy* grabbedEnemy = player->GetGrabbedEnemy();
            if (grabbedEnemy) {
                Collision::ResolveGrabPhysics(player, grabbedEnemy, 0.8f);
            }
        }
    }

    // ステージクリア・進行度の監視
    if (!Manager::m_IsBossStage) {
        // 通常ステージ：モブ敵の全滅監視
        bool attackingEnemyExists = false; 
        for (GameObject* obj : Manager::m_GameObjects) {
            if (obj && obj->GetObjectType() == ObjectType::Enemy) {
                Enemy* enemy = static_cast<Enemy*>(obj);
                if (enemy->IsAttackingEnemy() && enemy->GetEnemyState() != EnemyState::DEFEATED) {
                    attackingEnemyExists = true;
                    break;
                }
            }
        }

        if (!attackingEnemyExists) {
            Manager::TransitionToBossStage();
        }
    } else {
        // ボスステージ：ボス撃破の監視
        if (!GameRule::IsGameClear()) {
            bool bossExists = false;
            for (GameObject* obj : Manager::m_GameObjects) {
                if (obj->GetObjectType() == ObjectType::Boss) {
                    Enemy* boss = static_cast<Enemy*>(obj);
                    if (boss->GetEnemyState() != EnemyState::DEFEATED) {
                        bossExists = true;
                        break;
                    }
                }
            }

            if (!bossExists) {
                // ボス全滅後、ディレイをかけてからリザルト画面に遷移する
                m_ClearDelayTimer++;
                if (m_ClearDelayTimer >= Constants::Boss::CLEAR_DELAY_FRAMES) {
                    GameRule::SetGameClear(true);
                    Manager::ChangeScene(Scene::CLEAR);
                    OutputDebugStringA("[GameRule] *** ボス撃破！ゲームクリア! ***\n");
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
