#include "WavetableLibrary.h"
#include "../Parameters.h"

#include <cmath>
#include <random>

namespace zw
{

namespace
{
    constexpr float kTwoPi    = 6.28318530717958647692f;
    constexpr int   kGenFrames = 48;   // frames per analytic factory table

    inline float ramp (float x) noexcept            // -1..1 sawtooth ramp
    {
        return 2.0f * (x - std::floor (x)) - 1.0f;
    }
}

//==============================================================================
WavetableLibrary::WavetableLibrary()
{
    buildFactory();
}

void WavetableLibrary::addFactory (const juce::String& name,
                                   const std::function<float (int, int, float)>& fn,
                                   int numFrames)
{
    auto wt = std::make_unique<Wavetable>();
    wt->buildFromGenerator (numFrames, fn);
    factory.push_back (std::move (wt));
    names.add (name);
}

void WavetableLibrary::buildFactory()
{
    factory.clear();
    names.clear();

    // ---- 0: Basic Shapes (the original sine -> saw morph, unchanged sound) ----
    {
        auto wt = std::make_unique<Wavetable>();
        wt->generateBasicShapes (64);
        factory.push_back (std::move (wt));
        names.add ("Basic Shapes");
    }

    // ---- 1: PWM Square — duty cycle narrows across frames ----
    addFactory ("PWM Square", [] (int f, int nf, float ph)
    {
        const float t    = (nf > 1) ? (float) f / (float) (nf - 1) : 0.0f;
        const float duty = 0.5f - 0.45f * t;          // 0.5 -> 0.05
        return (ph < duty) ? 1.0f : -1.0f;
    }, kGenFrames);

    // ---- 2: Additive Sweep — a Gaussian harmonic emphasis that slides up ----
    addFactory ("Additive Sweep", [] (int f, int nf, float ph)
    {
        const float t     = (nf > 1) ? (float) f / (float) (nf - 1) : 0.0f;
        const float peakH = 1.0f + t * 31.0f;         // emphasised harmonic 1..32
        float s = 0.0f;
        for (int n = 1; n <= 48; ++n)
        {
            const float a = std::exp (-0.5f * (((float) n - peakH) / 6.0f) * (((float) n - peakH) / 6.0f));
            s += a * std::sin (kTwoPi * (float) n * ph);
        }
        return s;
    }, kGenFrames);

    // ---- 3: FM Bell — modulation index grows with frame (ratio 3) ----
    addFactory ("FM Bell", [] (int f, int nf, float ph)
    {
        const float t = (nf > 1) ? (float) f / (float) (nf - 1) : 0.0f;
        const float I = 0.5f + 6.0f * t;
        return std::sin (kTwoPi * ph + I * std::sin (kTwoPi * 3.0f * ph));
    }, kGenFrames);

    // ---- 4: Formant Vocal — two formant peaks that shift (vowel morph) ----
    addFactory ("Formant Vocal", [] (int f, int nf, float ph)
    {
        const float t  = (nf > 1) ? (float) f / (float) (nf - 1) : 0.0f;
        const float f1 = 3.0f + 9.0f * t;             // first formant harmonic
        const float f2 = 12.0f + 12.0f * t;           // second formant harmonic
        float s = 0.0f;
        for (int n = 1; n <= 40; ++n)
        {
            const float a1 = std::exp (-0.5f * (((float) n - f1) / 2.5f) * (((float) n - f1) / 2.5f));
            const float a2 = 0.6f * std::exp (-0.5f * (((float) n - f2) / 3.0f) * (((float) n - f2) / 3.0f));
            s += (a1 + a2) * std::sin (kTwoPi * (float) n * ph);
        }
        return s;
    }, kGenFrames);

    // ---- 5: Saw Stack — several phase-spread ramps (supersaw-like comb) ----
    addFactory ("Saw Stack", [] (int f, int nf, float ph)
    {
        const float t      = (nf > 1) ? (float) f / (float) (nf - 1) : 0.0f;
        const float spread = 0.004f + 0.03f * t;
        float s = 0.0f;
        for (int k = -2; k <= 2; ++k)
            s += ramp (ph + (float) k * spread);
        return s * 0.4f;
    }, kGenFrames);

    // ---- 6: Sync Saw — hard-sync ratio rises with frame (more teeth) ----
    addFactory ("Sync Saw", [] (int f, int nf, float ph)
    {
        const float t     = (nf > 1) ? (float) f / (float) (nf - 1) : 0.0f;
        const float ratio = 1.0f + 4.0f * t;
        return ramp (ph * ratio);
    }, kGenFrames);

    // ---- 7: Metallic — rounded inharmonic partials with a bell-like decay ----
    addFactory ("Metallic", [] (int f, int nf, float ph)
    {
        const float t = (nf > 1) ? (float) f / (float) (nf - 1) : 0.0f;
        static const int   partials[] = { 1, 3, 5, 9, 13, 19, 27 };
        static const float decays[]   = { 1.0f, 0.7f, 0.55f, 0.4f, 0.3f, 0.22f, 0.16f };
        float s = 0.0f;
        for (int k = 0; k < 7; ++k)
        {
            // Brightness moves up the partial stack as the frame advances.
            const float em = 0.4f + 0.6f * std::exp (-2.0f * std::abs (t - (float) k / 6.0f));
            s += decays[k] * em * std::sin (kTwoPi * (float) partials[k] * ph);
        }
        return s;
    }, kGenFrames);

    // ---- 8: Pulse Train — a periodic pulse that sharpens across frames ----
    addFactory ("Pulse Train", [] (int f, int nf, float ph)
    {
        const float t   = (nf > 1) ? (float) f / (float) (nf - 1) : 0.0f;
        const float exp = 1.0f + 60.0f * t;           // higher -> narrower pulse
        const float bump = 0.5f + 0.5f * std::cos (kTwoPi * ph);  // 1 at ph=0
        return std::pow (bump, exp);
    }, kGenFrames);

    // ---- 9: Noise Band — fixed-seed random harmonic phases in a moving band ----
    {
        const int N = Wavetable::kFrameSize;
        std::vector<std::vector<float>> frames ((size_t) kGenFrames, std::vector<float> ((size_t) N, 0.0f));
        for (int f = 0; f < kGenFrames; ++f)
        {
            const float t = (kGenFrames > 1) ? (float) f / (float) (kGenFrames - 1) : 0.0f;
            std::mt19937 rng (0x9E37u + (unsigned) f * 2654435761u);  // deterministic per frame
            std::uniform_real_distribution<float> phase (0.0f, kTwoPi);

            // Band of active harmonics rises with the frame position.
            const int loH = 2 + (int) (t * 30.0f);
            const int hiH = loH + 24;
            std::vector<float> amp ((size_t) hiH + 1, 0.0f);
            std::vector<float> phs ((size_t) hiH + 1, 0.0f);
            for (int n = loH; n <= hiH; ++n) { amp[(size_t) n] = 1.0f / std::sqrt ((float) (n - loH + 1)); phs[(size_t) n] = phase (rng); }

            for (int i = 0; i < N; ++i)
            {
                const float p = (float) i / (float) N;
                float s = 0.0f;
                for (int n = loH; n <= hiH; ++n)
                    s += amp[(size_t) n] * std::sin (kTwoPi * (float) n * p + phs[(size_t) n]);
                frames[(size_t) f][(size_t) i] = s;
            }
        }
        auto wt = std::make_unique<Wavetable>();
        wt->buildFromFrames (frames);
        factory.push_back (std::move (wt));
        names.add ("Noise Band");
    }

    // ---- 10: Sub Sines — near-pure fundamental with a touch of 2nd/3rd ----
    addFactory ("Sub Sines", [] (int f, int nf, float ph)
    {
        const float t = (nf > 1) ? (float) f / (float) (nf - 1) : 0.0f;
        return std::sin (kTwoPi * ph)
             + (0.15f + 0.10f * t) * std::sin (kTwoPi * 2.0f * ph)
             + (0.05f * t)         * std::sin (kTwoPi * 3.0f * ph);
    }, kGenFrames);

    // ---- 11: Bitcrush — amplitude quantised, fewer levels across frames ----
    addFactory ("Bitcrush", [] (int f, int nf, float ph)
    {
        const float t      = (nf > 1) ? (float) f / (float) (nf - 1) : 0.0f;
        const float levels = 2.0f + (1.0f - t) * 14.0f;   // 16 -> 2 steps
        const float base   = std::sin (kTwoPi * ph);
        return std::round (base * levels) / levels;
    }, kGenFrames);

    // The factory order must match choices::wavetable() (minus its trailing
    // "User Import" entry), so the wtselect choice param indexes the same tables.
    jassert (names.size() + 1 == choices::wavetable().size());
}

//==============================================================================
juce::String WavetableLibrary::getName (int index) const
{
    if (index >= 0 && index < names.size())
        return names[index];
    return "User Import";
}

const Wavetable* WavetableLibrary::getByIndex (int index) const noexcept
{
    if (index >= 0 && index < (int) factory.size())
        return factory[(size_t) index].get();

    // User slot (or out-of-range): use the imported table, else fall back to the
    // first factory table so a voice always has a valid, non-empty table.
    const Wavetable* user = userTable.load();
    return (user != nullptr) ? user : factory[0].get();
}

//==============================================================================
bool WavetableLibrary::loadFromWav (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels <= 0)
        return false;

