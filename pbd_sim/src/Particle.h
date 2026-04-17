#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <functional>

struct Particle
{
    glm::vec3 position  = {0.f, 0.f, 0.f};
    glm::vec3 predicted = {0.f, 0.f, 0.f};
    glm::vec3 velocity  = {0.f, 0.f, 0.f};
    float     invMass   = 1.f;

    static Particle at(glm::vec3 pos, float invMass = 1.f)
    {
        Particle p;
        p.position = p.predicted = pos;
        p.invMass  = invMass;
        return p;
    }
};

// ============================================================
//  DistanceConstraint  (XPBD version)
//
//  Vanilla PBD has a known flaw: stiffness is entangled with
//  both dt and solverIterations.  Run at 60fps vs 30fps and
//  the cloth behaves differently.  XPBD (Extended PBD) fixes
//  this by introducing a `compliance` parameter (alpha) that
//  is the physical inverse of stiffness, independent of dt.
//
//  compliance = 0         →  perfectly rigid  (same as stiffness=1 in vanilla)
//  compliance = 1e-7      →  very stiff cloth
//  compliance = 1e-4      →  soft/stretchy
//  compliance = 1.0       →  very compliant (jelly)
//
//  Each constraint accumulates a Lagrange multiplier `lambda`
//  over the solver iterations within one timestep.
//  lambda must be reset to 0 at the start of each frame.
//
//  The XPBD update rule:
//    alpha_tilde = compliance / (dt * dt)
//    delta_lambda = (-C - alpha_tilde * lambda) / (w_sum + alpha_tilde)
//    lambda += delta_lambda
//    correction = delta_lambda * gradient
// ============================================================
struct DistanceConstraint
{
    int   i, j;
    float restLength;
    float compliance     = 0.f;     // 0 = rigid, > 0 = soft
    float breakThreshold = 1e9f;
    bool  torn           = false;

    // Accumulated Lagrange multiplier — reset each frame
    mutable float lambda = 0.f;

    void resetLambda() const { lambda = 0.f; }

    void project(std::vector<Particle>& particles, float dt) const
    {
        if (torn) return;

        Particle& a = particles[i];
        Particle& b = particles[j];
        glm::vec3 d = b.predicted - a.predicted;
        float len = glm::length(d);
        if (len < 1e-7f) return;

        if (len > restLength * breakThreshold)
        {
            const_cast<DistanceConstraint*>(this)->torn = true;
            return;
        }

        float C          = len - restLength;
        float wSum       = a.invMass + b.invMass;
        if (wSum < 1e-7f) return;

        float alphaTilde = compliance / (dt * dt);
        float dLambda    = (-C - alphaTilde * lambda) / (wSum + alphaTilde);
        lambda          += dLambda;

        glm::vec3 n = d / len;
        a.predicted -= a.invMass * dLambda * n;
        b.predicted += b.invMass * dLambda * n;
    }
};

// ============================================================
//  MouseConstraint
// ============================================================
struct MouseConstraint
{
    bool      active        = false;
    int       particleIndex = -1;
    glm::vec3 target        = {0.f, 0.f, 0.f};
    float     stiffness     = 0.8f;

    void project(std::vector<Particle>& particles) const
    {
        if (!active || particleIndex < 0) return;
        Particle& p = particles[particleIndex];
        if (p.invMass == 0.f) return;
        p.predicted += (target - p.predicted) * stiffness;
    }
};

// ============================================================
//  SphereCollider
// ============================================================
struct SphereCollider
{
    glm::vec3 center = {0.f, 0.f, 0.f};
    float     radius = 1.f;

    void resolveParticle(Particle& p) const
    {
        glm::vec3 delta = p.predicted - center;
        float dist = glm::length(delta);
        if (dist < 1e-7f) { p.predicted = center + glm::vec3(0.f, radius, 0.f); return; }
        if (dist < radius)
            p.predicted = center + (delta / dist) * radius;
    }
};

