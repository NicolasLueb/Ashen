#define NOMINMAX
#include "Game.h"
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <functional>

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
    m_audio.init();
    m_save.init();

    // Restore highest level from save
    m_gs.highestReached = m_save.data().highestLevel;
    m_menu.highestReached = m_gs.highestReached;

    m_gs.reset();
    m_gs.screen = GameScreen::MainMenu;
    m_gs.highestReached = m_save.data().highestLevel;
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
//  applyPhysicsPreset
// ============================================================
void Game::applyPhysicsPreset(const PhysicsPreset& p)
{
    SimConfig& cfg       = m_sim->config();
    cfg.gravity.y        = p.gravity;
    cfg.damping          = p.damping;
    cfg.restitution      = p.restitution;
    cfg.solverIterations = p.solverIterations;
    cfg.floorY           = m_level.deathY + 1.f;

    m_player.cfg.jumpVelocity     = p.playerJumpVel;
    m_player.cfg.moveSpeed        = p.playerMoveSpeed;
    m_player.cfg.fallGravScale    = p.playerFallGrav;
    m_player.cfg.jumpCutGravScale = p.playerJumpCutGrav;
    m_player.cfg.grappleRange     = p.grappleRange;
    m_player.cfg.gravityScale     = 1.f;
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
    m_gs.runThisLevel = 0;

    auto cfg    = m_gs.currentConfig();
    auto preset = m_gs.currentPreset();

    m_sim->reset();
    m_level.generate(cfg.seed, cfg.platformCount, cfg.biome);
    m_level.deathY = -25.f;

    applyPhysicsPreset(preset);
    respawnPlayer();

    // Place portal at end
    if (!m_level.platforms.empty())
    {
        const auto& last = m_level.platforms.back();
        m_portalPos = {last.pos.x, last.pos.y+last.size.y+1.5f};
    }
    m_portalOpen  = !cfg.isBossLevel;
    m_portalAnim  = 0.f;

    // Place text fragments
    m_fragmentPositions.clear();
    m_fragmentFound.clear();
    m_fragmentPopups.clear();
    int np = (int)m_level.platforms.size();
    for (int i=0; i<3; ++i)
    {
        int pidx = ((i*np/3+2)<(np-1)?(i*np/3+2):(np-1));
        // Skip spike/ceiling platforms
        while (pidx < np && (m_level.platforms[pidx].isSpike ||
                              m_level.platforms[pidx].isCeiling))
            pidx++;
        if (pidx >= np) pidx = np-1;
        const auto& plat = m_level.platforms[pidx];
        m_fragmentPositions.push_back({plat.pos.x, plat.pos.y+plat.size.y+0.8f});
        m_fragmentFound.push_back(false);
    }

    m_gs.screen = cfg.isBossLevel ? GameScreen::BossFight : GameScreen::Playing;
    if (cfg.isBossLevel) { m_bossIntroTimer=2.f; buildBossArena(); }

    // Ghost: start new recording, begin playback of previous best
    m_ghost.beginPlayback();
    m_ghost.beginRecord();

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
    m_level.windZones.clear();

    BossType btype = BossType::Golem;
    switch(m_gs.currentBiome())
    {
    case Biome::Catacombs: btype = BossType::Golem;  break;
    case Biome::Cathedral: btype = BossType::Wraith; break;
    case Biome::Abyss:     btype = BossType::Dragon; break;
    }

    if (btype == BossType::Golem)
    {
        float floorY = -4.5f;
        float ceilY  = floorY + 5.5f;
        m_level.deathY = -20.f;
        addArenaPlat({0.f, floorY},        {9.f, 0.5f});
        addArenaPlat({-9.5f, floorY+3.f},  {0.3f, 4.f});
        addArenaPlat({ 9.5f, floorY+3.f},  {0.3f, 4.f});
        addArenaPlat({0.f, ceilY},         {9.5f, 0.3f}, true);
        addArenaPlat({-6.f, floorY+1.8f},  {1.6f, 0.3f});
        addArenaPlat({-2.f, floorY+3.0f},  {1.5f, 0.3f});
        addArenaPlat({ 2.f, floorY+3.0f},  {1.5f, 0.3f});
        addArenaPlat({ 6.f, floorY+1.8f},  {1.6f, 0.3f});
        m_boss.start(BossType::Golem, floorY, -9.f, 9.f, ceilY);
        m_portalPos = {0.f, floorY + 2.f};
    }
    else if (btype == BossType::Wraith)
    {
        float floorY = -5.f;
        float ceilY  = floorY + 16.f;
        m_level.deathY = -22.f;
        addArenaPlat({0.f,   floorY},      {8.f,  0.5f});
        addArenaPlat({-7.5f, floorY+2.f},  {1.2f, 0.3f});
        addArenaPlat({-4.f,  floorY+5.f},  {1.3f, 0.3f});
        addArenaPlat({ 0.f,  floorY+7.5f}, {1.4f, 0.3f});
        addArenaPlat({ 4.f,  floorY+5.f},  {1.3f, 0.3f});
        addArenaPlat({ 7.5f, floorY+2.f},  {1.2f, 0.3f});
        addArenaPlat({-5.f,  floorY+10.f}, {1.0f, 0.3f});
        addArenaPlat({ 5.f,  floorY+10.f}, {1.0f, 0.3f});
        addArenaPlat({-9.5f, floorY+6.f},  {0.3f, 7.f});
        addArenaPlat({ 9.5f, floorY+6.f},  {0.3f, 7.f});
        m_boss.start(BossType::Wraith, floorY, -9.f, 9.f, ceilY);
        m_portalPos = {0.f, floorY + 2.f};
    }
    else
    {
        float floorY = -6.f;
        float ceilY  = floorY + 18.f;
        m_level.deathY = -25.f;
        addArenaPlat({ 0.f,  floorY},      {4.f,  0.4f});
        addArenaPlat({-6.f,  floorY+2.f},  {1.5f, 0.35f}, false, true);
        addArenaPlat({ 6.f,  floorY+2.f},  {1.5f, 0.35f}, false, true);
        addArenaPlat({-3.f,  floorY+5.5f}, {1.2f, 0.35f});
        addArenaPlat({ 3.f,  floorY+5.5f}, {1.2f, 0.35f});
        addArenaPlat({ 0.f,  floorY+9.f},  {1.0f, 0.35f});
        addArenaPlat({-7.f,  floorY+7.f},  {0.9f, 0.35f});
        addArenaPlat({ 7.f,  floorY+7.f},  {0.9f, 0.35f});
        m_boss.start(BossType::Dragon, floorY, -10.f, 10.f, ceilY);
        m_portalPos = {0.f, floorY + 2.f};
    }

    m_gs.screen  = GameScreen::BossFight;
    m_portalOpen = false;
}

