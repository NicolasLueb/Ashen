#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ============================================================
//  Camera
//
//  A 2D orthographic camera that follows the player with:
//    - Smooth lerp following (feels better than snap)
//    - Look-ahead: shifts toward the player's direction of travel
//    - Vertical lag: camera lags behind vertically so the
//      player can see more of the ground ahead of them
//    - Screen shake: impulse-based, decays exponentially
//    - Zoom: smooth lerp to target zoom level
//
//  The camera works in world space.  The view-projection
//  matrix it outputs maps world coordinates to NDC.
//
//  halfHeight = how many world units are visible vertically
//               from center to edge (default 8 = 16 units tall)
// ============================================================
struct Camera
{
    glm::vec2 position    = {0.f, 0.f};  // current camera center
    glm::vec2 target      = {0.f, 0.f};  // desired camera center
    float     halfHeight  = 8.f;         // vertical extent
    float     aspect      = 1.f;         // viewport aspect ratio

    // Follow settings
    float followSpeedX    = 6.f;   // horizontal follow lerp speed
    float followSpeedY    = 4.f;   // vertical (slower = more lag = feels spacious)
    float lookAheadDist   = 2.5f;  // how far ahead to look in facing direction
    float lookAheadSpeed  = 3.f;   // how fast look-ahead shifts

    // Internal look-ahead state
    glm::vec2 lookAheadOffset = {0.f, 0.f};

    // Screen shake
    glm::vec2 shakeOffset  = {0.f, 0.f};
    float     shakeMagnitude = 0.f;
    float     shakeDecay   = 8.f;   // how fast shake dies out

    // Zoom
    float zoom            = 1.f;    // multiplier on halfHeight (1=normal, 0.5=zoomed in)
    float targetZoom      = 1.f;
    float zoomSpeed       = 3.f;

    // Update camera — call once per frame
    void update(glm::vec2 playerPos, glm::vec2 playerVel, float dt)
    {
        // Look-ahead: shift camera in direction player is moving
        float lookDir = 0.f;
        if      (playerVel.x >  0.5f) lookDir =  1.f;
        else if (playerVel.x < -0.5f) lookDir = -1.f;

        float targetLookX = lookDir * lookAheadDist;
        lookAheadOffset.x += (targetLookX - lookAheadOffset.x) * lookAheadSpeed * dt;

        // Target position = player + look ahead offset
        target = playerPos + lookAheadOffset;

        // Smooth follow
        position.x += (target.x - position.x) * followSpeedX * dt;
        position.y += (target.y - position.y) * followSpeedY * dt;

        // Smooth zoom
        zoom += (targetZoom - zoom) * zoomSpeed * dt;

        // Decay screen shake
        shakeMagnitude *= expf(-shakeDecay * dt);
        if (shakeMagnitude < 0.001f) shakeMagnitude = 0.f;

        // Generate shake offset (pseudo-random using sin waves at different frequencies)
        static float shakeTime = 0.f;
        shakeTime += dt;
        shakeOffset.x = sinf(shakeTime * 37.f) * shakeMagnitude;
        shakeOffset.y = sinf(shakeTime * 29.f) * shakeMagnitude;
    }

    // Add a screen shake impulse (magnitude in world units)
    void shake(float magnitude)
    {
        shakeMagnitude = glm::max(shakeMagnitude, magnitude);
    }

    // Get view-projection matrix
    glm::mat4 viewProjection() const
    {
        glm::vec2 center = position + shakeOffset;
        float hH = halfHeight * zoom;
        float hW = hH * aspect;
        return glm::ortho(
            center.x - hW, center.x + hW,
            center.y - hH, center.y + hH,
            -10.f, 10.f
        );
    }

    // Convert screen pixel coordinates to world position
    glm::vec2 screenToWorld(float sx, float sy, int vpW, int vpH) const
    {
        glm::vec2 center = position + shakeOffset;
        float hH = halfHeight * zoom;
        float hW = hH * aspect;
        float wx = center.x - hW + (sx / vpW) * hH * 2.f * aspect;
        float wy = center.y + hH - (sy / vpH) * hH * 2.f;
        return {wx, wy};
    }

    // Set aspect ratio from viewport dimensions
    void setViewport(int w, int h)
    {
        aspect = h > 0 ? (float)w / h : 1.f;
    }
};
