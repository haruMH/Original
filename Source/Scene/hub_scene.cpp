#include "hub_scene.h"
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
    const XMFLOAT3 FORGE_POS = XMFLOAT3(-5.0f, 0.0f, 4.0f);
    const XMFLOAT3 GATE_POS  = XMFLOAT3(5.0f, 0.0f, 4.0f);
    const float INTERACT_RADIUS = 3.0f;
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
    Manager::AddGameObject<Field>();
    Manager::AddGameObject<Skybox>();

    // 2. プレイヤー
    Player* player = Manager::AddGameObject<Player>();
    player->SetPosition(XMFLOAT3(0.0f, -0.5f, -3.0f));
    player->SetRotation(XMFLOAT3(0.0f, 0.0f, 0.0f));

    if (g_Camera) {
        g_Camera->SetAngles(0.25f, 0.0f);
    }

    // 3. 拠点の外周壁 (16m x 16m)
    float halfSize = 8.0f;
    float wallH = 4.0f;
    float wallY = 1.5f;
    float thick = 1.0f;

    // 北壁
    Wall* wN = Manager::AddGameObject<Wall>();
    wN->SetPosition(XMFLOAT3(0.0f, wallY, halfSize + thick * 0.5f));
    wN->SetScale(XMFLOAT3(halfSize * 2.0f + thick * 2.0f, wallH, thick));

    // 南壁
    Wall* wS = Manager::AddGameObject<Wall>();
    wS->SetPosition(XMFLOAT3(0.0f, wallY, -(halfSize + thick * 0.5f)));
    wS->SetScale(XMFLOAT3(halfSize * 2.0f + thick * 2.0f, wallH, thick));

    // 東壁
    Wall* wE = Manager::AddGameObject<Wall>();
    wE->SetPosition(XMFLOAT3(halfSize + thick * 0.5f, wallY, 0.0f));
    wE->SetScale(XMFLOAT3(thick, wallH, halfSize * 2.0f + thick * 2.0f));

    // 西壁
    Wall* wW = Manager::AddGameObject<Wall>();
    wW->SetPosition(XMFLOAT3(-(halfSize + thick * 0.5f), wallY, 0.0f));
    wW->SetScale(XMFLOAT3(thick, wallH, halfSize * 2.0f + thick * 2.0f));

    // 4. 工房（Forge）3Dモデルオブジェクト（Kenney: Workbench Anvil）
    PropModel* forgeAnvil = Manager::AddGameObject<PropModel>();
    forgeAnvil->LoadModel("Assets/model/workbench_anvil/workbenchAnvil.obj", "Assets/model/workbench_anvil/workbenchAnvil.mtl");
    forgeAnvil->SetPosition(XMFLOAT3(FORGE_POS.x, -1.0f, FORGE_POS.z)); // 地面(-1.0f)に接地
    forgeAnvil->SetScale(XMFLOAT3(7.0f, 7.0f, 7.0f));
    forgeAnvil->SetRotation(XMFLOAT3(0.0f, XM_PI, 0.0f));     // 時計回りに90度回転 (XM_PI)
    forgeAnvil->SetSize(XMFLOAT3(0.35f, 0.35f, 0.35f));       // 当たり判定のローカル直方体サイズ (実寸 0.35 * 7.0 = 2.45m)

    // 5. 出撃ゲート（Quest Gate）オブジェクト
    Wall* gateL = Manager::AddGameObject<Wall>();
    gateL->SetPosition(XMFLOAT3(GATE_POS.x - 1.2f, 1.6f, GATE_POS.z));
    gateL->SetScale(XMFLOAT3(0.7f, 3.5f, 0.7f));

    Wall* gateR = Manager::AddGameObject<Wall>();
    gateR->SetPosition(XMFLOAT3(GATE_POS.x + 1.2f, 1.6f, GATE_POS.z));
    gateR->SetScale(XMFLOAT3(0.7f, 3.5f, 0.7f));

    Wall* gateTop = Manager::AddGameObject<Wall>();
    gateTop->SetPosition(XMFLOAT3(GATE_POS.x, 3.4f, GATE_POS.z));
    gateTop->SetScale(XMFLOAT3(3.2f, 0.6f, 0.8f));

    // 6. 試し振りサンドバッグ (HP無限)
    Enemy* dummy = Manager::AddGameObject<Enemy>();
    dummy->SetPosition(XMFLOAT3(0.0f, -0.5f, 4.5f));
    dummy->SetSandbag(true, 999999);
}

void HubScene::Uninit()
{
    Input::SetMouseLocked(false);
}

void HubScene::Update()
{
    UpdateFacilities();

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
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##HubMaterialBar", nullptr, hudFlags)) {
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "[ BASE CAMP ]");
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        for (int i = 0; i < static_cast<int>(MaterialType::COUNT); ++i) {
            MaterialType mType = static_cast<MaterialType>(i);
            int count = SaveSystem::GetMaterialCount(mType);
            XMFLOAT3 col = ProgressionData::GetMaterialColor(mType);

            // ワイド文字列から簡易UTF-8名取得
            const char* asciiNames[] = { "Scrap", "Cell", "Powder", "Plug", "Armor", "Core" };
            const char* name = (i < 6) ? asciiNames[i] : "Mat";

            ImGui::TextColored(ImVec4(col.x, col.y, col.z, 1.0f), "%s: %d", name, count);
            if (i < static_cast<int>(MaterialType::COUNT) - 1) {
                ImGui::SameLine();
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.85f, 0.9f, 1.0f, 1.0f), "[Tab] Menu / Save / Quit");

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
                if (ImGui::Button("UNLOCK ABILITY", ImVec2(200.0f, 40.0f))) {
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

        if (ImGui::Button("DEPART ON QUEST", ImVec2(200.0f, 42.0f))) {
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
