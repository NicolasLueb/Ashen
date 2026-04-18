#pragma once
#include "Biome.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdlib>
#include <cmath>
#include <algorithm>

// ============================================================
//  Platform
// ============================================================
struct Platform
{
    glm::vec2 pos;
    glm::vec2 size;
    bool      isGround     = true;
    bool      isCeiling    = false;
    bool      isSpike      = false;
    bool      isMoving     = false;
    bool      isDisappear  = false;  // NEW: crumbles after standing on it
    glm::vec2 moveAxis     = {1.f,0.f};
    float     moveRange    = 0.f;
    float     moveSpeed    = 1.f;
    float     movePhase    = 0.f;
    // Base position for moving platforms
    glm::vec2 basePos      = {0.f,0.f};

    // Disappear state
    float     standTimer   = 0.f;   // how long player has been on it
    float     standLimit   = 1.2f;  // seconds before it drops
    bool      dropped      = false;
    float     dropTimer    = 0.f;

    glm::vec2 min() const { return pos - size; }
    glm::vec2 max() const { return pos + size; }

    // Update moving platform — call each frame with elapsed time
    void updateMoving(float time)
    {
        if (!isMoving) return;
        float t = sinf(time * moveSpeed + movePhase) * moveRange;
        pos = basePos + moveAxis * t;
    }
};

// ============================================================
//  WindZone
// ============================================================
struct WindZone
{
    glm::vec2 pos;
    glm::vec2 size;
    glm::vec2 force;
    bool contains(glm::vec2 p) const
    {
        return p.x>pos.x-size.x&&p.x<pos.x+size.x
            && p.y>pos.y-size.y&&p.y<pos.y+size.y;
    }
};

// ============================================================
//  Level
// ============================================================
struct Level
{
    std::vector<Platform> platforms;
    std::vector<WindZone> windZones;
    float startX  =  0.f;
    float startY  =  2.f;
    float deathY  = -25.f;

    // ---- Update ----
    void update(float dt, float time, glm::vec2 playerPos)
    {
        for (auto& p : platforms)
        {
            if (p.isMoving) p.updateMoving(time);

            // Disappear platform logic
            if (p.isDisappear && !p.dropped)
            {
                // Check if player is standing on it (within XY bounds)
                glm::vec2 pMin = p.min(), pMax = p.max();
                bool playerAbove =
                    playerPos.x > pMin.x && playerPos.x < pMax.x &&
                    fabsf(playerPos.y - pMax.y) < 0.15f;

                if (playerAbove)
                    p.standTimer += dt;
                else
                    p.standTimer = glm::max(p.standTimer - dt * 2.f, 0.f);

                if (p.standTimer >= p.standLimit)
                {
                    p.dropped   = true;
                    p.dropTimer = 0.f;
                }
            }

            // Animate platform falling off screen
            if (p.dropped)
            {
                p.dropTimer += dt;
                p.pos.y -= dt * (5.f + p.dropTimer * 12.f); // accelerating fall
            }
        }
    }

    // ---- Reset disappear platforms ----
    void resetDisappear()
    {
        for (auto& p : platforms)
        {
            if (p.isDisappear)
            {
                p.pos       = p.basePos;
                p.standTimer= 0.f;
                p.dropped   = false;
                p.dropTimer = 0.f;
            }
        }
    }

