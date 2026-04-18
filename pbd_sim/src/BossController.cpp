#include "BossController.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void BossController::start(BossType t, float floorY, float left,
                            float right, float ceilY)
{
    type=t; arenaFloorY=floorY; arenaLeft=left;
    arenaRight=right; arenaCeilY=ceilY;
    active=true; attack=BossAttack::Idle;
    phaseTimer=0.f; phaseDur=2.f; attackIndex=0;
    rage=false; animTime=0.f; shakeAmount=0.f;
    orbs.clear(); breathCols.clear(); tailBlocks.clear();
    shockwaves.clear(); boulders.clear();
    golemInWindup=false; wraithAlpha=1.f;
    dragonBreathX=(left+right)*0.5f; dragonBreathDir=1.f;

    switch(t)
    {
    case BossType::Golem:
        position={(left+right)*0.5f, floorY+2.2f}; size=2.0f; break;
    case BossType::Wraith:
        position={(left+right)*0.5f, floorY+4.f};  size=1.3f; break;
    case BossType::Dragon:
        position={(left+right)*0.5f, ceilY-3.5f};  size=3.5f; break;
    }
}

void BossController::stop()
{
    active=false;
    orbs.clear(); breathCols.clear(); tailBlocks.clear();
    shockwaves.clear(); boulders.clear();
}

bool BossController::update(
    float bossTimer, float surviveDur, glm::vec2 playerPos,
    std::vector<Particle>& particles,
    std::vector<DistanceConstraint>& constraints,
    float dt)
{
    if (!active) return false;
    animTime+=dt; shakeAmount=0.f;
    rage=(bossTimer>=surviveDur*0.7f);

    // Update hazards
    for(auto& sw:shockwaves){sw.update(dt);
        if(sw.x<arenaLeft-4.f||sw.x>arenaRight+4.f)sw.alive=false;}
    shockwaves.erase(std::remove_if(shockwaves.begin(),shockwaves.end(),
        [](const Shockwave& s){return !s.alive;}),shockwaves.end());
    for(auto& b:boulders)b.update(dt);
    for(auto& o:orbs)    o.update(dt);
    for(auto& bc:breathCols)bc.update(dt);
    for(auto& tb:tailBlocks)tb.update(dt);
    orbs.erase(std::remove_if(orbs.begin(),orbs.end(),
        [](const Orb& o){return !o.alive;}),orbs.end());
    breathCols.erase(std::remove_if(breathCols.begin(),breathCols.end(),
        [](const BreathColumn& b){return !b.alive;}),breathCols.end());
    tailBlocks.erase(std::remove_if(tailBlocks.begin(),tailBlocks.end(),
        [](const TailBlock& t){return !t.alive;}),tailBlocks.end());

    switch(type)
    {
    case BossType::Golem:  updateGolem(bossTimer,surviveDur,playerPos,particles,constraints,dt); break;
    case BossType::Wraith: updateWraith(bossTimer,surviveDur,playerPos,dt); break;
    case BossType::Dragon: updateDragon(bossTimer,surviveDur,playerPos,dt); break;
    }
    return shakeAmount>0.f;
}