    const int N        = Wavetable::kFrameSize;
    const int maxFrames = 256;                                   // cap import size
    const int total    = (int) juce::jmin ((juce::int64) (maxFrames * N), reader->lengthInSamples);
    if (total <= 0)
        return false;

    juce::AudioBuffer<float> buf ((int) reader->numChannels, total);
    reader->read (&buf, 0, total, 0, true, true);

    // Mix down to mono.
    std::vector<float> mono ((size_t) total, 0.0f);
    const int ch = buf.getNumChannels();
    for (int c = 0; c < ch; ++c)
    {
        const float* src = buf.getReadPointer (c);
        for (int i = 0; i < total; ++i)
            mono[(size_t) i] += src[i];
    }
    if (ch > 1)
        for (auto& v : mono) v /= (float) ch;

    // Slice into kFrameSize single-cycle frames (band-limiting happens in build).
    const int nf = juce::jmax (2, total / N);
    std::vector<std::vector<float>> frames;
    frames.reserve ((size_t) nf);
    for (int f = 0; f < nf; ++f)
    {
        std::vector<float> cyc ((size_t) N, 0.0f);
        const int base = f * N;
        for (int i = 0; i < N; ++i)
        {
            const int idx = base + i;
            cyc[(size_t) i] = (idx < total) ? mono[(size_t) idx] : 0.0f;
        }
        frames.push_back (std::move (cyc));
    }

    auto wt = std::make_unique<Wavetable>();
    wt->buildFromFrames (frames);
    const Wavetable* ptr = wt.get();
    ownedUser.push_back (std::move (wt));   // retain so cached pointers stay valid
    userTable.store (ptr);                  // publish atomically to the audio thread
    return true;
}

} // namespace zw
