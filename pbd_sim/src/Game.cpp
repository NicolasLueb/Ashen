#include "Game.h"
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  Init
// ============================================================
void Game::init(GLFWwindow* win, Simulation* sim,
                GameRenderer* renderer, int vpW, int vpH)
{
    m_win=win; m_sim=sim; m_renderer=renderer;
    m_vpW=vpW; m_vpH=vpH;

    m_camera.setViewport(vpW,vpH);
    m_camera.halfHeight   = 7.f;
    m_camera.followSpeedX = 6.f;
    m_camera.followSpeedY = 4.f;
    m_camera.lookAheadDist= 2.5f;

    m_hud.init(vpW,vpH);
    m_menu.init(vpW,vpH);
    m_vfx.init(vpW,vpH);

    m_gs.reset();
    m_gs.screen = GameScreen::MainMenu;
    m_fadeAlpha = 1.f;
    m_fadeDir   = -1.f;
}

void Game::resize(int w, int h)
{
    m_vpW=w; m_vpH=h;
    m_camera.setViewport(w,h);
    m_hud.resize(w,h);
    m_menu.resize(w,h);
    m_vfx.resize(w,h);
}

// ============================================================
//  applyPhysicsPreset — LOCKED, never exposed in play mode
// ============================================================
void Game::applyPhysicsPreset(const PhysicsPreset& p)
{
    SimConfig& cfg       = m_sim->config();
    cfg.gravity.y        = p.gravity;
    cfg.damping          = p.damping;
    cfg.restitution      = p.restitution;
    cfg.solverIterations = p.solverIterations;
    cfg.floorY           = m_level.deathY + 1.f;

    m_player.cfg.jumpVelocity  = p.playerJumpVel;
    m_player.cfg.moveSpeed     = p.playerMoveSpeed;
    m_player.cfg.fallGravScale = p.playerFallGrav;
    m_player.cfg.grappleRange  = p.grappleRange;
    m_player.cfg.gravityScale  = 1.f;
}

// ============================================================
//  loadLevel
// ============================================================
void Game::loadLevel(int index)
{
    m_gs.currentLevel = index;
    m_gs.levelTimer   = 0.f;
    m_gs.bossTimer    = 0.f;
    m_gs.bossRage     = false;

    auto cfg    = m_gs.currentConfig();
    auto preset = m_gs.currentPreset();

    m_sim->reset();
    m_level.generate(cfg.seed, cfg.platformCount);
    m_level.deathY = -25.f;

    applyPhysicsPreset(preset);
    respawnPlayer();

    // Place portal
    if (!m_level.platforms.empty())
    {
        const auto& last = m_level.platforms.back();
        m_portalPos = {last.pos.x, last.pos.y+last.size.y+1.5f};
    }
    m_portalOpen  = !cfg.isBossLevel;
    m_portalAnim  = 0.f;

    // Place text fragments at interesting positions
    m_fragmentPositions.clear();
    m_fragmentFound.clear();
    int numPlatforms = (int)m_level.platforms.size();
    for (int i=0; i<3; ++i)
    {
        int pidx = glm::min(i * numPlatforms/3 + 2, numPlatforms-1);
        const auto& plat = m_level.platforms[pidx];
        m_fragmentPositions.push_back({plat.pos.x, plat.pos.y+plat.size.y+0.8f});
        m_fragmentFound.push_back(false);
    }

    m_gs.screen = cfg.isBossLevel ? GameScreen::BossFight : GameScreen::Playing;
    if (cfg.isBossLevel) { m_bossIntroTimer=2.f; buildBossArena(); }

    m_fadeAlpha = 1.f;
    m_fadeDir   = -1.f;

    printf("Level %d: %s\n", index+1, cfg.name.c_str());
}

// ============================================================
//  buildBossArena
// ============================================================
void Game::buildBossArena()
{
    m_level.platforms.clear();
    m_level.walls.clear();
    float floorY = -4.5f;
    m_level.deathY = -20.f;

    m_level.platforms.push_back({{0.f,floorY},{9.f,0.5f},true});
    m_level.platforms.push_back({{-6.f,floorY+2.5f},{1.8f,0.3f},true});
    m_level.platforms.push_back({{-2.f,floorY+3.8f},{1.8f,0.3f},true});
    m_level.platforms.push_back({{ 2.f,floorY+3.8f},{1.8f,0.3f},true});
    m_level.platforms.push_back({{ 6.f,floorY+2.5f},{1.8f,0.3f},true});
    m_level.platforms.push_back({{-9.5f,floorY+3.f},{0.3f,4.f},false});
    m_level.platforms.push_back({{ 9.5f,floorY+3.f},{0.3f,4.f},false});

    m_boss.start(floorY,-9.f,9.f);
    m_gs.screen  = GameScreen::BossFight;
    m_portalOpen = false;
    m_portalPos  = {0.f,floorY+2.f};
}

