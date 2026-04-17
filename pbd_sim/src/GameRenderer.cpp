#include "GameRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <stdexcept>
#include <cmath>
#include <vector>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char* kVS = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uVP;
void main(){ gl_Position=uVP*vec4(aPos,1.0); }
)glsl";
static const char* kFS = R"glsl(
#version 330 core
out vec4 O; uniform vec4 uColor;
void main(){ O=uColor; }
)glsl";

GameRenderer::GameRenderer()=default;
GameRenderer::~GameRenderer()
{
    glDeleteVertexArrays(1,&m_VAO);
    glDeleteBuffers(1,&m_VBO);
    glDeleteBuffers(1,&m_EBO);
    glDeleteProgram(m_colorShader);
}

void GameRenderer::init(int w,int h)
{
    buildShaders(); buildBuffers(); resize(w,h);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}

void GameRenderer::resize(int w,int h)
{
    m_vpW=w; m_vpH=h;
    m_aspect=h>0?(float)w/h:1.f;
    glViewport(0,0,w,h);
}

glm::mat4 GameRenderer::makeVP(const Camera& cam) const
{ return cam.viewProjection(); }

glm::vec2 GameRenderer::screenToWorld(double sx,double sy) const
{
    float hH=8.f,hW=hH*m_aspect;
    return{((float)sx/m_vpW*2.f-1.f)*hW,-((float)sy/m_vpH*2.f-1.f)*hH};
}

// ============================================================
//  Biome palettes
// ============================================================
GameRenderer::Palette GameRenderer::paletteFor(Biome b)
{
    switch(b)
    {
    default:
    case Biome::Catacombs:
        return {0.22f,0.18f,0.20f,  // platform fill — dark stone
                0.45f,0.38f,0.52f,  // edge glow — dusty purple
                0.10f,0.07f,0.10f,  // bg near
                0.06f,0.04f,0.08f}; // bg far
    case Biome::Cathedral:
        return {0.18f,0.18f,0.28f,  // platform — dark blue stone
                0.40f,0.50f,0.70f,  // edge — cool blue highlight
                0.08f,0.08f,0.16f,  // bg near
                0.05f,0.05f,0.12f}; // bg far
    case Biome::Abyss:
        return {0.08f,0.06f,0.18f,  // platform — very dark void
                0.30f,0.20f,0.60f,  // edge — violet glow
                0.04f,0.03f,0.10f,  // bg near
                0.02f,0.01f,0.06f}; // bg far
    }
}

