#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>
#include <functional>
#include <glm/glm.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  Audio — procedural sound synthesis, no files needed.
//
//  Uses Windows WAVE output (winmm.lib) for zero-dependency audio.
//  All sounds are synthesized at runtime from math:
//    - Jump: rising sine chirp
//    - Land: low thud (noise burst with fast decay)
//    - Death: descending pitch glide with distortion
//    - Spike: sharp high crack
//    - Boss hit: low resonant boom
//    - Portal: ascending harmonic chime
//
//  Architecture: pre-render sound buffers at init time.
//  Playback: Win32 waveOut API with double-buffering.
// ============================================================

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

static const int kSampleRate = 44100;
static const int kChannels   = 1;
static const int kBitDepth   = 16;

enum class SoundID
{
    Jump = 0,
    Land,
    Death,
    Spike,
    BossHit,
    BossWarn,
    Portal,
    Fragment,
    COUNT
};

class Audio
{
public:
    void init()
    {
        WAVEFORMATEX wf = {};
        wf.wFormatTag      = WAVE_FORMAT_PCM;
        wf.nChannels       = kChannels;
        wf.nSamplesPerSec  = kSampleRate;
        wf.wBitsPerSample  = kBitDepth;
        wf.nBlockAlign     = kChannels * kBitDepth / 8;
        wf.nAvgBytesPerSec = kSampleRate * wf.nBlockAlign;

        if (waveOutOpen(&m_hWave, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        {
            m_enabled = false;
            return;
        }
        m_enabled = true;

        // Pre-render all sounds
        renderJump();
        renderLand();
        renderDeath();
        renderSpike();
        renderBossHit();
        renderBossWarn();
        renderPortal();
        renderFragment();
    }

    ~Audio()
    {
        if (!m_enabled) return;
        waveOutReset(m_hWave);
        for (auto& buf : m_buffers)
        {
            waveOutUnprepareHeader(m_hWave, &buf.hdr, sizeof(WAVEHDR));
        }
        waveOutClose(m_hWave);
    }

    void play(SoundID id)
    {
        if (!m_enabled) return;
        int idx = (int)id;
        if (idx < 0 || idx >= (int)SoundID::COUNT) return;

        auto& buf = m_buffers[idx];
        if (buf.samples.empty()) return;

        // If currently playing, stop it and restart
        if (buf.hdr.dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(m_hWave, &buf.hdr, sizeof(WAVEHDR));

        buf.hdr = {};
        buf.hdr.lpData         = (LPSTR)buf.samples.data();
        buf.hdr.dwBufferLength = (DWORD)(buf.samples.size() * sizeof(int16_t));
        buf.hdr.dwFlags        = 0;

        waveOutPrepareHeader(m_hWave, &buf.hdr, sizeof(WAVEHDR));
        waveOutWrite(m_hWave, &buf.hdr, sizeof(WAVEHDR));
    }

    bool enabled() const { return m_enabled; }

private:
    struct SoundBuf
    {
        std::vector<int16_t> samples;
        WAVEHDR              hdr = {};
    };

    HWAVEOUT m_hWave   = nullptr;
    bool     m_enabled = false;
    SoundBuf m_buffers[(int)SoundID::COUNT];

    // ---- Synthesis helpers ----

    float env_adsr(int t, int total, int attack, int decay, float sustain, int release)
    {
        if (t < attack)  return (float)t / attack;
        if (t < attack+decay) return 1.f - (1.f-sustain)*(float)(t-attack)/decay;
        if (t < total-release) return sustain;
        return sustain * (float)(total-t) / (float)((release)>1?(release):1);
    }

    void synth(SoundID id, float durationSec,
               std::function<float(float t, float dur)> fn)
    {
        int n = (int)(durationSec * kSampleRate);
        auto& buf = m_buffers[(int)id];
        buf.samples.resize(n);
        for (int i = 0; i < n; ++i)
        {
            float t   = (float)i / kSampleRate;
            float sv  = fn(t, durationSec);
            float s   = sv < -1.f ? -1.f : (sv > 1.f ? 1.f : sv);
            buf.samples[i] = (int16_t)(s * 28000.f);
        }
    }

    float noise() const { return ((float)rand()/RAND_MAX)*2.f-1.f; }

    // ---- Sound recipes ----

    void renderJump()
    {
        synth(SoundID::Jump, 0.18f, [](float t, float dur){
            // Rising sine chirp: freq goes from 220 to 440 Hz
            float freq  = 220.f + 220.f * (t/dur);
            float phase = 2.f*(float)M_PI * freq * t;
            float env   = expf(-t * 12.f);
            return sinf(phase) * env * 0.6f;
        });
    }

    void renderLand()
    {
        synth(SoundID::Land, 0.12f, [this](float t, float dur){
            // Low thud: noise + 80Hz sine, very fast decay
            float env  = expf(-t * 40.f);
            float sine = sinf(2.f*(float)M_PI * 80.f * t);
            return (noise()*0.5f + sine*0.5f) * env * 0.8f;
        });
    }

    void renderDeath()
    {
        synth(SoundID::Death, 0.5f, [this](float t, float dur){
            // Descending pitch glide with distortion
            float freq  = 440.f * expf(-t * 4.f);
            float phase = 2.f*(float)M_PI * freq * t;
            float saw   = 2.f*(t * freq - floorf(t * freq)) - 1.f;
            float env   = expf(-t * 3.f);
            return (sinf(phase)*0.4f + saw*0.4f + noise()*0.2f) * env;
        });
    }

    void renderSpike()
    {
        synth(SoundID::Spike, 0.08f, [this](float t, float dur){
            // Sharp crack: high-freq noise burst
            float env = expf(-t * 60.f);
            return noise() * env * 0.9f;
        });
    }

    void renderBossHit()
    {
        synth(SoundID::BossHit, 0.4f, [this](float t, float dur){
            // Low resonant boom
            float low  = sinf(2.f*(float)M_PI * 55.f * t);
            float mid  = sinf(2.f*(float)M_PI * 110.f * t) * 0.4f;
            float env  = expf(-t * 5.f);
            float rumble = noise() * 0.2f * expf(-t*10.f);
            return (low + mid + rumble) * env;
        });
    }

    void renderBossWarn()
    {
        synth(SoundID::BossWarn, 0.3f, [](float t, float dur){
            // Two quick pulsing tones — warning signal
            float freq = (t < 0.15f) ? 330.f : 440.f;
            float env  = (sinf((float)M_PI * t / 0.15f) > 0.f) ? 1.f : 0.f;
            return sinf(2.f*(float)M_PI*freq*t) * env * 0.5f;
        });
    }

    void renderPortal()
    {
        synth(SoundID::Portal, 0.6f, [](float t, float dur){
            // Ascending harmonic chime
            float f1 = 440.f;
            float f2 = 554.f;
            float f3 = 659.f;
            float env = sinf((float)M_PI * t / dur);
            return (sinf(2.f*(float)M_PI*f1*t)*0.4f
                  + sinf(2.f*(float)M_PI*f2*t)*0.3f
                  + sinf(2.f*(float)M_PI*f3*t)*0.3f) * env * 0.5f;
        });
    }

    void renderFragment()
    {
        synth(SoundID::Fragment, 0.25f, [](float t, float dur){
            // Soft shimmer
            float env = expf(-t*6.f);
            return (sinf(2.f*(float)M_PI*880.f*t)*0.5f
                  + sinf(2.f*(float)M_PI*1320.f*t)*0.3f) * env * 0.4f;
        });
    }
};

#else
// Non-Windows stub
enum class SoundID { Jump,Land,Death,Spike,BossHit,BossWarn,Portal,Fragment,COUNT };
class Audio {
public:
    void init(){}
    void play(SoundID){}
    bool enabled() const { return false; }
};
#endif