// ============================================================
//  respawnPlayer — instant, no fade
// ============================================================
void Game::respawnPlayer()
{
    m_player.position     = {0.f, m_level.startY+2.f};
    m_player.velocity     = {0.f, 0.f};
    m_player.state        = PlayerState::Falling;
    m_player.grappleActive = false;
    m_invincTimer         = INVINCIBLE_DUR;
    m_deathPending        = false;
    m_deathTimer          = 0.f;
    m_camera.position     = m_player.position;
    m_camera.lookAheadOffset = {0.f,0.f};
}

// ============================================================
//  killPlayer — instant death, Super Meat Boy style
//  Brief freeze frame, VFX burst, then instant respawn
// ============================================================
void Game::killPlayer()
{
    if (m_invincTimer > 0.f || m_deathPending) return;

    m_gs.die();
    m_vfx.spawnDeathBurst(m_player.position);
    m_vfx.spawnGlitch(m_player.position, 1.2f);
    m_camera.shake(0.6f);

    m_deathPending = true;
    m_deathTimer   = DEATH_PAUSE;

    // Release grapple instantly
    m_ctrl.releaseGrapple(m_player,
                           m_sim->particles_mut(),
                           m_sim->constraints_mut());
}

// ============================================================
//  Update
// ============================================================
void Game::update(float dt)
{
    dt = glm::min(dt,0.05f);

    // Screen fade
    m_fadeAlpha = glm::clamp(m_fadeAlpha + m_fadeDir*dt*1.8f, 0.f,1.f);

    // Pending level load (after fade out)
    if (m_pendingLevel && m_fadeAlpha >= 1.f)
    {
        loadLevel(m_nextLevel);
        m_pendingLevel = false;
    }

    // Death pause timer
    if (m_deathPending)
    {
        m_deathTimer -= dt;
        if (m_deathTimer <= 0.f) respawnPlayer();
        m_vfx.update(dt);
        m_camera.update(m_player.position, {0.f,0.f}, dt);
        return;  // freeze the world during death pause
    }

    m_invincTimer   = glm::max(m_invincTimer-dt, 0.f);
    m_portalAnim   += dt;
    m_menuAnimTime += dt;

    m_vfx.update(dt);

    // Apply VFX screen shake to camera
    if (m_vfx.screenShake() > 0.f)
        m_camera.shake(m_vfx.screenShake());

    switch(m_gs.screen)
    {
    case GameScreen::MainMenu:     updateMainMenu(dt);   break;
    case GameScreen::Playing:      updatePlaying(dt);    break;
    case GameScreen::BossFight:    updateBossFight(dt);  break;
    case GameScreen::LevelComplete:
    case GameScreen::BossDefeated:
    case GameScreen::GameOver:
    case GameScreen::Victory:      updateTransition(dt); break;
    }
}

void Game::updateMainMenu(float dt)
{
    m_menu.highestReached = m_gs.highestReached;
}

void Game::updatePlaying(float dt)
{
    m_gs.levelTimer += dt;
    auto preset = m_gs.currentPreset();

    m_ctrl.update(m_win, m_player, m_level,
                  m_sim->particles_mut(), m_sim->constraints_mut(),
                  preset.gravity, dt);
    m_sim->step(dt);
    m_camera.update(m_player.position, m_player.velocity, dt);

    // Check text fragments
    for (int i=0;i<(int)m_fragmentPositions.size();++i)
    {
        if (m_fragmentFound[i]) continue;
        glm::vec2 d = m_player.position - m_fragmentPositions[i];
        if (glm::dot(d,d) < 1.2f*1.2f)
        {
            m_fragmentFound[i] = true;
            m_vfx.spawnGlitch(m_fragmentPositions[i], 0.4f);
        }
    }

    // Death
    if (m_player.position.y < m_level.deathY) killPlayer();

    // Portal
    if (m_portalOpen)
    {
        glm::vec2 d = m_player.position - m_portalPos;
        if (glm::dot(d,d) < 1.5f*1.5f)
        {
            m_gs.completeLevel();
            if (m_gs.screen == GameScreen::LevelComplete ||
                m_gs.screen == GameScreen::Victory)
            {
                m_fadeDir = 1.f;
                if (!m_gs.isLastLevel())
                { m_pendingLevel=true; m_nextLevel=m_gs.currentLevel; }
            }
        }
    }
}