    // ---- Collision resolution ----
    glm::vec2 resolvePlayer(
        glm::vec2 pos, glm::vec2 vel,
        float halfW, float halfH,
        bool& onGround, bool& onWall, int& wallDir,
        bool& hitCeiling, bool& hitSpike) const
    {
        onGround=false; onWall=false; wallDir=0;
        hitCeiling=false; hitSpike=false;

        for (const auto& p : platforms)
        {
            if (p.dropped) continue;  // fallen platforms no longer collide

            glm::vec2 aMin=pos-glm::vec2(halfW,halfH);
            glm::vec2 aMax=pos+glm::vec2(halfW,halfH);
            float ox=glm::min(aMax.x,p.max().x)-glm::max(aMin.x,p.min().x);
            float oy=glm::min(aMax.y,p.max().y)-glm::max(aMin.y,p.min().y);

            if (ox<=0.f||oy<=0.f) continue;

            if (p.isSpike) { hitSpike=true; continue; }

            if (oy<ox)
            {
                if (pos.y>p.pos.y) { pos.y+=oy; onGround=true; }
                else               { pos.y-=oy; hitCeiling=true; }
            }
            else
            {
                if (pos.x<p.pos.x) { pos.x-=ox; onWall=true; wallDir=-1; }
                else               { pos.x+=ox; onWall=true; wallDir= 1; }
            }
        }
        return pos;
    }

    glm::vec2 windAt(glm::vec2 p) const
    {
        glm::vec2 t{0.f,0.f};
        for (const auto& z:windZones) if(z.contains(p)) t+=z.force;
        return t;
    }

    float rayCast(glm::vec2 origin, glm::vec2 dir, float maxDist) const
    {
        float nearest=maxDist; bool hit=false;
        glm::vec2 inv={fabsf(dir.x)>1e-6f?1.f/dir.x:1e9f,
                       fabsf(dir.y)>1e-6f?1.f/dir.y:1e9f};
        for (const auto& p:platforms)
        {
            if (p.isSpike||p.dropped) continue;
            glm::vec2 t0=(p.min()-origin)*inv, t1=(p.max()-origin)*inv;
            if(t0.x>t1.x)std::swap(t0.x,t1.x);
            if(t0.y>t1.y)std::swap(t0.y,t1.y);
            float te=glm::max(t0.x,t0.y), tx=glm::min(t1.x,t1.y);
            if(te<tx&&te>0.f&&te<nearest){nearest=te;hit=true;}
        }
        return hit?nearest:-1.f;
    }

    // ====================================================
    //  GENERATORS
    // ====================================================

    void generate(unsigned int seed, int count, Biome biome=Biome::Catacombs)
    {
        platforms.clear(); windZones.clear();
        srand(seed);
        switch(biome)
        {
        case Biome::Catacombs: generateCatacombs(count); break;
        case Biome::Cathedral: generateCathedral(count); break;
        case Biome::Abyss:     generateAbyss(count);     break;
        }
    }

private:
    float frand() const { return (float)rand()/RAND_MAX; }

    void addPlatform(glm::vec2 c, glm::vec2 h,
                     bool ceiling=false, bool spike=false,
                     bool moving=false, glm::vec2 axis={1,0},
                     float range=0, float speed=1, float phase=0,
                     bool disappear=false, float standLimit=1.2f)
    {
        Platform p;
        p.pos=p.basePos=c; p.size=h;
        p.isCeiling=ceiling; p.isSpike=spike;
        p.isGround=!ceiling&&!spike;
        p.isMoving=moving; p.moveAxis=axis;
        p.moveRange=range; p.moveSpeed=speed; p.movePhase=phase;
        p.isDisappear=disappear; p.standLimit=standLimit;
        platforms.push_back(p);
    }

