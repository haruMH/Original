#include <algorithm>
#include <vector>
#include <fstream>
#include "hub_scene.h"
#include "localization.h"
#include "fire_effect.h"
#include "ambient_effect.h"
#include "grass_system.h"
#include "hill_terrain.h"
#include "ui_theme.h"
#include "manager.h"
#include "player.h"
#include "enemy.h"
#include "wall.h"
#include "prop_model.h"
#include "field.h"
#include "collision_system.h"
#include "collision.h"
#include "progression_manager.h"
#include "save_system.h"
#include "sound_manager.h"
#include "camera.h"
#include "input.h"
#include "player_controller.h"
#include "game_constants.h"
#include "imgui/imgui.h"

using namespace DirectX;

namespace {
    const XMFLOAT3 FORGE_POS = XMFLOAT3(-12.0f, 0.0f, 13.0f);
    const XMFLOAT3 GATE_POS  = XMFLOAT3(0.0f, 0.0f, 19.0f);   // 道の突き当たり（北端の中央）
    const float INTERACT_RADIUS = 4.0f;
}

HubScene::HubScene()
    : m_IsWorkshopOpen(false)
    , m_IsQuestMenuOpen(false)
    , m_NearFacility(0)
    , m_SelectedAbilityIndex(0)
    , m_SelectedQuestIndex(0)
{
}

HubScene::~HubScene()
{
}

