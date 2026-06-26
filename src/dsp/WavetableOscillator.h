#pragma once

#include <JuceHeader.h>
#include "Wavetable.h"
#include <array>
#include <cmath>

namespace zw
{

//==============================================================================
// One wavetable oscillator with unison (1..16 detuned voices), per-osc level,
// pan, and a selectable "warp" that reshapes the read phase each sample. The
// warp is driven by BOTH a continuous amount (0..1) and a mode selector whose
// order matches the "Warp Mode" choice param (Off, Sync, Bend, PWM, Asym,
// Remap, Quantize). Renders additively into a stereo accumulator.
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
        int    warpMode = 0;   // 0=Off,1=Sync,2=Bend,3=PWM,4=Asym,5=Remap,6=Quantize
    };

    void update (const UpdateParams& p) noexcept
    {
        framePos = p.framePos01;
        warp     = p.warp01;
        warpMode = p.warpMode;
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
            const float ph = warpPhase (warpMode, phases[(size_t) i], warp);
            const float s  = cursor[(size_t) i].read (ph) * gain;
            outL += s * gainL[(size_t) i];
            outR += s * gainR[(size_t) i];

            phases[(size_t) i] += inc[(size_t) i];
            if (phases[(size_t) i] >= 1.0f) phases[(size_t) i] -= std::floor (phases[(size_t) i]);
        }
    }

private:
    // Reshape the read phase for the active warp mode. Every mode is the
    // identity at w<=0 and intensifies toward w=1, so the warp amount and the
    // mode together drive the timbre. Mode indices match the "Warp Mode"
    // choice param order: 0=Off,1=Sync,2=Bend,3=PWM,4=Asym,5=Remap,6=Quantize.
    // A pure phase remap keeps every mode band-limited-friendly (a single mip
    // read) and allocation-free on the audio thread.
    static float warpPhase (int mode, float ph, float w) noexcept
    {
        if (mode == 0 || w <= 0.001f)
            return ph;

        switch (mode)
        {
            case 1: // Sync: a virtual sync oscillator whose ratio rises with warp.
            {
                const float p = ph * (1.0f + 3.0f * w);   // 1x..4x retrigger
                return p - std::floor (p);
            }
            case 2: // Bend: skew the read about a moving pivot (the original warp).
            {
                const float pivot = 0.5f + 0.49f * w;
                return (ph < pivot) ? (0.5f * ph / pivot)
                                    : (0.5f + 0.5f * (ph - pivot) / (1.0f - pivot));
            }
            case 3: // PWM: pulse-width — collapse the first half toward zero width.
            {
                const float pw = 0.5f * (1.0f - w) + 0.005f;  // 0.505 -> 0.005
                return (ph < pw) ? (0.5f * ph / pw)
                                 : (0.5f + 0.5f * (ph - pw) / (1.0f - pw));
            }
            case 4: // Asym: asymmetric (CZ-style) phase distortion via a power curve.
            {
                return std::pow (ph, 1.0f + 4.0f * w);        // p^1 .. p^5
            }
            case 5: // Remap: blend the phase toward a smoothstep curve.
            {
                const float curve = ph * ph * (3.0f - 2.0f * ph);
                return ph + (curve - ph) * w;
            }
            case 6: // Quantize: stepped phase for a digital/formant edge.
            {
                const float n = 2.0f + (1.0f - w) * 62.0f;    // 64 -> 2 steps
                const float q = (std::floor (ph * n) + 0.5f) / n;
                return ph + (q - ph) * w;
            }
            default:
                return ph;
        }
    }

    const Wavetable* table = nullptr;
    double sampleRate = 44100.0;
    juce::Random rng;

    int   count = 1;
    float framePos = 0.0f;
    float warp = 0.0f;
    int   warpMode = 0;
    float gain = 0.0f;
    std::array<float, kMaxUnison> phases {};
    std::array<float, kMaxUnison> inc {};
    std::array<float, kMaxUnison> freqHz {};
    std::array<float, kMaxUnison> gainL {};
    std::array<float, kMaxUnison> gainR {};
    std::array<Wavetable::Cursor, kMaxUnison> cursor {};
};

} // namespace zw
