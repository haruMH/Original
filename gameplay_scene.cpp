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
// �R���X�g���N�^ / �f�X�g���N�^
// =================================================================
GameplayScene::GameplayScene()
{
}

GameplayScene::~GameplayScene()
{
}

// =================================================================
// ������
// =================================================================
void GameplayScene::Init()
{
    // �Q�[���v���C�{�҂̏�����
    GameRule::Init(); // �X�R�A��G���A�Q�[���N���A��Ԃ̃��Z�b�g
    
    Manager::m_HitStopFrames = 0;
    Manager::m_SlowMotionTimer = 0;
    Manager::m_SlowMotionDuration = 0;
    m_ClearDelayTimer = 0;

    // 1. �n�ʃI�u�W�F�N�g�̐���
    Manager::AddGameObject<Field>();

    // 2. �v���C���[�I�u�W�F�N�g�̐���
    Player* player = Manager::AddGameObject<Player>();
    player->SetPosition(XMFLOAT3(0.0f, -0.5f, 0.0f));

    // �{�X����J�n���ʏ킩��J�n���̕���
    if (Constants::Debug::START_FROM_BOSS) {
        Manager::m_IsBossStage = true;
        
        // �{�X�����̕ǂ𐶐�
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
        Manager::m_IsBossStage = false;
        
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

    if (Manager::m_IsBossStage) {
        itemVacuum->SetPosition(XMFLOAT3(0.0f, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_BOSS_VACUUM_Z));
        itemGigant->SetPosition(XMFLOAT3(Constants::Stage::ITEM_BOSS_GIGANT_X, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_BOSS_GIGANT_Z));
        itemLightning->SetPosition(XMFLOAT3(Constants::Stage::ITEM_BOSS_LIGHTNING_X, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_BOSS_LIGHTNING_Z));
    } else {
        itemVacuum->SetPosition(XMFLOAT3(0.0f, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_NORMAL_VACUUM_Z));
        itemGigant->SetPosition(XMFLOAT3(Constants::Stage::ITEM_NORMAL_GIGANT_X, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_NORMAL_GIGANT_Z));
        itemLightning->SetPosition(XMFLOAT3(Constants::Stage::ITEM_NORMAL_LIGHTNING_X, Constants::Stage::ITEM_SPAWN_Y, Constants::Stage::ITEM_NORMAL_LIGHTNING_Z));
    }
}

// =================================================================
// �I������
// =================================================================
void GameplayScene::Uninit()
{
}

// =================================================================
// ���t���[���X�V����
// =================================================================
void GameplayScene::Update()
{
    // �v���C���[�̐����m�F
    Player* player = Manager::GetGameObject<Player>();
    if (player && player->GetHP() <= 0) {
        Manager::ChangeScene(Scene::GAMEOVER);
        return;
    }

    // �Q�[���v���C�{�҂̕����E�X�V���W�b�N����s
    UpdateGameplay();
}

// =================================================================
// �Q�[���v���C�̍X�V�����̎��́i�� Manager::UpdateGameplay�j
// =================================================================
void GameplayScene::UpdateGameplay()
{
    // �X���[���[�V�����^�C�}�[�̍X�V
    if (Manager::m_SlowMotionTimer > 0) {
        Manager::m_SlowMotionTimer--;
    }

    // �X���[���[�V�������͍X�V�p�x�� 1/5 �ɊԈ���
    bool updateOthers = true;
    if (Manager::m_SlowMotionTimer > 0) {
        updateOthers = (Manager::m_SlowMotionTimer % 5 == 0);
    }

    // �Ռ��g�V�X�e���̍X�V
    if (updateOthers) {
        ShockwaveSystem::Update();
    }

    // �Q�[���N���A��͍X�V��s��Ȃ�
    if (GameRule::IsGameClear()) return;

    // �q�b�g�X�g�b�v���͑��̍X�V��X�L�b�v
    if (Manager::m_HitStopFrames > 0) {
        Manager::m_HitStopFrames--;
        return;
    }

    Player* player = Manager::GetGameObject<Player>();

    // �S�I�u�W�F�N�g�̍X�V�Ɣj���̊Ǘ�
    for (size_t i = 0; i < Manager::m_UpdateObjects.size(); i++) {
        GameObject* obj = Manager::m_UpdateObjects[i];
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

    // �X�e�[�W�N���A�E�i�s�x�̊Ď�
    if (!Manager::m_IsBossStage) {
        // �ʏ�X�e�[�W�F���u�G�̑S�ŊĎ�
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
        // �{�X�X�e�[�W�F�{�X���j�̊Ď�
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
                // �{�X�S�Ō�A�f�B���C������Ă��烊�U���g��ʂɑJ�ڂ���
                m_ClearDelayTimer++;
                if (m_ClearDelayTimer >= Constants::Boss::CLEAR_DELAY_FRAMES) {
                    GameRule::SetGameClear(true);
                    Manager::ChangeScene(Scene::CLEAR);
                    OutputDebugStringA("[GameRule] *** �{�X���j�I�Q�[���N���A! ***\n");
                }
            } else {
                m_ClearDelayTimer = 0;
            }
        }
    }
}

// =================================================================
// �`��
// =================================================================
void GameplayScene::Draw()
{
}
