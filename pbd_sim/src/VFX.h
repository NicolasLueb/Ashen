#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  GlitchEvent
//  A brief RGB-split screen distortion at a world position.
//  Spawned when a constraint tears or a boss attack lands.
// ============================================================
struct GlitchEvent
{
    glm::vec2 worldPos;    // where it happened
    float     age        = 0.f;
    float     lifetime   = 0.25f;
    float     intensity  = 1.f;
    bool      alive      = true;

    void update(float dt)
    {
        age += dt;
        if (age >= lifetime) alive = false;
    }

    float progress() const { return age / lifetime; }
};

// ============================================================
//  DeathParticle — tiny fragment ejected on player death
// ============================================================
struct DeathParticle
{
    glm::vec2 pos;
    glm::vec2 vel;
    float     life     = 1.f;
    float     maxLife  = 1.f;
    float     size     = 4.f;
    float     r,g,b;

    void update(float dt)
    {
        pos  += vel * dt;
        vel.y -= 15.f * dt;   // gravity
        vel  *= 0.95f;         // drag
        life -= dt;
    }
    bool alive() const { return life > 0.f; }
    float alpha()    const { return life / maxLife; }
    float progress() const { return 1.f - (life / maxLife); }
};

// ============================================================
//  VFX
//
//  Manages all visual effects:
//    - Glitch/RGB-split overlay on constraint tears
//    - Death particle burst
//    - Screen grain overlay (hand-painted texture feel)
//    - Vignette (darkened edges)
//    - Physics glow auras on moving objects
//
//  All effects are drawn in screen space on top of the world.
// ============================================================
class VFX
{
public:
    ~VFX()
    {
        glDeleteVertexArrays(1,&m_vao); glDeleteBuffers(1,&m_vbo);
        glDeleteBuffers(1,&m_ebo); glDeleteProgram(m_shader);
        glDeleteProgram(m_glitchShader);
    }

    void init(int w, int h)
    {
        m_vpW=w; m_vpH=h;
        buildShaders(); buildBuffers();
    }
    void resize(int w, int h){ m_vpW=w; m_vpH=h; }

    // ---- Spawn effects ----

    void spawnGlitch(glm::vec2 worldPos, float intensity=1.f)
    {
        GlitchEvent g;
        g.worldPos = worldPos;
        g.intensity = intensity;
        g.alive = true;
        m_glitches.push_back(g);
        // Also add screen shake flag
        m_screenShakeLeft = glm::max(m_screenShakeLeft, 0.12f * intensity);
    }

    void spawnDeathBurst(glm::vec2 worldPos)
    {
        for (int i = 0; i < 24; ++i)
        {
            float angle = 2.f*(float)M_PI*i/24.f + frand()*0.5f;
            float speed = 4.f + frand()*8.f;
            DeathParticle p;
            p.pos  = worldPos;
            p.vel  = {cosf(angle)*speed, sinf(angle)*speed};
            p.life = p.maxLife = 0.5f + frand()*0.5f;
            p.size = 3.f + frand()*5.f;
            // Ice-blue to white palette for player death
            float t = frand();
            p.r = 0.6f + t*0.4f;
            p.g = 0.7f + t*0.3f;
            p.b = 1.0f;
            m_deathParticles.push_back(p);
        }
    }

    // ---- Update ----
    void update(float dt)
    {
        m_time += dt;
        m_screenShakeLeft = glm::max(m_screenShakeLeft - dt * 4.f, 0.f);

        for (auto& g : m_glitches) g.update(dt);
        m_glitches.erase(
            std::remove_if(m_glitches.begin(), m_glitches.end(),
                [](const GlitchEvent& g){ return !g.alive; }),
            m_glitches.end());

        for (auto& p : m_deathParticles) p.update(dt);
        m_deathParticles.erase(
            std::remove_if(m_deathParticles.begin(), m_deathParticles.end(),
                [](const DeathParticle& p){ return !p.alive(); }),
            m_deathParticles.end());
    }

