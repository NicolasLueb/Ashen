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
#include "Audio.h"
#include "Ghost.h"
#include "SaveSystem.h"
#include "Simulation.h"
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

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
    void addArenaPlat(glm::vec2 center, glm::vec2 half,
                      bool ceiling=false, bool moving=false);
    void updateMainMenu(float dt);
    void updatePlaying(float dt);
    void updateBossFight(float dt);
    void updateTransition(float dt);
    void renderGame();
    void renderHUD();
    void renderDevOverlay();

    void respawnPlayer();
    void killPlayer();

    // Fragment pickup display
    struct FragmentPopup {
        std::string text;
        float       timer = 0.f;
        glm::vec2   screenPos = {0.f,0.f};
    };
    std::vector<FragmentPopup> m_fragmentPopups;
    void updateFragmentPopups(float dt);

    glm::vec2 m_portalPos  = {0.f,0.f};
    bool      m_portalOpen = false;
    float     m_portalAnim = 0.f;

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
    Audio            m_audio;
    Ghost            m_ghost;
    SaveSystem       m_save;

    int   m_vpW = 1200, m_vpH = 850;
    float m_fadeAlpha      = 1.f;
    float m_fadeDir        = -1.f;
    bool  m_pendingLevel   = false;
    int   m_nextLevel      = 0;
    float m_menuAnimTime   = 0.f;
    float m_invincTimer    = 0.f;
    float m_bossIntroTimer = 0.f;
    float m_deathTimer     = 0.f;
    bool  m_deathPending   = false;
    bool  m_devOverlay     = false;
    const float DEATH_PAUSE    = 0.35f;
    const float INVINCIBLE_DUR = 0.8f;
};
