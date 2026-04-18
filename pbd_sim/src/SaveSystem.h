#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <fstream>
#include <cstdint>
#include <cstring>
#include <string>

// ============================================================
//  SaveSystem — persist progress to disk.
//
//  Saves to %APPDATA%\Ashen\save.dat (Windows) or
//  ./ashen_save.dat (fallback).
//
//  Data stored:
//    - Highest level reached (for level select unlock)
//    - Best time per level
//    - Total death count
//    - Settings (reserved for audio volume etc.)
//
//  Format: fixed-size binary, versioned header.
// ============================================================

struct SaveData
{
    uint32_t magic       = 0x4153484E;  // 'ASHN'
    uint32_t version     = 1;
    int32_t  highestLevel = 0;
    int32_t  totalDeaths  = 0;
    float    bestTime[9]  = {999.f,999.f,999.f,999.f,999.f,999.f,999.f,999.f,999.f};
    float    audioVolume  = 1.0f;
    uint8_t  _pad[48]     = {};  // reserved
};

// Size varies by platform — no static assert

class SaveSystem
{
public:
    void init()
    {
        m_path = getSavePath();
        load();
    }

    const SaveData& data() const { return m_data; }
    SaveData&       data()       { return m_data; }

    void save()
    {
        std::ofstream f(m_path, std::ios::binary | std::ios::trunc);
        if (f) f.write((const char*)&m_data, sizeof(SaveData));
    }

    void load()
    {
        std::ifstream f(m_path, std::ios::binary);
        if (!f) return;
        SaveData tmp;
        f.read((char*)&tmp, sizeof(SaveData));
        if (tmp.magic == m_data.magic && tmp.version == m_data.version)
            m_data = tmp;
    }

    // Called when player completes a level
    void onLevelComplete(int levelIdx, float time, int deathsThisLevel)
    {
        m_data.highestLevel = (m_data.highestLevel > levelIdx+1) ? m_data.highestLevel : levelIdx+1;
        m_data.totalDeaths += deathsThisLevel;
        if (levelIdx >= 0 && levelIdx < 9)
            m_data.bestTime[levelIdx] = (m_data.bestTime[levelIdx] < time) ? m_data.bestTime[levelIdx] : time;
        save();
    }

    void onDeath() { ++m_data.totalDeaths; }

private:
    std::string getSavePath()
    {
#ifdef _WIN32
        char* appdata = nullptr;
        size_t sz = 0;
        if (_dupenv_s(&appdata, &sz, "APPDATA") == 0 && appdata)
        {
            std::string p = std::string(appdata) + "\\Ashen\\";
            free(appdata);
            // Create directory
            CreateDirectoryA(p.c_str(), nullptr);
            return p + "save.dat";
        }
#endif
        return "ashen_save.dat";
    }

    SaveData    m_data;
    std::string m_path;
};
