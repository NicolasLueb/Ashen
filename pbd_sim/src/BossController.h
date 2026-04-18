#pragma once
#include "Hazard.h"
#include "Player.h"
#include "Biome.h"
#include "Particle.h"
#include <glm/glm.hpp>
#include <vector>

// ============================================================
//  BossType — which boss is active
// ============================================================
enum class BossType { Golem, Wraith, Dragon };

// ============================================================
//  BossAttack — current attack phase
// ============================================================
enum class BossAttack
{
    Idle,
    // Golem attacks
    GolemStomp,       // ground shockwave + platform crack
    GolemBoulder,     // slow heavy boulder thrown
    GolemGroundSlam,  // massive AoE covering 60% of arena
    // Wraith attacks
    WraithTeleport,   // teleports to new position
    WraithOrbs,       // 3 orbs from different angles simultaneously
    WraithShadow,     // rapid dark orb spray
    // Dragon attacks
    DragonBreath,     // fire column sweeping floor to ceiling
    DragonTail,       // tail closes off half arena
    DragonDive,       // dives through arena requiring grapple to dodge
    DragonRage        // all attacks at once
};

// ============================================================
//  BossController
//  Three completely distinct bosses with unique attacks,
//  appearances, and difficulty curves.
// ============================================================
class BossController
{
public:
    // Arena bounds
    float arenaLeft  = -9.f;
    float arenaRight =  9.f;
    float arenaFloorY = -4.5f;
    float arenaCeilY  =  8.f;

    // Boss state
    BossType  type       = BossType::Golem;
    BossAttack attack    = BossAttack::Idle;
    glm::vec2 position   = {0.f, -3.f};
    float     size       = 1.8f;
    float     animTime   = 0.f;
    float     phaseTimer = 0.f;
    float     phaseDur   = 3.f;
    float     shakeAmount= 0.f;
    bool      active     = false;
    bool      rage       = false;
    int       attackIndex = 0;

    // Golem specific
    float     golemWindupTimer = 0.f;   // wind-up before slam
    bool      golemInWindup    = false;
    glm::vec2 golemSlamPos     = {0.f,0.f};

    // Wraith specific
    glm::vec2 wraithTarget    = {0.f,0.f};  // teleport destination
    float     wraithAlpha     = 1.f;         // 0=invisible, 1=solid
    float     wraithTeleTimer = 0.f;

    // Dragon specific
    float     dragonBreathX   = 0.f;   // current breath column x
    float     dragonBreathDir = 1.f;
    bool      dragonTailLeft  = true;
    float     dragonTailX     = 0.f;

    // Active hazards
    std::vector<Shockwave> shockwaves;
    std::vector<Boulder>   boulders;

    // Generic "orb" projectiles (Wraith + Dragon)
    struct Orb {
        glm::vec2 pos, vel;
        float     radius = 0.25f;
        float     age    = 0.f;
        float     life   = 5.f;
        bool      alive  = true;
        void update(float dt){ pos+=vel*dt; age+=dt; if(age>life)alive=false; }
        bool kills(glm::vec2 p, float r) const {
            if(!alive)return false;
            glm::vec2 d=p-pos; return glm::dot(d,d)<(radius+r)*(radius+r);
        }
    };
    std::vector<Orb> orbs;

    // Dragon breath columns
    struct BreathColumn {
        float x, width;
        float age=0.f, life=1.2f;
        bool alive=true;
        void update(float dt){age+=dt;if(age>life)alive=false;}
        bool kills(glm::vec2 p) const {
            if(!alive)return false;
            return p.x>x-width&&p.x<x+width;
        }
    };
    std::vector<BreathColumn> breathCols;

    // Dragon tail blocker
    struct TailBlock {
        float x, yMin, yMax;
        float age=0.f, life=2.5f;
        bool alive=true;
        int  side=-1;  // -1=blocks left half, +1=blocks right half
        void update(float dt){age+=dt;if(age>life)alive=false;}
        bool kills(glm::vec2 p) const {
            if(!alive)return false;
            if(side==-1) return p.x<x;
            return p.x>x;
        }
    };
    std::vector<TailBlock> tailBlocks;

    // ---- API ----
    void start(BossType t, float floorY, float left, float right, float ceilY);
    void stop();
    bool update(float bossTimer, float surviveDur, glm::vec2 playerPos,
                std::vector<Particle>& particles,
                std::vector<DistanceConstraint>& constraints,
                float dt);
    bool checkPlayerHit(glm::vec2 playerPos, float playerRadius) const;
    void cleanup(std::vector<Particle>& particles,
                 std::vector<DistanceConstraint>& constraints);

private:
    void updateGolem(float bossTimer, float surviveDur, glm::vec2 playerPos,
                     std::vector<Particle>& particles,
                     std::vector<DistanceConstraint>& constraints, float dt);
    void updateWraith(float bossTimer, float surviveDur, glm::vec2 playerPos, float dt);
    void updateDragon(float bossTimer, float surviveDur, glm::vec2 playerPos, float dt);

    void golemTransition(glm::vec2 playerPos,
                         std::vector<Particle>& particles,
                         std::vector<DistanceConstraint>& constraints);
    void wraithTransition(glm::vec2 playerPos);
    void dragonTransition(glm::vec2 playerPos);

    void launchBoulder(glm::vec2 playerPos,
                       std::vector<Particle>& particles,
                       std::vector<DistanceConstraint>& constraints, float speedMult=1.f);
    void launchShockwave(bool fromLeft, float speedMult=1.f);
    void launchOrb(glm::vec2 from, glm::vec2 vel);
    void fireBreath(float x, float width);

    float frand() const { return (float)rand() / RAND_MAX; }
};
