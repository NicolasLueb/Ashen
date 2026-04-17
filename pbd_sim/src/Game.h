#pragma once
#include "GameState.h"
#include "Player.h"
#include "PlayerController.h"
#include "Camera.h"
#include "Level.h"
#include "BossController.h"
#include "HUD.h"
#include "MainMenu.h"
#include "GameRenderer.h"
#include "VFX.h"
#include "Simulation.h"
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>

class Game
{
public:
    void init(GLFWwindow* win, Simulation* sim,
              GameRenderer* renderer, int vpW, int vpH);
    void resize(int w, int h);
    void update(float dt);
    void render();
    void onMouseButton(int button, int action, double mx, double my);
    void onKey(int key, int action);

    GameState& state()  { return m_gs; }
    Camera&    camera() { return m_camera; }

private:
    void loadLevel(int index);
    void applyPhysicsPreset(const PhysicsPreset& preset);
    void buildBossArena();
    void updateMainMenu(float dt);
    void updatePlaying(float dt);
    void updateBossFight(float dt);
    void updateTransition(float dt);
    void renderGame();
    void renderHUD();
    void renderDevOverlay();   // F1 in-game dev shortcuts panel

    // REVISED: instant death, no lives
    void respawnPlayer();
    void killPlayer();           // instant restart, no penalty

    // Portal and fragment state
    glm::vec2 m_portalPos  = {0.f,0.f};
    bool      m_portalOpen = false;
    float     m_portalAnim = 0.f;

    // Text fragments for current level
    std::vector<glm::vec2> m_fragmentPositions;
    std::vector<bool>      m_fragmentFound;

    GLFWwindow*      m_win      = nullptr;
    Simulation*      m_sim      = nullptr;
    GameRenderer*    m_renderer = nullptr;

    GameState        m_gs;
    Player           m_player;
    PlayerController m_ctrl;
    Camera           m_camera;
    Level            m_level;
    BossController   m_boss;
    HUD              m_hud;
    MainMenu         m_menu;
    VFX              m_vfx;

    int   m_vpW = 1200, m_vpH = 850;
    float m_fadeAlpha      = 1.f;
    float m_fadeDir        = -1.f;
    bool  m_pendingLevel   = false;
    int   m_nextLevel      = 0;
    float m_menuAnimTime   = 0.f;
    float m_invincTimer    = 0.f;
    float m_bossIntroTimer = 0.f;
    float m_deathTimer     = 0.f;   // brief pause before respawn
    bool  m_deathPending   = false;
    bool  m_devOverlay     = false;  // F1 toggles dev shortcut overlay
    const float DEATH_PAUSE    = 0.35f;
    const float INVINCIBLE_DUR = 0.8f;
};