// ============================================================
//  Main draw
// ============================================================
void GameRenderer::draw(
    const Camera& cam,
    const Level& level,
    const Player& player,
    const std::vector<Particle>& particles,
    const std::vector<DistanceConstraint>& constraints,
    Biome biome)
{
    auto pal = paletteFor(biome);
    glClearColor(pal.bgFarR, pal.bgFarG, pal.bgFarB, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glm::mat4 VP = makeVP(cam);

    drawBackground(cam, biome);
    drawLevel(level, VP, biome);
    drawPhysicsObjects(particles, constraints, VP);
    drawGrapple(player, particles, VP);
    drawPlayer(player, VP);
    if (debugDraw) drawDebugOverlay(player, level, VP);
}

// ============================================================
//  Background — rich parallax layers, biome-specific details
// ============================================================
void GameRenderer::drawBackground(const Camera& cam, Biome biome)
{
    auto pal = paletteFor(biome);
    float hH = cam.halfHeight * cam.zoom;
    float hW = hH * m_aspect;
    float cy  = cam.position.y;
    float cx  = cam.position.x;

    // ---- Layer helper: builds a parallax VP ----
    auto layerVP = [&](float speed) {
        float px = cx * speed;
        float py = cy * speed * 0.3f;  // subtle vertical parallax
        return glm::ortho(cx-hW-px, cx+hW-px,
                          cy-hH-py, cy+hH-py, -10.f, 10.f);
    };

    // ---- Far background: fog/sky fill ----
    {
        glm::mat4 vp = layerVP(0.f);  // locked to screen
        // Gradient-like using stacked horizontal bands
        int bands = 16;
        for (int i = 0; i < bands; ++i)
        {
            float t  = (float)i / bands;
            float bh = hH * 2.f / bands;
            float by = cy - hH + i * bh;
            float alpha = 0.06f + t * 0.08f;
            drawRect({cx, by + bh*0.5f}, {hW, bh*0.6f}, vp,
                     pal.bgFarR, pal.bgFarG, pal.bgFarB + 0.04f, alpha);
        }
    }

    if (biome == Biome::Catacombs)
    {
        // ---- Catacombs: deep cave ceiling stalactites + torch glows ----

        // Far layer — thin hanging stalactites from ceiling
        {
            glm::mat4 vp = layerVP(0.04f);
            float ceilY  = cy + hH;
            float stW    = 0.18f, stH = 1.2f + 0.8f;
            int   count  = (int)(hW * 2.f / 1.4f) + 4;
            float startX = floorf((cx - hW) / 1.4f) * 1.4f;
            for (int i = 0; i < count; ++i)
            {
                float sx = startX + i * 1.4f + sinf(i * 2.3f) * 0.3f;
                float sh = stH * (0.5f + 0.5f * fabsf(sinf(i * 1.7f)));
                std::vector<float> tv = {
                    sx - stW, ceilY,      0.f,
                    sx + stW, ceilY,      0.f,
                    sx,       ceilY - sh, 0.f,
                };
                std::vector<unsigned int> ti = {0,1,2};
                drawTris(tv, ti, vp, pal.bgNearR*0.7f, pal.bgNearG*0.7f, pal.bgNearB*0.7f, 0.7f);
            }
        }

        // Mid layer — gothic pillars with carved details
        {
            glm::mat4 vp = layerVP(0.18f);
            float archW = 2.8f;
            int   count = (int)(hW * 2.f / archW) + 4;
            float startX = floorf((cx - hW) / archW) * archW;
            for (int i = 0; i < count; ++i)
            {
                float ax = startX + i * archW;
                float ay = cy - hH;
                float ph2 = hH * 1.6f;

                // Pillar body
                drawRect({ax, ay + ph2*0.5f}, {archW*0.22f, ph2*0.5f}, vp,
                         pal.bgNearR, pal.bgNearG, pal.bgNearB, 0.75f);

                // Pointed arch
                std::vector<float> tv = {
                    ax-archW*0.22f, ay+ph2, 0.f,
                    ax+archW*0.22f, ay+ph2, 0.f,
                    ax,             ay+ph2+archW*0.55f, 0.f,
                };
                std::vector<unsigned int> ti = {0,1,2};
                drawTris(tv, ti, vp, pal.bgNearR, pal.bgNearG, pal.bgNearB, 0.75f);

                // Carved horizontal band midway up pillar
                drawRect({ax, ay + ph2*0.35f}, {archW*0.25f, 0.08f}, vp,
                         pal.edgeR*0.5f, pal.edgeG*0.5f, pal.edgeB*0.5f, 0.4f);
            }
        }

        // Torch glow spots scattered at mid-level
        {
            glm::mat4 vp = layerVP(0.25f);
            int tcount = (int)(hW * 2.f / 5.f) + 3;
            float startX = floorf((cx - hW) / 5.f) * 5.f;
            float torchAnim = sinf(cam.position.x * 0.1f) * 0.5f; // use pos as seed
            for (int i = 0; i < tcount; ++i)
            {
                float tx = startX + i * 5.f + 1.5f;
                float ty = cy - hH * 0.1f + sinf(i * 3.1f) * hH * 0.25f;
                float flicker = 0.7f + 0.3f * sinf(torchAnim * 7.f + i * 1.9f);

                // Warm glow circle — large, very transparent
                drawCircle({tx, ty}, 2.2f, vp, 0.8f, 0.45f, 0.05f, 0.04f * flicker);
                drawCircle({tx, ty}, 1.1f, vp, 0.9f, 0.55f, 0.1f,  0.07f * flicker);
                // Torch bracket
                drawRect({tx, ty}, {0.06f, 0.3f}, vp, 0.4f, 0.3f, 0.2f, 0.6f);
                // Flame tip
                drawCircle({tx, ty + 0.25f}, 0.12f, vp, 1.f, 0.7f, 0.1f, 0.8f * flicker);
            }
        }

        // Near layer — thick hanging moss/chains
        {
            glm::mat4 vp = layerVP(0.45f);
            float ceilY = cy + hH;
            int   count = (int)(hW * 2.f / 0.9f) + 4;
            float startX = floorf((cx - hW) / 0.9f) * 0.9f;
            for (int i = 0; i < count; ++i)
            {
                if ((i % 3) == 0) continue;  // gaps
                float sx = startX + i * 0.9f;
                float sh = 0.5f + fabsf(sinf(i * 2.1f)) * 1.2f;
                drawRect({sx, ceilY - sh*0.5f}, {0.04f, sh*0.5f}, vp,
                         0.12f, 0.10f, 0.14f, 0.5f);
            }
        }
    }
    else if (biome == Biome::Cathedral)
    {
        // ---- Cathedral: stained glass light beams + ornate arches ----

        // Far layer — light beams from above (stained glass effect)
        {
            glm::mat4 vp = layerVP(0.02f);
            int beams = 7;
            float beamW = hW * 2.f / beams;
            // Coloured light shafts from ceiling
            float beamColors[][3] = {
                {0.6f, 0.2f, 0.8f},  // violet
                {0.2f, 0.4f, 0.9f},  // blue
                {0.8f, 0.3f, 0.2f},  // red
                {0.2f, 0.7f, 0.4f},  // green
                {0.9f, 0.7f, 0.1f},  // gold
                {0.3f, 0.6f, 0.9f},  // sky blue
                {0.8f, 0.2f, 0.5f},  // rose
            };
            for (int i = 0; i < beams; ++i)
            {
                float bx  = cx - hW + i * beamW + beamW * 0.5f;
                float top = cy + hH;
                float bot = cy - hH * 1.5f;
                float bh  = top - bot;
                float* bc = beamColors[i % 7];

                // Angled beam (slightly off vertical)
                float lean = (i % 2 == 0) ? 0.3f : -0.3f;
                std::vector<float> tv = {
                    bx - beamW*0.3f + lean, top,  0.f,
                    bx + beamW*0.3f + lean, top,  0.f,
                    bx + beamW*0.5f,        bot,  0.f,
                    bx - beamW*0.5f,        bot,  0.f,
                };
                std::vector<unsigned int> ti = {0,1,2, 0,2,3};
                drawTris(tv, ti, vp, bc[0], bc[1], bc[2], 0.04f);
            }
        }

        // Mid layer — ornate gothic windows
        {
            glm::mat4 vp = layerVP(0.15f);
            float archW = 3.2f;
            int   count = (int)(hW * 2.f / archW) + 4;
            float startX = floorf((cx - hW) / archW) * archW;
            for (int i = 0; i < count; ++i)
            {
                float ax  = startX + i * archW;
                float ay  = cy - hH;
                float ph2 = hH * 2.0f;

                // Main arch column
                drawRect({ax, ay + ph2*0.5f}, {archW*0.2f, ph2*0.5f}, vp,
                         pal.bgNearR, pal.bgNearG, pal.bgNearB, 0.8f);

                // Double pointed arch
                for (float ox : {-archW*0.08f, archW*0.08f})
                {
                    std::vector<float> tv = {
                        ax+ox-archW*0.12f, ay+ph2*0.9f, 0.f,
                        ax+ox+archW*0.12f, ay+ph2*0.9f, 0.f,
                        ax+ox,             ay+ph2*1.25f, 0.f,
                    };
                    std::vector<unsigned int> ti = {0,1,2};
                    drawTris(tv, ti, vp, pal.bgNearR, pal.bgNearG, pal.bgNearB, 0.8f);
                }

                // Window tracery — small circular rose window
                drawCircle({ax, ay+ph2*0.65f}, archW*0.18f, vp,
                           pal.bgNearR*1.3f, pal.bgNearG*1.3f, pal.bgNearB*1.5f, 0.3f);
                drawCircle({ax, ay+ph2*0.65f}, archW*0.10f, vp,
                           0.3f, 0.2f, 0.5f, 0.25f);
            }
        }

        // Near layer — broken arches and rubble
        {
            glm::mat4 vp = layerVP(0.38f);
            float archW = 1.8f;
            int   count = (int)(hW * 2.f / archW) + 4;
            float startX = floorf((cx - hW) / archW) * archW;
            for (int i = 0; i < count; ++i)
            {
                float ax = startX + i * archW;
                float ay = cy - hH;
                // Broken column stumps at floor level
                float sh = 0.8f + fabsf(sinf(i * 1.9f)) * 1.5f;
                drawRect({ax, ay + sh*0.5f}, {archW*0.18f, sh*0.5f}, vp,
                         pal.bgNearR*0.8f, pal.bgNearG*0.8f, pal.bgNearB*0.8f, 0.6f);
            }
        }
    }
    else  // Abyss
    {
        // ---- Abyss: void stars, drifting rocks, reality tears ----

        // Stars — tiny points scattered across background
        {
            glm::mat4 vp = layerVP(0.01f);
            // Use deterministic pattern based on camera region
            int   gridX = (int)(cx / 3.f);
            int   gridY = (int)(cy / 3.f);
            for (int dy = -3; dy <= 3; ++dy)
            for (int dx = -5; dx <= 5; ++dx)
            {
                int gx = gridX + dx, gy = gridY + dy;
                // Pseudo-random from grid cell
                float rx = sinf(gx * 127.3f + gy * 311.7f) * 0.5f + 0.5f;
                float ry = sinf(gx * 269.5f + gy * 183.1f) * 0.5f + 0.5f;
                float sx  = (gx + rx) * 3.f;
                float sy  = (gy + ry) * 3.f;
                float sz  = 0.04f + sinf(gx * 73.f + gy * 41.f) * 0.02f;
                float bri = 0.4f + fabsf(sinf(gx * 57.f + gy * 89.f)) * 0.5f;
                drawCircle({sx, sy}, sz, vp, bri, bri, bri + 0.2f, 0.7f, 6);
            }
        }

        // Drifting void rocks (mid layer)
        {
            glm::mat4 vp = layerVP(0.12f);
            float rockSpacing = 4.5f;
            int   count = (int)(hW * 2.f / rockSpacing) + 4;
            float startX = floorf((cx - hW) / rockSpacing) * rockSpacing;
            for (int i = 0; i < count; ++i)
            {
                float rx  = startX + i * rockSpacing + sinf(i * 2.7f) * 1.2f;
                float ry  = cy + sinf(i * 1.3f + cx * 0.05f) * hH * 0.5f;
                float rw  = 0.4f + fabsf(sinf(i * 3.1f)) * 0.8f;
                float rh  = 0.3f + fabsf(sinf(i * 2.3f)) * 0.5f;

                // Jagged rock shape
                std::vector<float> tv = {
                    rx-rw,   ry,      0.f,
                    rx-rw*0.3f, ry+rh, 0.f,
                    rx+rw*0.4f, ry+rh*1.2f, 0.f,
                    rx+rw,   ry+rh*0.3f, 0.f,
                    rx+rw*0.6f, ry-rh*0.2f, 0.f,
                };
                std::vector<unsigned int> ti = {0,1,2, 0,2,3, 0,3,4};
                drawTris(tv, ti, vp,
                         pal.bgNearR*1.5f, pal.bgNearG*1.2f, pal.bgNearB*2.f, 0.55f);

                // Violet glow edge
                drawCircle({rx, ry+rh*0.5f}, rw*0.9f, vp,
                           0.25f, 0.15f, 0.5f, 0.08f);
            }
        }

        // Reality tears — thin glowing cracks in the void
        {
            glm::mat4 vp = layerVP(0.22f);
            int   count = (int)(hW * 2.f / 6.f) + 3;
            float startX = floorf((cx - hW) / 6.f) * 6.f;
            for (int i = 0; i < count; ++i)
            {
                float tx  = startX + i * 6.f + sinf(i * 4.1f) * 1.5f;
                float ty  = cy + sinf(i * 2.7f) * hH * 0.6f;
                float len = 1.5f + fabsf(sinf(i * 1.9f)) * 2.f;
                float ang = sinf(i * 3.3f) * 1.2f;
                float ex  = tx + cosf(ang) * len;
                float ey  = ty + sinf(ang) * len;

                // Main crack line
                drawLine({tx, ty}, {ex, ey}, vp,
                         0.55f, 0.25f, 0.9f, 0.35f);
                // Glow around it
                drawLine({tx, ty}, {ex, ey}, vp,
                         0.4f, 0.15f, 0.7f, 0.12f);
            }
        }

        // Near layer — close drifting debris
        {
            glm::mat4 vp = layerVP(0.42f);
            int   count = (int)(hW * 2.f / 2.5f) + 4;
            float startX = floorf((cx - hW) / 2.5f) * 2.5f;
            for (int i = 0; i < count; ++i)
            {
                float dx  = startX + i * 2.5f;
                float dy  = cy + sinf(i * 1.7f + cx * 0.08f) * hH * 0.7f;
                float ds  = 0.08f + fabsf(sinf(i * 4.3f)) * 0.18f;
                drawRect({dx, dy}, {ds, ds}, vp,
                         0.18f, 0.12f, 0.35f, 0.5f);
            }
        }
    }
}

// ============================================================
//  Level — platforms with outlined hand-painted look
// ============================================================
void GameRenderer::drawLevel(const Level& level,
                               const glm::mat4& VP, Biome biome)
{
    auto pal = paletteFor(biome);

    for (const auto& p : level.platforms)
    {
        // Outline pass — draw slightly larger in dark colour
        float os = 0.07f;  // outline size in world units
        drawRect(p.pos, {p.size.x+os, p.size.y+os}, VP,
                 0.04f,0.03f,0.06f,0.95f);

        // Fill pass
        drawRect(p.pos, p.size, VP,
                 pal.platformR, pal.platformG, pal.platformB);

        // Top edge highlight — the "lit from above" hand-drawn look
        drawRect({p.pos.x, p.pos.y+p.size.y-0.04f},
                 {p.size.x, 0.04f}, VP,
                 pal.edgeR, pal.edgeG, pal.edgeB, 0.85f);

        // Subtle side lines
        drawRect({p.pos.x-p.size.x+0.03f, p.pos.y},
                 {0.03f, p.size.y}, VP,
                 pal.edgeR*0.6f, pal.edgeG*0.6f, pal.edgeB*0.6f, 0.4f);
        drawRect({p.pos.x+p.size.x-0.03f, p.pos.y},
                 {0.03f, p.size.y}, VP,
                 pal.edgeR*0.6f, pal.edgeG*0.6f, pal.edgeB*0.6f, 0.4f);

        // Abyss: add floating rock cracks
        if (biome == Biome::Abyss)
        {
            // Diagonal crack lines across platform surface
            drawLine({p.pos.x-p.size.x*0.3f, p.pos.y+p.size.y*0.4f},
                     {p.pos.x+p.size.x*0.1f, p.pos.y-p.size.y*0.3f},
                     VP, 0.0f,0.0f,0.0f,0.3f);
            drawLine({p.pos.x+p.size.x*0.2f, p.pos.y+p.size.y*0.5f},
                     {p.pos.x+p.size.x*0.5f, p.pos.y-p.size.y*0.2f},
                     VP, 0.0f,0.0f,0.0f,0.25f);
        }
    }
}

// ============================================================
//  Player — capsule with outlined hand-painted style
// ============================================================
void GameRenderer::drawPlayer(const Player& p, const glm::mat4& VP)
{
    float hw = p.cfg.bodyRadius;
    float hh = p.cfg.height*0.5f;

    // State colours
    float r=0.75f,g=0.88f,b=1.0f;
    switch(p.state)
    {
    case PlayerState::Jumping:
    case PlayerState::WallJumping: r=0.88f;g=1.0f;b=0.7f; break;
    case PlayerState::Falling:     r=0.55f;g=0.68f;b=0.95f; break;
    case PlayerState::WallSliding: r=1.0f;g=0.72f;b=0.28f; break;
    case PlayerState::Swinging:    r=0.85f;g=0.45f;b=1.0f; break;
    case PlayerState::Dead:        r=0.9f;g=0.2f;b=0.2f; break;
    default: break;
    }

    float os = 0.06f;  // outline size

    // Outline pass (dark)
    drawCircle({p.position.x, p.position.y+hh-hw}, hw+os, VP, 0.04f,0.03f,0.06f);
    drawCircle({p.position.x, p.position.y-hh+hw}, hw+os, VP, 0.04f,0.03f,0.06f);
    drawRect(p.position, {hw+os, hh-hw+os}, VP, 0.04f,0.03f,0.06f);

    // Fill pass
    drawRect(p.position, {hw, hh-hw}, VP, r,g,b);
    drawCircle({p.position.x, p.position.y+hh-hw}, hw, VP, r,g,b);
    drawCircle({p.position.x, p.position.y-hh+hw}, hw, VP, r,g,b);

    // Eye
    float ex = p.position.x + (p.facingRight ? hw*0.45f : -hw*0.45f);
    float ey = p.position.y + hh*0.3f;
    drawCircle({ex,ey}, hw*0.20f, VP, 0.04f,0.03f,0.07f);

    // Direction nub
    float tipX = p.position.x + (p.facingRight ? hw*1.25f : -hw*1.25f);
    std::vector<float> tv={
        p.position.x+(p.facingRight?hw*0.7f:-hw*0.7f), p.position.y+hw*0.25f, 0.f,
        p.position.x+(p.facingRight?hw*0.7f:-hw*0.7f), p.position.y-hw*0.25f, 0.f,
        tipX, p.position.y, 0.f,
    };
    std::vector<unsigned int> ti={0,1,2};
    drawTris(tv,ti,VP, r*0.7f,g*0.7f,b*0.7f,0.85f);
}

// ============================================================
//  Grapple rope — drawn as a simple tether line
// ============================================================
void GameRenderer::drawGrapple(
    const Player& p,
    const std::vector<Particle>& particles,
    const glm::mat4& VP)
{
    if (!p.grappleActive) return;

    glm::vec2 anchor = p.grappleAnchor;
    glm::vec2 ppos   = p.position;

    // Rope outline (thick dark)
    drawLine(ppos, anchor, VP, 0.06f,0.04f,0.08f, 1.f);
    // Rope fill (warm rope colour)
    drawLine(ppos, anchor, VP, 0.85f,0.70f,0.35f, 0.95f);

    // Anchor point — small spike embedded in wall
    drawCircle(anchor, 0.18f, VP, 0.06f,0.04f,0.08f);
    drawCircle(anchor, 0.12f, VP, 1.0f, 0.88f, 0.3f);

    // Tension indicator — small circle at midpoint
    glm::vec2 mid   = (ppos + anchor) * 0.5f;
    float     slack = glm::length(anchor - ppos) / glm::max(p.grappleLength, 0.1f);
    float     tr    = slack > 0.9f ? 0.9f : 0.4f;  // red when taut
    float     tg    = slack > 0.9f ? 0.2f : 0.8f;
    drawCircle(mid, 0.07f, VP, tr, tg, 0.2f, 0.7f);
}

// ============================================================
//  Physics objects (boulders = groups of 4 particles + constraints)
//  Drawn with warm glow aura to indicate they're active physics
// ============================================================
void GameRenderer::drawPhysicsObjects(
    const std::vector<Particle>& particles,
    const std::vector<DistanceConstraint>& constraints,
    const glm::mat4& VP)
{
    // Draw constraint edges (rope, cloth remnants)
    for (const auto& c : constraints)
    {
        if (c.i >= (int)particles.size() || c.j >= (int)particles.size()) continue;
        const auto& a=particles[c.i].position;
        const auto& b=particles[c.j].position;
        float len = glm::distance(glm::vec2(a),glm::vec2(b));
        if (len > 5.f) continue;  // skip crazy long constraints
        drawLine({a.x,a.y},{b.x,b.y},VP, 0.45f,0.38f,0.55f,0.6f);
    }
}

// ============================================================
//  Boss visual
// ============================================================
void GameRenderer::drawBoss(glm::vec2 pos, float size, bool rage,
                             float animTime, const glm::mat4& VP)
{
    float pulse = sinf(animTime*3.f)*0.05f;
    float s = size + pulse;

    // Glow aura (rage = red, normal = dark amber)
    float gr = rage?0.8f:0.4f, gg=rage?0.1f:0.25f, gb=rage?0.1f:0.1f;
    drawCircle(pos, s*1.6f, VP, gr,gg,gb,0.15f);
    drawCircle(pos, s*1.3f, VP, gr,gg,gb,0.2f);

    // Body outline
    drawRect(pos, {s+0.1f,s*1.2f+0.1f}, VP, 0.06f,0.04f,0.06f);
    // Body fill
    float r=rage?0.55f:0.38f, g=rage?0.12f:0.22f, b=0.18f;
    drawRect(pos, {s,s*1.2f}, VP, r,g,b);

    // Head
    drawCircle({pos.x,pos.y+s*1.1f}, s*0.55f, VP, 0.06f,0.04f,0.06f);
    drawCircle({pos.x,pos.y+s*1.1f}, s*0.5f,  VP, r*1.1f,g*1.1f,b);

    // Eyes (glowing red in rage)
    float er=rage?1.f:0.85f, eg=rage?0.1f:0.4f, eb=rage?0.1f:0.3f;
    drawCircle({pos.x-s*0.2f,pos.y+s*1.2f},s*0.12f,VP,er,eg,eb);
    drawCircle({pos.x+s*0.2f,pos.y+s*1.2f},s*0.12f,VP,er,eg,eb);

    // Fists (two circles at sides, animated)
    float fistY = pos.y + sinf(animTime*2.f)*0.15f;
    drawCircle({pos.x-s*1.1f,fistY}, s*0.4f, VP, 0.05f,0.04f,0.05f);
    drawCircle({pos.x-s*1.1f,fistY}, s*0.35f,VP, r,g,b);
    drawCircle({pos.x+s*1.1f,fistY}, s*0.4f, VP, 0.05f,0.04f,0.05f);
    drawCircle({pos.x+s*1.1f,fistY}, s*0.35f,VP, r,g,b);
}

// ============================================================
//  Shockwave
// ============================================================
void GameRenderer::drawShockwave(float x, float y, float w, float h,
                                  float animTime, const glm::mat4& VP)
{
    float pulse = sinf(animTime*15.f)*0.1f;
    // Outer glow
    drawRect({x,y+h*0.5f},{w+0.3f,h+0.3f+pulse},VP,0.9f,0.5f,0.1f,0.15f);
    // Core
    drawRect({x,y+h*0.5f},{w,h},VP,1.f,0.7f,0.2f,0.85f);
    // Bright top edge
    drawRect({x,y+h-0.05f},{w,0.05f},VP,1.f,1.f,0.6f,0.9f);
}

// ============================================================
//  Exit portal
// ============================================================
void GameRenderer::drawPortal(glm::vec2 pos, float animTime,
                               const glm::mat4& VP)
{
    float pulse = sinf(animTime*3.f)*0.15f;
    float r = 0.8f + pulse;

    // Outer glow rings
    drawCircle(pos, r*1.8f, VP, 0.3f,0.8f,0.6f,0.08f);
    drawCircle(pos, r*1.4f, VP, 0.3f,0.8f,0.6f,0.15f);
    // Outline
    drawCircle(pos, r+0.1f, VP, 0.04f,0.08f,0.06f,0.9f);
    // Fill — swirling void colour
    drawCircle(pos, r, VP, 0.08f,0.35f,0.28f,0.95f);
    // Inner bright core
    drawCircle(pos, r*0.4f, VP, 0.5f,1.0f,0.8f,0.7f);

    // Rotating tick marks around the portal
    for (int i=0;i<8;++i)
    {
        float angle = animTime*1.5f + i*(float)M_PI/4.f;
        float tx = pos.x + cosf(angle)*(r+0.25f);
        float ty = pos.y + sinf(angle)*(r+0.25f);
        drawCircle({tx,ty},0.08f,VP,0.4f,0.9f,0.7f,0.7f);
    }
}

// ============================================================
//  Text fragment
// ============================================================
void GameRenderer::drawFragment(glm::vec2 pos, bool found,
                                 float animTime, const glm::mat4& VP)
{
    float pulse = found ? 0.f : sinf(animTime*4.f)*0.08f;
    float alpha = found ? 0.3f : 0.85f;

    // Small glowing rune marker
    drawCircle(pos, 0.22f+pulse, VP,
               found?0.2f:0.8f, found?0.2f:0.7f, found?0.3f:0.3f, alpha*0.4f);
    drawCircle(pos, 0.15f, VP,
               found?0.15f:0.7f, found?0.15f:0.6f, found?0.2f:0.25f, alpha);

    if (!found)
    {
        // Cross mark
        drawLine({pos.x-0.1f,pos.y},{pos.x+0.1f,pos.y},VP,1.f,0.9f,0.4f,alpha);
        drawLine({pos.x,pos.y-0.1f},{pos.x,pos.y+0.1f},VP,1.f,0.9f,0.4f,alpha);
    }
}

// ============================================================
//  Debug overlay
// ============================================================
void GameRenderer::drawDebugOverlay(const Player& p, const Level& level,
                                     const glm::mat4& VP)
{
    // Player hitbox
    drawRect(p.position,{p.cfg.bodyRadius,p.cfg.height*0.5f},
             VP,0.f,1.f,0.f,0.25f);
    // Velocity vector
    glm::vec2 vEnd=p.position+p.velocity*0.08f;
    drawLine(p.position,vEnd,VP,1.f,1.f,0.f,0.8f);
    // Ground sensor
    glm::vec2 feet={p.position.x,p.position.y-p.cfg.height*0.5f-0.08f};
    drawCircle(feet,0.1f,VP,p.onGround?0.f:1.f,p.onGround?1.f:0.f,0.f,0.8f);
}

// ============================================================
//  Primitives
// ============================================================

void GameRenderer::drawRect(glm::vec2 c, glm::vec2 h,
                              const glm::mat4& VP,
                              float r, float g, float b, float a)
{
    std::vector<float> v={
        c.x-h.x,c.y-h.y,0.f, c.x+h.x,c.y-h.y,0.f,
        c.x+h.x,c.y+h.y,0.f, c.x-h.x,c.y+h.y,0.f,
    };
    std::vector<unsigned int> idx={0,1,2,0,2,3};
    drawTris(v,idx,VP,r,g,b,a);
}

void GameRenderer::drawCircle(glm::vec2 c, float radius,
                               const glm::mat4& VP,
                               float r, float g, float b, float a, int segs)
{
    std::vector<float> v;
    std::vector<unsigned int> idx;
    v.push_back(c.x);v.push_back(c.y);v.push_back(0.f);
    for(int i=0;i<=segs;++i)
    {
        float angle=2.f*(float)M_PI*i/segs;
        v.push_back(c.x+cosf(angle)*radius);
        v.push_back(c.y+sinf(angle)*radius);
        v.push_back(0.f);
    }
    for(int i=0;i<segs;++i){idx.push_back(0);idx.push_back(i+1);idx.push_back(i+2);}
    drawTris(v,idx,VP,r,g,b,a);
}

void GameRenderer::drawLine(glm::vec2 p0, glm::vec2 p1,
                             const glm::mat4& VP,
                             float r, float g, float b, float alpha)
{
    std::vector<float> v={p0.x,p0.y,0.f,p1.x,p1.y,0.f};
    drawLines(v,VP,r,g,b,alpha);
}

void GameRenderer::drawLines(const std::vector<float>& verts,
                              const glm::mat4& VP,
                              float r, float g, float b, float a)
{
    glUseProgram(m_colorShader);
    glUniformMatrix4fv(glGetUniformLocation(m_colorShader,"uVP"),1,GL_FALSE,
                       glm::value_ptr(VP));
    glUniform4f(glGetUniformLocation(m_colorShader,"uColor"),r,g,b,a);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_VBO);
    glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(float),verts.data(),GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES,0,(GLsizei)(verts.size()/3));
    glBindVertexArray(0);
}

