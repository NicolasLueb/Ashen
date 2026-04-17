#include "Simulation.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

// ============================================================
//  Constructor & reset
// ============================================================

Simulation::Simulation(SimConfig cfg) : m_cfg(cfg)
{
    m_broadPhase.setCellSize(cfg.collisionRadius * 2.f);
    reset();
}

void Simulation::reset()
{
    m_particles.clear();
    m_constraints.clear();
    m_rigidBodies.clear();
    m_mouse     = MouseConstraint{};
    m_sphere    = SphereCollider{m_cfg.sphereCenter, m_cfg.sphereRadius};
    m_tornCount = 0;
    m_accumDt   = 0.f;

    switch (m_cfg.mode)
    {
    case 0: buildSingleParticle(); break;
    case 1: buildStick();          break;
    case 2: buildRope();           break;
    case 3: buildCloth();          break;
    case 4: buildRigidStack();     break;
    case 5: buildSoftBody();       break;
    default: buildRigidStack();    break;
    }
}

// ============================================================
//  Low-level helpers
// ============================================================

int Simulation::addParticle(glm::vec3 pos, float invMass)
{
    int idx = (int)m_particles.size();
    m_particles.push_back(Particle::at(pos, invMass));
    return idx;
}

void Simulation::addDistConstraint(int i, int j, float compliance, float breakThresh)
{
    DistanceConstraint c;
    c.i = i; c.j = j;
    c.restLength     = glm::distance(m_particles[i].position, m_particles[j].position);
    c.compliance     = compliance;
    c.breakThreshold = breakThresh;
    m_constraints.push_back(c);
}

// ============================================================
//  addBox
//
//  Creates a rigid body shaped like a box with:
//    - 4 corner particles
//    - 4 edge distance constraints
//    - 2 diagonal distance constraints (prevent shear)
//
//  Returns the index of the new RigidBody in m_rigidBodies.
//
//  The shape-matching in RigidBody::projectShape() will keep
//  the box rigid every frame on top of these constraints.
//  The constraints give initial structural resistance while
//  shape-matching gives the final rigidity correction.
// ============================================================
int Simulation::addBox(glm::vec3 center, float halfW, float halfH,
                       float stiffness, bool isStatic)
{
    float invM = isStatic ? 0.f : 1.f;

    // Corner particles: top-left, top-right, bottom-right, bottom-left
    int tl = addParticle(center + glm::vec3(-halfW,  halfH, 0.f), invM);
    int tr = addParticle(center + glm::vec3( halfW,  halfH, 0.f), invM);
    int br = addParticle(center + glm::vec3( halfW, -halfH, 0.f), invM);
    int bl = addParticle(center + glm::vec3(-halfW, -halfH, 0.f), invM);

    // Edge constraints (compliance=0 = rigid edges)
    addDistConstraint(tl, tr, 0.f);
    addDistConstraint(tr, br, 0.f);
    addDistConstraint(br, bl, 0.f);
    addDistConstraint(bl, tl, 0.f);

    // Diagonal constraints (prevent the box from collapsing)
    addDistConstraint(tl, br, 0.f);
    addDistConstraint(tr, bl, 0.f);

    // Build rigid body
    RigidBody rb;
    rb.particleIndices = {tl, tr, br, bl};
    rb.shapeStiffness  = stiffness;
    rb.isStatic        = isStatic;
    rb.computeRestOffsets(m_particles);
    m_rigidBodies.push_back(std::move(rb));

    return (int)m_rigidBodies.size() - 1;
}

// ============================================================
//  Scene builders
// ============================================================

void Simulation::buildSingleParticle()
{
    addParticle({0.f, 2.f, 0.f});
}

void Simulation::buildStick()
{
    int a = addParticle({-1.f, 2.f, 0.f});
    int b = addParticle({ 1.f, 2.f, 0.f});
    addDistConstraint(a, b, 0.f);
}

