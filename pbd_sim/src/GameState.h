#pragma once
#include <string>
#include <array>
#include <glm/glm.hpp>

enum class Biome { Catacombs = 0, Cathedral = 1, Abyss = 2 };

// ============================================================
//  PhysicsPreset — locked per biome, never exposed in play mode
// ============================================================
struct PhysicsPreset
{
    float gravity;
    float damping;
    float restitution;
    float playerMoveSpeed;
    float playerJumpVel;
    float playerFallGrav;
    float grappleRange;
    int   solverIterations;

    static PhysicsPreset forBiome(Biome b)
    {
        switch (b)
        {
        default:
        case Biome::Catacombs: return {-9.81f,0.999f,0.15f, 7.f,12.f,1.6f,10.f,20};
        case Biome::Cathedral: return {-7.50f,0.999f,0.20f, 8.f,14.f,1.4f,13.f,18};
        case Biome::Abyss:     return {-4.50f,0.998f,0.35f, 9.f,16.f,1.1f,16.f,16};
        }
    }
};

// ============================================================
//  TextFragment — cryptic story fragment found in a level
// ============================================================
struct TextFragment
{
    glm::vec2   worldPos;    // where it appears in the level
    std::string text;        // the fragment text
    bool        found = false;
};

// ============================================================
//  LevelConfig
// ============================================================
struct LevelConfig
{
    int          levelIndex;
    Biome        biome;
    std::string  name;
    unsigned int seed;
    int          platformCount;
    bool         isBossLevel;
    float        bossTimer;
    float        difficultyScale;
    float        gapMin;
    float        gapMax;

    // Text fragments for this level
    std::array<const char*, 3> fragments;

    static std::array<LevelConfig, 9> allLevels()
    {
        return {{
            {0,Biome::Catacombs,"The Sunken Halls",   1001,18,false,0.f, 0.20f,1.5f,3.0f,
             {"It fell here first.","The stone remembers weight.","Do not look down."}},
            {1,Biome::Catacombs,"The Bone Corridor",  1002,22,false,0.f, 0.40f,1.8f,3.5f,
             {"Someone ran this path before.","The rope saved them.","It did not save them."}},
            {2,Biome::Catacombs,"Throne of the Golem",1003,10,true, 60.f,0.55f,2.0f,3.8f,
             {"It was made to guard nothing.","Still it guards.","Outlast it."}},
            {3,Biome::Cathedral,"The Shattered Nave", 2001,20,false,0.f, 0.50f,2.0f,4.0f,
             {"Light broke here.","The glass remembers colour.","We do not."}},
            {4,Biome::Cathedral,"Stained Light",      2002,24,false,0.f, 0.65f,2.2f,4.5f,
             {"Do not use the rope.","(they used the rope)","(they are still here)"}},
            {5,Biome::Cathedral,"The Wraith's Altar", 2003,10,true, 90.f,0.75f,2.4f,5.0f,
             {"It was a person once.","It forgot.","You will too."}},
            {6,Biome::Abyss,    "Edge of Nothing",    3001,22,false,0.f, 0.70f,2.5f,5.5f,
             {"The void is not empty.","It is full of things that fell.","You are falling."}},
            {7,Biome::Abyss,    "The Void Expanse",   3002,26,false,0.f, 0.85f,2.8f,6.0f,
             {"Gravity is a suggestion here.","Reject it.","Carefully."}},
            {8,Biome::Abyss,    "Bone Dragon's Lair", 3003,10,true,120.f,1.00f,3.0f,6.5f,
             {"It has been here since the fall.","Everything ends here.","Prove it wrong."}},
        }};
    }
};

// ============================================================
//  GameScreen
// ============================================================
enum class GameScreen
{
    MainMenu,
    Playing,
    BossFight,
    LevelComplete,
    BossDefeated,
    GameOver,
    Victory
};

// ============================================================
//  GameState
//
//  REVISED: No lives system. Pure run-based like Super Meat Boy.
//  Death = instant level restart, no penalty except time lost.
//  The run counter tracks how many attempts the player has made.
// ============================================================
struct GameState
{
    GameScreen screen       = GameScreen::MainMenu;
    int        currentLevel = 0;
    int        score        = 0;
    int        runCount     = 0;   // total deaths this session
    int        runThisLevel = 0;   // deaths on current level
    float      bossTimer    = 0.f;
    float      levelTimer   = 0.f;
    bool       bossRage     = false;
    float      transitionTimer = 0.f;

    // Highest level reached this session (for level select lock)
    int highestReached = 0;

    LevelConfig currentConfig() const
    {
        auto levels = LevelConfig::allLevels();
        return levels[currentLevel];
    }

    Biome         currentBiome()  const { return currentConfig().biome; }
    PhysicsPreset currentPreset() const { return PhysicsPreset::forBiome(currentBiome()); }
    bool          isLastLevel()   const { return currentLevel >= 8; }

    // Instant death — restart level, no lives lost
    void die()
    {
        ++runCount;
        ++runThisLevel;
        // screen stays the same, game manager handles respawn
    }

    void completeLevel()
    {
        score += 100 + (int)(1000.f / (levelTimer + 1.f));
        if (currentLevel > highestReached) highestReached = currentLevel;
        if (isLastLevel())
            screen = GameScreen::Victory;
        else
        {
            currentLevel++;
            runThisLevel = 0;
            screen = GameScreen::LevelComplete;
            transitionTimer = 0.f;
        }
    }

    void defeatBoss()
    {
        score += 500 + (int)(2000.f / (bossTimer + 1.f));
        if (currentLevel > highestReached) highestReached = currentLevel;
        screen = GameScreen::BossDefeated;
        transitionTimer = 0.f;
    }

    void reset()
    {
        screen         = GameScreen::MainMenu;
        currentLevel   = 0;
        score          = 0;
        runCount       = 0;
        runThisLevel   = 0;
        bossTimer      = 0.f;
        levelTimer     = 0.f;
        bossRage       = false;
        transitionTimer = 0.f;
        // highestReached persists for level select
    }
};