void Game::addArenaPlat(glm::vec2 center, glm::vec2 half,
                         bool ceiling, bool moving)
{
    Platform p;
    p.pos=p.basePos=center; p.size=half;
    p.isCeiling=ceiling; p.isGround=!ceiling;
    p.isMoving=moving;
    if (moving)
    {
        p.moveAxis={0.f,1.f}; p.moveRange=0.8f;
        p.moveSpeed=1.2f; p.movePhase=((float)rand()/RAND_MAX)*6.28f;
    }
    m_level.platforms.push_back(p);
}

// ============================================================
//  respawnPlayer
// ============================================================
void Game::respawnPlayer()
{
    m_level.resetDisappear();
    m_player.position      = {0.f, m_level.startY+2.f};
    m_player.velocity      = {0.f, 0.f};
    m_player.state         = PlayerState::Falling;
    m_player.grappleActive = false;
    m_player.scaleX = m_player.scaleY = 1.f;
    m_invincTimer          = INVINCIBLE_DUR;
    m_deathPending         = false;
    m_deathTimer           = 0.f;
    m_camera.position      = m_player.position;
    m_camera.lookAheadOffset = {0.f,0.f};

    // Restart ghost recording for new attempt
    m_ghost.beginRecord();
}

// ============================================================
//  killPlayer
// ============================================================
void Game::killPlayer()
{
    if (m_invincTimer > 0.f || m_deathPending) return;

    m_gs.die();
    m_audio.play(SoundID::Death);
    m_vfx.spawnDeathBurst(m_player.position);
    m_vfx.spawnGlitch(m_player.position, 1.2f);
    m_camera.shake(0.6f);
    m_ghost.commitOnDeath();  // save best run ghost

    m_deathPending = true;
    m_deathTimer   = DEATH_PAUSE;

    m_ctrl.releaseGrapple(m_player,
                           m_sim->particles_mut(),
                           m_sim->constraints_mut());
}

