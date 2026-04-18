#pragma once
#include "Biome.h"
#include <string>
#include <array>
#include <glm/glm.hpp>

// ============================================================
//  PhysicsPreset — locked per biome
//  TUNED for difficulty: snappier gravity, tighter movement
// ============================================================
struct PhysicsPreset
{
    float gravity;
    float damping;
    float restitution;
    float playerMoveSpeed;
    float playerJumpVel;
    float playerFallGrav;    // multiplier when falling — higher = snappier arc
    float playerJumpCutGrav; // multiplier when jump released early
    float grappleRange;
    int   solverIterations;

    static PhysicsPreset forBiome(Biome b)
    {
        switch (b)
        {
        default:
        case Biome::Catacombs:
            // Heavy, grounded, unforgiving. Short jump window.
            return {-14.f, 0.999f, 0.05f,
                    6.5f, 13.f, 2.2f, 3.5f, 10.f, 20};
        case Biome::Cathedral:
            // Lighter gravity but still snappy. Wide gaps demand grapple.
            return {-11.f, 0.999f, 0.10f,
                    7.5f, 15.f, 1.8f, 3.0f, 14.f, 18};
        case Biome::Abyss:
            // Low gravity but very fast fall-off. Grapple mandatory.
            return {-7.f,  0.998f, 0.20f,
                    9.f,  17.f, 1.4f, 2.5f, 18.f, 16};
        }
    }
};

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
    std::array<const char*, 3> fragments;

    static std::array<LevelConfig, 9> allLevels()
    {
        return {{
            {0,Biome::Catacombs,"The Sunken Halls",   1001,20,false,0.f, 0.30f,1.2f,2.8f,
             {"It fell here first.","The stone remembers weight.","Do not look down."}},
            {1,Biome::Catacombs,"The Bone Corridor",  1002,24,false,0.f, 0.55f,1.5f,3.5f,
             {"Someone ran this path before.","The rope saved them.","It did not save them."}},
            {2,Biome::Catacombs,"Throne of the Golem",1003,10,true, 60.f,0.70f,2.0f,4.0f,
             {"It was made to guard nothing.","Still it guards.","Outlast it."}},
            {3,Biome::Cathedral,"The Shattered Nave", 2001,22,false,0.f, 0.60f,2.0f,5.0f,
             {"Light broke here.","The glass remembers colour.","We do not."}},
            {4,Biome::Cathedral,"Stained Light",      2002,26,false,0.f, 0.75f,2.5f,6.0f,
             {"Do not use the rope.","(they used the rope)","(they are still here)"}},
            {5,Biome::Cathedral,"The Wraith's Altar", 2003,10,true, 90.f,0.85f,2.8f,6.5f,
             {"It was a person once.","It forgot.","You will too."}},
            {6,Biome::Abyss,    "Edge of Nothing",    3001,24,false,0.f, 0.80f,3.0f,7.0f,
             {"The void is not empty.","It is full of things that fell.","You are falling."}},
            {7,Biome::Abyss,    "The Void Expanse",   3002,28,false,0.f, 0.90f,3.5f,8.0f,
             {"Gravity is a suggestion here.","Reject it.","Carefully."}},
            {8,Biome::Abyss,    "Bone Dragon's Lair", 3003,10,true,120.f,1.00f,4.0f,9.0f,
             {"It has been here since the fall.","Everything ends here.","Prove it wrong."}},
        }};
    }
};

enum class GameScreen
{
    MainMenu, Playing, BossFight,
    LevelComplete, BossDefeated, GameOver, Victory
};

struct GameState
{
    GameScreen screen       = GameScreen::MainMenu;
    int        currentLevel = 0;
    int        score        = 0;
    int        runCount     = 0;
    int        runThisLevel = 0;
    float      bossTimer    = 0.f;
    float      levelTimer   = 0.f;
    bool       bossRage     = false;
    float      transitionTimer = 0.f;
    int        highestReached  = 0;

    LevelConfig   currentConfig() const { return LevelConfig::allLevels()[currentLevel]; }
    Biome         currentBiome()  const { return currentConfig().biome; }
    PhysicsPreset currentPreset() const { return PhysicsPreset::forBiome(currentBiome()); }
    bool          isLastLevel()   const { return currentLevel >= 8; }

    void die()            { ++runCount; ++runThisLevel; }
    void completeLevel()
    {
        score += 100 + (int)(1000.f/(levelTimer+1.f));
        if (currentLevel > highestReached) highestReached = currentLevel;
        if (isLastLevel()) screen = GameScreen::Victory;
        else { currentLevel++; runThisLevel=0; screen=GameScreen::LevelComplete; transitionTimer=0.f; }
    }
    void defeatBoss()
    {
        score += 500 + (int)(2000.f/(bossTimer+1.f));
        if (currentLevel > highestReached) highestReached = currentLevel;
        screen = GameScreen::BossDefeated; transitionTimer=0.f;
    }
    void reset()
    {
        screen=GameScreen::MainMenu; currentLevel=0; score=0;
        runCount=0; runThisLevel=0; bossTimer=0.f; levelTimer=0.f;
        bossRage=false; transitionTimer=0.f;
    }
};
