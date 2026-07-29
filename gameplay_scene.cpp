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

using namespace DirectX;

// =================================================================
// �R���X�g���N�^ / �f�X�g���N�^
// =================================================================
GameplayScene::GameplayScene()
{
}

GameplayScene::~GameplayScene()
{
}

// =================================================================
// 
// =================================================================
void GameplayScene::Init()
{
    // イベント購読の登録
    EventSystem::Subscribe<PlayerHitEvent>([](const PlayerHitEvent& ev) {
        if (g_Camera) g_Camera->Shake(0.35f, 12);
        Manager::AddHitStop(5);
        Manager::TriggerFlash(XMFLOAT4(1.0f, 0.0f, 0.0f, 0.4f), 0.04f); // 被弾時に赤いフラッシュ
    });

    EventSystem::Subscribe<EnemyDefeatedEvent>([this](const EnemyDefeatedEvent& ev) {
        m_ComboTimer = 120; // 2秒間コンボ維持
        m_ComboCount++;

        int finalScore = ev.scoreValue;
        if (m_ComboCount >= 3) {
            // 3コンボ以上でスコア倍率を適用 (x1.3, x1.4...)
            float multiplier = 1.0f + (m_ComboCount * 0.1f);
            finalScore = static_cast<int>(ev.scoreValue * multiplier);
        }

        GameRule::OnEnemyDefeated(finalScore);
        ScorePopupSystem::AddPopup(ev.position.x, ev.position.y, ev.position.z, finalScore, ev.popupColor.x, ev.popupColor.y, ev.popupColor.z);

        if (m_ComboCount >= 3) {
            // コンボ数を金色ポップアップで通知
            ScorePopupSystem::AddPopup(ev.position.x, ev.position.y + 0.8f, ev.position.z, m_ComboCount, 2.5f, 1.8f, 0.0f);
            Manager::AddHitStop(3); // コンボ時の手応えヒットストップ
            if (g_Camera) g_Camera->Shake(0.15f, 6);
        }
    });

    EventSystem::Subscribe<BossHitEvent>([](const BossHitEvent& ev) {
        Manager::AddHitStop(12);
        if (g_Camera) g_Camera->Shake(0.40f, 12);
        Manager::TriggerFlash(XMFLOAT4(1.0f, 0.8f, 0.0f, 0.3f), 0.05f); // ボス被弾時に黄色閃光
    });

    EventSystem::Subscribe<PlayerParriedEvent>([](const PlayerParriedEvent& ev) {
        Manager::TriggerFlash(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.02f); // パリィ成功時に真っ白閃光
        Manager::StartSlowMotion(90); // 1.5秒のスローモーション (彩度補正ON)
        Manager::AddHitStop(15);      // 手応えヒットストップ
    });

    EventSystem::Subscribe<BossSpawnEvent>([](const BossSpawnEvent& ev) {
        if (g_Camera) {
            XMFLOAT3 eye = ev.bossPosition;
            eye.y += 6.0f;  // ボスの巨大さ(スケール5)に合わせて十分な高さに配置
            eye.z -= 17.0f; // ボスがはみ出さないように十分な距離(17m)まで手前に引く
            XMFLOAT3 target = ev.bossPosition;
            target.y += 2.5f; // 巨大ボスの中心高さ(胸元)を注視点にする
            g_Camera->SetCutsceneMode(true, eye, target);
        }
    });

    EventSystem::Subscribe<BossBattleStartEvent>([](const BossBattleStartEvent& ev) {
        if (g_Camera) {
            g_Camera->SetCutsceneMode(false);
            g_Camera->Shake(0.5f, 15); // 戦闘開始時の咆哮の余韻シェイク
        }
    });

    // Q[vC{҂̏
    GameRule::Init(); // XRAGAQ[NAԂ̃Zbg
    
    Manager::SetHitStopFrames(0);
    Manager::SetSlowMotionTimer(0);
    Manager::SetSlowMotionDuration(0);
    m_ClearDelayTimer = 0;

    // 1. nʃIuWFNg̐
    Manager::AddGameObject<Field>();

    // 2. vC[IuWFNg̐
    Player* player = Manager::AddGameObject<Player>();
    player->SetPosition(XMFLOAT3(0.0f, -0.5f, 0.0f));

    // {XJnʏ킩Jn̕
    if (Constants::Debug::START_FROM_BOSS) {
        Manager::SetIsBossStage(true);
        
        // {X̕ǂ𐶐
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

        // �{�X�G�l�~�[�̐���
        BossEnemy* boss = Manager::AddGameObject<BossEnemy>();
        boss->SetPosition(XMFLOAT3(0.0f, 1.5f, Constants::Stage::BOSS_SPAWN_OFFSET_Z));

        GameRule::SetTotalEnemies(1);
    } else {
        Manager::SetIsBossStage(false);
        
        // �ʏ�X�e�[�W�F�ǂ�G�G���A�̊O���ɔz�u
        Wall* wall = Manager::AddGameObject<Wall>();
        wall->SetPosition(XMFLOAT3(Constants::Stage::WALL_DEFAULT_POS_X, Constants::Stage::WALL_DEFAULT_POS_Y, Constants::Stage::WALL_DEFAULT_POS_Z));
        wall->SetScale(XMFLOAT3(Constants::Stage::WALL_DEFAULT_SCALE, Constants::Stage::WALL_DEFAULT_SCALE, Constants::Stage::WALL_DEFAULT_SCALE));

        // �v���C���[������ʒu��
        player->SetPosition(XMFLOAT3(0.0f, -0.5f, Constants::Stage::PLAYER_START_POS_Z));

        // ���u�G��O���b�h�z�u
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
                float posX = (float)x * Constants::Stage::ENEMY_GRID_SPAWN_INTERVAL_XZ + Constants::Stage::ENEMY_GRID_SPAWN_OFFSET_X;
                float posZ = (float)z * Constants::Stage::ENEMY_GRID_SPAWN_INTERVAL_XZ + Constants::Stage::ENEMY_GRID_SPAWN_OFFSET_Z;
                enemy->SetPosition(XMFLOAT3(posX, -0.5f, posZ));
                totalEnemies++;
            }
        }
        GameRule::SetTotalEnemies(totalEnemies);
    }

    // �e��A�C�e���𐶐�
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

    if (Constants::Debug::START_FROM_BOSS) {
        Manager::TriggerBossSpawnCutscene();
    }
}