    // --------------------------------------------------
    //  CATACOMBS — tight tunnels, spike pits, low CONTINUOUS ceiling
    //
    //  Key fixes vs previous version:
    //  - Ceiling is now a solid continuous slab, not per-platform chunks.
    //    The ceiling Y is fixed at a global level (floorBand + ceilH).
    //    Platforms must stay within the band — no open space above.
    //  - Platform heights are constrained to a narrow band so the
    //    level feels claustrophobic, not open.
    //  - Width starts at 3 and collapses to 0.6 by end.
    //  - Spike pit fills the FULL gap width (not 45%).
    //  - Height variation is more aggressive — steps up AND down sharply.
    // --------------------------------------------------
    void generateCatacombs(int count)
    {
        deathY = -20.f;
        startY = -2.f;

        // Tight band — platforms stay in a narrow Y range
        const float bandLow  = -4.5f;
        const float bandHigh = -1.5f;
        const float ceilH    = 2.1f;  // ceiling above each individual platform

        float x = 0.f;
        float y = -3.f;

        // Wide safe start with ceiling above it only
        addPlatform({0.f, y}, {4.f, 0.5f});
        addPlatform({0.f, y + ceilH}, {4.2f, 0.3f}, true); // ceiling above start only

        float prevY = y;

        for (int i = 0; i < count; ++i)
        {
            float diff = glm::min((float)i / count, 1.f);

            // Gap sizing
            float r = frand();
            float tinyThresh = glm::max(0.05f, 0.30f - diff*0.20f);
            float medThresh  = glm::max(0.20f, 0.65f - diff*0.25f);
            float gap;
            if      (r < tinyThresh) gap = 0.7f + frand()*0.5f;
            else if (r < medThresh)  gap = 1.4f + frand()*0.8f;
            else                     gap = 2.2f + diff*1.8f + frand()*0.8f;

            // Height steps — aggressive
            float maxStep = 0.8f + diff * 1.8f;
            float dy = (frand()*2.f - 1.f) * maxStep;
            if (frand() < 0.25f) dy = (frand() > 0.5f ? 1.f : -1.f) * maxStep * 1.5f;
            y = glm::clamp(prevY + dy, bandLow, bandHigh);
            prevY = y;

            // Platform width shrinks with difficulty
            float pw = glm::max(0.5f, 3.0f - diff * 2.4f) + frand() * 0.4f;
            x += gap + pw;

            bool isDisappear = diff > 0.25f && frand() < 0.40f;
            bool isMoving    = !isDisappear && diff > 0.15f && frand() < 0.28f;
            float standLim   = 0.6f + frand() * 0.7f;

            addPlatform({x, y}, {pw * 0.5f, 0.4f},
                        false, false, isMoving, {1, 0},
                        0.6f + frand()*0.7f, 1.0f + frand()*1.0f, frand()*6.28f,
                        isDisappear, standLim);

            // Ceiling ONLY above the platform itself — same width, no overlap into gap
            // This creates ceiling chunks above each platform but open sky in gaps
            float ceilW = pw * 0.5f + 0.2f;  // just slightly wider than platform
            addPlatform({x, y + ceilH}, {ceilW, 0.25f}, true);

            // FULL-WIDTH spike pit
            float pitX = x - gap * 0.5f;
            float pitW = gap * 0.90f;
            if (pitW > 0.2f)
                addPlatform({pitX, y - 0.5f}, {pitW, 0.22f}, false, true);

            // Mid-air spike column in wider gaps
            if (gap > 1.8f && frand() < 0.45f + diff * 0.2f)
            {
                float spikeH = 0.4f + frand() * 0.8f;
                float spikeX = x - gap * (0.3f + frand() * 0.4f);
                float spikeY = y + 0.3f + frand() * (ceilH - 0.8f);
                addPlatform({spikeX, spikeY}, {0.12f, spikeH}, false, true);
            }

            // Second mid-air spike
            if (gap > 2.5f && diff > 0.4f && frand() < 0.4f)
            {
                float s2X = x - gap * (0.6f + frand() * 0.2f);
                float s2Y = y + 0.5f + frand() * (ceilH - 1.0f);
                addPlatform({s2X, s2Y}, {0.12f, 0.5f}, false, true);
            }

            // Wind zone pushing into spikes
            if (diff > 0.35f && frand() < 0.30f)
            {
                WindZone wz;
                wz.pos   = {x - gap * 0.5f, y + 0.8f};
                wz.size  = {gap * 0.4f, 1.5f};
                wz.force = {(frand() > 0.5f ? 1.f : -1.f) * 11.f, 0.f};
                windZones.push_back(wz);
            }
        }
    }

