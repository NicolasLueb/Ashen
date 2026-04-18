#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <fstream>

// ============================================================
//  Ghost — records and replays the player's best run.
//
//  On every frame during a level attempt, the player's
//  position + state is sampled at 30fps and stored.
//  On death, if this attempt is faster than the best,
//  it replaces the stored ghost.
//  During the next attempt, the ghost is replayed as a
//  translucent version of the player.
//
//  This is the core Super Meat Boy "ghost of your best run"
//  feature that gives meaningful feedback on improvement.
// ============================================================

struct GhostFrame
{
    glm::vec2 pos;
    bool      facingRight;
    uint8_t   stateIdx;   // PlayerState as byte
};

class Ghost
{
public:
    static constexpr float RECORD_INTERVAL = 1.f / 30.f;  // 30fps

    // Start recording a new attempt
    void beginRecord()
    {
        m_recording.clear();
        m_recordTimer = 0.f;
        m_isRecording = true;
    }

    // Sample player each frame
    void record(glm::vec2 pos, bool facingRight, int stateIdx, float dt)
    {
        if (!m_isRecording) return;
        m_recordTimer += dt;
        if (m_recordTimer >= RECORD_INTERVAL)
        {
            m_recordTimer -= RECORD_INTERVAL;
            GhostFrame f;
            f.pos         = pos;
            f.facingRight = facingRight;
            f.stateIdx    = (uint8_t)stateIdx;
            m_recording.push_back(f);
        }
    }

    // Called on death — save if this run reached further or was faster
    void commitOnDeath()
    {
        m_isRecording = false;
        // Only keep if this attempt is longer (got further)
        if (m_recording.size() > m_best.size())
            m_best = m_recording;
        m_recording.clear();
    }

    // Called on level complete — always save the winning run
    void commitOnComplete()
    {
        m_isRecording = false;
        m_best = m_recording;
        m_recording.clear();
    }

    // Start playing back the ghost
    void beginPlayback()
    {
        m_playFrame = 0;
        m_playTimer = 0.f;
        m_isPlaying = !m_best.empty();
    }

    // Advance ghost playback — returns current frame (or nullptr if done)
    const GhostFrame* playback(float dt)
    {
        if (!m_isPlaying || m_best.empty()) return nullptr;
        m_playTimer += dt;
        while (m_playTimer >= RECORD_INTERVAL)
        {
            m_playTimer -= RECORD_INTERVAL;
            m_playFrame++;
        }
        if (m_playFrame >= (int)m_best.size())
        {
            m_isPlaying = false;
            return nullptr;
        }
        return &m_best[m_playFrame];
    }

    bool hasGhost()    const { return !m_best.empty(); }
    bool isRecording() const { return m_isRecording; }
    bool isPlaying()   const { return m_isPlaying; }

    // Clear ghost for this level (called on level change)
    void reset() { m_best.clear(); m_recording.clear(); m_isRecording=false; m_isPlaying=false; }

private:
    std::vector<GhostFrame> m_best;
    std::vector<GhostFrame> m_recording;
    float m_recordTimer = 0.f;
    float m_playTimer   = 0.f;
    int   m_playFrame   = 0;
    bool  m_isRecording = false;
    bool  m_isPlaying   = false;
};
