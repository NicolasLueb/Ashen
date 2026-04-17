#pragma once
#include "GameState.h"
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  MenuAction — what the menu wants the game to do this frame
// ============================================================
enum class MenuAction
{
    None,
    StartGame,          // start from level 0
    GoToLevel,          // jump to m_selectedLevel
    GoToBoss,           // teleport to boss of current biome
    Quit
};

// ============================================================
//  MainMenu
//
//  A proper in-engine menu — no ImGui.  Drawn in screen space
//  using the same primitive renderer as the HUD.
//
//  Screens:
//    Root      — Play / Level Select / Quit
//    LevelSel  — grid of 9 levels, locked levels greyed out
//    Debug     — dev shortcuts: jump to any level / boss
//
//  Navigation: arrow keys or WASD to move cursor, Enter/Space
//  to confirm, Backspace/Escape to go back.
//
//  Level lock rules:
//    - Level 0 always unlocked
//    - Level N unlocked if highestReached >= N
//    - Boss levels unlocked same as normal
//    - In debug mode ALL levels are unlocked
// ============================================================
class MainMenu
{
public:
    // Which sub-screen
    enum class Screen { Root, LevelSelect, Debug };

    Screen      screen          = Screen::Root;
    int         rootCursor      = 0;   // 0=Play 1=LevelSel 2=Debug 3=Quit
    int         levelCursor     = 0;   // 0-8
    int         debugCursor     = 0;   // 0-8 for boss teleport
    int         highestReached  = 0;   // set by game on level complete
    int         m_selectedLevel = 0;   // set when GoToLevel fires

    void init(int w, int h) { m_vpW=w; m_vpH=h; buildShader(); buildBuffers(); }
    void resize(int w, int h) { m_vpW=w; m_vpH=h; }

    // Returns the action to take this frame (if any)
    MenuAction handleKey(int key, int action)
    {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return MenuAction::None;

        if (screen == Screen::Root) return handleRootKey(key);
        if (screen == Screen::LevelSelect) return handleLevelKey(key);
        if (screen == Screen::Debug) return handleDebugKey(key);
        return MenuAction::None;
    }

    void draw(float animTime)
    {
        m_anim = animTime;
        float pw = (float)m_vpW, ph = (float)m_vpH;
        glm::mat4 proj = glm::ortho(0.f,pw,ph,0.f,-1.f,1.f);
        glUseProgram(m_shader);
        glUniformMatrix4fv(glGetUniformLocation(m_shader,"uProj"),
                           1,GL_FALSE,glm::value_ptr(proj));
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

        // Dark gothic background fill
        drawBar(0,0,pw,ph, 0.04f,0.03f,0.06f,1.f);

        // Animated void swirl lines
        drawVoidDecor(pw,ph);

        if (screen == Screen::Root)       drawRoot(pw,ph);
        if (screen == Screen::LevelSelect) drawLevelSelect(pw,ph);
        if (screen == Screen::Debug)       drawDebug(pw,ph);
    }

private:
    // ---- Key handlers ----

    MenuAction handleRootKey(int key)
    {
        const int ITEMS = 4;
        if (key==GLFW_KEY_UP||key==GLFW_KEY_W)
            rootCursor = (rootCursor-1+ITEMS)%ITEMS;
        else if (key==GLFW_KEY_DOWN||key==GLFW_KEY_S)
            rootCursor = (rootCursor+1)%ITEMS;
        else if (key==GLFW_KEY_ENTER||key==GLFW_KEY_SPACE)
        {
            if (rootCursor==0) return MenuAction::StartGame;
            if (rootCursor==1){ screen=Screen::LevelSelect; levelCursor=0; }
            if (rootCursor==2){ screen=Screen::Debug; debugCursor=0; }
            if (rootCursor==3) return MenuAction::Quit;
        }
        return MenuAction::None;
    }