void Simulation::buildRope()
{
    const int   N   = m_cfg.ropeSegments;
    const float seg = m_cfg.ropeSegLen;
    for (int i = 0; i < N; ++i)
        addParticle({0.f, 4.f - i*seg, 0.f}, i==0 ? 0.f : 1.f);
    float breakT = m_cfg.tearEnabled ? m_cfg.breakThreshold : 1e9f;
    for (int i = 0; i < N-1; ++i)
        addDistConstraint(i, i+1, 0.f, breakT);
}

void Simulation::buildCloth()
{
    const int   rows = m_cfg.clothRows;
    const int   cols = m_cfg.clothCols;
    const float sp   = m_cfg.clothSpacing;
    const float sx   = -(cols-1)*sp*0.5f;
    const float sy   = 4.5f;
    const float comp = m_cfg.clothCompliance;

    for (int r=0;r<rows;++r)
    for (int c=0;c<cols;++c)
    {
        bool pin = (r==0)&&(m_cfg.clothPinMode==1||c==0||c==cols-1);
        addParticle({sx+c*sp, sy-r*sp, 0.f}, pin?0.f:1.f);
    }

    auto idx=[&](int r,int c){return r*cols+c;};
    float sBreak = m_cfg.tearEnabled ? m_cfg.breakThreshold : 1e9f;
    float oBreak = (m_cfg.tearEnabled&&!m_cfg.tearStructuralOnly) ? m_cfg.breakThreshold : 1e9f;

    for(int r=0;r<rows;++r) for(int c=0;c<cols;++c)
    {
        if(c+1<cols) addDistConstraint(idx(r,c),idx(r,c+1),  comp, sBreak);
        if(r+1<rows) addDistConstraint(idx(r,c),idx(r+1,c),  comp, sBreak);
    }
    if(m_cfg.clothShear)
    {
        for(int r=0;r<rows-1;++r) for(int c=0;c<cols-1;++c)
        {
            addDistConstraint(idx(r,c),  idx(r+1,c+1), comp, oBreak);
            addDistConstraint(idx(r,c+1),idx(r+1,c),   comp, oBreak);
        }
    }
    if(m_cfg.clothBend)
    {
        for(int r=0;r<rows;++r) for(int c=0;c<cols;++c)
        {
            if(c+2<cols) addDistConstraint(idx(r,c),idx(r,c+2),   comp, oBreak);
            if(r+2<rows) addDistConstraint(idx(r,c),idx(r+2,c),   comp, oBreak);
            if(r+2<rows&&c+2<cols) addDistConstraint(idx(r,c),idx(r+2,c+2),comp,oBreak);
        }
    }
}

// ============================================================
//  buildRigidStack
//
//  Creates a pyramid of boxes to demonstrate rigid body stacking,
//  collisions, and shape matching all at once.
//
//  Layout (5 boxes across bottom, then 4, 3, 2, 1):
//      []
//     [][] 
//    [][][]
//   [][][][]
//  [][][][][]
//
//  A static floor box is added at the bottom so boxes land
//  on something solid rather than the floor line.
// ============================================================
void Simulation::buildRigidStack()
{
    const float h     = m_cfg.rigidSize;   // half-extent
    const float gap   = 0.05f;             // small gap to avoid initial overlap
    const float step  = h * 2.f + gap;
    const float stiff = m_cfg.rigidStiffness;

    // Static floor platform
    addBox({0.f, m_cfg.floorY + h, 0.f}, 8.f, h, 1.f, true);

    // Drop N boxes scattered above, they'll fall and stack
    const int N = m_cfg.rigidCount;
    for (int i = 0; i < N; ++i)
    {
        float x = ((float)i - (N-1)*0.5f) * step * 0.7f;
        float y = 2.f + (float)(i % 3) * step * 1.5f;
        addBox({x, y, 0.f}, h, h, stiff, false);
    }
}

