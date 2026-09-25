#include "progression_manager.h"
#include <windows.h>

int ProgressionManager::m_SessionMaterials[static_cast<int>(MaterialType::COUNT)] = { 0 };
int ProgressionManager::m_SessionKills = 0;

void ProgressionManager::Init()
{
    SaveSystem::Init();
    StartSession();
}

void ProgressionManager::StartSession()
{
    for (int i = 0; i < static_cast<int>(MaterialType::COUNT); ++i)
    {
        m_SessionMaterials[i] = 0;
    }
    m_SessionKills = 0;
}

void ProgressionManager::AddSessionMaterial(MaterialType type, int count)
{
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(MaterialType::COUNT))
    {
        m_SessionMaterials[idx] += count;
    }
}

int ProgressionManager::GetSessionMaterial(MaterialType type)
{
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(MaterialType::COUNT))
    {
        return m_SessionMaterials[idx];
    }
    return 0;
}

void ProgressionManager::AddSessionKill()
{
    m_SessionKills++;
}

void ProgressionManager::CommitSession(bool isClear)
{
    for (int i = 0; i < static_cast<int>(MaterialType::COUNT); ++i)
    {
        if (m_SessionMaterials[i] > 0)
        {
            SaveSystem::AddMaterial(static_cast<MaterialType>(i), m_SessionMaterials[i]);
        }
    }

    if (m_SessionKills > 0)
    {
        SaveSystem::AddKillCount(m_SessionKills);
    }

    if (isClear)
    {
        SaveSystem::AddClearCount();
    }

    SaveSystem::Save();
    OutputDebugStringA("[ProgressionManager] Session committed and saved.\n");
}

bool ProgressionManager::CanUnlockAbility(AbilityType type)
{
    if (IsAbilityUnlocked(type)) return false;

    const AbilityDef& def = ProgressionData::GetAbilityDef(type);

    if (def.prerequisite != AbilityType::COUNT)
    {
        if (!IsAbilityUnlocked(def.prerequisite))
        {
            return false;
        }
    }

    for (const auto& cost : def.costs)
    {
        if (SaveSystem::GetMaterialCount(cost.type) < cost.amount)
        {
            return false;
        }
    }

    return true;
}

bool ProgressionManager::TryUnlockAbility(AbilityType type)
{
    if (!CanUnlockAbility(type)) return false;

    const AbilityDef& def = ProgressionData::GetAbilityDef(type);

    for (const auto& cost : def.costs)
    {
        SaveSystem::ConsumeMaterial(cost.type, cost.amount);
    }

    return SaveSystem::UnlockAbility(type);
}

bool ProgressionManager::IsAbilityUnlocked(AbilityType type)
{
    return SaveSystem::IsAbilityUnlocked(type);
}

bool ProgressionManager::HasHeavyThrow()
{
    return IsAbilityUnlocked(AbilityType::HEAVY_THROW);
}

bool ProgressionManager::HasGiantSpin()
{
    return IsAbilityUnlocked(AbilityType::GIANT_SPIN);
}

bool ProgressionManager::HasQuickGrab()
{
    return IsAbilityUnlocked(AbilityType::QUICK_GRAB);
}

bool ProgressionManager::HasDashCancel()
{
    return IsAbilityUnlocked(AbilityType::DASH_CANCEL);
}

bool ProgressionManager::HasQuickReverse()
{
    return IsAbilityUnlocked(AbilityType::QUICK_REVERSE);
}

bool ProgressionManager::HasCounterShock()
{
    return IsAbilityUnlocked(AbilityType::COUNTER_SHOCK);
}

bool ProgressionManager::HasParryExtension()
{
    return IsAbilityUnlocked(AbilityType::PARRY_EXTENSION);
}

bool ProgressionManager::HasMaterialVacuum()
{
    return IsAbilityUnlocked(AbilityType::MATERIAL_VACUUM);
}

int ProgressionManager::GetExtraMaxHP()
{
    int extra = 0;
    if (IsAbilityUnlocked(AbilityType::SURVIVAL_TOUGHNESS_1)) extra++;
    if (IsAbilityUnlocked(AbilityType::SURVIVAL_TOUGHNESS_2)) extra++;
    return extra;
}
