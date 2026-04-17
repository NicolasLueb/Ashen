#include "PlayerController.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

// ============================================================
//  Main update
// ============================================================

void PlayerController::update(
    GLFWwindow* win, Player& p, const Level& level,
    std::vector<Particle>& particles,
    std::vector<DistanceConstraint>& constraints,
    float gravity, float dt)
{
    // ---- Read jump input ----
    bool jumpPressed = key(win, GLFW_KEY_SPACE) ||
                       key(win, GLFW_KEY_W)     ||
                       key(win, GLFW_KEY_UP);
    bool jumpJustPressed = jumpPressed && !p.jumpWasPressed;
    p.jumpWasPressed = jumpPressed;
    p.jumpHeld       = jumpPressed;

    if (jumpJustPressed)
        p.jumpBufferTimer = p.cfg.jumpBufferTime;

    // ---- Resolve against level geometry ----
    bool prevOnGround = p.onGround;
    p.position = level.resolvePlayer(
        p.position, p.velocity,
        p.cfg.bodyRadius, p.cfg.height * 0.5f,
        p.onGround, p.onWall, p.wallDir);

    // ---- Coyote time ----
    if (prevOnGround && !p.onGround && p.velocity.y <= 0.f)
        p.coyoteTimer = p.cfg.coyoteTime;

    // ---- Gravity ----
    if (!p.onGround)
    {
        float gScale = p.cfg.gravityScale;

        // Stronger gravity when falling (snappier arc)
        if (p.velocity.y < 0.f)
            gScale = p.cfg.fallGravScale;

        // Cut jump short when button released
        if (!p.jumpHeld && p.velocity.y > 0.f)
            gScale = p.cfg.jumpCutGravScale;

        p.velocity.y += gravity * gScale * dt;
    }
    else
    {
        // Snap velocity to zero when grounded (no bouncing)
        if (p.velocity.y < 0.f) p.velocity.y = 0.f;
        p.coyoteTimer = 0.f;
    }

    // ---- Wall slide ----
    if (p.onWall && !p.onGround && p.velocity.y < 0.f)
    {
        p.velocity.y = glm::max(p.velocity.y, -p.cfg.wallSlideSpeed);
        p.state = PlayerState::WallSliding;
    }

    // ---- Horizontal movement ----
    bool left  = key(win, GLFW_KEY_A) || key(win, GLFW_KEY_LEFT);
    bool right = key(win, GLFW_KEY_D) || key(win, GLFW_KEY_RIGHT);
    float inputX = (right ? 1.f : 0.f) - (left ? 1.f : 0.f);

    if (inputX != 0.f) p.facingRight = (inputX > 0.f);

    float targetVX, accel;
    if (p.onGround)
    {
        targetVX = inputX * p.cfg.moveSpeed;
        accel    = (inputX != 0.f) ? p.cfg.acceleration : p.cfg.deceleration;
    }
    else
    {
        targetVX = inputX * p.cfg.airMoveSpeed;
        accel    = (inputX != 0.f) ? p.cfg.acceleration * 0.6f
                                   : p.cfg.deceleration * 0.4f;
    }

    // Don't override wall jump horizontal if still in lockout
    if (p.wallJumpTimer > 0.f)
    {
        // Only allow input in same direction as wall jump
        if (inputX * (p.facingRight ? 1.f : -1.f) < 0.f)
            accel *= 0.2f; // very slow correction against wall jump direction
    }

    p.velocity.x += (targetVX - p.velocity.x) * glm::min(accel * dt, 1.f);

    // ---- Jump logic ----
    bool canJump = (p.onGround || p.canCoyote()) && p.wantsJump();
    bool canWallJump = p.onWall && !p.onGround && p.wantsJump()
                       && p.wallJumpTimer <= 0.f;

    if (canJump)
    {
        p.velocity.y     = p.cfg.jumpVelocity;
        p.jumpBufferTimer = 0.f;
        p.coyoteTimer     = 0.f;
        p.state           = PlayerState::Jumping;
    }
    else if (canWallJump)
    {
        // Jump away from the wall
        p.velocity.y     = p.cfg.wallJumpVY;
        p.velocity.x     = -p.wallDir * p.cfg.wallJumpVX;
        p.facingRight    = (p.velocity.x > 0.f);
        p.jumpBufferTimer = 0.f;
        p.wallJumpTimer  = p.cfg.wallJumpLockout;
        p.state          = PlayerState::WallJumping;
    }

    // ---- Update grapple (pendulum constraint) ----
    updateGrapple(p, particles, constraints, dt);

    // ---- Integrate position ----
    p.position += p.velocity * dt;

    // ---- Update timers ----
    updateTimers(p, dt);

    // ---- Update state ----
    updateState(p);
}

// ============================================================
//  Grapple
// ============================================================

