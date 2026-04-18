#pragma once
#define GLFW_INCLUDE_NONE
#include "Particle.h"
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

enum class PlayerState
{
    Idle, Running, Jumping, Falling,
    WallSliding, WallJumping, Swinging, Dead
};

struct PlayerConfig
{
    float moveSpeed        = 6.5f;
    float airMoveSpeed     = 4.5f;
    float acceleration     = 45.f;
    float deceleration     = 35.f;
    float jumpVelocity     = 13.f;
    float wallJumpVX       = 8.5f;
    float wallJumpVY       = 12.f;
    float wallSlideSpeed   = 1.2f;
    float gravityScale     = 1.f;
    float fallGravScale    = 2.2f;
    float jumpCutGravScale = 3.5f;
    float coyoteTime       = 0.10f;
    float jumpBufferTime   = 0.10f;
    float wallJumpLockout  = 0.18f;
    float bodyRadius       = 0.32f;
    float height           = 0.65f;
    float grappleRange     = 12.f;
    float grappleSegLen    = 0.4f;
    int   grappleSegs      = 20;
};

struct Player
{
    glm::vec2   position    = {0.f, 2.f};
    glm::vec2   velocity    = {0.f, 0.f};
    PlayerState state       = PlayerState::Falling;
    bool        facingRight = true;
    bool        onGround    = false;
    bool        onWall      = false;
    int         wallDir     = 0;

    float coyoteTimer     = 0.f;
    float jumpBufferTimer = 0.f;
    float wallJumpTimer   = 0.f;
    bool  jumpHeld        = false;
    bool  jumpWasPressed  = false;

    bool      grappleActive   = false;
    glm::vec2 grappleAnchor   = {0.f, 0.f};
    float     grappleLength   = 0.f;
    int       grappleBaseIdx  = -1;
    int       grappleSegCount = 0;

    // ---- Squash/stretch animation ----
    // scaleX/scaleY: applied to the drawn capsule (1=normal)
    float scaleX        = 1.f;
    float scaleY        = 1.f;
    float squashTimer   = 0.f;  // countdown for land squash
    float stretchTimer  = 0.f;  // countdown for jump stretch
    bool  wasOnGround   = false;// prev frame ground state

    // Update squash/stretch each frame
    void updateSquash(float dt)
    {
        // Decay back toward 1.0
        float decay = 8.f;
        scaleX += (1.f - scaleX) * decay * dt;
        scaleY += (1.f - scaleY) * decay * dt;

        // Jump stretch: tall and thin
        if (stretchTimer > 0.f)
        {
            stretchTimer -= dt;
            float t = glm::max(stretchTimer / 0.12f, 0.f);
            scaleX = 1.f - 0.28f * t;
            scaleY = 1.f + 0.40f * t;
        }

        // Land squash: wide and flat
        if (squashTimer > 0.f)
        {
            squashTimer -= dt;
            float t = glm::max(squashTimer / 0.10f, 0.f);
            scaleX = 1.f + 0.35f * t;
            scaleY = 1.f - 0.30f * t;
        }
    }

    void onJump()
    {
        stretchTimer = 0.12f;
        squashTimer  = 0.f;
        scaleX = 0.72f; scaleY = 1.40f;
    }

    void onLand()
    {
        squashTimer  = 0.10f;
        stretchTimer = 0.f;
        scaleX = 1.35f; scaleY = 0.70f;
    }

    PlayerConfig cfg;

    bool wantsJump() const { return jumpBufferTimer > 0.f; }
    bool canCoyote() const { return coyoteTimer > 0.f; }
};