// ============================================================
//  buildSoftBody
//
//  A soft body is a closed mesh of particles where:
//  - Edge constraints keep the shape (with soft compliance)
//  - A volume conservation constraint (approximated here as
//    diagonal cross constraints with higher compliance) keeps
//    the body from collapsing
//
//  We build a simple grid mesh that forms a squishy blob.
//  The compliance value controls squishiness:
//    1e-6 = slightly soft
//    1e-3 = very squishy
//    1e-1 = nearly liquid
// ============================================================
void Simulation::buildSoftBody()
{
    const int   N    = 6;    // grid size N×N
    const float sp   = 0.5f;
    const float comp = m_cfg.softCompliance;
    const float sx   = -(N-1)*sp*0.5f;
    const float sy   = 2.f;

    // Create particle grid
    for(int r=0;r<N;++r) for(int c=0;c<N;++c)
        addParticle({sx+c*sp, sy-r*sp, 0.f});

    auto idx=[&](int r,int c){return r*N+c;};

    // Structural edges (soft)
    for(int r=0;r<N;++r) for(int c=0;c<N;++c)
    {
        if(c+1<N) addDistConstraint(idx(r,c),idx(r,c+1),comp);
        if(r+1<N) addDistConstraint(idx(r,c),idx(r+1,c),comp);
    }
    // Shear (slightly stiffer than structural)
    float shearComp = comp * 0.5f;
    for(int r=0;r<N-1;++r) for(int c=0;c<N-1;++c)
    {
        addDistConstraint(idx(r,c),  idx(r+1,c+1), shearComp);
        addDistConstraint(idx(r,c+1),idx(r+1,c),   shearComp);
    }
    // Long-range volume-like constraints (skip-2 diagonals)
    // These resist compression and give the body volume
    float volComp = comp * 0.25f;
    for(int r=0;r<N;++r) for(int c=0;c<N;++c)
    {
        if(c+2<N) addDistConstraint(idx(r,c),idx(r,c+2),  volComp);
        if(r+2<N) addDistConstraint(idx(r,c),idx(r+2,c),  volComp);
        if(r+2<N&&c+2<N) addDistConstraint(idx(r,c),idx(r+2,c+2),volComp);
        if(r+2<N&&c-2>=0) addDistConstraint(idx(r,c),idx(r+2,c-2),volComp);
    }

    // Wrap in a rigid body with LOW stiffness so it shape-matches softly
    RigidBody rb;
    for(int i=0;i<N*N;++i) rb.particleIndices.push_back(i);
    rb.shapeStiffness = 0.15f; // very low = very squishy
    rb.computeRestOffsets(m_particles);
    m_rigidBodies.push_back(std::move(rb));
}

// ============================================================
//  Main step loop — fixed timestep accumulator
//
//  XPBD requires a consistent dt for compliance to be
//  physically meaningful.  We use a fixed timestep of
//  cfg.dt (default 1/60s) with an accumulator:
//
//    accumulator += wallDt
//    while accumulator >= fixedDt:
//        tickOnce(fixedDt)
//        accumulator -= fixedDt
//
//  This means the simulation always runs at exactly cfg.dt
//  regardless of monitor refresh rate or frame time spikes.
//  If the CPU can't keep up, we cap at 3 steps per frame to
//  avoid the "spiral of death" (slow frame → more steps →
//  even slower frame).
// ============================================================
void Simulation::step(float wallDt)
{
    m_accumDt += wallDt;
    const float fixedDt = m_cfg.dt;
    const int   maxSteps = 3;
    int steps = 0;

    while (m_accumDt >= fixedDt && steps < maxSteps)
    {
        tickOnce(fixedDt);
        m_accumDt -= fixedDt;
        ++steps;
    }
}

void Simulation::tickOnce(float dt)
{
    m_sphere.center = m_cfg.sphereCenter;
    m_sphere.radius = m_cfg.sphereRadius;
    m_broadPhase.setCellSize(m_cfg.collisionRadius * 2.f);

    applyExternalForces(dt);
    predictPositions(dt);
    resetLambdas();

    for (int iter = 0; iter < m_cfg.solverIterations; ++iter)
    {
        solveConstraints(dt);
        projectRigidBodies();
        m_mouse.project(m_particles);
    }

    resolveParticleCollisions();
    resolveCollisions();
    removeTornConstraints();
    updateVelocitiesAndPositions(dt);
}

// ============================================================
//  PBD substeps
// ============================================================

