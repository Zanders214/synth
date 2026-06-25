#pragma once

#include <cmath>

namespace zw
{

//==============================================================================
// Analog-style exponential ADSR (Pirkle/Reaktor-style one-pole segments). Gives
// a natural, click-free amp/mod envelope. Times are in seconds; sustain 0..1.
//==============================================================================
class Envelope
{
public:
    void setSampleRate (double sr) noexcept { sampleRate = sr; }

    void setParameters (float attackSec, float decaySec, float sustainLvl, float releaseSec) noexcept
    {
        attack = attackSec; decay = decaySec; sustain = sustainLvl; release = releaseSec;
        recalc();
    }

    void noteOn() noexcept  { stage = Stage::Attack; recalc(); }
    void noteOff() noexcept { if (stage != Stage::Idle) stage = Stage::Release; }
    void reset() noexcept   { stage = Stage::Idle; value = 0.0f; }

    bool isActive() const noexcept { return stage != Stage::Idle; }

    // Current level without advancing (for use as a modulation source).
    float getValue() const noexcept { return value; }

    // Advance by a whole block and return the resulting level (control-rate use).
    float processBlock (int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            getNextSample();
        return value;
    }

    float getNextSample() noexcept
    {
        switch (stage)
        {
            case Stage::Attack:
                value = attackBase + value * attackCoef;
                if (value >= 1.0f) { value = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                value = decayBase + value * decayCoef;
                if (value <= sustain) { value = sustain; stage = Stage::Sustain; }
                break;
            case Stage::Sustain:
                value = sustain;
                break;
            case Stage::Release:
                value = releaseBase + value * releaseCoef;
                if (value <= 0.0001f) { value = 0.0f; stage = Stage::Idle; }
                break;
            case Stage::Idle:
            default:
                value = 0.0f;
                break;
        }
        return value;
    }

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    static float calcCoef (float samples, float ratio) noexcept
    {
        if (samples <= 0.0f) return 0.0f;
        return std::exp (-std::log ((1.0f + ratio) / ratio) / samples);
    }

    void recalc() noexcept
    {
        const auto sr = (float) sampleRate;
        const float aS = juce_maxf (1.0f, attack  * sr);
        const float dS = juce_maxf (1.0f, decay   * sr);
        const float rS = juce_maxf (1.0f, release * sr);

        attackCoef  = calcCoef (aS, ratioA);
        attackBase  = (1.0f + ratioA) * (1.0f - attackCoef);
        decayCoef   = calcCoef (dS, ratioDR);
        decayBase   = (sustain - ratioDR) * (1.0f - decayCoef);
        releaseCoef = calcCoef (rS, ratioDR);
        releaseBase = (0.0f - ratioDR) * (1.0f - releaseCoef);
    }

    static float juce_maxf (float a, float b) noexcept { return a > b ? a : b; }

    double sampleRate = 44100.0;
    Stage  stage = Stage::Idle;
    float  value = 0.0f;
    float  attack = 0.01f;
    float  decay = 0.3f;
    float  sustain = 0.7f;
    float  release = 0.4f;
    float  ratioA = 0.3f;
    float  ratioDR = 0.0001f;
    float  attackCoef = 0;
    float  attackBase = 0;
    float  decayCoef = 0;
    float  decayBase = 0;
    float  releaseCoef = 0;
    float  releaseBase = 0;
};

} // namespace zw
