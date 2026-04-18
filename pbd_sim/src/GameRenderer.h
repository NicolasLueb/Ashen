#pragma once
#include <glad/glad.h>
#include "Camera.h"
#include "BossController.h"
#include "Level.h"
#include "Player.h"
#include "Particle.h"
#include "GameState.h"
#include <glm/glm.hpp>
#include <vector>

class GameRenderer
{
public:
    GameRenderer();
    ~GameRenderer();

    void init(int w, int h);
    void resize(int w, int h);

    void draw(const Camera& cam, const Level& level, const Player& player,
              const std::vector<Particle>& particles,
              const std::vector<DistanceConstraint>& constraints,
              Biome biome = Biome::Catacombs);

    void drawBoss(glm::vec2 pos, float size, bool rage, float animTime,
                  const glm::mat4& VP, BossType type = BossType::Golem, float alpha = 1.f);
    void drawShockwave(float x, float y, float w, float h, float animTime, const glm::mat4& VP);
    void drawOrb(glm::vec2 pos, float radius, const glm::mat4& VP, BossType type);
    void drawBreathColumn(float x, float width, float progress, float floorY, float ceilY, const glm::mat4& VP);
    void drawTailBlock(float x, int side, float progress, float floorY, float ceilY, const glm::mat4& VP);
    void drawWindZone(glm::vec2 pos, glm::vec2 size, glm::vec2 force, float animTime, const glm::mat4& VP);
    void drawSpikes(glm::vec2 pos, glm::vec2 size, const glm::mat4& VP);

    // Draw ghost replay (translucent player silhouette)
    void drawGhost(glm::vec2 pos, bool facingRight, int stateIdx,
                   const glm::mat4& VP);
    void drawPortal(glm::vec2 pos, float animTime, const glm::mat4& VP);
    void drawFragment(glm::vec2 pos, bool found, float animTime, const glm::mat4& VP);

    // Primitives — public so Game.cpp decorations can use them
    void drawRect(glm::vec2 center, glm::vec2 half, const glm::mat4& VP,
                  float r, float g, float b, float a=1.f);
    void drawCircle(glm::vec2 center, float radius, const glm::mat4& VP,
                    float r, float g, float b, float a=1.f, int segs=24);
    void drawLine(glm::vec2 p0, glm::vec2 p1, const glm::mat4& VP,
                  float r, float g, float b, float alpha=1.f);
    void drawTris(const std::vector<float>& verts, const std::vector<unsigned int>& idx,
                  const glm::mat4& VP, float r, float g, float b, float a=1.f);
    void drawLines(const std::vector<float>& verts, const glm::mat4& VP,
                   float r, float g, float b, float a=1.f);

    bool      debugDraw = false;
    glm::vec2 screenToWorld(double sx, double sy) const;

private:
    void buildShaders();
    void buildBuffers();
    glm::mat4 makeVP(const Camera& cam) const;

    struct Palette {
        float platformR,platformG,platformB;
        float edgeR,edgeG,edgeB;
        float bgNearR,bgNearG,bgNearB;
        float bgFarR,bgFarG,bgFarB;
    };
    static Palette paletteFor(Biome b);

    void drawBackground(const Camera& cam, Biome biome);
    void drawLevel(const Level& level, const glm::mat4& VP, Biome biome);
    void drawPlayer(const Player& p, const glm::mat4& VP);
    void drawGrapple(const Player& p, const std::vector<Particle>& particles, const glm::mat4& VP);
    void drawPhysicsObjects(const std::vector<Particle>& particles,
                            const std::vector<DistanceConstraint>& constraints, const glm::mat4& VP);
    void drawDebugOverlay(const Player& p, const Level& level, const glm::mat4& VP);
    void setColor(float r, float g, float b, float a);

    GLuint m_colorShader = 0;
    GLuint m_VAO=0, m_VBO=0, m_EBO=0;
    int    m_vpW=1200, m_vpH=850;
    float  m_aspect=1.f;

    static GLuint makeShader(const char* vs, const char* fs);
};
