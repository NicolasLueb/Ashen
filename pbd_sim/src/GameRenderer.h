#pragma once
#include <glad/glad.h>
#include "Camera.h"
#include "Level.h"
#include "Player.h"
#include "Particle.h"
#include "GameState.h"
#include <glm/glm.hpp>
#include <vector>

// ============================================================
//  GameRenderer — REVISED with hand-painted visual style
//
//  Visual technique: "outline + textured fill"
//    1. Draw each shape slightly enlarged in dark outline colour
//    2. Draw the actual shape on top in fill colour
//    3. Result: every object looks like it has a hand-drawn border
//
//  Biome colour palettes applied to all world geometry.
//  Active physics objects (boulders, swinging rope) get
//  a warm aura drawn as a soft circle behind them.
// ============================================================
class GameRenderer
{
public:
    GameRenderer();
    ~GameRenderer();

    void init(int w, int h);
    void resize(int w, int h);

    // Draw the full world
    void draw(const Camera& cam,
              const Level& level,
              const Player& player,
              const std::vector<Particle>& particles,
              const std::vector<DistanceConstraint>& constraints,
              Biome biome = Biome::Catacombs);

    // Draw boss visual (called from Game)
    void drawBoss(glm::vec2 pos, float size, bool rage,
                  float animTime, const glm::mat4& VP);

    // Draw a shockwave
    void drawShockwave(float x, float y, float w, float h,
                       float animTime, const glm::mat4& VP);

    // Draw exit portal
    void drawPortal(glm::vec2 pos, float animTime, const glm::mat4& VP);

    // Draw text fragment pickup
    void drawFragment(glm::vec2 pos, bool found,
                      float animTime, const glm::mat4& VP);

    bool debugDraw = false;

    glm::vec2 screenToWorld(double sx, double sy) const;

private:
    void buildShaders();
    void buildBuffers();
    glm::mat4 makeVP(const Camera& cam) const;

    // Biome colour palette
    struct Palette {
        float platformR,platformG,platformB;
        float edgeR,edgeG,edgeB;
        float bgNearR,bgNearG,bgNearB;
        float bgFarR,bgFarG,bgFarB;
    };
    static Palette paletteFor(Biome b);

    // Draw with outline technique
    void drawShapeOutlined(const std::vector<float>& verts,
                           const std::vector<unsigned int>& idx,
                           const glm::mat4& VP,
                           float fr, float fg, float fb, float fa,
                           float or_, float og, float ob,
                           float outlineSize = 0.08f);

    void drawBackground(const Camera& cam, Biome biome);
    void drawLevel(const Level& level, const glm::mat4& VP, Biome biome);
    void drawPlayer(const Player& p, const glm::mat4& VP);
    void drawGrapple(const Player& p,
                     const std::vector<Particle>& particles,
                     const glm::mat4& VP);
    void drawPhysicsObjects(const std::vector<Particle>& particles,
                            const std::vector<DistanceConstraint>& constraints,
                            const glm::mat4& VP);
    void drawDebugOverlay(const Player& p, const Level& level,
                          const glm::mat4& VP);

    void setColor(float r, float g, float b, float a);
    void drawRect(glm::vec2 center, glm::vec2 half,
                  const glm::mat4& VP, float r, float g, float b, float a=1.f);
    void drawCircle(glm::vec2 center, float radius,
                    const glm::mat4& VP, float r, float g, float b,
                    float a=1.f, int segs=24);
    void drawLine(glm::vec2 p0, glm::vec2 p1,
                  const glm::mat4& VP, float r, float g, float b, float alpha=1.f);
    void drawTris(const std::vector<float>& verts,
                  const std::vector<unsigned int>& idx,
                  const glm::mat4& VP, float r, float g, float b, float a=1.f);
    void drawLines(const std::vector<float>& verts,
                   const glm::mat4& VP, float r, float g, float b, float a=1.f);

    GLuint m_colorShader = 0;
    GLuint m_VAO=0, m_VBO=0, m_EBO=0;

    int   m_vpW=1200, m_vpH=850;
    float m_aspect=1.f;

    static GLuint makeShader(const char* vs, const char* fs);
};