    MenuAction handleLevelKey(int key)
    {
        if (key==GLFW_KEY_ESCAPE||key==GLFW_KEY_BACKSPACE)
        { screen=Screen::Root; return MenuAction::None; }

        if (key==GLFW_KEY_LEFT||key==GLFW_KEY_A)
            levelCursor = glm::max(levelCursor-1,0);
        else if (key==GLFW_KEY_RIGHT||key==GLFW_KEY_D)
            levelCursor = glm::min(levelCursor+1,8);
        else if (key==GLFW_KEY_UP||key==GLFW_KEY_W)
            levelCursor = glm::max(levelCursor-3,0);
        else if (key==GLFW_KEY_DOWN||key==GLFW_KEY_S)
            levelCursor = glm::min(levelCursor+3,8);
        else if (key==GLFW_KEY_ENTER||key==GLFW_KEY_SPACE)
        {
            // Allow any level that has been reached
            if (levelCursor <= highestReached)
            {
                m_selectedLevel = levelCursor;
                return MenuAction::GoToLevel;
            }
        }
        return MenuAction::None;
    }

    MenuAction handleDebugKey(int key)
    {
        if (key==GLFW_KEY_ESCAPE||key==GLFW_KEY_BACKSPACE)
        { screen=Screen::Root; return MenuAction::None; }

        // 0-8 number keys = jump straight to that level
        if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
        {
            int lvl = key - GLFW_KEY_0 - 1; // 1=level0 ... 9=level8
            if (lvl < 0) lvl = 8;
            m_selectedLevel = glm::clamp(lvl,0,8);
            return MenuAction::GoToLevel;
        }

        if (key==GLFW_KEY_UP||key==GLFW_KEY_W)
            debugCursor = (debugCursor-1+9)%9;
        else if (key==GLFW_KEY_DOWN||key==GLFW_KEY_S)
            debugCursor = (debugCursor+1)%9;
        else if (key==GLFW_KEY_ENTER||key==GLFW_KEY_SPACE)
        {
            m_selectedLevel = debugCursor;
            return MenuAction::GoToLevel;
        }

        // B = jump to boss of current biome
        if (key==GLFW_KEY_B)
        {
            // Boss levels are 2, 5, 8
            int bosses[] = {2,5,8};
            for (int b : bosses)
                if (b >= debugCursor) { m_selectedLevel=b; break; }
            return MenuAction::GoToLevel;
        }
        return MenuAction::None;
    }

    // ---- Draw screens ----

    void drawRoot(float pw, float ph)
    {
        // Title
        float ty = ph*0.28f;
        drawBar(pw*0.5f - 160.f, ty, 320.f, 48.f,
                0.18f,0.08f,0.28f,0.95f);
        drawBar(pw*0.5f - 158.f, ty+2.f, 316.f, 2.f,
                0.55f,0.35f,0.75f,0.8f);  // top accent
        // Subtitle flash
        float pulse = sinf(m_anim*2.f)*0.15f+0.85f;
        drawBar(pw*0.5f-100.f, ty+54.f, 200.f, 18.f,
                0.25f*pulse,0.15f*pulse,0.38f*pulse,0.7f);

        // Menu items
        const char* items[] = {"Play","Level Select","Debug / Dev","Quit"};
        for (int i=0;i<4;++i)
        {
            float iy = ph*0.46f + i*52.f;
            bool  sel = (i == rootCursor);
            float br = sel ? 0.30f : 0.12f;
            float bg = sel ? 0.12f : 0.08f;
            float bb = sel ? 0.45f : 0.18f;
            drawBar(pw*0.5f-120.f, iy, 240.f, 38.f, br,bg,bb, sel?0.95f:0.7f);
            if (sel)
            {
                // Selection arrow
                drawBar(pw*0.5f-138.f, iy+14.f, 10.f, 10.f,
                        0.7f,0.5f,1.f,0.9f);
                // Glow border
                drawBarOutline(pw*0.5f-120.f,iy,240.f,38.f,
                               0.55f,0.35f,0.8f,0.8f);
            }
        }
    }