void GameRenderer::drawTris(const std::vector<float>& verts,
                             const std::vector<unsigned int>& idx,
                             const glm::mat4& VP,
                             float r, float g, float b, float a)
{
    glUseProgram(m_colorShader);
    glUniformMatrix4fv(glGetUniformLocation(m_colorShader,"uVP"),1,GL_FALSE,
                       glm::value_ptr(VP));
    glUniform4f(glGetUniformLocation(m_colorShader,"uColor"),r,g,b,a);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_VBO);
    glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(float),verts.data(),GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,idx.size()*sizeof(unsigned int),
                 idx.data(),GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES,(GLsizei)idx.size(),GL_UNSIGNED_INT,0);
    glBindVertexArray(0);
}

void GameRenderer::buildBuffers()
{
    glGenVertexArrays(1,&m_VAO);glGenBuffers(1,&m_VBO);glGenBuffers(1,&m_EBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_VBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m_EBO);
    glBindVertexArray(0);
}

void GameRenderer::buildShaders()
{ m_colorShader=makeShader(kVS,kFS); }

GLuint GameRenderer::makeShader(const char* vs,const char* fs)
{
    auto cs=[](GLenum t,const char* s)->GLuint{
        GLuint id=glCreateShader(t);
        glShaderSource(id,1,&s,nullptr);glCompileShader(id);
        GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
        if(!ok){char l[512];glGetShaderInfoLog(id,512,nullptr,l);
            fprintf(stderr,"Shader:\n%s\n",l);throw std::runtime_error("shader");}
        return id;
    };
    GLuint v=cs(GL_VERTEX_SHADER,vs),f=cs(GL_FRAGMENT_SHADER,fs);
    GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char l[512];glGetProgramInfoLog(p,512,nullptr,l);throw std::runtime_error("link");}
    glDeleteShader(v);glDeleteShader(f);return p;
}
