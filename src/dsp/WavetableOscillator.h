#pragma once

#include <JuceHeader.h>
#include "Wavetable.h"
#include <array>
#include <cmath>

namespace zw
{

//==============================================================================
// One wavetable oscillator with unison (1..16 detuned voices), per-osc level,
// pan, and a continuous "warp" phase-bend (a stand-in for full warp modes, M7).
// Renders additively into a stereo accumulator.
//==============================================================================
class WavetableOscillator
{
public:
    static constexpr int kMaxUnison = 16;

    void prepare (double sr) noexcept { sampleRate = sr; }
    void setWavetable (const Wavetable* wt) noexcept { table = wt; }

    void noteOn (bool randomisePhase, float startPhase01) noexcept
    {
        for (int i = 0; i < kMaxUnison; ++i)
            phases[(size_t) i] = randomisePhase ? rng.nextFloat() : startPhase01;
    }

    // Per-block parameter update. detuneCents is the full spread; pan -1..1.
    struct UpdateParams
    {
        float  framePos01;
        float  warp01;
        int    unison;
        float  detuneCents;
        float  level01;
        float  pan;
        float  width01;
        double baseFreqHz;
    };

    void update (const UpdateParams& p) noexcept
    {
        framePos = p.framePos01;
        warp     = p.warp01;
        count    = juce::jlimit (1, kMaxUnison, p.unison);

        for (int i = 0; i < count; ++i)
        {
            const float spread = (count > 1) ? ((float) i / (float) (count - 1)) * 2.0f - 1.0f : 0.0f;
            const float cents  = p.detuneCents * spread;
            const float ratio  = std::pow (2.0f, cents / 1200.0f);
            const float freq   = (float) p.baseFreqHz * ratio;
            inc[(size_t) i]    = freq / (float) sampleRate;
            freqHz[(size_t) i] = freq;

            // Select mip + frames once per block; the per-sample loop just reads.
            if (table != nullptr)
                cursor[(size_t) i] = table->makeCursor (framePos, freq, sampleRate);

            const float vpan   = juce::jlimit (-1.0f, 1.0f, p.pan + p.width01 * spread);
            const float ang    = (vpan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
            gainL[(size_t) i]  = std::cos (ang);
            gainR[(size_t) i]  = std::sin (ang);
        }

        // Equal-ish power across unison voices.
        gain = p.level01 / std::sqrt ((float) count);
    }

    void renderAdd (float& outL, float& outR) noexcept
    {
        if (table == nullptr || gain <= 0.0f)
            return;

        for (int i = 0; i < count; ++i)
        {
            const float ph = warpPhase (phases[(size_t) i], warp);
            const float s  = cursor[(size_t) i].read (ph) * gain;
            outL += s * gainL[(size_t) i];
            outR += s * gainR[(size_t) i];

            phases[(size_t) i] += inc[(size_t) i];
            if (phases[(size_t) i] >= 1.0f) phases[(size_t) i] -= std::floor (phases[(size_t) i]);
        }
    }

private:
    // Continuous "bend" warp: skews the read phase about a moving pivot.
    static float warpPhase (float ph, float w) noexcept
    {
        if (w <= 0.001f) return ph;
        const float pivot = 0.5f + 0.49f * w;
        return (ph < pivot) ? (0.5f * ph / pivot)
                            : (0.5f + 0.5f * (ph - pivot) / (1.0f - pivot));
    }

    const Wavetable* table = nullptr;
    double sampleRate = 44100.0;
    juce::Random rng;

    int   count = 1;
    float framePos = 0.0f;
    float warp = 0.0f;
    float gain = 0.0f;
    std::array<float, kMaxUnison> phases {};
    std::array<float, kMaxUnison> inc {};
    std::array<float, kMaxUnison> freqHz {};
    std::array<float, kMaxUnison> gainL {};
    std::array<float, kMaxUnison> gainR {};
    std::array<Wavetable::Cursor, kMaxUnison> cursor {};
};

} // namespace zw
