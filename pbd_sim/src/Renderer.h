#pragma once
#include "Simulation.h"
#include <glad/glad.h>
#include <glm/glm.hpp>

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void init(int w, int h);
    void draw(const Simulation& sim);
    void resize(int w, int h);
    glm::vec2 screenToWorld(double sx, double sy) const;

    bool showWireframe   = false;
    bool showParticles   = true;
    bool showConstraints = false;
    bool showRigidDebug  = false; // draw rigid body shape-match goals

private:
    void buildShaders();
    void buildBuffers();
    glm::mat4 makeVP() const;

    void drawFloor(float floorY, float hW, const glm::mat4& VP);
    void drawSphere(const SphereCollider& s, const glm::mat4& VP);
    void drawClothMesh(const Simulation& sim, const glm::mat4& VP);
    void drawRigidBodies(const Simulation& sim, const glm::mat4& VP);
    void drawConstraintLines(const Simulation& sim, const glm::mat4& VP);
    void drawParticles(const Simulation& sim, const glm::mat4& VP);
    void drawDragLine(const Simulation& sim, const glm::mat4& VP);

    void uploadLines(const std::vector<float>& verts);
    void uploadAndDrawTriangles(const std::vector<float>& verts,
                                const std::vector<unsigned int>& idx);

    GLuint m_ptShader   = 0;
    GLuint m_lineShader = 0;
    GLuint m_faceShader = 0;

    GLuint m_ptVAO=0,   m_ptVBO=0;
    GLuint m_lineVAO=0, m_lineVBO=0;
    GLuint m_faceVAO=0, m_faceVBO=0, m_faceEBO=0;

    int   m_vpW=1100, m_vpH=800;
    float m_aspect=1.f;

    static GLuint compileShader(GLenum type, const char* src);
    static GLuint linkProgram(GLuint v, GLuint f);
};
