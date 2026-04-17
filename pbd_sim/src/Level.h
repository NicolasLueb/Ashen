#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdlib>
#include <cmath>
#include <algorithm>

// ============================================================
//  Platform — an axis-aligned rectangle in world space
// ============================================================
struct Platform
{
    glm::vec2 pos;       // center position
    glm::vec2 size;      // half-extents (half-width, half-height)
    bool      isGround;  // true = solid ground, false = wall/ceiling

    glm::vec2 min() const { return pos - size; }
    glm::vec2 max() const { return pos + size; }
};

// ============================================================
//  WallSegment — a vertical wall for wall-jumping
// ============================================================
struct WallSegment
{
    float x;        // x position of wall
    float yMin;     // bottom of wall
    float yMax;     // top of wall
    int   side;     // -1 = left face, +1 = right face
};

// ============================================================
//  Level
//
//  Holds all static geometry for a level — platforms and walls.
//  Also contains a simple procedural generator that creates
//  a series of platforms with increasing gaps and heights,
//  suitable for a gothic platformer.
//
//  Collision detection against the level is done here with
//  simple AABB sweeps rather than through the PBD solver —
//  the player character uses direct velocity integration
//  and resolves against level geometry directly each frame.
//
//  The PBD solver handles cloth, ropes, and destructible
//  objects in the scene.  Static level geometry is handled
//  as a separate fast path.
// ============================================================
struct Level
{
    std::vector<Platform>    platforms;
    std::vector<WallSegment> walls;
    float                    startX = 0.f;
    float                    startY = 2.f;  // player spawn Y
    float                    deathY = -20.f; // fall-off death plane

    // --------------------------------------------------------
    //  generate
    //
    //  Creates a scrolling series of platforms.  Each chunk
    //  is slightly harder than the last — wider gaps, more
    //  height variation.  Walls are added on platform sides
    //  to enable wall-jumping routes.
    //
    //  seed  = random seed for reproducible levels
    //  count = number of platform groups to generate
    // --------------------------------------------------------
    void generate(unsigned int seed = 42, int count = 30)
    {
        platforms.clear();
        walls.clear();

        srand(seed);

        // Starting ground — wide flat platform
        addPlatform({0.f, -3.f}, {8.f, 0.5f});
        addPlatform({0.f, -3.f}, {8.f, 0.5f}); // duplicate for wall segs

        float x = 4.f;   // cursor x
        float y = -3.f;  // cursor y

        for (int i = 0; i < count; ++i)
        {
            float difficulty = glm::min((float)i / count, 1.f);

            // Gap between platforms (grows with difficulty)
            float minGap = 1.5f + difficulty * 1.5f;
            float maxGap = 3.5f + difficulty * 2.5f;
            float gap    = minGap + frand() * (maxGap - minGap);

            // Height change (increases with difficulty)
            float maxDY  = 1.5f + difficulty * 2.5f;
            float dy     = (frand() * 2.f - 1.f) * maxDY;
            y = glm::clamp(y + dy, -5.f, 6.f);

            // Platform width (narrows with difficulty)
            float minW = 2.5f - difficulty * 1.2f;
            float maxW = 5.0f - difficulty * 1.5f;
            float w    = glm::max(minW + frand() * (maxW - minW), 1.2f);

            x += gap;
            glm::vec2 center = {x + w * 0.5f, y};
            glm::vec2 half   = {w * 0.5f, 0.45f};
            addPlatform(center, half);

            // Occasionally add a tall wall segment for wall-jumping
            if (frand() < 0.35f)
            {
                float wallH = 3.f + frand() * 3.f;
                float wallX = center.x + half.x + 0.4f + frand() * 2.f;
                float wallY = center.y + half.y;
                // Thin vertical platform = wall
                addPlatform({wallX, wallY + wallH * 0.5f}, {0.3f, wallH * 0.5f});
            }

            // Occasionally add a floating platform above
            if (frand() < 0.25f)
            {
                float fy  = y + 2.5f + frand() * 2.f;
                float fw  = 1.5f + frand() * 1.5f;
                float fx  = center.x + (frand() - 0.5f) * w;
                addPlatform({fx, fy}, {fw * 0.5f, 0.35f});
            }

            x += w;
        }
    }

    // --------------------------------------------------------
    //  Player vs Level AABB collision
    //
    //  Returns the resolved position and sets:
    //    onGround = true if player is standing on a platform
    //    onWall   = true if player is touching a vertical wall
    //    wallDir  = direction of wall (-1 left, +1 right)
    //
    //  The player is treated as a small AABB centered on pos.
    //  We resolve the shallowest penetration axis first.
    // --------------------------------------------------------
    glm::vec2 resolvePlayer(glm::vec2 pos, glm::vec2 vel,
                             float halfW, float halfH,
                             bool& onGround, bool& onWall, int& wallDir) const
    {
        onGround = false;
        onWall   = false;
        wallDir  = 0;

        for (const auto& p : platforms)
        {
            glm::vec2 pMin = p.min();
            glm::vec2 pMax = p.max();

            // Player bounds
            glm::vec2 aMin = pos - glm::vec2(halfW, halfH);
            glm::vec2 aMax = pos + glm::vec2(halfW, halfH);

            // Overlap on each axis
            float ox = glm::min(aMax.x, pMax.x) - glm::max(aMin.x, pMin.x);
            float oy = glm::min(aMax.y, pMax.y) - glm::max(aMin.y, pMin.y);

            if (ox <= 0.f || oy <= 0.f) continue; // no overlap

            // Resolve shallowest axis
            if (oy < ox)
            {
                // Vertical resolution
                if (pos.y > p.pos.y)
                {
                    pos.y += oy;   // push up (landed on top)
                    onGround = true;
                }
                else
                {
                    pos.y -= oy;   // push down (hit ceiling)
                }
            }
            else
            {
                // Horizontal resolution
                if (pos.x < p.pos.x)
                {
                    pos.x -= ox;   // push left
                    onWall  = true;
                    wallDir = -1;
                }
                else
                {
                    pos.x += ox;   // push right
                    onWall  = true;
                    wallDir =  1;
                }
            }
        }

        return pos;
    }

    // Check if a ray from `origin` in `dir` hits any platform
    // Returns hit distance or -1 if no hit within maxDist
    float rayCast(glm::vec2 origin, glm::vec2 dir, float maxDist) const
    {
        float nearest = maxDist;
        bool  hit     = false;

        glm::vec2 invDir = {
            fabsf(dir.x) > 1e-6f ? 1.f / dir.x : 1e9f,
            fabsf(dir.y) > 1e-6f ? 1.f / dir.y : 1e9f
        };

        for (const auto& p : platforms)
        {
            glm::vec2 tMin = (p.min() - origin) * invDir;
            glm::vec2 tMax = (p.max() - origin) * invDir;
            if (tMin.x > tMax.x) std::swap(tMin.x, tMax.x);
            if (tMin.y > tMax.y) std::swap(tMin.y, tMax.y);
            float tEnter = glm::max(tMin.x, tMin.y);
            float tExit  = glm::min(tMax.x, tMax.y);
            if (tEnter < tExit && tEnter > 0.f && tEnter < nearest)
            {
                nearest = tEnter;
                hit = true;
            }
        }
        return hit ? nearest : -1.f;
    }

private:
    void addPlatform(glm::vec2 center, glm::vec2 half)
    {
        Platform p;
        p.pos     = center;
        p.size    = half;
        p.isGround = true;
        platforms.push_back(p);
    }

    float frand() const { return (float)rand() / RAND_MAX; }
};
