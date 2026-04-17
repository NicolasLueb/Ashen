#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "Simulation.h"
#include "Renderer.h"
#include "Game.h"
#include "GameRenderer.h"

#include <cstdio>

// ============================================================
//  App mode
// ============================================================
enum class AppMode { Game, Sandbox };
static AppMode g_mode = AppMode::Game;

// ============================================================
//  Globals
// ============================================================
static Renderer*     g_sbRend   = nullptr;
static Simulation*   g_sim      = nullptr;
static Game*         g_game     = nullptr;
static GameRenderer* g_gameRend = nullptr;

static bool g_sbDragging = false;
static int  g_vpW = 1200, g_vpH = 850;

// ============================================================
//  Callbacks
// ============================================================

static void framebufferSizeCallback(GLFWwindow*, int w, int h)
{
    g_vpW = w; g_vpH = h;
    if (g_sbRend)   g_sbRend->resize(w, h);
    if (g_gameRend) g_gameRend->resize(w, h);
    if (g_game)     g_game->resize(w, h);
}

static void mouseButtonCallback(GLFWwindow* win, int btn, int act, int)
{
    if (ImGui::GetIO().WantCaptureMouse) return;
    double mx, my;
    glfwGetCursorPos(win, &mx, &my);

    if (g_mode == AppMode::Sandbox)
    {
        if (btn == GLFW_MOUSE_BUTTON_LEFT)
        {
            if (act == GLFW_PRESS)
            {
                g_sim->beginDrag(g_sbRend->screenToWorld(mx, my));
                g_sbDragging = true;
            }
            else { g_sim->endDrag(); g_sbDragging = false; }
        }
    }
    else
    {
        g_game->onMouseButton(btn, act, mx, my);
    }
}

static void cursorPosCallback(GLFWwindow*, double mx, double my)
{
    if (g_mode == AppMode::Sandbox && g_sbDragging)
        g_sim->updateDrag(g_sbRend->screenToWorld(mx, my));
}

static void keyCallback(GLFWwindow*, int key, int, int action, int)
{
    // ESC toggles mode
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        g_mode = (g_mode == AppMode::Game) ? AppMode::Sandbox : AppMode::Game;

    if (g_mode == AppMode::Game && g_game)
        g_game->onKey(key, action);
}

