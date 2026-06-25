#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace zw
{

//==============================================================================
// Sub oscillator: sine / triangle / square with octave offset and tanh saturate.
// (Naive shapes are fine here because the sub sits well below the others; full
// band-limiting is a later polish item.)
//==============================================================================
class SubOscillator
{
public:
    enum Wave { Sine = 0, Tri, Square };

    void prepare (double sr) noexcept { sampleRate = sr; }
    void noteOn() noexcept            { phase = 0.0f; }

    void update (int wave_, int octave, float saturate01, float level01, double baseFreqHz) noexcept
    {
        wave  = wave_;
        sat   = 1.0f + saturate01 * 4.0f;
        satComp = 1.0f / std::tanh (sat);
        gain  = level01;
        inc   = (float) (baseFreqHz * std::pow (2.0, (double) octave) / sampleRate);
    }

    float render() noexcept
    {
        if (gain <= 0.0f) return 0.0f;

        float s = 0.0f;
        switch (wave)
        {
            case Tri:    s = 4.0f * std::abs (phase - 0.5f) - 1.0f; break;
            case Square: s = (phase < 0.5f) ? 1.0f : -1.0f;        break;
            case Sine:
            default:     s = std::sin (phase * juce::MathConstants<float>::twoPi); break;
        }

        if (sat > 1.0001f)
            s = std::tanh (s * sat) * satComp;

        phase += inc;
        if (phase >= 1.0f) phase -= std::floor (phase);
        return s * gain;
    }

private:
    double sampleRate = 44100.0;
    int    wave = Sine;
    float  phase = 0.0f;
    float  inc = 0.0f;
    float  gain = 0.0f;
    float  sat = 1.0f;
    float  satComp = 1.0f;
};

//==============================================================================
// Noise: white / pink / vinyl with a one-pole "color" tilt (LP below 0.5, HP above).
//==============================================================================
class NoiseOscillator
{
public:
    enum Type { White = 0, Pink, Vinyl };

    void prepare (double) noexcept { reset(); }
    void reset() noexcept { lp = 0.0f; for (auto& s : pinkState) s = 0.0f; crackle = 0.0f; }

    void update (int type_, float color01, float level01) noexcept
    {
        type  = type_;
        color = color01;
        gain  = level01;
    }

    float render() noexcept
    {
        if (gain <= 0.0f) return 0.0f;

        float w = rng.nextFloat() * 2.0f - 1.0f;
        float s = w;

        if (type == Pink || type == Vinyl)
            s = pink (w);

        if (type == Vinyl)
        {
            // sparse crackle on top of the pink bed
            if (rng.nextFloat() < 0.0008f) crackle = (rng.nextFloat() * 2.0f - 1.0f);
            crackle *= 0.85f;
            s = s * 0.6f + crackle * 0.8f;
        }

        // Color tilt: 0 -> dark (LP), 1 -> bright (HP-ish).
        const float coef = 0.05f + color * 0.9f;
        lp += coef * (s - lp);
        s = (color < 0.5f) ? lp : (s - lp);

        return juce::jlimit (-1.0f, 1.0f, s * 1.5f) * gain;
    }

private:
    float pink (float white) noexcept
    {
        // Paul Kellet's economical pink filter.
        pinkState[0] = 0.99886f * pinkState[0] + white * 0.0555179f;
        pinkState[1] = 0.99332f * pinkState[1] + white * 0.0750759f;
        pinkState[2] = 0.96900f * pinkState[2] + white * 0.1538520f;
        pinkState[3] = 0.86650f * pinkState[3] + white * 0.3104856f;
        pinkState[4] = 0.55000f * pinkState[4] + white * 0.5329522f;
        pinkState[5] = -0.7616f * pinkState[5] - white * 0.0168980f;
        const float out = pinkState[0] + pinkState[1] + pinkState[2] + pinkState[3]
                        + pinkState[4] + pinkState[5] + pinkState[6] + white * 0.5362f;
        pinkState[6] = white * 0.115926f;
        return out * 0.2f;
    }

    juce::Random rng;
    int   type = White;
    float color = 0.4f;
    float gain = 0.0f;
    float lp = 0.0f;
    float crackle = 0.0f;
    float pinkState[7] = { 0,0,0,0,0,0,0 };
};

} // namespace zw