    // --------------------------------------------------
    //  CATHEDRAL — tall vertical space, grapple mandatory
    //  Gaps: 2-10 units (massive range forces grapple use)
    //  Height: -8 to +10 — huge vertical range
    //  Platforms: 0.7 to 3 wide, shrink sharply
    // --------------------------------------------------
    void generateCathedral(int count)
    {
        deathY=-28.f; startY=0.f;
        float x=0.f, y=-2.f;
        addPlatform({0.f,y},{4.f,0.5f});

        float prevY = y;

        for(int i=0;i<count;++i)
        {
            float diff=glm::min((float)i/count,1.f);

            // Cathedral gaps are the widest in the game
            float r = frand();
            float gap;
            if      (r < 0.15f)                gap = 2.0f + frand();            // occasionally close
            else if (r < 0.50f)                gap = 3.5f + frand()*2.f;        // medium
            else if (r < 0.80f)                gap = 5.5f + diff*3.f;           // wide — needs grapple
            else                               gap = 7.f  + diff*3.f;           // brutal

            // Height: very aggressive, full vertical range
            float maxStep = 2.0f + diff * 4.0f;
            float dy = (frand()*2.f - 1.f) * maxStep;
            if (frand() < 0.2f) dy *= 1.5f;  // occasional big leaps
            y = glm::clamp(prevY + dy, -8.f, 10.f);
            prevY = y;

            // Width: starts generous, collapses fast
            float pw = glm::max(0.6f, 3.0f - diff*2.2f) + frand()*0.5f;
            x += gap + pw;

            bool isMoving    = diff>0.10f && frand()<0.55f;
            bool isDisappear = !isMoving && diff>0.30f && frand()<0.35f;
            // Cathedral: platforms oscillate vertically
            glm::vec2 axis   = frand()>0.35f ? glm::vec2{0,1} : glm::vec2{1,0};
            float range      = 0.8f + frand()*2.5f;

            addPlatform({x,y},{pw*0.5f,0.45f},
                        false,false,isMoving,axis,range,
                        0.7f+frand(),frand()*6.28f,
                        isDisappear,0.9f+frand()*0.8f);

            // Spike underside — punishes grappling under platforms
            if(diff>0.20f && frand()<0.40f)
                addPlatform({x, y-0.65f},{pw*0.45f,0.15f},false,true);

            // Vertical wind column in gap — huge force, pushes up OR down
            if(diff>0.20f && frand()<0.45f)
            {
                WindZone wz;
                wz.pos   = {x-gap*0.5f, y+3.f};
                wz.size  = {1.3f, 7.f};
                wz.force = {0.f, (frand()>0.5f?1.f:-1.f)*16.f};
                windZones.push_back(wz);
            }

            // Horizontal wind zone near platform edge — pushes into void
            if(diff>0.40f && frand()<0.30f)
            {
                WindZone wz;
                wz.pos   = {x + pw*0.7f, y + 1.5f};
                wz.size  = {1.5f, 2.5f};
                wz.force = {(frand()>0.5f?1.f:-1.f)*12.f, 0.f};
                windZones.push_back(wz);
            }

            // Floating upper routing platform (required for some gaps)
            if(frand()<0.35f)
            {
                float hy  = y + 3.f + frand()*5.f;
                float hpw = 0.7f + frand()*1.3f;
                bool  hm  = frand()<0.5f;
                addPlatform({x+pw*0.3f, hy},{hpw*0.5f,0.35f},
                            false,false,hm,{0,1},0.8f,1.f,frand()*6.28f);
            }

            // Spike pillar mid-gap
            if(gap>4.f && frand()<0.50f)
            {
                float sy = y + (frand()-0.5f)*4.f;
                float sh = 1.0f + frand()*2.f;
                addPlatform({x-gap*(0.3f+frand()*0.4f), sy},
                            {0.14f, sh}, false, true);
            }
        }
    }