void Game::updateBossFight(float dt)
{
    if (m_bossIntroTimer > 0.f)
    {
        m_bossIntroTimer -= dt;
        m_camera.update(m_player.position, m_player.velocity, dt);
        return;
    }

    m_gs.bossTimer += dt;
    m_gs.bossRage = (m_gs.bossTimer >= m_gs.currentConfig().bossTimer * 0.75f);

    auto preset = m_gs.currentPreset();
    m_ctrl.update(m_win, m_player, m_level,
                  m_sim->particles_mut(), m_sim->constraints_mut(),
                  preset.gravity, dt);

    bool wantsShake = m_boss.update(
        m_gs.bossTimer, m_gs.currentConfig().bossTimer,
        m_player.position,
        m_sim->particles_mut(), m_sim->constraints_mut(), dt);
    if (wantsShake) m_camera.shake(m_boss.shakeAmount);

    m_sim->step(dt);
    m_camera.update(m_player.position, m_player.velocity, dt);

    // Boss hits player — trigger glitch + death
    if (m_invincTimer <= 0.f &&
        m_boss.checkPlayerHit(m_player.position, m_player.cfg.bodyRadius))
    {
        m_vfx.spawnGlitch(m_player.position, 0.8f);
        killPlayer();
    }

    // Win condition
    if (m_gs.bossTimer >= m_gs.currentConfig().bossTimer)
    {
        m_boss.stop();
        m_portalOpen = true;
        glm::vec2 d = m_player.position - m_portalPos;
        if (glm::dot(d,d) < 2.f*2.f)
        {
            m_gs.defeatBoss();
            m_fadeDir = 1.f;
            if (!m_gs.isLastLevel())
            { m_pendingLevel=true; m_nextLevel=m_gs.currentLevel; }
        }
    }

    if (m_player.position.y < m_level.deathY) killPlayer();
    m_boss.cleanup(m_sim->particles_mut(), m_sim->constraints_mut());
}

void Game::updateTransition(float dt)
{
    m_gs.transitionTimer += dt;
    if (m_gs.transitionTimer > 2.5f)
    {
        switch(m_gs.screen)
        {
        case GameScreen::GameOver:
        case GameScreen::Victory:
            m_gs.reset();
            m_gs.screen = GameScreen::MainMenu;
            m_menu.screen = MainMenu::Screen::Root;
            m_fadeDir = -1.f;
            break;
        case GameScreen::BossDefeated:
            m_gs.transitionTimer=0.f;
            if (!m_gs.isLastLevel())
            { m_pendingLevel=true; m_nextLevel=m_gs.currentLevel; m_fadeDir=1.f; }
            else m_gs.screen=GameScreen::Victory;
            break;
        default: break;
        }
    }
}

// ============================================================
//  Render
// ============================================================
void Game::render()
{
    if (m_gs.screen == GameScreen::MainMenu)
    {
        // Plain dark background behind menu
        glClearColor(0.04f,0.03f,0.06f,1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        renderHUD();
        return;
    }

    // World render
    m_renderer->draw(m_camera, m_level, m_player,
                     m_sim->particles(), m_sim->constraints(),
                     m_gs.currentBiome());

    glm::mat4 VP = m_camera.viewProjection();

    // Boss
    if ((m_gs.screen==GameScreen::BossFight) && m_boss.active)
    {
        m_renderer->drawBoss(m_boss.position, m_boss.size,
                             m_gs.bossRage, m_boss.animTime, VP);
        for (const auto& sw : m_boss.shockwaves)
            if (sw.alive)
                m_renderer->drawShockwave(sw.x, sw.y, sw.width, sw.height,
                                          m_boss.animTime, VP);
    }

    // Portal
    if (m_portalOpen)
        m_renderer->drawPortal(m_portalPos, m_portalAnim, VP);

    // Text fragments
    auto cfg = m_gs.currentConfig();
    for (int i=0;i<(int)m_fragmentPositions.size();++i)
        m_renderer->drawFragment(m_fragmentPositions[i],
                                 m_fragmentFound[i],
                                 m_portalAnim, VP);

    // VFX (grain, vignette, glitch, death particles)
    m_vfx.draw(VP, (float)m_vpW, (float)m_vpH);

    // Dev overlay (F1)
    if (m_devOverlay) renderDevOverlay();

    renderHUD();
}

void Game::renderHUD()
{
    float pw=(float)m_vpW, ph=(float)m_vpH;

    switch(m_gs.screen)
    {
    case GameScreen::MainMenu:
        m_menu.draw(m_menuAnimTime);
        break;
    case GameScreen::Playing:
    case GameScreen::BossFight:
        m_hud.draw(m_gs, m_gs.bossTimer,
                   m_gs.currentConfig().bossTimer,
                   m_gs.bossRage, m_fadeAlpha);
        break;
    case GameScreen::GameOver:
        m_hud.draw(m_gs,0.f,1.f,false,m_fadeAlpha);
        m_hud.drawGameOver(pw,ph,m_gs.score);
        break;
    case GameScreen::Victory:
        m_hud.draw(m_gs,0.f,1.f,false,m_fadeAlpha);
        m_hud.drawVictory(pw,ph,m_gs.score);
        break;
    case GameScreen::LevelComplete:
    case GameScreen::BossDefeated:
        m_hud.draw(m_gs,0.f,1.f,false,m_fadeAlpha);
        m_hud.drawLevelComplete(pw,ph);
        break;
    }
}

// ============================================================
//  Input
// ============================================================
void Game::onMouseButton(int button, int action, double mx, double my)
{
    if (button==GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action==GLFW_PRESS)
        {
            glm::vec2 world=m_camera.screenToWorld((float)mx,(float)my,m_vpW,m_vpH);
            m_ctrl.fireGrapple(m_player,m_level,
                               m_sim->particles_mut(),m_sim->constraints_mut(),world);
        }
        else
            m_ctrl.releaseGrapple(m_player,
                                   m_sim->particles_mut(),m_sim->constraints_mut());
    }
}

