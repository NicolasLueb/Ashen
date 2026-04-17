#include "BossController.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>

// ============================================================
//  update — main boss tick
// ============================================================
bool BossController::update(
    float bossTimer, float surviveDur,
    glm::vec2 playerPos,
    std::vector<Particle>& particles,
    std::vector<DistanceConstraint>& constraints,
    float dt)
{
    if (!active) return false;

    animTime   += dt;
    shakeAmount = 0.f;

    // Check rage threshold (75% of survive duration)
    bool rage = (bossTimer >= surviveDur * 0.75f);

    // Update all shockwaves
    for (auto& sw : shockwaves)
    {
        sw.update(dt);
        // Kill shockwave when it exits arena
        if (sw.x < arenaLeft - 3.f || sw.x > arenaRight + 3.f)
            sw.alive = false;
    }
    shockwaves.erase(
        std::remove_if(shockwaves.begin(), shockwaves.end(),
            [](const Shockwave& s){ return !s.alive; }),
        shockwaves.end());

    // Update boulder lifetimes
    for (auto& b : boulders)
        b.update(dt);

    // ---- Phase state machine ----
    phaseTimer += dt;
    if (phaseTimer >= phaseDur)
    {
        phaseTimer = 0.f;
        transitionPhase(rage, playerPos, particles, constraints);
    }

    // Boss "breathing" bob
    position.y = arenaFloorY + size + 0.5f
                 + sinf(animTime * 1.2f) * 0.08f;

    // Rage visual shake
    if (rage)
    {
        position.x = (arenaLeft+arenaRight)*0.5f
                     + sinf(animTime * 18.f) * 0.06f;
    }

    return shakeAmount > 0.f;
}

// ============================================================
//  transitionPhase — move to next attack
// ============================================================
void BossController::transitionPhase(
    bool rage, glm::vec2 playerPos,
    std::vector<Particle>& particles,
    std::vector<DistanceConstraint>& constraints)
{
    // Attack sequence: 0=Boulder, 1=Stomp, 2=Crumble
    // In rage we add extra boulders
    const int ATTACKS = 3;
    m_attackIndex = (m_attackIndex + 1) % ATTACKS;

    float speedMult = rage ? 1.6f : 1.f;

    switch (m_attackIndex)
    {
    // ---- Boulder throw ----
    case 0:
        phase    = BossPhase::BoulderThrow;
        phaseDur = rage ? 6.f : 8.f;
        launchBoulder(playerPos, particles, constraints, speedMult);
        if (rage)
        {
            // Second boulder from opposite side in rage
            glm::vec2 fakeTarget = {
                arenaLeft + (arenaRight - arenaLeft) * 0.3f,
                playerPos.y
            };
            launchBoulder(fakeTarget, particles, constraints, speedMult * 1.2f);
        }
        shakeAmount = 0.3f;
        break;

    // ---- Ground stomp / shockwave ----
    case 1:
        phase    = BossPhase::GroundStomp;
        phaseDur = rage ? 4.f : 5.f;
        // Launch from both sides in rage
        launchShockwave(true);
        if (rage) launchShockwave(false);
        shakeAmount = 0.6f;
        break;

    // ---- Crumble platforms ----
    case 2:
        phase    = BossPhase::Crumble;
        phaseDur = rage ? 5.f : 7.f;
        // Lower tearing threshold on all constraints
        // (platforms crumble under the player's weight)
        for (auto& c : constraints)
        {
            if (c.breakThreshold > 1e8f)  // was indestructible
            {
                c.breakThreshold = rage ? 1.25f : 1.45f;
            }
        }
        shakeAmount = 0.2f;
        break;
    }

    // Idle phase between attacks
    if (phase == BossPhase::Idle)
        phaseDur = rage ? 1.f : 2.f;
}

// ============================================================
//  launchBoulder
//
//  Creates a 4-particle rigid box in the particle array,
//  launched from the boss position toward the player.
//  It uses the same physics engine as everything else — it
//  will collide with the floor, roll, and bounce naturally.
// ============================================================
void BossController::launchBoulder(
    glm::vec2 playerPos,
    std::vector<Particle>& particles,
    std::vector<DistanceConstraint>& constraints,
    float speedMult)
{
    Boulder boulder;
    boulder.particleBase = (int)particles.size();
    boulder.radius       = 0.55f;
    boulder.alive        = true;
    boulder.age          = 0.f;
    boulder.lifetime     = 10.f;

    float r = boulder.radius * 0.8f;

    // Direction toward player with some upward arc
    glm::vec2 dir = glm::normalize(playerPos - position);
    dir.y = glm::max(dir.y + 0.4f, 0.2f);  // always has some arc
    dir = glm::normalize(dir);

    float speed = 8.f * speedMult;
    glm::vec3 vel = {dir.x * speed, dir.y * speed, 0.f};

    // 4 corner particles
    glm::vec2 c = position + glm::vec2(0.f, size + r + 0.2f);
    int offsets[4][2] = {{-1,1},{1,1},{1,-1},{-1,-1}};
    for (auto& o : offsets)
    {
        Particle p = Particle::at(
            {c.x + o[0]*r, c.y + o[1]*r, 0.f}, 1.f);
        p.velocity = vel;
        particles.push_back(p);
    }

    int b = boulder.particleBase;
    // Edge constraints
    auto addC = [&](int i, int j){
        DistanceConstraint dc;
        dc.i = b+i; dc.j = b+j;
        dc.restLength = glm::distance(particles[b+i].position,
                                      particles[b+j].position);
        dc.compliance = 0.f;
        dc.breakThreshold = 1e9f;
        constraints.push_back(dc);
    };
    addC(0,1); addC(1,2); addC(2,3); addC(3,0);
    addC(0,2); addC(1,3); // diagonals

    boulders.push_back(boulder);
}

// ============================================================
//  launchShockwave
// ============================================================
void BossController::launchShockwave(bool fromLeft)
{
    Shockwave sw;
    sw.y     = arenaFloorY;
    sw.dir   = fromLeft ? 1.f : -1.f;
    sw.x     = fromLeft ? arenaLeft - 0.5f : arenaRight + 0.5f;
    sw.speed = 7.f;
    sw.width = 1.0f;
    sw.height = 1.8f;
    sw.alive = true;
    shockwaves.push_back(sw);
}

// ============================================================
//  checkPlayerHit
// ============================================================
bool BossController::checkPlayerHit(
    glm::vec2 playerPos, float playerRadius) const
{
    // Check shockwaves
    for (const auto& sw : shockwaves)
        if (sw.kills(playerPos)) return true;

    return false;
}

// ============================================================
//  cleanup — remove dead boulders from particle array
// ============================================================
void BossController::cleanup(
    std::vector<Particle>& particles,
    std::vector<DistanceConstraint>& constraints)
{
    // Remove boulders that have expired
    for (auto it = boulders.begin(); it != boulders.end();)
    {
        if (!it->alive && it->particleBase >= 0)
        {
            int base = it->particleBase;
            // Remove 4 particles starting at base
            if (base + 4 <= (int)particles.size())
            {
                particles.erase(particles.begin()+base,
                                particles.begin()+base+4);
                // Fix all constraint indices
                for (auto& c : constraints)
                {
                    if (c.i >= base) c.i -= 4;
                    if (c.j >= base) c.j -= 4;
                }
                // Fix other boulder bases
                for (auto& b2 : boulders)
                    if (b2.particleBase > base) b2.particleBase -= 4;
            }
            it = boulders.erase(it);
        }
        else ++it;
    }
}