void HubScene::Init()
{
    m_IsWorkshopOpen = false;
    m_IsQuestMenuOpen = false;
    m_NearFacility = 0;
    m_SelectedAbilityIndex = 0;
    m_SelectedQuestIndex = 0;

    // 拠点キャンプ到達時にチュートリアル完了を記録・保存
    SaveSystem::SetTutorialCompleted(true);
    SaveSystem::Save();

    // 1. 地面と空
    Field* field = Manager::AddGameObject<Field>();
    field->SetFloorTexture("cobble.png", "cobble_normal.png");   // 石畳
    field->SetRoad(-3.0f, 3.0f, -22.0f, 22.0f, "road.png", "road_normal.png");   // 中央の道（幅6m、南北に貫く）
    Manager::AddGameObject<Skybox>();   // 空は時間経過で変化する

    // 2. プレイヤー（広場南側からキャンプ全体を見渡せる位置にスポーン）
    Player* player = Manager::AddGameObject<Player>();
    player->SetPosition(XMFLOAT3(0.0f, -0.5f, -6.0f));
    player->SetRotation(XMFLOAT3(0.0f, 0.0f, 0.0f));

    if (g_Camera) {
        g_Camera->SetAngles(0.28f, 0.0f);
    }
    
    // 3. 拠点の外周壁 (44m x 44m) - プレイヤーの落下防止境界（広大な開放感）
    Wall::CreateBoundaryWalls(22.0f, 4.0f, 1.5f, 1.0f, false);   // 当たり判定のみ（見た目は柵と遠景の山）

    // プロップ配置ヘルパー（collSizeが(0,0,0)の場合は当たり判定なし）
    auto AddProp = [](const std::string& obj, const std::string& mtl, const XMFLOAT3& pos, const XMFLOAT3& scale, float rotY = 0.0f, const XMFLOAT3& collSize = XMFLOAT3(0.0f, 0.0f, 0.0f)) -> PropModel* {
        PropModel* prop = Manager::AddGameObject<PropModel>();
        prop->LoadModel(obj, mtl);
        prop->SetPosition(pos);
        prop->SetScale(scale);
        prop->SetRotation(XMFLOAT3(0.0f, rotY, 0.0f));
        prop->SetSize(collSize);
        return prop;
    };

    // =================================================================
    // 4. キャラバン・ベースキャンプ 広々プロップ配置（44m x 44m空間）
    // =================================================================

    // --- (A) 広場中央：憩いの焚き火エリア ---
    // 中央の焚き火に常時炎をともす
    FireEffect::SetEmitter(XMFLOAT3(0.0f, -0.55f, 0.0f));
    // 拠点の空気感: 昼は舞う埃、夜は蛍
    AmbientEffect::SetEnabled(true);
    // キャンプの外側に風で揺れる草原を広げる
    GrassSystem::SetEnabled(true);
    ObjModel::SetFoliageHidden(false);
    // 草原の向こうに丘と木の遠景を広げる
    HillTerrain::SetEnabled(true);


    // --- (C) 北西エリア：工房・鍛冶屋エリア ---

    // --- (D) 北東エリア：出撃ゲート・クエスト掲示板エリア ---

    // 出撃ゲート（Quest Gate）柱オブジェクト
    Wall* gateL = Manager::AddGameObject<Wall>();
    // 道(幅6m)の両端に太い柱を立て、上に梁を渡す大きな門。柱の内側の通り幅は4.8m
    gateL->SetPosition(XMFLOAT3(GATE_POS.x - 3.0f, 2.25f, GATE_POS.z));
    gateL->SetScale(XMFLOAT3(1.2f, 6.5f, 1.2f));

    Wall* gateR = Manager::AddGameObject<Wall>();
    gateR->SetPosition(XMFLOAT3(GATE_POS.x + 3.0f, 2.25f, GATE_POS.z));
    gateR->SetScale(XMFLOAT3(1.2f, 6.5f, 1.2f));

    Wall* gateTop = Manager::AddGameObject<Wall>();
    gateTop->SetPosition(XMFLOAT3(GATE_POS.x, 5.0f, GATE_POS.z));
    gateTop->SetScale(XMFLOAT3(7.2f, 1.0f, 1.3f));
    gateL->SetOutline(false);   // 門はアウトラインなし
    gateR->SetOutline(false);
    gateTop->SetOutline(false);

    // --- 松明（夜は橙色の点光源になる。ゲート両脇と工房の入口） ---
    // 焚き火と同じモデル（石の火床）を小さくした「かがり火」にして、画風を揃える
    auto AddTorch = [&](float x, float z) {
        FireEffect::AddTorch(XMFLOAT3(x, -0.85f, z));
    };
    AddTorch(GATE_POS.x - 4.8f, GATE_POS.z - 1.0f);
    AddTorch(GATE_POS.x + 4.8f, GATE_POS.z - 1.0f);
    AddTorch(FORGE_POS.x + 3.0f, FORGE_POS.z - 2.5f);

    // --- (E) 北中央：修練エリア（試し振りサンドバッグ） ---
    Enemy* dummy = Manager::AddGameObject<Enemy>();
    dummy->SetPosition(XMFLOAT3(13.0f, -0.5f, 14.0f));
    dummy->SetSandbag(true, 999999);



    // --- 草原の自然物（Stylized Nature MegaKit）。柵の外の草原に、木・岩・茂み・花を散らす ---
    // 位置は固定の乱数列で決める（毎回同じ景色になる）。当たり判定なし（外周は見えない壁が止める）
    {
        const std::string N = "Assets/model/nature/";
        auto nat = [&](const std::string& sub, const std::string& name, float x, float z, float scale, float rotY) {
            AddProp(N + sub + "/" + name + ".obj", N + sub + "/" + name + ".mtl",
                    XMFLOAT3(x, -1.0f, z), XMFLOAT3(scale, scale, scale), rotY);
        };

        unsigned int seed = 20260101u;
        auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return (float)(seed >> 8) / (float)(1 << 24); };
        auto range = [&](float a, float b) { return a + (b - a) * rnd(); };
        auto pickName = [&](const char* base, int count) { return std::string(base) + std::to_string(1 + (int)(rnd() * count) % count); };

        // 柵の外側(キャンプの外)で、草原の端(±46m)までの範囲から、既存の配置と離れた位置を選ぶ
        std::vector<XMFLOAT2> placed;

        // --- 中世の村の家（Medieval Village MegaKit）。柵の外に数軒、広場の方を向けて建てる。当たり判定なし ---
        // 壁は幅2mのモジュール。土台は 幅6m(x) × 奥行8m(z)、正面(+z)にドア。S倍に拡大して置く
        {
            const std::string M = "Assets/model/medieval/";
            const float S = 1.0f, GY = -1.0f;
            // 家のローカル座標(lx,lz,高さly)に置く。rot=家の向き、rotAdd=その部品だけの追加回転。collSize>0 で当たり判定あり
            auto mp = [&](const std::string& n, float lx, float lz, float ly, float cx, float cz, float rot, float rotAdd,
                          const XMFLOAT3& collSize = XMFLOAT3(0.0f, 0.0f, 0.0f), float sx = 1.0f) {
                const float c = cosf(rot), s = sinf(rot);
                AddProp(M + n + ".obj", M + n + ".mtl",
                        XMFLOAT3(cx + (lx * c + lz * s) * S, GY + ly * S, cz + (-lx * s + lz * c) * S),
                        XMFLOAT3(S * sx, S, S), rot + rotAdd, collSize);
            };
            // rot: 0=正面が+Z向き / π=-Z向き / π/2=+X向き / -π/2=-X向き（広場の中心に向ける）
            auto house = [&](float cx, float cz, bool brick, float rot, int nx, int nz, int deco, int floors = 1) {
                // deco: 1=煙突 2=煙突(別形) 4=丸いドア 8=雨戸(開) 16=雨戸(閉) 32=塔(円錐の屋根・細い丸窓)。floors=階数(1階=3.12m)
                const float HW = (float)nx, HD = (float)nz;   // 半幅・半奥行(m)。nx,nzは壁モジュール数(幅2m)
                const bool tower = (deco & 32) != 0;
                const std::string wall = brick ? "Wall_UnevenBrick_" : "Wall_Plaster_";
                const std::string winWall  = wall + (tower ? "Window_Thin_Round" : "Window_Wide_Flat");
                const char*       winGlass = tower ? "Window_Thin_Round1" : "Window_Wide_Flat1";
                for (int f = 0; f < floors; ++f) {
                    const float ly = 3.12f * (float)f;
                    for (int i = 0; i < nx; ++i) {        // 正面(1階はドア+窓、2階以上は窓)と背面
                        const float lx = -(float)(nx - 1) + 2.0f * i;
                        const bool door = (f == 0 && i == nx / 2);
                        const bool rnd = (deco & 4) != 0;
                        mp(door ? wall + (rnd ? "Door_Round" : "Door_Flat") : winWall, lx, HD, ly, cx, cz, rot, 0.0f);
                        // ドア板は蝶番側が原点(x:-0.05〜1.07、幅1.12)で、壁の開口は x:-0.65〜0.65(幅1.3)。
                        // 開口に合わせて横に1.17倍し、-0.59ずらして隙間をなくす
                        if (door) mp(rnd ? "Door_1_Round" : "Door_1_Flat", lx - 0.59f, HD, ly, cx, cz, rot, 0.0f, XMFLOAT3(0.0f, 0.0f, 0.0f), 1.17f);
                        else {
                            mp(winGlass, lx, HD, ly, cx, cz, rot, 0.0f);
                            if (!tower && (deco & 24)) mp((deco & 8) ? "WindowShutters_Wide_Flat_Open" : "WindowShutters_Wide_Flat_Closed", lx, HD, ly, cx, cz, rot, 0.0f);
                        }
                        mp(wall + "Straight", lx, -HD, ly, cx, cz, rot, XM_PI);
                    }
                    for (int i = 0; i < nz; ++i) {        // 左右の側面（家は両端以外に窓、塔は2階以上の全面に窓）
                        const float lz = -(float)(nz - 1) + 2.0f * i;
                        const bool win = tower ? (f >= 1) : (i >= 1 && i <= nz - 2);
                        const std::string nm = win ? winWall : wall + "Straight";
                        mp(nm, HW, lz, ly, cx, cz, rot, XM_PIDIV2);
                        mp(nm, -HW, lz, ly, cx, cz, rot, -XM_PIDIV2);
                        if (win) {
                            mp(winGlass, HW, lz, ly, cx, cz, rot, XM_PIDIV2);
                            mp(winGlass, -HW, lz, ly, cx, cz, rot, -XM_PIDIV2);
                            if (!tower && (deco & 24)) {
                                const char* sh = (deco & 8) ? "WindowShutters_Wide_Flat_Open" : "WindowShutters_Wide_Flat_Closed";
                                mp(sh, HW, lz, ly, cx, cz, rot, XM_PIDIV2);
                                mp(sh, -HW, lz, ly, cx, cz, rot, -XM_PIDIV2);
                            }
                        }
                    }
                    mp("Corner_Exterior_Wood",  HW,  HD, ly, cx, cz, rot, 0.0f);
                    mp("Corner_Exterior_Wood", -HW,  HD, ly, cx, cz, rot, 0.0f);
                    mp("Corner_Exterior_Wood",  HW, -HD, ly, cx, cz, rot, 0.0f);
                    mp("Corner_Exterior_Wood", -HW, -HD, ly, cx, cz, rot, 0.0f);
                }
                const float top = 3.12f * (float)floors;   // 壁の上端の高さ
                const float up  = 3.12f * (float)(floors - 1);
                if (tower) {
                    // 塔の屋根は軒先が壁の上端より1.3m下がる位置(原点 = 壁上端 - 0.73)。ここに土台の当たり判定を付ける
                    mp("Roof_Tower_RoundTiles", 0.0f, 0.0f, top - 0.73f, cx, cz, rot, 0.0f, XMFLOAT3(2.0f * HW, 1.0f, 2.0f * HD));
                    return;
                }
                const std::string gable = "Roof_Front_Brick" + std::to_string(nx * 2);
                mp(gable, 0.0f,  HD, top, cx, cz, rot, 0.0f);     // 屋根の下の三角壁
                mp(gable, 0.0f, -HD, top, cx, cz, rot, XM_PI);
                // 屋根の原点が家の中心。ここに土台の当たり判定を付ける（家の中には入れない）
                // 屋根の原点の高さは部品ごとに違う（軒先を同じ高さにそろえる）。小さい4m幅だけ低い
                const float roofY = ((nx == 2) ? 2.34f : 2.6f) + up;
                mp("Roof_RoundTiles_" + std::to_string(nx * 2) + "x" + std::to_string(nz * 2), 0.0f, 0.0f, roofY, cx, cz, rot, 0.0f, XMFLOAT3(2.0f * HW, 1.0f, 2.0f * HD));
                if (deco & 3) {                       // 煙突：底のV字の切り欠きが棟(屋根の頂上)をまたぐ向きに置く
                    const float peak = ((nx == 2) ? 6.06f : (nx == 3 ? 7.49f : 8.6f)) + up;
                    mp((deco & 1) ? "Prop_Chimney" : "Prop_Chimney2", 0.0f, HD * 0.3f, peak - 1.3f, cx, cz, rot, XM_PIDIV2);
                }
            };
            // 拠点（44m四方）の四隅に1軒ずつ。既存のプロップと重ならない位置
            // 中央の道(幅6m, x:-3〜3)の左右に、正面を道へ向けて並べる。rot π/2=+X向き(左側) / -π/2=-X向き(右側)
            // 中心x = 道の端(±6) ± 奥行の半分。z は建物の幅が重ならないように
            house( -8.0f, -14.0f, true,  XM_PIDIV2,  2, 2, 32, 3);      // 左: 4x4m 石造りの塔(3階)
            house(-10.0f,  -6.0f, false, XM_PIDIV2,  3, 4, 1 | 8);      // 左: 6x8m 漆喰
            house( -9.0f,   2.0f, false, XM_PIDIV2,  3, 3, 4 | 2, 2);   // 左: 6x6m 二階建て 漆喰
            house( 11.0f, -12.0f, false, -XM_PIDIV2, 3, 5, 4 | 2 | 8);  // 右: 6x10m 漆喰(細長)
            house( 10.0f,  -3.0f, true,  -XM_PIDIV2, 4, 4, 2 | 16);     // 右: 8x8m レンガ(大)
            house(  9.0f,   6.0f, true,  -XM_PIDIV2, 2, 3, 4 | 1);      // 右: 4x6m レンガ(小)
            // 道の奥の脇に荷車と木箱
            mp("Prop_Wagon", 0.0f, 0.0f, 0.0f, -5.4f, 15.5f, 1.3f, 0.0f);
            mp("Prop_Crate", 0.0f, 0.0f, 0.0f, 5.0f, 16.2f, 0.4f, 0.0f, XMFLOAT3(1.0f, 1.0f, 1.0f));
            mp("Prop_Crate", 0.0f, 0.0f, 0.0f, 4.6f, 17.4f, 1.1f, 0.0f, XMFLOAT3(1.0f, 1.0f, 1.0f));
        }

        auto place =[&](float minDist, XMFLOAT2* out) {
            for (int tries = 0; tries < 40; ++tries) {
                float x = range(-46.0f, 46.0f), z = range(-46.0f, 46.0f);
                if ((std::max)(fabsf(x), fabsf(z)) < 24.5f) continue;          // キャンプの内側は除外
                bool ok = true;
                for (const XMFLOAT2& p : placed) {
                    float dx = p.x - x, dz = p.y - z;
                    if (dx * dx + dz * dz < minDist * minDist) { ok = false; break; }
                }
                if (!ok) continue;
                placed.push_back(XMFLOAT2(x, z));
                *out = XMFLOAT2(x, z);
                return true;
            }
            return false;
        };

        XMFLOAT2 p;
        // 木: 大きなものを少数、離して置く
        for (int i = 0; i < 16; ++i) {
            if (!place(9.0f, &p)) continue;
            float r = rnd();
            std::string name = (r < 0.45f) ? pickName("CommonTree_", 5)
                             : (r < 0.90f) ? pickName("Pine_", 5)
                             : pickName("DeadTree_", 2);
            nat("trees", name, p.x, p.y, range(1.3f, 1.9f), range(0.0f, 6.28f));
        }
        // 岩: 大小まぜて。まわりに茂みを添える
        for (int i = 0; i < 12; ++i) {
            if (!place(4.0f, &p)) continue;
            nat("rocks", pickName("Rock_Medium_", 3), p.x, p.y, range(1.0f, 2.4f), range(0.0f, 6.28f));
            if (rnd() < 0.7f) {
                nat("bushes", rnd() < 0.5f ? "Bush_Common" : "Bush_Common_Flowers",
                    p.x + range(-2.2f, 2.2f), p.y + range(-2.2f, 2.2f), range(1.0f, 1.7f), range(0.0f, 6.28f));
            }
        }
        // 茂み・シダ・草花の株
        for (int i = 0; i < 18; ++i) {
            if (!place(2.5f, &p)) continue;
            float r = rnd();
            if (r < 0.40f)      nat("bushes", rnd() < 0.5f ? "Bush_Common" : "Bush_Common_Flowers", p.x, p.y, range(1.0f, 1.8f), range(0.0f, 6.28f));
            else if (r < 0.70f) nat("bushes", "Fern_1", p.x, p.y, range(0.9f, 1.5f), range(0.0f, 6.28f));
            else if (r < 0.85f) nat("bushes", rnd() < 0.5f ? "Plant_1_Big" : "Plant_7_Big", p.x, p.y, range(0.9f, 1.5f), range(0.0f, 6.28f));
            else                nat("bushes", rnd() < 0.5f ? "Plant_1" : "Plant_7", p.x, p.y, range(1.0f, 1.6f), range(0.0f, 6.28f));
        }
        // 花のかたまり（中心のまわりに数株）。キノコも少し
        for (int i = 0; i < 14; ++i) {
            if (!place(3.0f, &p)) continue;
            const char* flower = (rnd() < 0.5f) ? "Flower_3_Group" : "Flower_4_Group";
            int n = 3 + (int)(rnd() * 3.0f);
            for (int k = 0; k < n; ++k) {
                nat("flowers", flower, p.x + range(-1.6f, 1.6f), p.y + range(-1.6f, 1.6f), range(1.0f, 1.6f), range(0.0f, 6.28f));
            }
            if (rnd() < 0.4f) {
                nat("flowers", rnd() < 0.5f ? "Mushroom_Common" : "Mushroom_Laetiporus",
                    p.x + range(-1.0f, 1.0f), p.y + range(-1.0f, 1.0f), range(1.4f, 2.2f), range(0.0f, 6.28f));
            }
        }
    }

}