// ============================================================
//  GOLEM — anti-camping enforced by 5 distinct attacks:
//
//  1. Boulder — tracks player's exact X AND Y position
//  2. Stomp shockwave — FROM BOTH SIDES simultaneously
//  3. Platform slam — Golem JUMPS to player's platform height
//     and creates a shockwave at that exact Y level
//  4. Debris rain — spike objects fall from ceiling at
//     player's current X, forces movement
//  5. Ground slam — massive AoE covering 70% of arena
//
//  Ceiling at arenaCeilY prevents hiding at top.
//  All attacks track player Y so high platforms don't help.
// ============================================================
void BossController::updateGolem(
    float bossTimer, float surviveDur, glm::vec2 playerPos,
    std::vector<Particle>& particles,
    std::vector<DistanceConstraint>& constraints,
    float dt)
{
    // Golem bobs and faces player
    position.y=arenaFloorY+size+0.5f+sinf(animTime*0.9f)*0.05f;
    float targetX=(playerPos.x<(arenaLeft+arenaRight)*0.5f)
                  ? (arenaLeft+arenaRight)*0.5f-1.f
                  : (arenaLeft+arenaRight)*0.5f+1.f;
    position.x+=(targetX-position.x)*dt*1.5f;

    if(golemInWindup)
    {
        golemWindupTimer-=dt;
        float t=1.f-golemWindupTimer/2.f;
        position.x=(arenaLeft+arenaRight)*0.5f+sinf(animTime*18.f)*t*0.4f;
        shakeAmount=t*0.5f;

        if(golemWindupTimer<=0.f)
        {
            // Release — shockwaves at player's Y height AND floor
            // This is the anti-camping move
            float slamY=playerPos.y-0.8f; // slam at player level

            // Floor shockwaves
            for(int i=0;i<3;++i)
            {
                Shockwave sw;
                sw.y=arenaFloorY; sw.x=(arenaLeft+arenaRight)*0.5f;
                sw.dir=(i%2==0)?1.f:-1.f;
                sw.speed=7.f+i*2.f; sw.width=1.5f+i*0.4f; sw.height=2.2f;
                shockwaves.push_back(sw);
            }
            // Anti-camping shockwave at player's Y
            if(slamY>arenaFloorY+1.f)
            {
                Shockwave sw2;
                sw2.y=slamY; sw2.x=arenaLeft-1.f; sw2.dir=1.f;
                sw2.speed=rage?11.f:9.f; sw2.width=1.8f; sw2.height=1.6f;
                shockwaves.push_back(sw2);
            }
            if(rage)
            {
                Shockwave sw3;
                sw3.y=arenaFloorY; sw3.x=arenaRight+1.f; sw3.dir=-1.f;
                sw3.speed=11.f; sw3.width=1.3f; sw3.height=2.8f;
                shockwaves.push_back(sw3);
            }
            shakeAmount=1.3f;
            golemInWindup=false;
            phaseTimer=0.f; phaseDur=rage?4.f:6.f;
        }
        return;
    }

    phaseTimer+=dt;
    if(phaseTimer<phaseDur) return;
    phaseTimer=0.f;

    int numAttacks=rage?5:5;  // always use all 5 attacks
    attackIndex=(attackIndex+1)%numAttacks;

    switch(attackIndex)
    {
    case 0: // Boulder tracking exact player position
        attack=BossAttack::GolemBoulder;
        phaseDur=rage?4.f:7.f;
        launchBoulder(playerPos,particles,constraints,rage?1.6f:1.f);
        if(rage){
            // Second boulder from opposite side
            glm::vec2 sidePos={(playerPos.x<0)?arenaRight-1.f:arenaLeft+1.f,
                               playerPos.y+1.f};
            launchBoulder(sidePos,particles,constraints,1.3f);
        }
        shakeAmount=0.5f;
        break;

    case 1: // Stomp — shockwaves from both sides at once
        attack=BossAttack::GolemStomp;
        phaseDur=rage?3.5f:5.5f;
        launchShockwave(true,  rage?1.6f:1.f);
        launchShockwave(false, rage?1.6f:1.f);
        // Also launch one at player's Y height
        {
            Shockwave sw;
            sw.y=playerPos.y-0.6f; sw.x=arenaLeft-0.5f;
            sw.dir=1.f; sw.speed=rage?10.f:8.f;
            sw.width=1.2f; sw.height=1.5f;
            shockwaves.push_back(sw);
        }
        shakeAmount=0.9f;
        break;

    case 2: // Platform slam — jump to player's height
        attack=BossAttack::GolemGroundSlam;
        {
            // Shockwave spawns at player's current Y, not the floor
            float impactY=playerPos.y-0.5f;
            for(int side=0;side<2;++side)
            {
                Shockwave sw;
                sw.y=impactY;
                sw.x=(side==0)?arenaLeft-0.5f:arenaRight+0.5f;
                sw.dir=(side==0)?1.f:-1.f;
                sw.speed=rage?10.f:8.f;
                sw.width=1.4f; sw.height=1.8f;
                shockwaves.push_back(sw);
            }
            // Visual: Golem lurches toward player Y
            position.y=glm::clamp(playerPos.y-0.5f,
                                   arenaFloorY+size,
                                   arenaCeilY-size-0.5f);
        }
        phaseDur=rage?4.f:6.f;
        shakeAmount=0.8f;
        break;

    case 3: // Debris rain — falls at player's X position
        attack=BossAttack::GolemBoulder;  // reuse phase label
        phaseDur=rage?3.f:5.f;
        {
            // Drop 3-5 debris boulders from ceiling aimed at player X
            int count=rage?5:3;
            for(int i=0;i<count;++i)
            {
                float dropX=playerPos.x+(i-count/2)*0.8f+(frand()-0.5f)*1.5f;
                float dropY=arenaCeilY-0.5f;
                glm::vec2 dropTarget={dropX,arenaFloorY};
                // Launch from top, aimed straight down with slight spread
                Boulder b;
                b.particleBase=(int)particles.size();
                b.radius=0.45f; b.alive=true; b.age=0.f; b.lifetime=8.f;
                float r2=b.radius*0.7f;
                glm::vec3 vel={
                    (dropTarget.x-dropX)*0.3f,
                    -(8.f+frand()*4.f)*(rage?1.4f:1.f),
                    0.f
                };
                int offsets[4][2]={{-1,1},{1,1},{1,-1},{-1,-1}};
                for(auto& o:offsets)
                {
                    Particle p=Particle::at({dropX+o[0]*r2,dropY+o[1]*r2,0.f},1.f);
                    p.velocity=vel;
                    particles.push_back(p);
                }
                int base=b.particleBase;
                auto addC=[&](int i,int j){
                    DistanceConstraint dc;
                    dc.i=base+i;dc.j=base+j;
                    dc.restLength=glm::distance(particles[base+i].position,particles[base+j].position);
                    dc.compliance=0.f;dc.breakThreshold=1e9f;
                    constraints.push_back(dc);
                };
                addC(0,1);addC(1,2);addC(2,3);addC(3,0);addC(0,2);addC(1,3);
                boulders.push_back(b);
            }
        }
        shakeAmount=0.6f;
        break;

    case 4: // Ground slam wind-up — massive
        attack=BossAttack::GolemGroundSlam;
        golemInWindup=true;
        golemWindupTimer=rage?0.8f:1.8f;
        phaseDur=999.f;
        shakeAmount=0.3f;
        break;
    }
}

