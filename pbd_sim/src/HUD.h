#pragma once
#include "GameState.h"
#include "Camera.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <cstdio>
#include <stdexcept>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  HUD
//
//  Draws the in-game heads-up display in screen space (not
//  world space) — so it doesn't move with the camera.
//
//  Elements:
//    - Lives indicator (top left) — skull icons
//    - Level name + biome (top center)
//    - Boss survival timer (center, only in boss fights)
//    - Score (top right)
//    - Rage warning flash
//    - Screen fade overlay (for transitions)
//
//  Uses a separate orthographic projection that maps directly
//  to pixel coordinates (0,0 = top-left, vpW,vpH = bottom-right)
//  so positioning is intuitive.
//
//  Note: We don't use a font library — all text is rendered
//  as simple geometric shapes (rectangles for letter segments).
//  This is a placeholder until a proper font system is added.
//  Numbers use a 7-segment display style.
// ============================================================
class HUD
{
public:
    HUD() = default;
    ~HUD()
    {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_vbo);
        glDeleteBuffers(1, &m_ebo);
        glDeleteProgram(m_shader);
    }

    void init(int w, int h)
    {
        m_vpW = w; m_vpH = h;
        buildShader();
        buildBuffers();
    }

    void resize(int w, int h) { m_vpW = w; m_vpH = h; }

    // ---- Main draw ----
    void draw(const GameState& gs, float bossTimer, float surviveDur,
              bool bossRage, float fadeAlpha = 0.f)
    {
        // Screen-space ortho: (0,0) top-left, (vpW, vpH) bottom-right
        glm::mat4 proj = glm::ortho(
            0.f, (float)m_vpW,
            (float)m_vpH, 0.f,
            -1.f, 1.f);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(m_shader);
        glUniformMatrix4fv(
            glGetUniformLocation(m_shader, "uProj"), 1, GL_FALSE,
            glm::value_ptr(proj));

        float pw = (float)m_vpW, ph = (float)m_vpH;

        // ---- Run counter top left (deaths this level) ----
        drawLives(gs.runThisLevel, 24.f, 24.f);

        // ---- Score (top right) ----
        drawScore(gs.score, pw - 24.f, 24.f);

        // ---- Level name (top center) ----
        auto cfg = gs.currentConfig();
        drawLabel(cfg.name, pw * 0.5f, 20.f);

        // ---- Boss timer (center screen, boss fights only) ----
        if (gs.screen == GameScreen::BossFight && surviveDur > 0.f)
        {
            float remaining = surviveDur - bossTimer;
            remaining = glm::max(remaining, 0.f);
            drawBossTimer(remaining, surviveDur, pw * 0.5f, ph * 0.15f, bossRage);
        }

        // ---- Rage warning ----
        if (bossRage)
        {
            float pulse = (sinf(m_time * 8.f) * 0.5f + 0.5f) * 0.12f;
            drawRect(0.f, 0.f, pw, ph, 0.8f, 0.1f, 0.05f, pulse);
        }

        // ---- Screen fade overlay ----
        if (fadeAlpha > 0.001f)
            drawRect(0.f, 0.f, pw, ph, 0.f, 0.f, 0.f, fadeAlpha);

        m_time += 0.016f; // approximate dt for animations
    }

    // ---- Full-screen overlays ----

    void drawMainMenu(float pw, float ph)
    {
        glm::mat4 proj = glm::ortho(0.f,pw,ph,0.f,-1.f,1.f);
        glUseProgram(m_shader);
        glUniformMatrix4fv(glGetUniformLocation(m_shader,"uProj"),1,GL_FALSE,glm::value_ptr(proj));

        // Dark overlay
        drawRect(0,0,pw,ph,0.f,0.f,0.f,0.75f);
        // Title bar
        drawRect(pw*0.2f, ph*0.3f, pw*0.6f, 60.f, 0.3f,0.1f,0.4f,0.9f);
        // Start hint bar
        drawRect(pw*0.3f, ph*0.55f, pw*0.4f, 40.f, 0.2f,0.2f,0.3f,0.8f);
    }

    void drawGameOver(float pw, float ph, int score)
    {
        glm::mat4 proj = glm::ortho(0.f,pw,ph,0.f,-1.f,1.f);
        glUseProgram(m_shader);
        glUniformMatrix4fv(glGetUniformLocation(m_shader,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        drawRect(0,0,pw,ph,0.f,0.f,0.f,0.82f);
        drawRect(pw*0.2f,ph*0.35f,pw*0.6f,55.f,0.5f,0.05f,0.05f,0.9f);
        drawRect(pw*0.3f,ph*0.52f,pw*0.4f,35.f,0.2f,0.15f,0.2f,0.8f);
    }

    void drawVictory(float pw, float ph, int score)
    {
        glm::mat4 proj = glm::ortho(0.f,pw,ph,0.f,-1.f,1.f);
        glUseProgram(m_shader);
        glUniformMatrix4fv(glGetUniformLocation(m_shader,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        drawRect(0,0,pw,ph,0.f,0.f,0.f,0.78f);
        drawRect(pw*0.15f,ph*0.3f,pw*0.7f,60.f,0.1f,0.3f,0.4f,0.9f);
        drawRect(pw*0.3f,ph*0.5f,pw*0.4f,35.f,0.15f,0.2f,0.3f,0.8f);
    }

    void drawLevelComplete(float pw, float ph)
    {
        glm::mat4 proj = glm::ortho(0.f,pw,ph,0.f,-1.f,1.f);
        glUseProgram(m_shader);
        glUniformMatrix4fv(glGetUniformLocation(m_shader,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        drawRect(0,0,pw,ph,0.f,0.f,0.f,0.6f);
        drawRect(pw*0.2f,ph*0.35f,pw*0.6f,55.f,0.1f,0.35f,0.2f,0.9f);
    }

private:
    void drawLives(int runCount, float x, float y)
    {
        // Show deaths this level as small red bars stacking up
        // Cap visual display at 10 to avoid overflow
        int show = glm::min(runCount, 10);
        for (int i = 0; i < show; ++i)
        {
            float cx = x + i * 12.f;
            drawRect(cx, y, 8.f, 16.f, 0.7f, 0.12f, 0.12f, 0.85f);
        }
        // If more than 10 show a + indicator
        if (runCount > 10)
            drawRect(x + 10*12.f, y+4.f, 8.f, 8.f, 0.9f,0.3f,0.3f,0.9f);
    }

    void drawScore(int score, float x, float y)
    {
        // Simple score bar
        float sw = glm::min((float)score / 3000.f, 1.f) * 120.f;
        drawRect(x - 130.f, y, 120.f, 18.f, 0.1f,0.1f,0.15f, 0.7f);
        drawRect(x - 130.f, y, sw,    18.f, 0.4f,0.7f,0.9f,  0.9f);
    }

    void drawBossTimer(float remaining, float total,
                       float cx, float cy, bool rage)
    {
        // Outer bar background
        float barW = 300.f, barH = 22.f;
        drawRect(cx - barW*0.5f, cy, barW, barH,
                 0.08f,0.06f,0.12f, 0.85f);

        // Fill based on time remaining
        float fill = remaining / total;
        float r = rage ? 0.9f : 0.3f;
        float g = rage ? 0.15f: 0.7f;
        float b = rage ? 0.1f : 0.85f;
        drawRect(cx - barW*0.5f, cy, barW*fill, barH, r,g,b, 0.9f);

        // Border
        drawRectOutline(cx - barW*0.5f, cy, barW, barH,
                        0.6f,0.5f,0.8f, 0.8f);

        // Tick marks every 15 seconds
        for (float t = 15.f; t < total; t += 15.f)
        {
            float tx = cx - barW*0.5f + (t/total)*barW;
            drawRect(tx-1.f, cy, 2.f, barH, 0.5f,0.5f,0.7f, 0.5f);
        }
    }

    void drawLabel(const std::string& text, float cx, float y)
    {
        // Just draw a decorative bar — text rendering TBD
        float w = glm::min((float)text.size() * 8.f + 20.f, 280.f);
        drawRect(cx - w*0.5f, y, w, 20.f, 0.15f,0.12f,0.22f, 0.75f);
        drawRectOutline(cx - w*0.5f, y, w, 20.f, 0.45f,0.38f,0.6f, 0.7f);
    }

    // ---- Primitives ----

    void drawRect(float x, float y, float w, float h,
                  float r, float g, float b, float a)
    {
        std::vector<float> v = {
            x,   y,   x+w, y,
            x+w, y+h, x,   y+h
        };
        std::vector<unsigned int> idx = {0,1,2, 0,2,3};

        glUseProgram(m_shader);
        glUniform4f(glGetUniformLocation(m_shader,"uColor"),r,g,b,a);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

        std::vector<float> verts = {
            x,   y,   0.f,
            x+w, y,   0.f,
            x+w, y+h, 0.f,
            x,   y+h, 0.f,
        };
        glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float),
                     verts.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        std::vector<unsigned int> eidx = {0,1,2,0,2,3};
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     eidx.size()*sizeof(unsigned int),
                     eidx.data(), GL_DYNAMIC_DRAW);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void drawRectOutline(float x, float y, float w, float h,
                         float r, float g, float b, float a)
    {
        float t = 1.5f; // border thickness
        drawRect(x,     y,     w, t, r,g,b,a);
        drawRect(x,     y+h-t, w, t, r,g,b,a);
        drawRect(x,     y,     t, h, r,g,b,a);
        drawRect(x+w-t, y,     t, h, r,g,b,a);
    }

    void buildShader()
    {
        const char* vs = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uProj;
void main(){ gl_Position = uProj * vec4(aPos, 1.0); }
)glsl";
        const char* fs = R"glsl(
#version 330 core
out vec4 O;
uniform vec4 uColor;
void main(){ O = uColor; }
)glsl";
        auto cs = [](GLenum t, const char* s) -> GLuint {
            GLuint id = glCreateShader(t);
            glShaderSource(id,1,&s,nullptr); glCompileShader(id);
            GLint ok=0; glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
            if(!ok){char l[512];glGetShaderInfoLog(id,512,nullptr,l);
                fprintf(stderr,"HUD shader:\n%s\n",l);}
            return id;
        };
        GLuint v = cs(GL_VERTEX_SHADER, vs);
        GLuint f = cs(GL_FRAGMENT_SHADER, fs);
        m_shader = glCreateProgram();
        glAttachShader(m_shader,v); glAttachShader(m_shader,f);
        glLinkProgram(m_shader);
        glDeleteShader(v); glDeleteShader(f);
    }

    void buildBuffers()
    {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              3*sizeof(float), (void*)0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBindVertexArray(0);
    }

    GLuint m_shader = 0;
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    int    m_vpW = 1200, m_vpH = 850;
    float  m_time = 0.f;
};