void HubScene::Uninit()
{
    Input::SetMouseLocked(false);
}

void HubScene::Update()
{
    UpdateFacilities();

    // 修練サンドバッグが壊れたら少し待って同じ場所に復活させる（無限に練習できる）
    if (!Manager::GetGameObject<Enemy>()) {
        if (++m_SandbagRespawnTimer >= 90) {
            m_SandbagRespawnTimer = 0;
            Enemy* dummy = Manager::AddGameObject<Enemy>();
            dummy->SetPosition(XMFLOAT3(13.0f, -0.5f, 14.0f));
            dummy->SetSandbag(true, 999999);
        }
    } else {
        m_SandbagRespawnTimer = 0;
    }

    // UIオープン時はマウスロック解除
    bool isAnyUIOpen = m_IsWorkshopOpen || m_IsQuestMenuOpen;
    Input::SetMouseLocked(!isAnyUIOpen);

    // オブジェクトリストの更新
    Player* player = Manager::GetGameObject<Player>();
    for (size_t i = 0; i < Manager::GetUpdateObjectList().size(); i++) {
        GameObject* obj = Manager::GetUpdateObjectList()[i];
        if (!obj) continue;

        // UIが開いている間はプレイヤー移動を静止
        if (isAnyUIOpen && obj->GetObjectType() == ObjectType::Player) {
            continue;
        }

        obj->Update();
    }

    // 掴み物理
    if (player && (player->GetState() == PlayerState::GRABBED || player->GetState() == PlayerState::SPINNING)) {
        Enemy* grabbed = player->GetGrabbedEnemy();
        if (grabbed) {
            Collision::ResolveGrabPhysics(player, grabbed, 0.8f);
        }
    }

    // 衝突判定
    CollisionSystem::Update();
}

