#pragma once
#include "Particle.h"
#include "BroadPhase.h"
#include <glm/glm.hpp>
#include <vector>

// ============================================================
//  SimConfig
// ============================================================
struct SimConfig
{
    glm::vec3 gravity          = {0.f, -9.81f, 0.f};
    float     damping          = 0.999f;
    float     floorY           = -5.f;
    float     restitution      = 0.3f;
    int       solverIterations = 20;
    float     dt               = 1.f / 60.f; // fixed timestep for XPBD

    // 0=single  1=stick  2=rope  3=cloth  4=rigid stack  5=soft body
    int mode = 4;

    // Rope
    int   ropeSegments = 12;
    float ropeSegLen   = 0.6f;

    // Cloth
    int   clothRows     = 14;
    int   clothCols     = 16;
    float clothSpacing  = 0.50f;
    int   clothPinMode  = 0;
    bool  clothShear    = true;
    bool  clothBend     = true;
    float clothCompliance = 0.f;

    // Tearing
    bool  tearEnabled        = true;
    float breakThreshold     = 1.7f;
    bool  tearStructuralOnly = true;

    // Rigid bodies
    int   rigidCount      = 6;     // how many boxes to spawn
    float rigidSize       = 0.8f;  // half-extent of each box
    float rigidStiffness  = 0.95f; // 1=fully rigid, 0.3=squishy

    // Soft body
    float softCompliance  = 1e-3f; // compliance for soft body constraints

    // Sphere collider
    bool      sphereEnabled = false;
    glm::vec3 sphereCenter  = {0.f, -0.5f, 0.f};
    float     sphereRadius  = 1.8f;

    // Particle–particle collision (for rigid bodies bumping each other)
    bool  particleCollision = true;
    float collisionRadius   = 0.35f; // radius at which particles push apart
};

// ============================================================
//  Simulation
// ============================================================
class Simulation
{
public:
    explicit Simulation(SimConfig cfg = {});

    void reset();

    // Step using fixed dt from config (XPBD requires consistent dt)
    void step(float wallDt);

    void beginDrag(glm::vec2 worldPos);
    void updateDrag(glm::vec2 worldPos);
    void endDrag();

    // Accessors
    const std::vector<Particle>&           particles()    const { return m_particles; }
    const std::vector<DistanceConstraint>& constraints()  const { return m_constraints; }
    const std::vector<RigidBody>&          rigidBodies()  const { return m_rigidBodies; }
    const MouseConstraint&                 mouseCon()     const { return m_mouse; }
    const SphereCollider&                  sphere()       const { return m_sphere; }
    SimConfig&       config()       { return m_cfg; }
    const SimConfig& config() const { return m_cfg; }
    std::vector<Particle>&           particles_mut()   { return m_particles; }
    std::vector<DistanceConstraint>& constraints_mut() { return m_constraints; }
    int tornCount()             const { return m_tornCount; }

private:
    // Scene builders
    void buildSingleParticle();
    void buildStick();
    void buildRope();
    void buildCloth();
    void buildRigidStack();
    void buildSoftBody();

    // Helpers
    int  addBox(glm::vec3 center, float halfW, float halfH,
                float stiffness, bool isStatic = false);
    int  addParticle(glm::vec3 pos, float invMass = 1.f);
    void addDistConstraint(int i, int j, float compliance = 0.f,
                           float breakThresh = 1e9f);

    // Per-frame pipeline
    void tickOnce(float dt);
    void applyExternalForces(float dt);
    void predictPositions(float dt);
    void resetLambdas();
    void solveConstraints(float dt);
    void projectRigidBodies();
    void resolveParticleCollisions();
    void resolveCollisions();
    void removeTornConstraints();
    void updateVelocitiesAndPositions(float dt);

    int nearestParticle(glm::vec2 wp, float maxDist) const;

    SimConfig                       m_cfg;
    std::vector<Particle>           m_particles;
    std::vector<DistanceConstraint> m_constraints;
    std::vector<RigidBody>          m_rigidBodies;
    MouseConstraint                 m_mouse;
    SphereCollider                  m_sphere;
    BroadPhase                      m_broadPhase;
    int                             m_tornCount  = 0;
    float                           m_accumDt    = 0.f; // leftover time for fixed step
};