// ============================================================
//  WRAITH — fast, teleports, attacks all heights
// ============================================================
void BossController::updateWraith(
    float bossTimer, float surviveDur, glm::vec2 playerPos, float dt)
{
    wraithAlpha=glm::min(wraithAlpha+dt*3.f,1.f);

    // Float and drift — always at player's approximate height
    glm::vec2 target={
        playerPos.x+sinf(animTime*0.9f)*3.5f,
        playerPos.y+sinf(animTime*0.5f)*1.5f  // tracks player Y
    };
    target.y=glm::clamp(target.y,arenaFloorY+2.f,arenaCeilY-2.f);
    glm::vec2 d=target-position;
    float dl=glm::length(d);
    if(dl>0.1f)
        position+=glm::normalize(d)*glm::min(dl*0.5f,4.f*(rage?2.f:1.f))*dt;

    phaseTimer+=dt;
    float interval=rage?0.9f:1.5f;
    if(phaseTimer<interval) return;
    phaseTimer=0.f;
    attackIndex=(attackIndex+1)%4;

    switch(attackIndex)
    {
    case 0:
    {   // Triple orb — tracks player Y
        attack=BossAttack::WraithOrbs;
        glm::vec2 d2=glm::normalize(playerPos-position);
        float base=atan2f(d2.y,d2.x);
        float spd=rage?10.f:7.f;
        for(int i=-1;i<=1;++i)
        {
            float a=base+i*0.4f;
            launchOrb(position,{cosf(a)*spd,sinf(a)*spd});
        }
        if(rage)launchOrb(position,{-d2.x*9.f,-d2.y*9.f});
        break;
    }
    case 1:
    {   // Fan spray toward player
        attack=BossAttack::WraithShadow;
        glm::vec2 d2=glm::normalize(playerPos-position);
        float base=atan2f(d2.y,d2.x);
        float spd=rage?13.f:9.f;
        int cnt=rage?9:6;
        for(int i=0;i<cnt;++i)
        {
            float a=base+(i-(cnt-1)*0.5f)*0.28f;
            launchOrb(position,{cosf(a)*spd,sinf(a)*spd});
        }
        break;
    }
    case 2:
    {   // Ring burst — no escape without moving
        attack=BossAttack::WraithOrbs;
        int cnt=rage?14:10;
        float spd=rage?8.f:5.5f;
        for(int i=0;i<cnt;++i)
        {
            float a=2.f*(float)M_PI*i/cnt;
            launchOrb(position,{cosf(a)*spd,sinf(a)*spd});
        }
        break;
    }
    case 3:
    {   // Teleport behind player + instant attack
        attack=BossAttack::WraithTeleport;
        wraithAlpha=0.f;
        float nx=(playerPos.x<(arenaLeft+arenaRight)*0.5f)
                 ?arenaRight-2.f:arenaLeft+2.f;
        position={nx,playerPos.y};  // appear at SAME HEIGHT as player
        shakeAmount=0.5f;
        glm::vec2 d2=glm::normalize(playerPos-position);
        float base=atan2f(d2.y,d2.x);
        float spd=rage?12.f:9.f;
        for(int i=-1;i<=1;++i)
        {
            float a=base+i*0.3f;
            launchOrb(position,{cosf(a)*spd,sinf(a)*spd});
        }
        break;
    }
    }
}