void HubScene::UpdateFacilities()
{
    Player* player = Manager::GetGameObject<Player>();
    if (!player) return;

    XMFLOAT3 pPos = player->GetPosition();

    // 工房との距離
    float dxF = pPos.x - FORGE_POS.x;
    float dzF = pPos.z - FORGE_POS.z;
    float distSqForge = dxF * dxF + dzF * dzF;

    // ゲートとの距離
    float dxG = pPos.x - GATE_POS.x;
    float dzG = pPos.z - GATE_POS.z;
    float distSqGate = dxG * dxG + dzG * dzG;

    float rSq = INTERACT_RADIUS * INTERACT_RADIUS;

    if (distSqForge < rSq) {
        m_NearFacility = 1;
    } else if (distSqGate < rSq) {
        m_NearFacility = 2;
    } else {
        m_NearFacility = 0;
    }

    // UI開閉入力検知
    if (Input::GetKeyTrigger('E') || PlayerController::IsGrabOrThrowAction()) {
        if (!m_IsWorkshopOpen && !m_IsQuestMenuOpen) {
            if (m_NearFacility == 1) {
                m_IsWorkshopOpen = true;
                SoundManager::PlaySE(SoundID::SE_DECIDE);
            } else if (m_NearFacility == 2) {
                m_IsQuestMenuOpen = true;
                SoundManager::PlaySE(SoundID::SE_DECIDE);
            }
        }
    }

    // Escキーで閉じる
    if (Input::GetKeyTrigger(VK_ESCAPE)) {
        if (m_IsWorkshopOpen) {
            m_IsWorkshopOpen = false;
        }
        if (m_IsQuestMenuOpen) {
            m_IsQuestMenuOpen = false;
        }
    }
}