    // ---- Draw (call after world, before HUD) ----
    void draw(const glm::mat4& worldVP, float pw, float ph)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Screen-space projection
        glm::mat4 screenProj = glm::ortho(0.f,pw,ph,0.f,-1.f,1.f);

        drawGrain(screenProj, pw, ph);
        drawVignette(screenProj, pw, ph);
        drawDeathParticles(worldVP);
        drawGlitchOverlay(screenProj, pw, ph);
    }

    float screenShake() const { return m_screenShakeLeft; }

private:
    // ---- Draw calls ----

    // Film grain overlay — makes flat geometry look painted/textured
    void drawGrain(const glm::mat4& proj, float pw, float ph)
    {
        // Animated noise using screen-space UV + time
        // We approximate with many tiny semi-transparent rects
        // (A proper implementation uses a noise shader — this is the
        //  geometry fallback for maximum compatibility)
        glUseProgram(m_shader);
        glUniformMatrix4fv(glGetUniformLocation(m_shader,"uProj"),
                           1,GL_FALSE,glm::value_ptr(proj));

        // Draw as a single full-screen quad with very low alpha
        // The "grain" effect comes from temporal variation (time-based noise)
        float grainAlpha = 0.04f + sinf(m_time*37.f)*0.01f;
        drawRect(0.f,0.f,pw,ph, 0.1f,0.08f,0.15f, grainAlpha);

        // Scanline effect — thin dark horizontal stripes
        float stripeH = 2.f;
        float stripeGap = 6.f;
        for (float y = 0.f; y < ph; y += stripeH + stripeGap)
            drawRect(0.f, y, pw, stripeH, 0.f,0.f,0.f, 0.03f);
    }

    // Vignette — very subtle edge darkening, no solid black boxes
    void drawVignette(const glm::mat4& proj, float pw, float ph)
    {
        glUseProgram(m_shader);
        glUniformMatrix4fv(glGetUniformLocation(m_shader,"uProj"),
                           1,GL_FALSE,glm::value_ptr(proj));

        // Use many thin overlapping rects at low alpha to fake a soft gradient
        // This avoids the harsh solid-box look
        int steps = 12;
        for (int i = 0; i < steps; ++i)
        {
            float t     = (float)i / steps;
            float alpha = t * t * 0.22f;  // quadratic falloff, max ~0.22
            float vw    = pw * 0.18f * (1.f - t);
            float vh    = ph * 0.20f * (1.f - t);

            // Left edge
            drawRect(0.f,           0.f, vw, ph, 0.f,0.f,0.f, alpha);
            // Right edge
            drawRect(pw - vw,       0.f, vw, ph, 0.f,0.f,0.f, alpha);
            // Top edge
            drawRect(0.f,           0.f, pw, vh, 0.f,0.f,0.f, alpha*0.6f);
            // Bottom edge
            drawRect(0.f,       ph-vh,   pw, vh, 0.f,0.f,0.f, alpha*0.6f);
        }
    }

    // Death particles drawn in world space
    void drawDeathParticles(const glm::mat4& VP)
    {
        if (m_deathParticles.empty()) return;
        glUseProgram(m_shader);
        glUniformMatrix4fv(glGetUniformLocation(m_shader,"uProj"),
                           1,GL_FALSE,glm::value_ptr(VP));

        for (const auto& p : m_deathParticles)
        {
            float s = p.size * 0.5f * (1.f - p.progress() * 0.5f);
            // Convert world pos to draw a small quad
            std::vector<float> v = {
                p.pos.x-s, p.pos.y-s, 0.f,
                p.pos.x+s, p.pos.y-s, 0.f,
                p.pos.x+s, p.pos.y+s, 0.f,
                p.pos.x-s, p.pos.y+s, 0.f,
            };
            std::vector<unsigned int> idx = {0,1,2,0,2,3};
            glUniform4f(glGetUniformLocation(m_shader,"uColor"),
                        p.r,p.g,p.b,p.alpha());
            uploadAndDraw(v,idx);
        }
    }

    // RGB-split glitch overlay
    void drawGlitchOverlay(const glm::mat4& proj, float pw, float ph)
    {
        if (m_glitches.empty()) return;
        glUseProgram(m_shader);
        glUniformMatrix4fv(glGetUniformLocation(m_shader,"uProj"),
                           1,GL_FALSE,glm::value_ptr(proj));

        for (const auto& g : m_glitches)
        {
            float t = g.progress();
            float intensity = g.intensity * (1.f - t);

            // Horizontal scanline tears near the glitch position
            // (approximated as red/blue offset rectangles)
            int numTears = (int)(8.f * intensity) + 1;
            for (int i = 0; i < numTears; ++i)
            {
                // Random-ish y positions using sin
                float y = (float)m_vpH * (0.3f + 0.4f * sinf(i * 2.7f + m_time*20.f));
                float h = 2.f + 4.f * sinf(i * 1.3f) * intensity;
                float offset = 8.f * intensity * sinf(i*5.f+m_time*15.f);

                // Red channel shifted right
                drawRect(offset,    y, pw, h, 1.f,0.f,0.f, 0.12f*intensity);
                // Blue channel shifted left
                drawRect(-offset,   y, pw, h, 0.f,0.2f,1.f, 0.12f*intensity);
                // White flash at tear
                drawRect(0.f, y, pw, h*0.3f, 1.f,1.f,1.f, 0.06f*intensity);
            }

            // Full-screen brief white flash at peak
            if (t < 0.1f)
                drawRect(0.f,0.f,pw,ph,1.f,1.f,1.f, 0.15f*(1.f-t*10.f)*g.intensity);
        }
    }

    // ---- Primitives ----
    void drawRect(float x, float y, float w, float h,
                  float r, float g, float b, float a)
    {
        std::vector<float> v={x,y,0.f, x+w,y,0.f, x+w,y+h,0.f, x,y+h,0.f};
        std::vector<unsigned int> idx={0,1,2,0,2,3};
        glUniform4f(glGetUniformLocation(m_shader,"uColor"),r,g,b,a);
        uploadAndDraw(v,idx);
    }

    void uploadAndDraw(const std::vector<float>& v,
                       const std::vector<unsigned int>& idx)
    {
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER,m_vbo);
        glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     idx.size()*sizeof(unsigned int),idx.data(),GL_DYNAMIC_DRAW);
        glDrawElements(GL_TRIANGLES,(GLsizei)idx.size(),GL_UNSIGNED_INT,0);
        glBindVertexArray(0);
    }

    float frand() const { return (float)rand()/RAND_MAX; }
    float progress(const GlitchEvent& g) const { return g.age/g.lifetime; }

    void buildShaders()
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
                fprintf(stderr,"VFX shader:\n%s\n",l);}
            return id;
        };
        GLuint v=cs(GL_VERTEX_SHADER,vs),f=cs(GL_FRAGMENT_SHADER,fs);
        m_shader=glCreateProgram();
        glAttachShader(m_shader,v);glAttachShader(m_shader,f);glLinkProgram(m_shader);
        glDeleteShader(v);glDeleteShader(f);
        m_glitchShader=m_shader; // same shader, different usage
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

    GLuint m_shader=0, m_glitchShader=0;
    GLuint m_vao=0, m_vbo=0, m_ebo=0;
    int    m_vpW=1200, m_vpH=850;
    float  m_time=0.f;
    float  m_screenShakeLeft=0.f;

    std::vector<GlitchEvent>   m_glitches;
    std::vector<DeathParticle> m_deathParticles;

    // Helper for GlitchEvent progress (avoids ambiguity)
    float glitchProgress(const GlitchEvent& g) const
    { return g.age / g.lifetime; }
};
