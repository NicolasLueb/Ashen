#pragma once
#include "Particle.h"
#include <glm/glm.hpp>
#include <vector>
#include <functional>

// ============================================================
//  HazardType
// ============================================================
enum class HazardType
{
    Boulder,        // physics rigid body launched at player
    Shockwave,      // horizontal kill zone sweeping across floor
    CrumblePlatform // platform with reduced tear threshold
};

// ============================================================
//  Shockwave
//
//  A horizontal line of death that travels across the floor.
//  Player must be airborne or on a high platform to survive.
//
//  x    = current x position of the wave front
//  dir  = +1 (moving right) or -1 (moving left)
//  speed= world units per second
//  y    = floor Y at which the wave travels
//  width= how wide the kill zone is
//  alive= false once it exits the screen
// ============================================================
struct Shockwave
{
    float x       = 0.f;
    float y       = -5.f;  // floor level
    float dir     = 1.f;
    float speed   = 8.f;
    float width   = 1.2f;
    float height  = 1.5f;  // how high the wave reaches
    bool  alive   = true;

    void update(float dt) { x += dir * speed * dt; }

    // Returns true if point p is inside the shockwave kill zone
    bool kills(glm::vec2 p) const
    {
        if (!alive) return false;
        return p.x > x - width && p.x < x + width
            && p.y < y + height && p.y > y - 0.5f;
    }
};

// ============================================================
//  Boulder
//
//  A physics rigid body (4 particles + constraints) launched
//  by the boss toward the player.  Uses the existing particle
//  system so it collides with everything naturally.
//
//  particleBase = index of first particle in global array
//  alive        = false once it hits the floor or exits
// ============================================================
struct Boulder
{
    int   particleBase = -1;  // first of 4 corner particles
    float radius       = 0.6f;
    bool  alive        = true;
    float lifetime     = 8.f;  // despawn after this many seconds
    float age          = 0.f;

    void update(float dt) { age += dt; if (age > lifetime) alive = false; }

    // Returns true if point p is within the boulder's radius
    bool kills(glm::vec2 center, glm::vec2 p) const
    {
        if (!alive) return false;
        glm::vec2 d = p - center;
        return glm::dot(d,d) < radius * radius * 1.2f;
    }
};