void HubScene::Draw()
{
    DrawFacilityHUD();


    if (m_IsWorkshopOpen) {
        DrawWorkshopUI();
    }
    if (m_IsQuestMenuOpen) {
        DrawQuestUI();
    }
}

void HubScene::DrawFacilityHUD()
{
    ImGuiIO& io = ImGui::GetIO();

    // 画面上部：所持素材サマリーバー
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 15.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.78f);
    ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

    // 上部バーは丸いピル形にして、拠点名タグと素材チップを並べる
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 24.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
    const bool hudOpen = ImGui::Begin("##HubMaterialBar", nullptr, hudFlags);
    ImGui::PopStyleVar(2);
    if (hudOpen) {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // 拠点名タグ（橙の塗りつぶし）
        {
            const char* tag = Loc::Tr("BASE CAMP", "ベースキャンプ");
            ImVec2 ts = ImGui::CalcTextSize(tag);
            ImVec2 p = ImGui::GetCursorScreenPos();
            float h = ts.y + 10.0f, w = ts.x + 26.0f;
            dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(255, 143, 46, 255), h * 0.5f);
            dl->AddText(ImVec2(p.x + 13.0f, p.y + 5.0f), IM_COL32(36, 20, 8, 255), tag);
            ImGui::Dummy(ImVec2(w, h));
            ImGui::SameLine(0.0f, 10.0f);
        }

        // 素材チップ（色の点＋名前＋数）
        auto Chip = [&](const char* label, const char* value, ImU32 dotCol) {
            ImVec2 ls = ImGui::CalcTextSize(label);
            ImVec2 vs = ImGui::CalcTextSize(value);
            ImVec2 p = ImGui::GetCursorScreenPos();
            float h = ls.y + 10.0f;
            float w = 14.0f + 12.0f + ls.x + 8.0f + vs.x + 14.0f;
            dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(12, 20, 48, 215), h * 0.5f);
            dl->AddRect(p, ImVec2(p.x + w, p.y + h), IM_COL32(230, 140, 66, 70), h * 0.5f, 0, 1.0f);
            dl->AddCircleFilled(ImVec2(p.x + 17.0f, p.y + h * 0.5f), 5.0f, dotCol);
            dl->AddText(ImVec2(p.x + 30.0f, p.y + 5.0f), IM_COL32(150, 165, 200, 255), label);
            dl->AddText(ImVec2(p.x + 30.0f + ls.x + 8.0f, p.y + 5.0f), IM_COL32(240, 244, 255, 255), value);
            ImGui::Dummy(ImVec2(w, h));
        };

        for (int i = 0; i < static_cast<int>(MaterialType::COUNT); ++i) {
            MaterialType mType = static_cast<MaterialType>(i);
            int count = SaveSystem::GetMaterialCount(mType);
            XMFLOAT3 col = ProgressionData::GetMaterialColor(mType);

            // ワイド文字列から簡易UTF-8名取得
            const char* namesEn[] = { "Scrap", "Cell", "Powder", "Plug", "Armor", "Core" };
            const char* namesJa[] = { "スクラップ", "セル", "火薬", "プラグ", "装甲", "コア" };
            const char* name = (i < 6) ? Loc::Tr(namesEn[i], namesJa[i]) : "Mat";

            char valueBuf[16];
            snprintf(valueBuf, sizeof(valueBuf), "%d", count);
            Chip(name, valueBuf, IM_COL32((int)(col.x * 255), (int)(col.y * 255), (int)(col.z * 255), 255));
            ImGui::SameLine(0.0f, 6.0f);
        }

        // 操作ヒント（目立たない小さな表示）
        {
            const char* hint = Loc::Tr("Tab: Menu", "Tab: メニュー");
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImVec2 hs = ImGui::CalcTextSize(hint);
            dl->AddText(ImVec2(p.x + 6.0f, p.y + 5.0f), IM_COL32(150, 165, 200, 255), hint);
            ImGui::Dummy(ImVec2(hs.x + 12.0f, hs.y + 10.0f));
        }

        ImGui::End();
    }

    // 施設近接時のインタラクトガイド
    if (!m_IsWorkshopOpen && !m_IsQuestMenuOpen && m_NearFacility > 0) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 80.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.85f);
        if (ImGui::Begin("##InteractGuide", nullptr, hudFlags)) {
            if (m_NearFacility == 1) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[E] Open Workshop (Ability Upgrade)");
            } else if (m_NearFacility == 2) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[E] Quest Gate (Depart to Battle)");
            }
            ImGui::End();
        }
    }
}

