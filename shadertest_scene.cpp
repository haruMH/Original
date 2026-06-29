#include "shadertest_scene.h"
#include "manager.h"
#include "input.h"
#include "player.h"
#include "wall.h"
#include "item.h"
#include "field.h"
#include "collision_system.h"
#include "game_rule.h"
#include "shockwave.h"

// 前方宣言のクラスの生成用
#include "camera.h"

using namespace DirectX;

// Skybox のヘッダーが含まれていないため、GameObjectを生成する形になりますが、
// 元のコードでは AddGameObject<Skybox>() となっていました。
// Skybox は gameobject.h などで定義されているか、あるいは未知のオブジェクトタイプ
// ObjectType::Unknown として定義されている可能性があります。
// 元の manager.cpp では AddGameObject<Skybox>() となっていたため、そのまま呼び出します。
// そのための前方宣言を定義します。
class Skybox;

// =================================================================
// コンストラクタ / デストラクタ
// =================================================================
ShaderTestScene::ShaderTestScene()
{
}

ShaderTestScene::~ShaderTestScene()
{
}

// =================================================================
// 初期化
// =================================================================
void ShaderTestScene::Init()
{
    GameRule::Init(); // スコアや敵数、ゲームクリア状態などのリセット
    
    Manager::m_HitStopFrames = 0;
    Manager::m_SlowMotionTimer = 0;
    Manager::m_SlowMotionDuration = 0;

    // 1. スカイボックス（空）の生成
    Manager::AddGameObject<Skybox>();

    // 2. 鏡面反射確認用のオブジェクト（アイテムなど）を生成
    Item* item1 = Manager::AddGameObject<Item>();
    item1->SetPosition(XMFLOAT3(-0.6f, 0.0f, 3.0f));
    item1->SetItemType(ItemType::VACUUM);

    Item* item2 = Manager::AddGameObject<Item>();
    item2->SetPosition(XMFLOAT3(0.0f, 0.0f, 3.0f));
    item2->SetItemType(ItemType::GIGANT);

    Item* item3 = Manager::AddGameObject<Item>();
    item3->SetPosition(XMFLOAT3(0.6f, 0.0f, 3.0f));
    item3->SetItemType(ItemType::LIGHTNING);

    // 3. 地面オブジェクトの生成
    Manager::AddGameObject<Field>();

    // 4. 壁オブジェクトの生成（シェーダーテスト用）
    Wall* wall = Manager::AddGameObject<Wall>();
    wall->SetPosition(XMFLOAT3(0.0f, 0.0f, 5.0f));
    wall->SetScale(XMFLOAT3(2.0f, 2.0f, 2.0f));
    wall->SetShaderTest(true);
    Manager::m_UpdateObjects.push_back(wall);

    GameRule::SetTotalEnemies(0); // 敵は存在しない
}

// =================================================================
// 終了処理
// =================================================================
void ShaderTestScene::Uninit()
{
}

// =================================================================
// 毎フレーム更新処理
// =================================================================
void ShaderTestScene::Update()
{
    // ESCキーが押されたらタイトルに戻る
    if (Input::GetKeyTrigger(VK_ESCAPE)) {
        Manager::ChangeScene(Scene::TITLE);
        return;
    }

    // 毎フレームのオブジェクト更新
    UpdateShaderTest();
}

// =================================================================
// 特殊ステージ用の毎フレーム更新処理
// =================================================================
void ShaderTestScene::UpdateShaderTest()
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

    // ヒットストップ中は他の更新をスキップ
    if (Manager::m_HitStopFrames > 0) {
        Manager::m_HitStopFrames--;
        return;
    }

    Player* player = Manager::GetGameObject<Player>();

    // 全オブジェクトの更新と破棄の管理
    for (size_t i = 0; i < Manager::m_UpdateObjects.size(); ) {
        GameObject* obj = Manager::m_UpdateObjects[i];
        bool shouldUpdate = true;

        if (obj->GetObjectType() != ObjectType::Player) {
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
}

// =================================================================
// 描画
// =================================================================
void ShaderTestScene::Draw()
{
}