// ============================================================
//  Grapple
//
//  Firing: cast ray toward cursor, snap anchor to hit point.
//  No rope particles — the grapple is a single rigid tether.
//  This avoids the rope-physics desync entirely and gives
//  snappy, predictable Celeste-style grapple feel.
//
//  Swing: every frame, apply a pendulum constraint.
//  The player is treated as a bob on a string of length
//  grappleLength.  If the player is further than the length,
//  project their position back and cancel outward velocity.
//  The player retains full air control while swinging.
// ============================================================

void PlayerController::fireGrapple(
    Player& p, const Level& level,
    std::vector<Particle>& particles,
    std::vector<DistanceConstraint>& constraints,
    glm::vec2 target)
{
    releaseGrapple(p, particles, constraints);

    glm::vec2 dir  = target - p.position;
    float     dist = glm::length(dir);
    if (dist < 0.5f) return;
    dir /= dist;

    // Only grapple upward or horizontally — not straight down
    // This prevents cheesing floors
    if (dir.y < -0.3f) dir.y = -0.3f;
    dir = glm::normalize(dir);

    float hitDist = level.rayCast(p.position, dir,
                                  glm::min(dist, p.cfg.grappleRange));
    if (hitDist < 1.0f) return;  // too close, ignore

    // Store anchor and rope length directly on the player
    p.grappleAnchor  = p.position + dir * hitDist;
    p.grappleLength  = hitDist;
    p.grappleActive  = true;
    p.grappleBaseIdx = -1;   // no rope particles in simple mode
    p.grappleSegCount = 0;

    // Give an initial swing velocity boost toward anchor
    glm::vec2 toAnchor = p.grappleAnchor - p.position;
    float td = glm::length(toAnchor);
    if (td > 0.1f)
    {
        glm::vec2 n = toAnchor / td;
        // Add a component toward the anchor
        float dot = glm::dot(p.velocity, n);
        if (dot < 2.f)
            p.velocity += n * (2.f - dot) * 0.4f;
    }

    p.state = PlayerState::Swinging;
}

void PlayerController::releaseGrapple(
    Player& p,
    std::vector<Particle>& particles,
    std::vector<DistanceConstraint>& constraints)
{
    if (!p.grappleActive) return;

    // Only remove particles if we had rope particles (old system)
    if (p.grappleBaseIdx >= 0 && p.grappleBaseIdx < (int)particles.size())
    {
        particles.erase(
            particles.begin() + p.grappleBaseIdx,
            particles.end());
        constraints.erase(
            std::remove_if(constraints.begin(), constraints.end(),
                [&](const DistanceConstraint& c){
                    return c.i >= p.grappleBaseIdx || c.j >= p.grappleBaseIdx;
                }),
            constraints.end());
    }

    p.grappleActive   = false;
    p.grappleBaseIdx  = -1;
    p.grappleSegCount = 0;
    p.grappleLength   = 0.f;
}

void PlayerController::updateGrapple(
    Player& p,
    std::vector<Particle>& particles,
    std::vector<DistanceConstraint>& constraints,
    float dt)
{
    if (!p.grappleActive) return;

    // ---- Pendulum constraint ----
    // Project player position onto circle of radius grappleLength
    // centered at grappleAnchor.
    glm::vec2 toPlayer = p.position - p.grappleAnchor;
    float     dist     = glm::length(toPlayer);

    if (dist < 1e-5f) return;

    // Only enforce max length (not min) — rope can go slack
    if (dist > p.grappleLength)
    {
        glm::vec2 n = toPlayer / dist;

        // Push player back to rope length
        p.position = p.grappleAnchor + n * p.grappleLength;

        // Cancel velocity component pulling away from anchor
        float vOut = glm::dot(p.velocity, n);
        if (vOut > 0.f)
            p.velocity -= n * vOut;

        // The remaining velocity is tangential — this IS the swing
    }

    // ---- Reel in / out with Up/Down while swinging ----
    // Pressing toward anchor shortens rope (reel in)
    // This lets skilled players control swing height
    float reelSpeed = 3.5f;
    // (Input handled in main update — just clamp length here)
    p.grappleLength = glm::max(p.grappleLength, 1.0f);
}

// ============================================================
//  Timer and state updates
// ============================================================

void PlayerController::updateTimers(Player& p, float dt)
{
    p.coyoteTimer     = glm::max(p.coyoteTimer     - dt, 0.f);
    p.jumpBufferTimer = glm::max(p.jumpBufferTimer - dt, 0.f);
    p.wallJumpTimer   = glm::max(p.wallJumpTimer   - dt, 0.f);
}

void PlayerController::updateState(Player& p)
{
    if (p.state == PlayerState::Dead) return;

    if      (p.grappleActive)                              p.state = PlayerState::Swinging;
    else if (p.onGround && fabsf(p.velocity.x) < 0.2f)   p.state = PlayerState::Idle;
    else if (p.onGround)                                   p.state = PlayerState::Running;
    else if (p.onWall && p.velocity.y < 0.f)              p.state = PlayerState::WallSliding;
    else if (p.velocity.y > 0.f)                          p.state = PlayerState::Jumping;
    else                                                   p.state = PlayerState::Falling;
}