    // --------------------------------------------------
    //  ABYSS — floating rocks, void everywhere
    //  Platforms: 0.4 to 1.8 wide — very small
    //  Gaps: 2-12 units — grapple absolutely required
    //  Nearly everything moves, diagonal paths
    // --------------------------------------------------
    void generateAbyss(int count)
    {
        deathY=-35.f; startY=2.f;
        float x=0.f, y=0.f;
        addPlatform({0.f,0.f},{3.f,0.4f});

        float prevY = y;

        for(int i=0;i<count;++i)
        {
            float diff=glm::min((float)i/count,1.f);

            // Abyss is the hardest — widest gaps, smallest platforms
            float r = frand();
            float gap;
            if      (r < 0.10f) gap = 1.8f + frand();          // rare mercy
            else if (r < 0.35f) gap = 3.0f + frand()*2.f;
            else if (r < 0.65f) gap = 5.0f + diff*4.f;
            else                gap = 7.5f + diff*4.5f;

            // Full vertical range — y can swing drastically
            float maxStep = 2.5f + diff * 5.f;
            float dy = (frand()*2.f - 1.f) * maxStep;
            if (frand() < 0.25f) dy = (frand()>0.5f?1.f:-1.f) * maxStep * 1.8f;
            y = glm::clamp(prevY + dy, -12.f, 14.f);
            prevY = y;

            // Very small platforms — shrink to 0.4 world units wide
            float pw = glm::max(0.35f, 1.8f - diff*1.4f) + frand()*0.3f;
            x += gap + pw;

            // Most abyss platforms move — diagonal preferred
            bool isMoving    = diff>0.05f || frand()<0.65f;
            float ang        = (frand()-0.5f)*2.0f;  // diagonal
            glm::vec2 axis   = glm::normalize(glm::vec2{cosf(ang), sinf(ang)*0.7f});
            float range      = 1.0f + frand()*3.0f;
            bool isDisappear = !isMoving && frand()<0.45f;

            addPlatform({x,y},{pw*0.5f,0.35f},
                        false,false,isMoving,axis,range,
                        0.6f+frand()*1.8f,frand()*6.28f,
                        isDisappear,0.7f+frand()*0.6f);

            // Spike spire FROM BELOW — menacing, forces careful landing
            if(frand()<0.55f)
            {
                float sy  = y - 1.8f - frand()*3.f;
                float sh  = 0.6f + frand()*1.5f;
                float sxOff = (frand()-0.5f)*gap*0.7f;
                addPlatform({x-gap*0.3f+sxOff, sy},{0.13f,sh},false,true);
            }

            // Spike spire FROM ABOVE — unique to Abyss
            if(diff>0.25f && frand()<0.45f)
            {
                float sy = y + 2.0f + frand()*2.5f;
                addPlatform({x+(frand()-0.5f)*pw, sy},{0.12f,0.8f},false,true);
            }

            // Second island at very different height (forces routing decisions)
            if(diff>0.15f && frand()<0.50f)
            {
                float yOff = (frand()>0.5f ? 1.f : -1.f) * (3.f + frand()*3.f);
                float pw2  = glm::max(0.35f, 1.2f - diff*0.6f);
                float x2   = x + pw + 1.5f + frand()*1.5f;
                addPlatform({x2, y+yOff},{pw2*0.5f,0.35f},
                            false,false,true,{0,1},0.7f,1.3f,frand()*6.28f);
            }

            // Omnidirectional wind zone — chaotic
            if(diff>0.15f && frand()<0.45f)
            {
                WindZone wz;
                float wAng = frand()*6.28f;
                wz.pos   = {x-gap*0.5f, y+frand()*4.f-2.f};
                wz.size  = {gap*0.4f, 4.f};
                wz.force = {cosf(wAng)*17.f, sinf(wAng)*10.f};
                windZones.push_back(wz);
            }
        }
    }
};