// ============================================================
//  Fragment popup helpers
// ============================================================
void Game::updateFragmentPopups(float dt)
{
    for (auto& fp : m_fragmentPopups)
        fp.timer -= dt;
    m_fragmentPopups.erase(
        std::remove_if(m_fragmentPopups.begin(), m_fragmentPopups.end(),
            [](const FragmentPopup& f){ return f.timer <= 0.f; }),
        m_fragmentPopups.end());
}

// ============================================================
//  Update
// ============================================================
void Game::update(float dt)
{
    dt = ((dt)<(0.05f)?(dt):(0.05f));

    // Screen fade
    m_fadeAlpha = ((m_fadeAlpha+m_fadeDir*dt*1.8f)<(0.f)?(0.f):((m_fadeAlpha+m_fadeDir*dt*1.8f)>(1.f)?(1.f):(m_fadeAlpha+m_fadeDir*dt*1.8f)));

    if (m_pendingLevel && m_fadeAlpha >= 1.f)
    {
        loadLevel(m_nextLevel);
        m_pendingLevel = false;
    }

    // Death pause
    if (m_deathPending)
    {
        m_deathTimer -= dt;
        if (m_deathTimer <= 0.f) respawnPlayer();
        m_vfx.update(dt);
        m_camera.update(m_player.position, {0.f,0.f}, dt);
        return;
    }

    m_invincTimer  = ((m_invincTimer-dt)>(0.f)?(m_invincTimer-dt):(0.f));
    m_portalAnim  += dt;
    m_menuAnimTime += dt;
    m_vfx.update(dt);
    updateFragmentPopups(dt);

    if (m_vfx.screenShake()>0.f) m_camera.shake(m_vfx.screenShake());

    switch(m_gs.screen)
    {
    case GameScreen::MainMenu:     updateMainMenu(dt);   break;
    case GameScreen::Playing:      updatePlaying(dt);    break;
    case GameScreen::BossFight:    updateBossFight(dt);  break;
    default:                       updateTransition(dt); break;
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

    // Update platforms
    m_level.update(dt, m_gs.levelTimer, m_player.position);

    // Player update
    bool hitSpike = m_ctrl.update(m_win, m_player, m_level,
                  m_sim->particles_mut(), m_sim->constraints_mut(),
                  preset.gravity, dt);

    // Audio events from controller
    if (m_ctrl.m_justJumped) { m_audio.play(SoundID::Jump); m_ctrl.m_justJumped=false; }
    if (m_ctrl.m_justLanded){ m_audio.play(SoundID::Land); m_ctrl.m_justLanded=false; }

    if (hitSpike) { m_audio.play(SoundID::Spike); killPlayer(); return; }

    // Ghost record
    m_ghost.record(m_player.position, m_player.facingRight,
                   (int)m_player.state, dt);

    m_sim->step(dt);
    m_camera.update(m_player.position, m_player.velocity, dt);

    // Fragment pickups
    auto cfg = m_gs.currentConfig();
    for (int i=0;i<(int)m_fragmentPositions.size();++i)
    {
        if (m_fragmentFound[i]) continue;
        glm::vec2 d = m_player.position - m_fragmentPositions[i];
        if (glm::dot(d,d) < 1.2f*1.2f)
        {
            m_fragmentFound[i] = true;
            m_audio.play(SoundID::Fragment);
            m_vfx.spawnGlitch(m_fragmentPositions[i], 0.4f);
            // Show fragment text popup
            FragmentPopup fp;
            fp.text  = cfg.fragments[i];
            fp.timer = 3.5f;
            // Convert world pos to screen pos (approximate)
            fp.screenPos = {(float)m_vpW*0.5f,
                            (float)m_vpH*0.35f};
            m_fragmentPopups.push_back(fp);
        }
    }

    if (m_player.position.y < m_level.deathY) killPlayer();

    if (m_portalOpen)
    {
        glm::vec2 d = m_player.position - m_portalPos;
        if (glm::dot(d,d) < 1.5f*1.5f)
        {
            m_audio.play(SoundID::Portal);
            m_ghost.commitOnComplete();
            m_save.onLevelComplete(m_gs.currentLevel,
                                    m_gs.levelTimer,
                                    m_gs.runThisLevel);
            m_gs.completeLevel();
            if (m_gs.screen==GameScreen::LevelComplete ||
                m_gs.screen==GameScreen::Victory)
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
    if (m_bossIntroTimer>0.f)
    {
        m_bossIntroTimer -= dt;
        m_camera.update(m_player.position, m_player.velocity, dt);
        return;
    }

    m_gs.bossTimer += dt;
    m_gs.bossRage = (m_gs.bossTimer>=m_gs.currentConfig().bossTimer*0.75f);

    m_level.update(dt, m_gs.bossTimer, m_player.position);

    auto preset = m_gs.currentPreset();
    bool hitSpike = m_ctrl.update(m_win, m_player, m_level,
                  m_sim->particles_mut(), m_sim->constraints_mut(),
                  preset.gravity, dt);

    if (m_ctrl.m_justJumped) { m_audio.play(SoundID::Jump); m_ctrl.m_justJumped=false; }
    if (m_ctrl.m_justLanded){ m_audio.play(SoundID::Land); m_ctrl.m_justLanded=false; }

    if (hitSpike) { m_audio.play(SoundID::Spike); killPlayer(); return; }

    // Ghost record during boss
    m_ghost.record(m_player.position, m_player.facingRight,
                   (int)m_player.state, dt);

    bool wantsShake = m_boss.update(
        m_gs.bossTimer, m_gs.currentConfig().bossTimer,
        m_player.position,
        m_sim->particles_mut(), m_sim->constraints_mut(), dt);
    if (wantsShake) m_camera.shake(m_boss.shakeAmount);

    m_sim->step(dt);
    m_camera.update(m_player.position, m_player.velocity, dt);

    // Boss rage transition — play warning sound once
    static bool s_rageAudioFired = false;
    if (m_gs.bossRage && !s_rageAudioFired)
    {
        m_audio.play(SoundID::BossWarn);
        s_rageAudioFired = true;
    }
    if (!m_gs.bossRage) s_rageAudioFired = false;

    // Boss hits player
    if (m_invincTimer<=0.f &&
        m_boss.checkPlayerHit(m_player.position, m_player.cfg.bodyRadius))
    {
        m_audio.play(SoundID::BossHit);
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
            m_audio.play(SoundID::Portal);
            m_ghost.commitOnComplete();
            m_save.onLevelComplete(m_gs.currentLevel,
                                    m_gs.bossTimer,
                                    m_gs.runThisLevel);
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
            m_gs.highestReached = m_save.data().highestLevel;
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
        glClearColor(0.04f,0.03f,0.06f,1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        renderHUD();
        return;
    }

    m_renderer->draw(m_camera, m_level, m_player,
                     m_sim->particles(), m_sim->constraints(),
                     m_gs.currentBiome());

    glm::mat4 VP = m_camera.viewProjection();

    // Ghost replay — draw before player so it appears behind
    const GhostFrame* gf = m_ghost.playback(0.016f); // approximate dt
    if (gf)
        m_renderer->drawGhost({gf->pos.x, gf->pos.y},
                               gf->facingRight, gf->stateIdx, VP);

    // Boss
    if ((m_gs.screen==GameScreen::BossFight) && m_boss.active)
    {
        m_renderer->drawBoss(m_boss.position, m_boss.size,
                             m_gs.bossRage, m_boss.animTime, VP,
                             m_boss.type, m_boss.wraithAlpha);
        for (const auto& sw : m_boss.shockwaves)
            if (sw.alive)
                m_renderer->drawShockwave(sw.x, sw.y, sw.width, sw.height,
                                          m_boss.animTime, VP);
        for (const auto& o : m_boss.orbs)
            if (o.alive)
                m_renderer->drawOrb({o.pos.x,o.pos.y}, o.radius, VP, m_boss.type);
        for (const auto& bc : m_boss.breathCols)
            if (bc.alive)
                m_renderer->drawBreathColumn(bc.x, bc.width, bc.age/bc.life,
                                             m_boss.arenaFloorY,
                                             m_boss.arenaCeilY, VP);
        for (const auto& tb : m_boss.tailBlocks)
            if (tb.alive)
                m_renderer->drawTailBlock(tb.x, tb.side, tb.age/tb.life,
                                          m_boss.arenaFloorY,
                                          m_boss.arenaCeilY, VP);
    }

    if (m_portalOpen)
        m_renderer->drawPortal(m_portalPos, m_portalAnim, VP);

    for (int i=0;i<(int)m_fragmentPositions.size();++i)
        m_renderer->drawFragment(m_fragmentPositions[i],
                                 m_fragmentFound[i], m_portalAnim, VP);

    m_vfx.draw(VP, (float)m_vpW, (float)m_vpH);

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
        // Fragment popups
        for (const auto& fp : m_fragmentPopups)
        {
            float a = ((fp.timer)<(1.f)?(fp.timer):(1.f));
            m_hud.drawFragmentPopup(fp.text.c_str(),
                                     fp.screenPos.x, fp.screenPos.y,
                                     a, m_vpW, m_vpH);
        }
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
        m_hud.draw(m_gs,0.f,1.f,false,m_fadeAlpha);
        m_hud.drawLevelComplete(pw,ph);
        break;
    case GameScreen::BossDefeated:
    {
        m_hud.draw(m_gs,0.f,1.f,false,m_fadeAlpha);
        const char* bossNames[] = {"STONE GOLEM","SHADOW WRAITH","BONE DRAGON"};
        int bi = (int)m_gs.currentBiome();
        m_hud.drawBossDefeated(pw, ph, bossNames[((bi)<(0)?(0):((bi)>(2)?(2):(bi)))]);
        break;
    }
    default: break;
    }
}

void Game::renderDevOverlay() {}  // handled by ImGui in main.cpp

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
            m_gs.reset();
            m_gs.highestReached = m_save.data().highestLevel;
            m_pendingLevel=true; m_nextLevel=0; m_fadeDir=1.f; break;
        case MenuAction::GoToLevel:
            m_gs.reset();
            m_gs.highestReached = m_save.data().highestLevel;
            m_pendingLevel=true;
            m_nextLevel=m_menu.m_selectedLevel; m_fadeDir=1.f; break;
        case MenuAction::Quit:
            glfwSetWindowShouldClose(m_win,GLFW_TRUE); break;
        default: break;
        }
        return;
    }

    if (action!=GLFW_PRESS) return;

    if (key==GLFW_KEY_F1){ m_devOverlay=!m_devOverlay; return; }

    if (m_devOverlay)
    {
        if (key==GLFW_KEY_N)
        {
            m_portalOpen=true; m_boss.stop();
            m_player.position=m_portalPos; return;
        }
        if (key>=GLFW_KEY_1 && key<=GLFW_KEY_9)
        {
            int lvl=((key-GLFW_KEY_1)<(0)?(0):((key-GLFW_KEY_1)>(8)?(8):(key-GLFW_KEY_1)));
            m_gs.reset(); m_pendingLevel=true;
            m_nextLevel=lvl; m_fadeDir=1.f;
            m_devOverlay=false; return;
        }
        if (key==GLFW_KEY_B)
        {
            int bosses[]={2,5,8};
            for (int b:bosses)
                if (b>=m_gs.currentLevel)
                {
                    m_gs.reset(); m_pendingLevel=true;
                    m_nextLevel=b; m_fadeDir=1.f;
                    m_devOverlay=false; return;
                }
        }
    }

    if (key==GLFW_KEY_R) respawnPlayer();

    if (key==GLFW_KEY_ENTER||key==GLFW_KEY_SPACE)
    {
        if (m_gs.screen==GameScreen::GameOver||m_gs.screen==GameScreen::Victory)
        {
            m_gs.reset();
            m_gs.highestReached=m_save.data().highestLevel;
            m_gs.screen=GameScreen::MainMenu;
            m_menu.screen=MainMenu::Screen::Root;
            m_fadeDir=-1.f;
        }
    }
}