    void drawLevelSelect(float pw, float ph)
    {
        // Header
        drawBar(pw*0.5f-140.f, ph*0.12f, 280.f, 36.f,
                0.12f,0.08f,0.20f,0.9f);
        drawBarOutline(pw*0.5f-140.f,ph*0.12f,280.f,36.f,
                       0.4f,0.3f,0.6f,0.7f);

        // 3×3 grid of level cards
        auto levels = LevelConfig::allLevels();
        float cardW=200.f, cardH=90.f, gapX=24.f, gapY=20.f;
        float gridW = cardW*3+gapX*2;
        float startX = pw*0.5f - gridW*0.5f;
        float startY = ph*0.25f;

        for (int i=0;i<9;++i)
        {
            int   row = i/3, col = i%3;
            float cx  = startX + col*(cardW+gapX);
            float cy  = startY + row*(cardH+gapY);

            bool locked   = (i > highestReached);
            bool selected = (i == levelCursor);
            bool isBoss   = levels[i].isBossLevel;

            // Card background — colour by biome
            float br,bg,bb;
            switch(levels[i].biome)
            {
            case Biome::Catacombs: br=0.20f;bg=0.12f;bb=0.14f; break;
            case Biome::Cathedral: br=0.12f;bg=0.16f;bb=0.24f; break;
            default:               br=0.08f;bg=0.08f;bb=0.20f; break;
            }
            float alpha = locked ? 0.35f : 0.90f;
            drawBar(cx,cy,cardW,cardH,br,bg,bb,alpha);

            if (selected && !locked)
                drawBarOutline(cx,cy,cardW,cardH,0.7f,0.5f,1.f,0.9f);
            else if (!locked)
                drawBarOutline(cx,cy,cardW,cardH,0.3f,0.25f,0.45f,0.6f);

            // Boss marker
            if (isBoss)
                drawBar(cx+cardW-22.f,cy+4.f,18.f,18.f,
                        0.7f,0.15f,0.15f,locked?0.2f:0.85f);

            // Level number indicator bar
            drawBar(cx+8.f,cy+cardH-16.f,(float)(i+1)/9.f*(cardW-16.f),8.f,
                    0.4f,0.3f,0.6f,locked?0.2f:0.7f);

            // Lock overlay
            if (locked)
                drawBar(cx,cy,cardW,cardH,0.f,0.f,0.f,0.5f);

            // Selection pulse
            if (selected && !locked)
            {
                float p=sinf(m_anim*4.f)*0.08f;
                drawBar(cx-2.f-p,cy-2.f-p,cardW+4.f+p*2,cardH+4.f+p*2,
                        0.5f,0.3f,0.8f,0.15f);
            }
        }

        // Back hint
        drawBar(pw*0.5f-80.f,ph*0.92f,160.f,24.f,
                0.1f,0.08f,0.15f,0.7f);
    }

    void drawDebug(float pw, float ph)
    {
        // Header
        drawBar(pw*0.5f-160.f,ph*0.1f,320.f,36.f,
                0.15f,0.12f,0.05f,0.9f);
        drawBarOutline(pw*0.5f-160.f,ph*0.1f,320.f,36.f,
                       0.7f,0.6f,0.1f,0.7f);

        // Warning stripe
        drawBar(0.f,ph*0.17f,pw,4.f,0.7f,0.55f,0.1f,0.5f);

        auto levels = LevelConfig::allLevels();

        // All 9 levels listed — all unlocked in debug
        float startY = ph*0.22f;
        float rowH   = 48.f;
        for (int i=0;i<9;++i)
        {
            float ry    = startY + i * rowH;
            bool  sel   = (i == debugCursor);
            bool  isBoss = levels[i].isBossLevel;

            float br = sel ? 0.28f : 0.10f;
            float bg = sel ? 0.22f : 0.08f;
            float bb = sel ? 0.08f : 0.06f;
            drawBar(pw*0.5f-220.f, ry, 440.f, rowH-4.f,
                    br,bg,bb, sel?0.95f:0.65f);

            if (isBoss)
                drawBar(pw*0.5f+170.f,ry+10.f,30.f,22.f,
                        0.7f,0.1f,0.1f,sel?0.9f:0.55f);

            if (sel)
                drawBarOutline(pw*0.5f-220.f,ry,440.f,rowH-4.f,
                               0.8f,0.6f,0.2f,0.85f);
        }

        // Key hints
        drawBar(pw*0.5f-200.f,ph*0.88f,400.f,56.f,
                0.08f,0.07f,0.04f,0.8f);
        drawBarOutline(pw*0.5f-200.f,ph*0.88f,400.f,56.f,
                       0.4f,0.35f,0.1f,0.6f);

        // Number key shortcuts legend
        for (int i=0;i<9;++i)
        {
            float kx = pw*0.5f - 180.f + i*40.f;
            drawBar(kx,ph*0.8f,32.f,24.f,
                    0.2f,0.18f,0.08f,0.8f);
            drawBarOutline(kx,ph*0.8f,32.f,24.f,
                           0.5f,0.45f,0.15f,0.6f);
        }
    }