// ============================================================
//  DRAGON — fills arena, all attacks reach all heights
// ============================================================
void BossController::updateDragon(
    float bossTimer, float surviveDur, glm::vec2 playerPos, float dt)
{
    float hoverY=arenaCeilY-size-0.8f;
    position.y=hoverY+sinf(animTime*0.7f)*0.5f;
    position.x=(arenaLeft+arenaRight)*0.5f+sinf(animTime*0.45f)*2.5f;

    dragonBreathX+=dragonBreathDir*(rage?8.f:6.f)*dt;
    if(dragonBreathX>arenaRight+1.f){dragonBreathDir=-1.f;dragonBreathX=arenaRight;}
    if(dragonBreathX<arenaLeft -1.f){dragonBreathDir= 1.f;dragonBreathX=arenaLeft; }

    phaseTimer+=dt;
    float interval=rage?2.f:3.5f;
    if(phaseTimer<interval) return;
    phaseTimer=0.f;
    attackIndex=(attackIndex+1)%(rage?4:3);

    switch(attackIndex)
    {
    case 0:
    {   // Breath sweep — columns floor to ceiling
        attack=BossAttack::DragonBreath;
        float cw=rage?2.5f:1.8f;
        int cnt=rage?6:4;
        // Space columns to cover entire arena width including near player
        for(int i=0;i<cnt;++i)
        {
            float cx=arenaLeft+(arenaRight-arenaLeft)*i/(cnt-1);
            // One column always near player X to prevent standing still
            if(i==cnt/2) cx=playerPos.x+(frand()-0.5f)*1.5f;
            fireBreath(cx,cw);
        }
        shakeAmount=0.8f;
        break;
    }
    case 1:
    {   // Tail block
        attack=BossAttack::DragonTail;
        dragonTailLeft=!dragonTailLeft;
        TailBlock tb;
        tb.x=(arenaLeft+arenaRight)*0.5f;
        tb.side=dragonTailLeft?-1:1;
        tb.yMin=arenaFloorY; tb.yMax=arenaCeilY;
        tb.life=rage?1.5f:2.2f;
        tailBlocks.push_back(tb);
        shakeAmount=0.6f;
        break;
    }
    case 2:
    {   // Dive shockwave — must grapple to ceiling
        attack=BossAttack::DragonDive;
        Shockwave sw;
        sw.y=arenaFloorY;
        sw.x=position.x;
        sw.dir=(position.x<(arenaLeft+arenaRight)*0.5f)?1.f:-1.f;
        sw.speed=rage?15.f:11.f;
        sw.width=4.f;
        sw.height=(arenaCeilY-arenaFloorY)*0.72f;
        shockwaves.push_back(sw);
        shakeAmount=1.1f;
        break;
    }
    case 3:
    {   // RAGE — everything
        attack=BossAttack::DragonRage;
        for(int i=0;i<5;++i)
            fireBreath(arenaLeft+(arenaRight-arenaLeft)*i/4.f,2.8f);
        TailBlock tb;
        tb.x=(arenaLeft+arenaRight)*0.5f+1.5f;
        tb.side=-1; tb.life=1.8f;
        tailBlocks.push_back(tb);
        Shockwave sw;
        sw.y=arenaFloorY;sw.x=arenaLeft;sw.dir=1.f;
        sw.speed=13.f;sw.width=2.2f;
        sw.height=(arenaCeilY-arenaFloorY)*0.68f;
        shockwaves.push_back(sw);
        shakeAmount=1.5f;
        break;
    }
    }
}