// ============================================================
//  RigidBody
//
//  A rigid body is a set of particles whose relative positions
//  are kept fixed by shape-matching.  Rather than adding
//  individual distance constraints between every pair of
//  particles (which is expensive and can still deform), we use
//  the shape-matching approach:
//
//  Each frame, after the constraint solver runs:
//    1. Compute the current centroid of the body's particles
//    2. Compute the best-fit rotation R that maps the rest
//       shape onto the current deformed shape (polar decomposition)
//    3. Pull each particle toward its "goal" position:
//         goal_i = centroid + R * restOffset_i
//       where restOffset_i is the particle's offset from the
//       centroid in the original rest pose.
//
//  This gives us a rigid body that:
//    - Falls and slides under gravity (particles integrate freely)
//    - Resists deformation (shape matching pulls it back)
//    - Can collide with the floor and sphere (particles are resolved)
//    - Can be made "soft" by reducing shapeStiffness (squishy!)
//
//  The body also tracks angular state so it can spin properly.
//
//  Particle indices are into the global particle array.
// ============================================================
struct RigidBody
{
    std::vector<int>       particleIndices; // indices into global particle vector
    std::vector<glm::vec3> restOffsets;     // offset from centroid in rest pose
    float                  shapeStiffness = 0.95f; // 0=jelly, 1=rigid
    float                  invMass        = 1.f;   // per-body (not per-particle)
    bool                   isStatic       = false;

    // Called once after particles are added — computes rest offsets
    void computeRestOffsets(const std::vector<Particle>& particles)
    {
        glm::vec3 centroid(0.f);
        for (int idx : particleIndices)
            centroid += particles[idx].position;
        centroid /= (float)particleIndices.size();

        restOffsets.clear();
        for (int idx : particleIndices)
            restOffsets.push_back(particles[idx].position - centroid);
    }

    // Shape matching — projects particles toward the rigid shape
    // Uses a simplified 2D rotation extraction (suitable for our 2D sim)
    void projectShape(std::vector<Particle>& particles) const
    {
        if (isStatic) return;
        const int N = (int)particleIndices.size();
        if (N == 0) return;

        // Step 1: current centroid of predicted positions
        glm::vec3 centroid(0.f);
        for (int idx : particleIndices)
            centroid += particles[idx].predicted;
        centroid /= (float)N;

        // Step 2: compute best-fit 2D rotation using polar decomposition
        // We build the covariance matrix A = sum(q_i * p_i^T)
        // where p_i = restOffset, q_i = current offset from centroid
        // Then extract rotation angle from A.
        float Axx = 0.f, Axy = 0.f, Ayx = 0.f, Ayy = 0.f;
        for (int k = 0; k < N; ++k)
        {
            int idx = particleIndices[k];
            glm::vec3 q = particles[idx].predicted - centroid;
            const glm::vec3& p = restOffsets[k];

            Axx += q.x * p.x;
            Axy += q.x * p.y;
            Ayx += q.y * p.x;
            Ayy += q.y * p.y;
        }

        // Rotation angle from atan2 of the covariance matrix
        float angle = atan2f(Ayx - Axy, Axx + Ayy);
        float cosA  = cosf(angle);
        float sinA  = sinf(angle);

        // Step 3: pull each particle toward its goal position
        for (int k = 0; k < N; ++k)
        {
            int idx = particleIndices[k];
            const glm::vec3& r = restOffsets[k];

            // Rotate rest offset by best-fit rotation
            glm::vec3 goal;
            goal.x = centroid.x + cosA * r.x - sinA * r.y;
            goal.y = centroid.y + sinA * r.x + cosA * r.y;
            goal.z = centroid.z + r.z;

            // Blend predicted toward goal
            particles[idx].predicted +=
                (goal - particles[idx].predicted) * shapeStiffness;
        }
    }
};
