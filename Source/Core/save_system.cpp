#include "save_system.h"
#include <fstream>
#include <sstream>
#include <windows.h>

std::string SaveSystem::m_SaveFilePath = "save_data.dat";
SaveData    SaveSystem::m_CurrentData;
bool        SaveSystem::m_IsLoaded = false;

void SaveSystem::Init()
{
    m_SaveFilePath = "save_data.dat";
    Load();
}

bool SaveSystem::Save()
{
    std::string tempPath = m_SaveFilePath + ".tmp";
    std::ofstream ofs(tempPath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open())
    {
        OutputDebugStringA("[SaveSystem] Failed to open temp save file for writing.\n");
        return false;
    }

    ofs << "VERSION=" << m_CurrentData.version << "\n";

    // Materials
    ofs << "MATERIALS=";
    for (int i = 0; i < static_cast<int>(MaterialType::COUNT); ++i)
    {
        ofs << m_CurrentData.materials[i];
        if (i < static_cast<int>(MaterialType::COUNT) - 1) ofs << ",";
    }
    ofs << "\n";

    // Abilities
    ofs << "ABILITIES=";
    for (int i = 0; i < static_cast<int>(AbilityType::COUNT); ++i)
    {
        ofs << (m_CurrentData.abilities[i] ? 1 : 0);
        if (i < static_cast<int>(AbilityType::COUNT) - 1) ofs << ",";
    }
    ofs << "\n";

    // Stats
    ofs << "TOTAL_KILLS=" << m_CurrentData.totalKills << "\n";
    ofs << "TOTAL_CLEARS=" << m_CurrentData.totalClears << "\n";
    ofs << "MAX_COMBO=" << m_CurrentData.highestCombo << "\n";
    ofs << "PLAY_TIME=" << m_CurrentData.playTimeSeconds << "\n";
    ofs << "TUTORIAL_COMPLETED=" << (m_CurrentData.tutorialCompleted ? 1 : 0) << "\n";

    ofs.flush();
    ofs.close();

    // Atomic move
    if (!MoveFileExA(tempPath.c_str(), m_SaveFilePath.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        std::remove(m_SaveFilePath.c_str());
        std::rename(tempPath.c_str(), m_SaveFilePath.c_str());
    }

    OutputDebugStringA("[SaveSystem] Save successfully completed.\n");
    return true;
}

bool SaveSystem::HasSaveFile()
{
    std::ifstream ifs(m_SaveFilePath);
    return ifs.good();
}

void SaveSystem::ResetData()
{
    m_CurrentData = SaveData();
    m_IsLoaded = true;
}

bool SaveSystem::Load()
{
    std::ifstream ifs(m_SaveFilePath);
    if (!ifs.is_open())
    {
        OutputDebugStringA("[SaveSystem] Save file not found. Starting with new save data.\n");
        m_CurrentData = SaveData();
        m_IsLoaded = true;
        return false;
    }

    std::string line;
    while (std::getline(ifs, line))
    {
        if (line.empty() || line[0] == '#') continue;

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = line.substr(0, eqPos);
        std::string val = line.substr(eqPos + 1);

        if (key == "VERSION")
        {
            m_CurrentData.version = std::stoi(val);
        }
        else if (key == "MATERIALS")
        {
            std::stringstream ss(val);
            std::string item;
            int idx = 0;
            while (std::getline(ss, item, ',') && idx < static_cast<int>(MaterialType::COUNT))
            {
                m_CurrentData.materials[idx++] = std::stoi(item);
            }
        }
        else if (key == "ABILITIES")
        {
            std::stringstream ss(val);
            std::string item;
            int idx = 0;
            while (std::getline(ss, item, ',') && idx < static_cast<int>(AbilityType::COUNT))
            {
                m_CurrentData.abilities[idx++] = (std::stoi(item) != 0);
            }
        }
        else if (key == "TOTAL_KILLS")
        {
            m_CurrentData.totalKills = std::stoi(val);
        }
        else if (key == "TOTAL_CLEARS")
        {
            m_CurrentData.totalClears = std::stoi(val);
        }
        else if (key == "MAX_COMBO")
        {
            m_CurrentData.highestCombo = std::stoi(val);
        }
        else if (key == "PLAY_TIME")
        {
            m_CurrentData.playTimeSeconds = std::stoi(val);
        }
        else if (key == "TUTORIAL_COMPLETED")
        {
            m_CurrentData.tutorialCompleted = (std::stoi(val) != 0);
        }
    }

    m_IsLoaded = true;
    OutputDebugStringA("[SaveSystem] Save successfully loaded.\n");
    return true;
}

int SaveSystem::GetMaterialCount(MaterialType type)
{
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(MaterialType::COUNT))
    {
        return m_CurrentData.materials[idx];
    }
    return 0;
}

void SaveSystem::AddMaterial(MaterialType type, int count)
{
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(MaterialType::COUNT))
    {
        m_CurrentData.materials[idx] += count;
        if (m_CurrentData.materials[idx] < 0) m_CurrentData.materials[idx] = 0;
    }
}

bool SaveSystem::ConsumeMaterial(MaterialType type, int count)
{
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(MaterialType::COUNT))
    {
        if (m_CurrentData.materials[idx] >= count)
        {
            m_CurrentData.materials[idx] -= count;
            return true;
        }
    }
    return false;
}

bool SaveSystem::IsAbilityUnlocked(AbilityType type)
{
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(AbilityType::COUNT))
    {
        return m_CurrentData.abilities[idx];
    }
    return false;
}

bool SaveSystem::UnlockAbility(AbilityType type)
{
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(AbilityType::COUNT))
    {
        m_CurrentData.abilities[idx] = true;
        Save();
        return true;
    }
    return false;
}

void SaveSystem::AddKillCount(int count)
{
    m_CurrentData.totalKills += count;
}

void SaveSystem::AddClearCount()
{
    m_CurrentData.totalClears++;
}

void SaveSystem::UpdateMaxCombo(int combo)
{
    if (combo > m_CurrentData.highestCombo)
    {
        m_CurrentData.highestCombo = combo;
    }
}
