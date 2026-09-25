#pragma once
#include "progression_types.h"
#include <string>

// =================================================================
// Save Data Structure
// =================================================================
struct SaveData
{
    int version = 1;

    // Material inventory
    int materials[static_cast<int>(MaterialType::COUNT)] = { 0 };

    // Unlocked abilities
    bool abilities[static_cast<int>(AbilityType::COUNT)] = { false };

    // Statistics
    int totalKills = 0;
    int totalClears = 0;
    int highestCombo = 0;
    int playTimeSeconds = 0;

    // Progression
    bool tutorialCompleted = false;
};

// =================================================================
// Save / Load System
// =================================================================
class SaveSystem
{
private:
    static std::string m_SaveFilePath;
    static SaveData    m_CurrentData;
    static bool        m_IsLoaded;

public:
    // Initialize & initial load
    static void Init();

    // Save (atomic write)
    static bool Save();

    // Load
    static bool Load();

    // Check save file existence
    static bool HasSaveFile();

    // Reset data to defaults (New Game)
    static void ResetData();

    // Current save data reference
    static SaveData& GetData() { return m_CurrentData; }

    static bool IsTutorialCompleted() { return m_CurrentData.tutorialCompleted; }
    static void SetTutorialCompleted(bool completed = true) { m_CurrentData.tutorialCompleted = completed; }

    // Material inventory operations
    static int  GetMaterialCount(MaterialType type);
    static void AddMaterial(MaterialType type, int count);
    static bool ConsumeMaterial(MaterialType type, int count);

    // Ability unlock operations
    static bool IsAbilityUnlocked(AbilityType type);
    static bool UnlockAbility(AbilityType type);

    // Stats
    static void AddKillCount(int count = 1);
    static void AddClearCount();
    static void UpdateMaxCombo(int combo);
};