    // ---- Void decoration ----
    void drawVoidDecor(float pw, float ph)
    {
        // Slow drifting horizontal lines — gothic atmosphere
        int lines = 12;
        for (int i=0;i<lines;++i)
        {
            float y = fmodf(i*(ph/lines) + m_anim*8.f, ph);
            float alpha = 0.03f + 0.02f*sinf(m_anim + i*1.3f);
            drawBar(0.f, y, pw, 1.f, 0.5f,0.3f,0.7f,alpha);
        }
        // Corner vignette approximation
        float vw=pw*0.25f, vh=ph*0.3f;
        drawBar(0.f, 0.f,       vw,vh, 0.f,0.f,0.f,0.4f);
        drawBar(pw-vw,0.f,      vw,vh, 0.f,0.f,0.f,0.4f);
        drawBar(0.f, ph-vh,     vw,vh, 0.f,0.f,0.f,0.4f);
        drawBar(pw-vw,ph-vh,    vw,vh, 0.f,0.f,0.f,0.4f);
    }

    // ---- Primitives ----
    void drawBar(float x,float y,float w,float h,
                 float r,float g,float b,float a)
    {
        std::vector<float> v={x,y,0.f, x+w,y,0.f, x+w,y+h,0.f, x,y+h,0.f};
        std::vector<unsigned int> idx={0,1,2,0,2,3};
        glUniform4f(glGetUniformLocation(m_shader,"uColor"),r,g,b,a);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER,m_vbo);
        glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     idx.size()*sizeof(unsigned int),idx.data(),GL_DYNAMIC_DRAW);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
        glBindVertexArray(0);
    }

    void drawBarOutline(float x,float y,float w,float h,
                        float r,float g,float b,float a)
    {
        float t=1.5f;
        drawBar(x,y,w,t,r,g,b,a);
        drawBar(x,y+h-t,w,t,r,g,b,a);
        drawBar(x,y,t,h,r,g,b,a);
        drawBar(x+w-t,y,t,h,r,g,b,a);
    }

    void buildShader()
    {
        const char* vs=R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uProj;
void main(){ gl_Position=uProj*vec4(aPos,1.0); }
)glsl";
        const char* fs=R"glsl(
#version 330 core
out vec4 O; uniform vec4 uColor;
void main(){ O=uColor; }
)glsl";
        auto cs=[](GLenum t,const char* s)->GLuint{
            GLuint id=glCreateShader(t);
            glShaderSource(id,1,&s,nullptr);glCompileShader(id);
            GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
            if(!ok){char l[512];glGetShaderInfoLog(id,512,nullptr,l);
                fprintf(stderr,"Menu shader:\n%s\n",l);}
            return id;
        };
        GLuint v=cs(GL_VERTEX_SHADER,vs),f=cs(GL_FRAGMENT_SHADER,fs);
        m_shader=glCreateProgram();
        glAttachShader(m_shader,v);glAttachShader(m_shader,f);glLinkProgram(m_shader);
        glDeleteShader(v);glDeleteShader(f);
    }

    void buildBuffers()
    {
        glGenVertexArrays(1,&m_vao);glGenBuffers(1,&m_vbo);glGenBuffers(1,&m_ebo);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER,m_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m_ebo);
        glBindVertexArray(0);
    }

    GLuint m_shader=0, m_vao=0, m_vbo=0, m_ebo=0;
    int    m_vpW=1200, m_vpH=850;
    float  m_anim=0.f;
};
