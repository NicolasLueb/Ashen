#pragma once
#define GLFW_INCLUDE_NONE
#include "Player.h"
#include "Level.h"
#include "Particle.h"
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <vector>

// ============================================================
//  PlayerController
//
//  Handles all player input, movement, and grapple logic.
//  Separated from Player struct so Player can be a pure data
//  container and the controller holds all the logic.
//
//  Each frame call:
//    controller.update(window, player, level, particles, dt)
//
//  The controller reads keyboard state, applies movement,
//  resolves against level geometry, and manages the grapple
//  rope particles in the global particle array.
// ============================================================
class PlayerController
{
public:
    // Update player for one frame
    void update(GLFWwindow* win, Player& player, const Level& level,
                std::vector<Particle>& particles,
                std::vector<DistanceConstraint>& constraints,
                float gravity, float dt);

    // Fire grapple toward target world position
    void fireGrapple(Player& player, const Level& level,
                     std::vector<Particle>& particles,
                     std::vector<DistanceConstraint>& constraints,
                     glm::vec2 target);

    // Release grapple
    void releaseGrapple(Player& player,
                        std::vector<Particle>& particles,
                        std::vector<DistanceConstraint>& constraints);

private:
    void handleGroundMovement(GLFWwindow* win, Player& p, float dt);
    void handleAirMovement(GLFWwindow* win, Player& p, float gravity, float dt);
    void handleJump(GLFWwindow* win, Player& p, float dt);
    void handleWallJump(GLFWwindow* win, Player& p, float dt);
    void updateTimers(Player& p, float dt);
    void updateState(Player& p);
    void updateGrapple(Player& p,
                       std::vector<Particle>& particles,
                       std::vector<DistanceConstraint>& constraints,
                       float dt);

    // Key helpers
    bool key(GLFWwindow* w, int k) const { return glfwGetKey(w,k)==GLFW_PRESS; }
};
