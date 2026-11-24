#include <algorithm>
#include <vector>
#include <string>
#include "town_backdrop.h"
#include "manager.h"
#include "field.h"
#include "prop_model.h"
#include "obj_model.h"
#include "fire_effect.h"
#include "ambient_effect.h"
#include "grass_system.h"
#include "hill_terrain.h"

using namespace DirectX;

// 拠点(HubScene)と同じ街並みの背景を作る（タイトル/クリア/ゲームオーバーでも共用）
void TownBackdrop::Build(bool campfire)
{
    // 地面(石畳+中央の道)と空
    Field* field = Manager::AddGameObject<Field>();
    field->SetFloorTexture("cobble.png", "cobble_normal.png");
    field->SetRoad(-3.0f, 3.0f, -22.0f, 22.0f, "road.png", "road_normal.png");
    Manager::AddGameObject<Skybox>();

    if (campfire) FireEffect::SetEmitter(XMFLOAT3(0.0f, -0.55f, 0.0f));
    AmbientEffect::SetEnabled(true);
    GrassSystem::SetEnabled(true);
    ObjModel::SetFoliageHidden(false);
    HillTerrain::SetEnabled(true);


    auto AddProp = [](const std::string& obj, const std::string& mtl, const XMFLOAT3& pos, const XMFLOAT3& scale, float rotY = 0.0f, const XMFLOAT3& collSize = XMFLOAT3(0.0f, 0.0f, 0.0f)) -> PropModel* {
        PropModel* prop = Manager::AddGameObject<PropModel>();
        prop->LoadModel(obj, mtl);
        prop->SetPosition(pos);
        prop->SetScale(scale);
        prop->SetRotation(XMFLOAT3(0.0f, rotY, 0.0f));
        prop->SetSize(collSize);
        return prop;
    };

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