// =================================================================
// �I������
// =================================================================
void GameplayScene::Uninit()
{
    EventSystem::Clear(); // イベントリスナーのクリア
}

// =================================================================
// t[XV
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
            Manager::SetLowHPWarning(false);
            Manager::ChangeScene(Scene::GAMEOVER);
            return;
        }
        bool isLowHP = (player->GetHP() == 1);
        Manager::SetLowHPWarning(isLowHP);
    } else {
        Manager::SetLowHPWarning(false);
    }

    // Q[vC{҂̕EXVWbNs
    UpdateGameplay();
}

// =================================================================
// �Q�[���v���C�̍X�V�����̎��́i�� Manager::UpdateGameplay�j
// =================================================================
void GameplayScene::UpdateGameplay()
{
    // �X���[���[�V�����^�C�}�[�̍X�V
    if (Manager::GetSlowMotionTimer() > 0) {
        Manager::SetSlowMotionTimer(Manager::GetSlowMotionTimer() - 1);
    }

    // �X���[���[�V�������͍X�V�p�x�� 1/5 �ɊԈ���
    bool updateOthers = true;
    if (Manager::GetSlowMotionTimer() > 0) {
        updateOthers = (Manager::GetSlowMotionTimer() % 5 == 0);
    }

    // �Ռ��g�V�X�e���̍X�V
    if (updateOthers) {
        ShockwaveSystem::Update();
    }

    // �Q�[���N���A��͍X�V��s��Ȃ�
    if (GameRule::IsGameClear()) return;

    // �q�b�g�X�g�b�v���͑��̍X�V��X�L�b�v
    if (Manager::GetHitStopFrames() > 0) {
        Manager::SetHitStopFrames(Manager::GetHitStopFrames() - 1);
        return;
    }

    Player* player = Manager::GetGameObject<Player>();

    // Update list iteration
    for (size_t i = 0; i < Manager::GetUpdateObjectList().size(); i++) {
        GameObject* obj = Manager::GetUpdateObjectList()[i];
        bool shouldUpdate = true;

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
    }

    // �j���ΏۃI�u�W�F�N�g��ꊇ�N���[���A�b�v
    Manager::DestroyObjectsIf();

    // �Փ˔���V�X�e���̎��s
    if (updateOthers) {
        CollisionSystem::Update();
    }

    // ���݈ʒu�̓����iLateUpdate�j�F�X���[���[�V�������ł���t���[���������ăK�N����h��
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
        // Normal stage: monitor all attacking enemies
        bool attackingEnemyExists = false; 
        for (GameObject* obj : Manager::GetGameObjectList()) {
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
        // Boss stage: monitor boss defeat
        if (!GameRule::IsGameClear()) {
            bool bossExists = false;
            for (GameObject* obj : Manager::GetGameObjectList()) {
                if (obj->GetObjectType() == ObjectType::Boss) {
                    Enemy* boss = static_cast<Enemy*>(obj);
                    if (boss->GetEnemyState() != EnemyState::DEFEATED) {
                        bossExists = true;
                        break;
                    }
                }
            }

            if (!bossExists) {
                // ボス撃破後、ディレイ時間を経てリザルト画面に遷移する
                m_ClearDelayTimer++;
                if (m_ClearDelayTimer >= Constants::Boss::CLEAR_DELAY_FRAMES) {
                    GameRule::SetGameClear(true);
                    Manager::ChangeScene(Scene::CLEAR);
                    OutputDebugStringA("[GameRule] *** ボス撃破！ゲームクリア！ ***\n");
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
