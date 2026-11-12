#pragma once
#include "scene_interface.h"
#include <DirectXMath.h>

// =================================================================
// 3D Hub Scene (Base Camp / Workshop / Quest Gate)
// =================================================================
class HubScene : public IScene
{
private:
    bool m_IsWorkshopOpen;
    bool m_IsQuestMenuOpen;
    int  m_NearFacility; // 0: None, 1: Workshop (Forge), 2: Quest Gate
    int  m_SelectedAbilityIndex;
    int  m_SelectedQuestIndex;
    int  m_SandbagRespawnTimer = 0; // 修練サンドバッグの再出現待ちフレーム

    void UpdateFacilities();
    void DrawWorkshopUI();
    void DrawQuestUI();
    void DrawFacilityHUD();

public:
    HubScene();
    ~HubScene() override;

    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;
};
