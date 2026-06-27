#include "WavetableLibrary.h"
#include "../Parameters.h"

#include <array>
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

    // Normalised frame position 0..1 across a table's frames.
    inline float frameT (int f, int nf) noexcept
    {
        return (nf > 1) ? (float) f / (float) (nf - 1) : 0.0f;
    }

    //==========================================================================
    // Analytic per-frame generators: (frame, numFrames, phase01) -> sample in
    // roughly [-1, 1]. Each is its plainest shape at frame 0 and intensifies
    // across frames. Kept as named free functions (rather than inline lambdas in
    // buildFactory) so each table reads on its own and buildFactory stays flat.

    float genPwmSquare (int f, int nf, float ph) noexcept       // duty narrows 0.5 -> 0.05
    {
        const float duty = 0.5f - 0.45f * frameT (f, nf);
        return (ph < duty) ? 1.0f : -1.0f;
    }

    float genAdditiveSweep (int f, int nf, float ph) noexcept   // Gaussian harmonic emphasis slides up
    {
        const float peakH = 1.0f + frameT (f, nf) * 31.0f;      // emphasised harmonic 1..32
        float s = 0.0f;
        for (int n = 1; n <= 48; ++n)
        {
            const float a = std::exp (-0.5f * (((float) n - peakH) / 6.0f) * (((float) n - peakH) / 6.0f));
            s += a * std::sin (kTwoPi * (float) n * ph);
        }
        return s;
    }

    float genFmBell (int f, int nf, float ph) noexcept          // modulation index grows (ratio 3)
    {
        const float I = 0.5f + 6.0f * frameT (f, nf);
        return std::sin (kTwoPi * ph + I * std::sin (kTwoPi * 3.0f * ph));
    }

    float genFormantVocal (int f, int nf, float ph) noexcept    // two shifting formant peaks (vowel morph)
    {
        const float t  = frameT (f, nf);
        const float f1 = 3.0f + 9.0f * t;                       // first formant harmonic
        const float f2 = 12.0f + 12.0f * t;                     // second formant harmonic
        float s = 0.0f;
        for (int n = 1; n <= 40; ++n)
        {
            const float a1 = std::exp (-0.5f * (((float) n - f1) / 2.5f) * (((float) n - f1) / 2.5f));
            const float a2 = 0.6f * std::exp (-0.5f * (((float) n - f2) / 3.0f) * (((float) n - f2) / 3.0f));
            s += (a1 + a2) * std::sin (kTwoPi * (float) n * ph);
        }
        return s;
    }

    float genSawStack (int f, int nf, float ph) noexcept        // phase-spread ramps (supersaw-like comb)
    {
        const float spread = 0.004f + 0.03f * frameT (f, nf);
        float s = 0.0f;
        for (int k = -2; k <= 2; ++k)
            s += ramp (ph + (float) k * spread);
        return s * 0.4f;
    }

    float genSyncSaw (int f, int nf, float ph) noexcept         // hard-sync ratio rises (more teeth)
    {
        const float ratio = 1.0f + 4.0f * frameT (f, nf);
        return ramp (ph * ratio);
    }

    float genMetallic (int f, int nf, float ph) noexcept        // rounded inharmonic partials, bell-like decay
    {
        const float t = frameT (f, nf);
        static constexpr std::array<int,   7> partials { 1, 3, 5, 9, 13, 19, 27 };
        static constexpr std::array<float, 7> decays   { 1.0f, 0.7f, 0.55f, 0.4f, 0.3f, 0.22f, 0.16f };
        float s = 0.0f;
        for (size_t k = 0; k < partials.size(); ++k)
        {
            // Brightness moves up the partial stack as the frame advances.
            const float em = 0.4f + 0.6f * std::exp (-2.0f * std::abs (t - (float) k / 6.0f));
            s += decays[k] * em * std::sin (kTwoPi * (float) partials[k] * ph);
        }
        return s;
    }

    float genPulseTrain (int f, int nf, float ph) noexcept      // periodic pulse sharpens across frames
    {
        const float exponent = 1.0f + 60.0f * frameT (f, nf);   // higher -> narrower pulse
        const float bump = 0.5f + 0.5f * std::cos (kTwoPi * ph); // 1 at ph=0
        return std::pow (bump, exponent);
    }

    float genSubSines (int f, int nf, float ph) noexcept        // near-pure fundamental + a touch of 2nd/3rd
    {
        const float t = frameT (f, nf);
        return std::sin (kTwoPi * ph)
             + (0.15f + 0.10f * t) * std::sin (kTwoPi * 2.0f * ph)
             + (0.05f * t)         * std::sin (kTwoPi * 3.0f * ph);
    }

    float genBitcrush (int f, int nf, float ph) noexcept        // amplitude quantised, fewer levels across frames
    {
        const float levels = 2.0f + (1.0f - frameT (f, nf)) * 14.0f;  // 16 -> 2 steps
        const float base   = std::sin (kTwoPi * ph);
        return std::round (base * levels) / levels;
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

// Noise Band (factory table 9): fixed-seed random harmonic phases in a band that
// rises with the frame. Built directly from per-frame data (not a single-sample
// generator), so it lives in its own helper rather than inline in buildFactory.
void WavetableLibrary::addNoiseBand()
{
    const int N = Wavetable::kFrameSize;
    std::vector<std::vector<float>> frames ((size_t) kGenFrames, std::vector<float> ((size_t) N, 0.0f));
    for (int f = 0; f < kGenFrames; ++f)
    {
        const float t = frameT (f, kGenFrames);
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

    // ---- 1..8: analytic single-sample generators (defined above) ----
    addFactory ("PWM Square",     genPwmSquare,     kGenFrames);
    addFactory ("Additive Sweep", genAdditiveSweep, kGenFrames);
    addFactory ("FM Bell",        genFmBell,        kGenFrames);
    addFactory ("Formant Vocal",  genFormantVocal,  kGenFrames);
    addFactory ("Saw Stack",      genSawStack,      kGenFrames);
    addFactory ("Sync Saw",       genSyncSaw,       kGenFrames);
    addFactory ("Metallic",       genMetallic,      kGenFrames);
    addFactory ("Pulse Train",    genPulseTrain,    kGenFrames);

    // ---- 9: Noise Band (built from per-frame data) ----
    addNoiseBand();

    // ---- 10, 11 ----
    addFactory ("Sub Sines",      genSubSines,      kGenFrames);
    addFactory ("Bitcrush",       genBitcrush,      kGenFrames);

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
    const auto total    = (int) juce::jmin ((juce::int64) (maxFrames * N), reader->lengthInSamples);
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