// ============================================================
//  main
// ============================================================
int main()
{
    if (!glfwInit()) { fprintf(stderr, "GLFW failed\n"); return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* win = glfwCreateWindow(g_vpW, g_vpH,
        "Ashen: Into the Void", nullptr, nullptr);
    if (!win) { fprintf(stderr, "Window failed\n"); glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    { fprintf(stderr, "glad failed\n"); return -1; }
    printf("OpenGL %s\n", glGetString(GL_VERSION));

    // ---- Subsystems ----
    Renderer     sbRend;
    Simulation   sim;
    GameRenderer gameRend;
    Game         game;

    g_sbRend   = &sbRend;
    g_sim      = &sim;
    g_gameRend = &gameRend;
    g_game     = &game;

    int fbW, fbH;
    glfwGetFramebufferSize(win, &fbW, &fbH);
    g_vpW = fbW; g_vpH = fbH;

    sbRend.init(fbW, fbH);
    gameRend.init(fbW, fbH);
    game.init(win, &sim, &gameRend, fbW, fbH);

    // ---- Callbacks ----
    glfwSetFramebufferSizeCallback(win, framebufferSizeCallback);
    glfwSetMouseButtonCallback(win,    mouseButtonCallback);
    glfwSetCursorPosCallback(win,      cursorPosCallback);
    glfwSetKeyCallback(win,            keyCallback);

    // ---- ImGui ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = st.FrameRounding = st.GrabRounding = 5.f;
    st.WindowBorderSize = 0.f;
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    double prev   = glfwGetTime();
    bool   paused = false;
    float  speed  = 1.f;

    // ============================================================
    //  Main loop
    // ============================================================
    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();
        double now = glfwGetTime();
        float  dt  = (float)(now - prev) * speed;
        prev = now;
        dt = glm::min(dt, 0.05f);

        if (!paused)
        {
            if (g_mode == AppMode::Game)
                game.update(dt);
            else
                sim.step(dt);
        }

        // ---- Render ----
        if (g_mode == AppMode::Game)
            game.render();
        else
            sbRend.draw(sim);

        // ---- ImGui ----
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({10, 10}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({300, 0}, ImGuiCond_Always);
        ImGui::Begin("Ashen Engine", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // Mode toggle
        ImGui::Text("Mode");  ImGui::Separator();
        bool inGame = (g_mode == AppMode::Game);
        if (ImGui::RadioButton("Game",    inGame))  g_mode = AppMode::Game;
        ImGui::SameLine();
        if (ImGui::RadioButton("Sandbox", !inGame)) g_mode = AppMode::Sandbox;
        ImGui::TextDisabled("ESC toggles mode");
        ImGui::Spacing();

        if (g_mode == AppMode::Game)
        {
            // ---- GAME MODE — locked physics, gameplay info only ----
            auto& gs  = game.state();
            auto  cfg = gs.currentConfig();
            auto  pre = gs.currentPreset();

            ImGui::Text("Game");  ImGui::Separator();
            ImGui::Text("Level:  %d / 9", gs.currentLevel + 1);
            ImGui::Text("Biome:  %s",
                gs.currentBiome()==Biome::Catacombs ? "Catacombs" :
                gs.currentBiome()==Biome::Cathedral ? "Cathedral" : "Abyss");
            ImGui::Text("Name:   %s", cfg.name.c_str());
            ImGui::Text("Deaths: %d this level (%d total)", gs.runThisLevel, gs.runCount);
            ImGui::Text("Score:  %d", gs.score);
            if (cfg.isBossLevel)
            {
                ImGui::Text("Boss:   %.1f / %.1f s",
                            gs.bossTimer, cfg.bossTimer);
                ImGui::Text("Rage:   %s", gs.bossRage ? "YES" : "no");
            }

            ImGui::Spacing();
            ImGui::Text("Controls"); ImGui::Separator();
            ImGui::TextDisabled("WASD / Arrows — move");
            ImGui::TextDisabled("Space — jump");
            ImGui::TextDisabled("Wall jump near walls");
            ImGui::TextDisabled("Right click — grapple");
            ImGui::TextDisabled("R — respawn");
            ImGui::TextDisabled("F1 — dev overlay");

            ImGui::Spacing();
            ImGui::Text("Physics (locked)"); ImGui::Separator();
            ImGui::TextDisabled("Gravity: %.2f", pre.gravity);
            ImGui::TextDisabled("Jump:    %.2f", pre.playerJumpVel);
            ImGui::TextDisabled("Grapple: %.1f units", pre.grappleRange);
            ImGui::TextDisabled("(values locked per biome)");

            ImGui::Spacing();
            if (ImGui::Button("Restart game"))
            {
                gs.reset();
                gs.screen = GameScreen::MainMenu;
            }
        }

        // ---- DEV OVERLAY (F1) — separate floating window ----
        if (g_mode == AppMode::Game && game.state().screen != GameScreen::MainMenu)
        {
            // Show dev panel whenever F1 was pressed in-game
            // We expose it as a separate ImGui window so it floats freely
            static bool devOpen = false;
            if (ImGui::IsKeyPressed(ImGuiKey_F1)) devOpen = !devOpen;

            if (devOpen)
            {
                ImGui::SetNextWindowPos({(float)g_vpW - 290.f, 10.f},
                                        ImGuiCond_Always);
                ImGui::SetNextWindowSize({280.f, 0.f}, ImGuiCond_Always);
                ImGui::PushStyleColor(ImGuiCol_WindowBg,
                                      ImVec4(0.08f,0.05f,0.12f,0.92f));
                ImGui::PushStyleColor(ImGuiCol_TitleBg,
                                      ImVec4(0.25f,0.08f,0.35f,1.f));
                ImGui::PushStyleColor(ImGuiCol_TitleBgActive,
                                      ImVec4(0.35f,0.10f,0.50f,1.f));
                ImGui::Begin("DEV — F1 to close", &devOpen,
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove);

                auto& gs = game.state();
                ImGui::TextColored({1.f,0.8f,0.2f,1.f},
                    "Level %d / 9  —  %s",
                    gs.currentLevel+1,
                    gs.currentConfig().name.c_str());
                ImGui::Separator();

                ImGui::Text("Teleport to level:");
                ImGui::Spacing();

                auto levels = LevelConfig::allLevels();
                for (int i=0;i<9;++i)
                {
                    bool isBoss = levels[i].isBossLevel;
                    bool isCur  = (i == gs.currentLevel);

                    if (isCur)
                        ImGui::PushStyleColor(ImGuiCol_Button,
                                              ImVec4(0.3f,0.1f,0.5f,1.f));
                    else if (isBoss)
                        ImGui::PushStyleColor(ImGuiCol_Button,
                                              ImVec4(0.4f,0.05f,0.05f,0.9f));
                    else
                        ImGui::PushStyleColor(ImGuiCol_Button,
                                              ImVec4(0.12f,0.10f,0.18f,0.9f));

                    char lbl[64];
                    snprintf(lbl,sizeof(lbl),"%d: %s%s",
                             i+1, levels[i].name.c_str(),
                             isBoss?" [BOSS]":"");

                    if (ImGui::Button(lbl, {250.f, 0.f}))
                    {
                        gs.reset();
                        game.state().screen = GameScreen::Playing; // trigger load
                        // Use key shortcut path
                        game.onKey(GLFW_KEY_1 + i, GLFW_PRESS);
                        devOpen = false;
                    }
                    ImGui::PopStyleColor();
                }

                ImGui::Spacing(); ImGui::Separator();
                ImGui::Text("Quick actions:");

                if (ImGui::Button("Skip to end  (N)", {250.f,0.f}))
                {
                    game.onKey(GLFW_KEY_N, GLFW_PRESS);
                }
                if (ImGui::Button("Next boss    (B)", {250.f,0.f}))
                {
                    game.onKey(GLFW_KEY_B, GLFW_PRESS);
                    devOpen = false;
                }
                if (ImGui::Button("Respawn      (R)", {250.f,0.f}))
                {
                    game.onKey(GLFW_KEY_R, GLFW_PRESS);
                }

                ImGui::Spacing();
                ImGui::TextDisabled("Deaths this level: %d", gs.runThisLevel);
                ImGui::TextDisabled("Total deaths:      %d", gs.runCount);

                ImGui::End();
                ImGui::PopStyleColor(3);
            }
        }
        else
        {
            // ---- SANDBOX MODE — full physics control ----
            ImGui::Text("Sandbox"); ImGui::Separator();
            SimConfig& cfg = sim.config(); bool changed = false;
            changed |= ImGui::RadioButton("Particle", &cfg.mode, 0); ImGui::SameLine();
            changed |= ImGui::RadioButton("Stick",    &cfg.mode, 1);
            changed |= ImGui::RadioButton("Rope",     &cfg.mode, 2); ImGui::SameLine();
            changed |= ImGui::RadioButton("Cloth",    &cfg.mode, 3);
            changed |= ImGui::RadioButton("Rigid",    &cfg.mode, 4); ImGui::SameLine();
            changed |= ImGui::RadioButton("Soft",     &cfg.mode, 5);
            if (changed) sim.reset();

            if (cfg.mode == 4)
            {
                ImGui::Spacing(); ImGui::Text("Rigid bodies"); ImGui::Separator();
                if (ImGui::SliderInt("Count",     &cfg.rigidCount,   1, 20))   sim.reset();
                if (ImGui::SliderFloat("Size",    &cfg.rigidSize,    0.3f,2.f,"%.2f")) sim.reset();
                if (ImGui::SliderFloat("Stiff",   &cfg.rigidStiffness,0.1f,1.f,"%.2f")) sim.reset();
            }
            if (cfg.mode == 3)
            {
                ImGui::Spacing(); ImGui::Text("Cloth"); ImGui::Separator();
                if (ImGui::SliderInt("Rows",  &cfg.clothRows, 4, 24)) sim.reset();
                if (ImGui::SliderInt("Cols",  &cfg.clothCols, 4, 28)) sim.reset();
                if (ImGui::Checkbox("Shear",  &cfg.clothShear))       sim.reset();
                if (ImGui::Checkbox("Bend",   &cfg.clothBend))        sim.reset();
                if (ImGui::Checkbox("Tear",   &cfg.tearEnabled))      sim.reset();
                if (cfg.tearEnabled)
                    if (ImGui::SliderFloat("Threshold",&cfg.breakThreshold,1.01f,3.f,"%.2f"))
                        sim.reset();
            }
            if (cfg.mode == 2)
            {
                ImGui::Spacing(); ImGui::Text("Rope"); ImGui::Separator();
                if (ImGui::SliderInt("Segs",    &cfg.ropeSegments, 3, 30)) sim.reset();
                if (ImGui::SliderFloat("Len",   &cfg.ropeSegLen,0.2f,1.5f,"%.2f")) sim.reset();
            }

            ImGui::Spacing(); ImGui::Text("Physics"); ImGui::Separator();
            ImGui::SliderFloat("Gravity",   &cfg.gravity.y,     -20.f, 0.f,  "%.2f");
            ImGui::SliderFloat("Damping",   &cfg.damping,         0.9f, 1.f,  "%.4f");
            ImGui::SliderFloat("Floor Y",   &cfg.floorY,         -8.f,  0.f,  "%.2f");
            ImGui::SliderFloat("Bounce",    &cfg.restitution,     0.f,   1.f,  "%.2f");
            ImGui::SliderInt("Iterations",  &cfg.solverIterations,1,    40);
            ImGui::Checkbox("Particle coll",&cfg.particleCollision);
        }

        // Shared controls
        ImGui::Spacing(); ImGui::Separator();
        ImGui::Checkbox("Paused", &paused);
        ImGui::SliderFloat("Speed", &speed, 0.05f, 3.f, "%.2fx");
        ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);

        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
