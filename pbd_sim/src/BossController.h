#pragma once
#include "Hazard.h"
#include "Player.h"
#include "GameState.h"
#include "Particle.h"
#include <glm/glm.hpp>
#include <vector>
#include <functional>

// ============================================================
//  BossPhase — which attack the boss is currently doing
// ============================================================
enum class BossPhase
{
    Idle,
    BoulderThrow,   // launches a rolling boulder
    GroundStomp,    // shockwave along floor
    Crumble,        // weakens platform tearing threshold
    Rage            // all attacks at once, faster
};

// ============================================================
//  BossController
//
//  Controls the Stone Golem boss fight.
//
//  The boss stands at the center of the arena and cycles
//  through attacks.  Each attack is a distinct phase that
//  lasts a fixed duration before transitioning to the next.
//
//  Attack rotation (cycles every 15s, 10s in rage):
//    Boulder → Stomp → Crumble → Boulder → ...
//
//  Rage phase triggers at 45s survived.  In rage:
//    - Boulder throw fires 2 boulders simultaneously
//    - Shockwave travels faster
//    - Crumble tears platforms faster
//
//  Win condition: survive until bossTimer >= surviveDuration
// ============================================================
class BossController
{
public:
    // Arena bounds — set when boss level loads
    float arenaLeft  = -8.f;
    float arenaRight =  8.f;
    float arenaFloorY = -5.f;

    // Boss visual position (center, does not use particles)
    glm::vec2 position = {0.f, -3.f};
    float     size     = 1.8f;   // half-extent for drawing

    // State
    BossPhase phase       = BossPhase::Idle;
    float     phaseTimer  = 0.f;   // time in current phase
    float     phaseDur    = 3.f;   // duration of current phase
    bool      active      = false;

    // Active hazards
    std::vector<Shockwave> shockwaves;
    std::vector<Boulder>   boulders;

    // Crumble state — indices of particles to make tearable
    std::vector<int> crumbleConstraintStart;  // start indices of constraints to weaken

    // Boss animation
    float animTime    = 0.f;
    float shakeAmount = 0.f;  // screen shake request this frame

    // ---- API ----

    void start(float floorY, float left, float right)
    {
        arenaFloorY = floorY;
        arenaLeft   = left;
        arenaRight  = right;
        position    = {(left+right)*0.5f, floorY + size + 0.5f};
        active      = true;
        phase       = BossPhase::Idle;
        phaseTimer  = 0.f;
        phaseDur    = 2.f;  // short idle at start
    }

    void stop() { active = false; shockwaves.clear(); boulders.clear(); }

    // Call every frame — returns true if boss requests screen shake
    bool update(float bossTimer, float surviveDur,
                glm::vec2 playerPos,
                std::vector<Particle>& particles,
                std::vector<DistanceConstraint>& constraints,
                float dt);

    // Check if player is hit by any active hazard
    // Returns true if player takes a hit
    bool checkPlayerHit(glm::vec2 playerPos, float playerRadius) const;

    // Remove dead hazards and their particles
    void cleanup(std::vector<Particle>& particles,
                 std::vector<DistanceConstraint>& constraints);

private:
    void transitionPhase(bool rage, glm::vec2 playerPos,
                         std::vector<Particle>& particles,
                         std::vector<DistanceConstraint>& constraints);

    void launchBoulder(glm::vec2 playerPos,
                       std::vector<Particle>& particles,
                       std::vector<DistanceConstraint>& constraints,
                       float speedMult = 1.f);

    void launchShockwave(bool fromLeft);

    int  m_attackIndex = 0;  // cycles through attacks
    int  m_boulderBase = -1; // track where boulders start in particle array
};
