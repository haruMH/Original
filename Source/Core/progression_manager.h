#pragma once
#include "progression_types.h"
#include "save_system.h"
#include <vector>

// =================================================================
// Progression and Session Material Manager
// =================================================================
class ProgressionManager
{
private:
    static int m_SessionMaterials[static_cast<int>(MaterialType::COUNT)];
    static int m_SessionKills;

public:
    static void Init();
    static void StartSession();

    static void AddSessionMaterial(MaterialType type, int count = 1);
    static int  GetSessionMaterial(MaterialType type);
    static void AddSessionKill();
    static int  GetSessionKills() { return m_SessionKills; }

    static void CommitSession(bool isClear = true);

    static bool CanUnlockAbility(AbilityType type);
    static bool TryUnlockAbility(AbilityType type);
    static bool IsAbilityUnlocked(AbilityType type);

    static bool  HasHeavyThrow();
    static bool  HasGiantSpin();
    static bool  HasQuickGrab();
    static bool  HasDashCancel();
    static bool  HasQuickReverse();
    static bool  HasCounterShock();
    static bool  HasParryExtension();
    static bool  HasMaterialVacuum();
    static int   GetExtraMaxHP();
};