void Game::onKey(int key, int action)
{
    if (m_gs.screen==GameScreen::MainMenu)
    {
        MenuAction act=m_menu.handleKey(key,action);
        switch(act)
        {
        case MenuAction::StartGame:
            m_gs.reset(); m_pendingLevel=true; m_nextLevel=0; m_fadeDir=1.f; break;
        case MenuAction::GoToLevel:
            m_gs.reset(); m_pendingLevel=true;
            m_nextLevel=m_menu.m_selectedLevel; m_fadeDir=1.f; break;
        case MenuAction::Quit:
            glfwSetWindowShouldClose(m_win,GLFW_TRUE); break;
        default: break;
        }
        return;
    }

    if (action!=GLFW_PRESS) return;

    // F1 = toggle dev overlay
    if (key == GLFW_KEY_F1) { m_devOverlay = !m_devOverlay; return; }

    // Dev shortcuts when overlay is open
    if (m_devOverlay)
    {
        // N = open portal right now (skip to end of level)
        if (key == GLFW_KEY_N)
        {
            m_portalOpen     = true;
            m_boss.stop();
            m_player.position = m_portalPos;
            return;
        }
        // 1-9 = jump directly to that level
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9)
        {
            int lvl = glm::clamp(key - GLFW_KEY_1, 0, 8);
            m_gs.reset();
            m_pendingLevel = true;
            m_nextLevel    = lvl;
            m_fadeDir      = 1.f;
            m_devOverlay   = false;
            return;
        }
        // B = jump to next boss from current level
        if (key == GLFW_KEY_B)
        {
            int bosses[] = {2,5,8};
            for (int b : bosses)
                if (b >= m_gs.currentLevel)
                {
                    m_gs.reset();
                    m_pendingLevel = true;
                    m_nextLevel    = b;
                    m_fadeDir      = 1.f;
                    m_devOverlay   = false;
                    return;
                }
        }
    }

    if (key==GLFW_KEY_R) respawnPlayer();

    if (key==GLFW_KEY_ENTER||key==GLFW_KEY_SPACE)
    {
        if (m_gs.screen==GameScreen::GameOver||m_gs.screen==GameScreen::Victory)
        {
            m_gs.reset(); m_gs.screen=GameScreen::MainMenu;
            m_menu.screen=MainMenu::Screen::Root; m_fadeDir=-1.f;
        }
    }
}

// ============================================================
//  renderDevOverlay
//  Drawn in screen space — uses the HUD's shader infrastructure.
//  Shows a compact panel with all dev shortcuts.
// ============================================================
void Game::renderDevOverlay()
{
    float pw = (float)m_vpW, ph = (float)m_vpH;

    // Semi-transparent dark panel on the right side
    // We reuse the HUD's drawRect — but HUD is a separate object.
    // Instead we draw via OpenGL directly using VFX's screen quad.
    // Simple approach: draw using the same ortho proj as HUD.
    glm::mat4 proj = glm::ortho(0.f,pw,ph,0.f,-1.f,1.f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Panel background — right side, 260px wide
    float px = pw - 280.f, py = 10.f, panW = 270.f, panH = 340.f;

    // Draw via m_vfx shader-less path — use HUD draw calls
    // Since HUD doesn't expose drawRect publicly, we use ImGui for this overlay
    // (ImGui is always available and perfect for dev tools)
    // We set a flag that main.cpp reads to draw the ImGui dev panel
    // Nothing to draw here directly — see main.cpp ImGui section
    (void)proj; (void)px; (void)py; (void)panW; (void)panH;
}