bool BossController::checkPlayerHit(glm::vec2 pos, float r) const
{
    for(const auto& sw:shockwaves) if(sw.kills(pos)) return true;
    for(const auto& o :orbs)       if(o.kills(pos,r)) return true;
    for(const auto& bc:breathCols) if(bc.kills(pos))  return true;
    for(const auto& tb:tailBlocks) if(tb.kills(pos))  return true;
    return false;
}

void BossController::launchBoulder(
    glm::vec2 playerPos,
    std::vector<Particle>& particles,
    std::vector<DistanceConstraint>& constraints,
    float speedMult)
{
    Boulder boulder;
    boulder.particleBase=(int)particles.size();
    boulder.radius=0.6f; boulder.alive=true; boulder.age=0.f; boulder.lifetime=10.f;
    float r2=boulder.radius*0.75f;

    // Aim DIRECTLY at player position — no arc bias
    glm::vec2 dir=glm::normalize(playerPos-position);
    // Add slight upward if player is higher than us
    if(playerPos.y>position.y) dir.y=glm::max(dir.y,0.2f);
    dir=glm::normalize(dir);
    float speed=8.f*speedMult;
    glm::vec3 vel={dir.x*speed,dir.y*speed,0.f};

    glm::vec2 c=position+glm::vec2(0.f,size*0.6f);
    int offsets[4][2]={{-1,1},{1,1},{1,-1},{-1,-1}};
    for(auto& o:offsets)
    {
        Particle p=Particle::at({c.x+o[0]*r2,c.y+o[1]*r2,0.f},1.f);
        p.velocity=vel; particles.push_back(p);
    }
    int b=boulder.particleBase;
    auto addC=[&](int i,int j){
        DistanceConstraint dc;
        dc.i=b+i;dc.j=b+j;
        dc.restLength=glm::distance(particles[b+i].position,particles[b+j].position);
        dc.compliance=0.f;dc.breakThreshold=1e9f;
        constraints.push_back(dc);
    };
    addC(0,1);addC(1,2);addC(2,3);addC(3,0);addC(0,2);addC(1,3);
    boulders.push_back(boulder);
}

void BossController::launchShockwave(bool fromLeft, float speedMult)
{
    Shockwave sw;
    sw.y=arenaFloorY;
    sw.dir=fromLeft?1.f:-1.f;
    sw.x=fromLeft?arenaLeft-0.5f:arenaRight+0.5f;
    sw.speed=8.f*speedMult;
    sw.width=1.1f;
    sw.height=rage?2.4f:1.9f;
    sw.alive=true;
    shockwaves.push_back(sw);
}

void BossController::launchOrb(glm::vec2 from, glm::vec2 vel)
{
    Orb o; o.pos=from; o.vel=vel;
    o.radius=0.22f; o.alive=true; o.age=0.f; o.life=6.f;
    orbs.push_back(o);
}

void BossController::fireBreath(float x, float width)
{
    BreathColumn bc;
    bc.x=x; bc.width=width; bc.alive=true; bc.age=0.f;
    bc.life=rage?0.8f:1.1f;
    breathCols.push_back(bc);
}

void BossController::cleanup(
    std::vector<Particle>& particles,
    std::vector<DistanceConstraint>& constraints)
{
    for(auto it=boulders.begin();it!=boulders.end();)
    {
        if(!it->alive&&it->particleBase>=0)
        {
            int base=it->particleBase;
            if(base+4<=(int)particles.size())
            {
                particles.erase(particles.begin()+base,particles.begin()+base+4);
                for(auto& c:constraints){if(c.i>=base)c.i-=4;if(c.j>=base)c.j-=4;}
                for(auto& b2:boulders) if(b2.particleBase>base)b2.particleBase-=4;
            }
            it=boulders.erase(it);
        }
        else ++it;
    }
}