void HubScene::DrawWorkshopUI()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(620.0f, 480.0f), ImGuiCond_Appearing);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin("WORKSHOP - Ability Upgrade", &m_IsWorkshopOpen, flags)) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Enhance your combat techniques using collected materials.");
        ImGui::Separator();

        // 左右2カラム分割
        ImGui::Columns(2, "WorkshopCols", true);
        ImGui::SetColumnWidth(0, 260.0f);

        // 左カラム：アビリティ一覧リスト
        ImGui::Text("Abilities");
        ImGui::Spacing();

        for (int i = 0; i < static_cast<int>(AbilityType::COUNT); ++i) {
            AbilityType aType = static_cast<AbilityType>(i);
            bool unlocked = ProgressionManager::IsAbilityUnlocked(aType);
            const AbilityDef& def = ProgressionData::GetAbilityDef(aType);

            const char* aNames[] = {
                "Heavy Throw", "Giant Spin", "Quick Grab",
                "Dash Cancel", "Quick Turn", "Counter Shock",
                "Parry Extension", "Material Vacuum",
                "Toughness Lv1", "Toughness Lv2"
            };
            const char* name = (i < 10) ? aNames[i] : "Ability";

            char label[64];
            if (unlocked) {
                sprintf_s(label, "[V] %s", name);
            } else {
                sprintf_s(label, "[ ] %s", name);
            }

            if (ImGui::Selectable(label, m_SelectedAbilityIndex == i)) {
                m_SelectedAbilityIndex = i;
            }
        }

        ImGui::NextColumn();

        // 右カラム：選択アビリティの詳細と解放ボタン
        AbilityType curType = static_cast<AbilityType>(m_SelectedAbilityIndex);
        const AbilityDef& curDef = ProgressionData::GetAbilityDef(curType);
        bool isUnlocked = ProgressionManager::IsAbilityUnlocked(curType);
        bool canUnlock = ProgressionManager::CanUnlockAbility(curType);

        const char* fullNames[] = {
            "Heavy Throw (Faster & Further Throws)",
            "Giant Spin (Speed & Suction Boost)",
            "Quick Grab (Extended Catch Range)",
            "Dash Cancel (Cancel Recovery into Dash)",
            "Quick Turn (Responsive Turning)",
            "Counter Shock (Shockwave on Parry)",
            "Parry Extension (Longer Slow Motion)",
            "Material Vacuum (Wide Drop Attraction)",
            "Toughness Lv1 (+1 Max HP)",
            "Toughness Lv2 (+1 Additional Max HP)"
        };
        const char* title = (m_SelectedAbilityIndex < 10) ? fullNames[m_SelectedAbilityIndex] : "Details";

        ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "%s", title);
        ImGui::Spacing();

        // 説明文
        const char* descList[] = {
            "Increases thrown enemy velocity and range by 25%.",
            "Accelerates spin rotation and expands suction vortex.",
            "Extends grab detection distance by 25%.",
            "Allows immediate dash to cancel attack & spin cooldowns.",
            "Significantly boosts player turning responsiveness.",
            "Releases a knockback shockwave upon successful parry.",
            "Extends parry slow-motion duration by 1.5x.",
            "Expands material attraction radius by 2.5x.",
            "Increases maximum HP by 1 (Base 3 -> 4).",
            "Increases maximum HP by an additional 1 (Max 5)."
        };
        const char* desc = (m_SelectedAbilityIndex < 10) ? descList[m_SelectedAbilityIndex] : "";
        ImGui::TextWrapped("%s", desc);
        ImGui::Spacing();
        ImGui::Separator();

        // 必要素材
        ImGui::Text("Required Materials:");
        for (const auto& cost : curDef.costs) {
            int have = SaveSystem::GetMaterialCount(cost.type);
            bool ok = have >= cost.amount;

            const char* mNames[] = { "Scrap Iron", "Energy Cell", "Explosive Powder", "Lightning Core", "Titan Armor", "Titan Core" };
            int mIdx = static_cast<int>(cost.type);
            const char* mName = (mIdx < 6) ? mNames[mIdx] : "Material";

            ImVec4 txtCol = ok ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            ImGui::TextColored(txtCol, " - %s: %d / %d", mName, have, cost.amount);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        if (isUnlocked) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "STATUS: UNLOCKED & ACTIVE");
        } else {
            if (canUnlock) {
                UITheme::PushPrimaryButton();
                const bool unlockClicked = ImGui::Button("UNLOCK ABILITY", ImVec2(200.0f, 40.0f));
                UITheme::PopButton();
                if (unlockClicked) {
                    if (ProgressionManager::TryUnlockAbility(curType)) {
                        SoundManager::PlaySE(SoundID::SE_DECIDE);
                    }
                }
            } else {
                ImGui::BeginDisabled();
                ImGui::Button("NOT ENOUGH MATERIALS", ImVec2(200.0f, 40.0f));
                ImGui::EndDisabled();
            }
        }

        ImGui::Columns(1);
        ImGui::Separator();

        if (ImGui::Button("Close [Esc]", ImVec2(100.0f, 28.0f))) {
            m_IsWorkshopOpen = false;
        }

        ImGui::End();
    }
}