void Simulation::applyExternalForces(float dt)
{
    for (auto& p : m_particles)
    {
        if (p.invMass == 0.f) continue;
        p.velocity += m_cfg.gravity * dt;
        p.velocity *= m_cfg.damping;
    }
}

void Simulation::predictPositions(float dt)
{
    for (auto& p : m_particles)
        p.predicted = p.position + p.velocity * dt;
}

// XPBD: reset accumulated lambda at the start of each frame
void Simulation::resetLambdas()
{
    for (auto& c : m_constraints)
        c.resetLambda();
}

void Simulation::solveConstraints(float dt)
{
    for (auto& c : m_constraints)
        c.project(m_particles, dt);
}

void Simulation::projectRigidBodies()
{
    for (const auto& rb : m_rigidBodies)
        rb.projectShape(m_particles);
}

// ============================================================
//  resolveParticleCollisions
//
//  Uses the broad phase to find pairs of particles that are
//  too close and pushes them apart.  This is what makes rigid
//  bodies collide with each other — the corner particles of
//  one box push against the corner particles of another.
//
//  This is a simplified approach (point vs point, not full
//  polygon vs polygon).  It works well for boxes because the
//  corner particles are the first things to touch.
// ============================================================
void Simulation::resolveParticleCollisions()
{
    if (!m_cfg.particleCollision) return;

    m_broadPhase.build(m_particles);

    const float r  = m_cfg.collisionRadius;
    const float r2 = r * r;

    m_broadPhase.queryPairs(m_particles, r * 2.f,
        [&](int i, int j)
        {
            Particle& a = m_particles[i];
            Particle& b = m_particles[j];

            glm::vec3 d    = b.predicted - a.predicted;
            float     dist = glm::length(d);
            if (dist < 1e-7f || dist >= r * 2.f) return;

            float     overlap = r * 2.f - dist;
            glm::vec3 n       = d / dist;
            float     wSum    = a.invMass + b.invMass;
            if (wSum < 1e-7f) return;

            a.predicted -= (a.invMass / wSum) * overlap * n;
            b.predicted += (b.invMass / wSum) * overlap * n;
        });
}

void Simulation::resolveCollisions()
{
    for (auto& p : m_particles)
    {
        if (m_cfg.sphereEnabled)
            for (int k = 0; k < 3; ++k)
                m_sphere.resolveParticle(p);

        if (p.predicted.y < m_cfg.floorY)
        {
            p.predicted.y = m_cfg.floorY;
            if (p.velocity.y < 0.f)
                p.velocity.y = -p.velocity.y * m_cfg.restitution;
        }
    }
}

void Simulation::removeTornConstraints()
{
    auto it = std::remove_if(m_constraints.begin(), m_constraints.end(),
        [this](const DistanceConstraint& c)
        { if (c.torn) { ++m_tornCount; return true; } return false; });
    m_constraints.erase(it, m_constraints.end());
}

void Simulation::updateVelocitiesAndPositions(float dt)
{
    const float invDt = 1.f / dt;
    for (auto& p : m_particles)
    {
        glm::vec3 nv = (p.predicted - p.position) * invDt;
        if (p.velocity.y > 0.f && nv.y <= 0.f) nv.y = p.velocity.y;
        p.velocity = nv;
        p.position = p.predicted;
    }
}

// ============================================================
//  Mouse drag
// ============================================================

void Simulation::beginDrag(glm::vec2 wp)
{
    int i = nearestParticle(wp, 2.f);
    if (i < 0) return;
    m_mouse = {true, i, {wp.x, wp.y, 0.f}, 0.8f};
}
void Simulation::updateDrag(glm::vec2 wp)
{ if (m_mouse.active) m_mouse.target = {wp.x, wp.y, 0.f}; }
void Simulation::endDrag() { m_mouse = {}; }

int Simulation::nearestParticle(glm::vec2 wp, float maxD) const
{
    int best=-1; float bestD=maxD*maxD;
    for (int i=0;i<(int)m_particles.size();++i)
    {
        glm::vec2 pp={m_particles[i].position.x, m_particles[i].position.y};
        float d=glm::dot(pp-wp,pp-wp);
        if(d<bestD){bestD=d;best=i;}
    }
    return best;
}
