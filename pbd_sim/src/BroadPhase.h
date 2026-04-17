#pragma once
#include "Particle.h"
#include <vector>
#include <unordered_map>
#include <functional>

// ============================================================
//  BroadPhase — Uniform Spatial Grid
//
//  Without a broad phase, collision detection is O(N^2) —
//  every particle checked against every other particle.
//  With 500 particles that's 250,000 checks per frame.
//
//  A spatial grid divides the world into cells of size
//  `cellSize`.  Each particle is hashed to a cell.  When
//  we need to find particles near a point, we only check
//  the 9 neighbouring cells (3x3 in 2D) instead of all N.
//
//  This makes collision detection O(N) on average.
//
//  Usage:
//    BroadPhase bp(cellSize);
//    bp.build(particles);                          // once per frame
//    bp.query(position, radius, callback);         // find nearby particles
//    bp.queryPairs(particles, radius, callback);   // all close pairs
//
//  The hash function maps (cellX, cellY) → a single integer
//  using a spatial hash so we don't need a 2D array.
// ============================================================
class BroadPhase
{
public:
    explicit BroadPhase(float cellSize = 1.f) : m_cellSize(cellSize) {}

    void setCellSize(float s) { m_cellSize = s; }

    // Rebuild the grid from current predicted positions
    void build(const std::vector<Particle>& particles)
    {
        m_grid.clear();
        m_grid.reserve(particles.size() * 2);

        for (int i = 0; i < (int)particles.size(); ++i)
        {
            auto [cx, cy] = cell(particles[i].predicted);
            m_grid[hashCell(cx, cy)].push_back(i);
        }
    }

    // Call callback(particleIndex) for every particle within
    // `radius` of `pos`.  Does NOT check exact distance —
    // that's up to the caller (narrow phase).
    void query(glm::vec3 pos, float radius,
               std::function<void(int)> callback) const
    {
        int r    = (int)ceilf(radius / m_cellSize);
        auto [cx, cy] = cell(pos);

        for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
        {
            auto it = m_grid.find(hashCell(cx+dx, cy+dy));
            if (it == m_grid.end()) continue;
            for (int idx : it->second)
                callback(idx);
        }
    }

    // Find all pairs of particles closer than `radius`.
    // Calls callback(i, j) for each pair (i < j).
    void queryPairs(const std::vector<Particle>& particles,
                    float radius,
                    std::function<void(int,int)> callback) const
    {
        float r2 = radius * radius;
        int   r  = (int)ceilf(radius / m_cellSize);

        for (auto& [hash, cell_particles] : m_grid)
        {
            for (int i : cell_particles)
            {
                // Query neighbourhood around particle i
                auto [cx, cy] = cellFromHash(hash);
                for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx)
                {
                    auto it = m_grid.find(hashCell(cx+dx, cy+dy));
                    if (it == m_grid.end()) continue;
                    for (int j : it->second)
                    {
                        if (j <= i) continue;  // avoid duplicates
                        glm::vec3 d = particles[j].predicted - particles[i].predicted;
                        if (glm::dot(d,d) < r2)
                            callback(i, j);
                    }
                }
            }
        }
    }

private:
    float m_cellSize;
    std::unordered_map<int64_t, std::vector<int>> m_grid;

    std::pair<int,int> cell(glm::vec3 p) const
    {
        return {
            (int)floorf(p.x / m_cellSize),
            (int)floorf(p.y / m_cellSize)
        };
    }

    // Decode cell coordinates back from a stored hash key
    // (only works for our specific encoding below)
    std::pair<int,int> cellFromHash(int64_t h) const
    {
        // We encode as h = cx * PRIME + cy — not reversible cleanly,
        // so instead we store both in the 64-bit key directly.
        int cx = (int)(h >> 32);
        int cy = (int)(h & 0xFFFFFFFF);
        return {cx, cy};
    }

    int64_t hashCell(int cx, int cy) const
    {
        // Pack two 32-bit ints into one 64-bit key.
        // Simple, collision-free, fast.
        return ((int64_t)cx << 32) | (uint32_t)cy;
    }
};
