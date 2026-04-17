#pragma once
#define GLFW_INCLUDE_NONE
#include "Particle.h"
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

// ============================================================
//  PlayerState — what the player is currently doing
// ============================================================
enum class PlayerState
{
    Idle,
    Running,
    Jumping,
    Falling,
    WallSliding,
    WallJumping,
    Swinging,    // on grapple rope
    Dead
};

// ============================================================
//  Player
//
//  The player is represented as two particles:
//    p[0] = body (main collider, carries weight)
//    p[1] = feet (slightly below body, detects floor)
//
//  This is simpler than a rigid body and good enough for a
//  2D platformer.  The body particle is moved directly each
//  frame based on input; velocity is derived from position
//  change (PBD style).
//
//  Movement systems:
//    - Ground movement:  direct velocity set with acceleration
//    - Jumping:          velocity impulse upward
//    - Coyote time:      can jump for a few frames after
//                        walking off a ledge
//    - Jump buffering:   jump input remembered for a few frames
//                        so pressing jump just before landing
//                        still triggers a jump
//    - Variable jump:    holding jump = higher arc, tapping = low hop
//    - Wall sliding:     touching a wall while falling = slow slide
//    - Wall jumping:     jump while wall sliding = bounce off wall
//    - Grapple hook:     fires a rope, attaches to geometry,
//                        swings like a pendulum
// ============================================================
struct PlayerConfig
{
    // Movement
    float moveSpeed       = 7.f;      // max horizontal speed on ground
    float airMoveSpeed    = 4.5f;     // horizontal control in air
    float acceleration    = 40.f;     // how fast we reach max speed
    float deceleration    = 30.f;     // how fast we slow down
    float jumpVelocity    = 12.f;     // initial jump impulse
    float wallJumpVX      = 8.f;      // horizontal push from wall jump
    float wallJumpVY      = 11.f;     // vertical push from wall jump
    float wallSlideSpeed  = 1.5f;     // max downward speed while wall sliding
    float gravityScale    = 1.f;      // multiplier on world gravity
    float fallGravScale   = 1.6f;     // extra gravity when falling (snappier arcs)
    float jumpCutGravScale= 2.5f;     // gravity when jump released early

    // Timers (in seconds)
    float coyoteTime      = 0.12f;
    float jumpBufferTime  = 0.12f;
    float wallJumpLockout = 0.18f;    // can't grab wall immediately after wall jump

    // Collider
    float bodyRadius      = 0.35f;    // radius for floor/wall detection
    float height          = 0.7f;     // total player height

    // Grapple
    float grappleRange    = 12.f;     // max grapple distance
    float grappleSegLen   = 0.4f;     // rope segment length
    int   grappleSegs     = 20;       // max rope segments
};

struct Player
{
    // World state
    glm::vec2   position  = {0.f, 2.f};
    glm::vec2   velocity  = {0.f, 0.f};
    PlayerState state     = PlayerState::Falling;
    bool        facingRight = true;

    // Physics flags (set each frame by collision detection)
    bool onGround     = false;
    bool onWall       = false;
    int  wallDir      = 0;    // -1 = left wall, +1 = right wall

    // Timers
    float coyoteTimer     = 0.f;
    float jumpBufferTimer = 0.f;
    float wallJumpTimer   = 0.f;
    float jumpHoldTimer   = 0.f;
    bool  jumpHeld        = false;
    bool  jumpWasPressed  = false;

    // Grapple
    bool      grappleActive   = false;
    glm::vec2 grappleAnchor   = {0.f, 0.f};  // world position of anchor point
    float     grappleLength   = 0.f;          // tether length
    int       grappleBaseIdx  = -1;  // unused in pendulum mode, kept for compat
    int       grappleSegCount = 0;

    PlayerConfig cfg;

    // Called once per frame — returns true if player requests jump
    bool wantsJump()  const { return jumpBufferTimer > 0.f; }
    bool canCoyote()  const { return coyoteTimer > 0.f; }
    bool isGrounded() const { return onGround; }
};