void HubScene::DrawQuestUI()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500.0f, 320.0f), ImGuiCond_Appearing);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin("QUEST GATE - Select Mission", &m_IsQuestMenuOpen, flags)) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Select your destination to collect materials.");
        ImGui::Separator();

        // クエスト1: 通常ウェーブ演習
        if (ImGui::Selectable("[Quest 1] Tactical Arena (Standard Wave -> Boss)", m_SelectedQuestIndex == 0)) {
            m_SelectedQuestIndex = 0;
        }
        ImGui::TextDisabled("  Enemies: Mobs, Explosive, Lightning, Shooters. Good for basic materials.");
        ImGui::Spacing();

        // クエスト2: 重装タイタン直行戦
        if (ImGui::Selectable("[Quest 2] Boss Titan Confrontation (Boss Direct)", m_SelectedQuestIndex == 1)) {
            m_SelectedQuestIndex = 1;
        }
        ImGui::TextDisabled("  Direct battle against Titan Boss. Rewards Rare Titan Armor & Core.");
        ImGui::Spacing();
        ImGui::Separator();

        UITheme::PushPrimaryButton();
        const bool departClicked = ImGui::Button("DEPART ON QUEST", ImVec2(200.0f, 42.0f));
        UITheme::PopButton();
        if (departClicked) {
            SoundManager::PlaySE(SoundID::SE_DECIDE);
            m_IsQuestMenuOpen = false;

            Manager::SetStartFromBoss(m_SelectedQuestIndex == 1);
            Manager::ChangeScene(Scene::GAMEPLAY);
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 42.0f))) {
            m_IsQuestMenuOpen = false;
        }

        ImGui::End();
    }
}
