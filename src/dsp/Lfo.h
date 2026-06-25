#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace zw
{

//==============================================================================
// Low-frequency oscillator: 6 shapes, free-Hz or tempo-synced rate, depth, a
// fade-in (rise), start phase, and a trigger/free mode. Advanced at control
// rate (once per block); output is bipolar (-1..1) scaled by depth.
//==============================================================================
class Lfo
{
public:
    enum Shape { Sine = 0, Triangle, Saw, Ramp, Square, Random };
    enum Mode  { Trigger = 0, Envelope, Free };

    void prepare (double sr) noexcept { sampleRate = sr; }

    void noteOn() noexcept
    {
        if (mode != Free) phase = startPhase;
        risePos = 0.0;
        sh = rng.nextFloat() * 2.0f - 1.0f;
    }

    // Per-block parameter refresh. freqHz already resolved (free or synced).
    void setParams (int shape_, float freqHz, float depth01, float riseSec,
                    float startPhase01, int mode_) noexcept
    {
        shape      = shape_;
        freq       = freqHz;
        depth      = depth01;
        riseTime   = riseSec;
        startPhase = startPhase01;
        mode       = mode_;
    }

    // Advance by numSamples (a block) and return the current bipolar value*depth.
    float processBlock (int numSamples) noexcept
    {
        const double before = phase;
        phase += (double) freq * (double) numSamples / sampleRate;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            if (shape == Random) sh = rng.nextFloat() * 2.0f - 1.0f;   // S&H per cycle
        }
        else if (std::floor (before) != std::floor (phase) && shape == Random)
        {
            sh = rng.nextFloat() * 2.0f - 1.0f;
        }

        float riseGain = 1.0f;
        if (riseTime > 0.0001f)
        {
            risePos += (double) numSamples / sampleRate;
            riseGain = (float) juce::jlimit (0.0, 1.0, risePos / (double) riseTime);
        }

        return shapeValue ((float) phase) * depth * riseGain;
    }

private:
    float shapeValue (float ph) const noexcept
    {
        switch (shape)
        {
            case Triangle: return 4.0f * std::abs (ph - 0.5f) - 1.0f;
            case Saw:      return 1.0f - 2.0f * ph;          // falling
            case Ramp:     return 2.0f * ph - 1.0f;          // rising
            case Square:   return ph < 0.5f ? 1.0f : -1.0f;
            case Random:   return sh;
            case Sine:
            default:       return std::sin (ph * juce::MathConstants<float>::twoPi);
        }
    }

    juce::Random rng;
    double sampleRate = 44100.0;
    double phase = 0.0;
    double risePos = 0.0;
    int    shape = Sine;
    int    mode = Trigger;
    float  freq = 1.0f;
    float  depth = 1.0f;
    float  riseTime = 0.0f;
    float  startPhase = 0.0f;
    float  sh = 0.0f;
};

} // namespace zw
