#include "Wavetable.h"

namespace zw
{

void Wavetable::generateBasicShapes (int frames)
{
    numFrames = juce::jmax (2, frames);

    constexpr int N        = kFrameSize;          // 2048
    constexpr int fftOrder = 11;                  // 2^11 = 2048
    const int maxHarmonic  = N / 2;               // 1024

    // Mip levels: full band, then halve harmonic count each level down to 1.
    std::vector<int> mipHarmonics;
    for (int h = maxHarmonic; h >= 1; h /= 2)
        mipHarmonics.push_back (h);

    mips.clear();
    for (int h : mipHarmonics)
    {
        Mip m;
        m.maxHarmonics = h;
        m.data.assign ((size_t) numFrames * N, 0.0f);
        mips.push_back (std::move (m));
    }

    juce::dsp::FFT fft (fftOrder);
    std::vector<float> spectrum ((size_t) N * 2, 0.0f);  // interleaved complex bins

    for (int f = 0; f < numFrames; ++f)
    {
        const float t = (numFrames > 1) ? (float) f / (float) (numFrames - 1) : 0.0f;
        // Harmonic limit grows with frame position: sine (1 harmonic) -> full saw.
        const float harmLimit = 1.0f + t * (float) (maxHarmonic - 1);

        // Target saw-ish spectrum a[n] = (1/n) tapered to the per-frame harmonic limit.
        auto amp = [&] (int n)
        {
            const float edge = juce::jlimit (0.0f, 1.0f, harmLimit - (float) n + 1.0f);
            return (1.0f / (float) n) * edge;
        };

        // Render mip 0 (full band) first to get this frame's peak for normalisation.
        std::fill (spectrum.begin(), spectrum.end(), 0.0f);
        for (int n = 1; n <= mips[0].maxHarmonics; ++n)
            spectrum[(size_t) (2 * n) + 1] = amp (n);     // imaginary => sine phase
        fft.performRealOnlyInverseTransform (spectrum.data());

        float peak = 1.0e-9f;
        for (int i = 0; i < N; ++i)
            peak = juce::jmax (peak, std::abs (spectrum[(size_t) i]));
        const float norm = 1.0f / peak;

        float* row0 = mips[0].data.data() + (size_t) f * N;
        for (int i = 0; i < N; ++i)
            row0[i] = spectrum[(size_t) i] * norm;

        // Remaining mips: same spectrum, fewer harmonics, same per-frame normalisation.
        for (size_t mi = 1; mi < mips.size(); ++mi)
        {
            std::fill (spectrum.begin(), spectrum.end(), 0.0f);
            for (int n = 1; n <= mips[mi].maxHarmonics; ++n)
                spectrum[(size_t) (2 * n) + 1] = amp (n);
            fft.performRealOnlyInverseTransform (spectrum.data());

            float* row = mips[mi].data.data() + (size_t) f * N;
            for (int i = 0; i < N; ++i)
                row[i] = spectrum[(size_t) i] * norm;
        }
    }
}

int Wavetable::mipForFreq (double freqHz, double sampleRate) const noexcept
{
    // Pick the most-detailed mip whose top harmonic stays under ~Nyquist.
    const double nyquist = sampleRate * 0.5 * 0.95;
    const double maxHarm = (freqHz > 1.0) ? nyquist / freqHz : 1.0e9;

    for (size_t m = 0; m < mips.size(); ++m)
        if ((double) mips[m].maxHarmonics <= maxHarm)
            return (int) m;

    return (int) mips.size() - 1;   // very high note: fewest harmonics
}

Wavetable::Cursor Wavetable::makeCursor (float framePos01, double freqHz, double sampleRate) const noexcept
{
    Cursor c;
    if (mips.empty() || numFrames == 0)
        return c;

    const int m = mipForFreq (freqHz, sampleRate);
    const float* base = mips[(size_t) m].data.data();

    const float framePos = juce::jlimit (0.0f, 1.0f, framePos01) * (float) (numFrames - 1);
    const auto  f0 = (int) framePos;
    const int   f1 = juce::jmin (f0 + 1, numFrames - 1);
    c.ff = framePos - (float) f0;
    c.r0 = base + (size_t) f0 * kFrameSize;
    c.r1 = base + (size_t) f1 * kFrameSize;
    return c;
}

float Wavetable::getSample (float framePos01, float phase01, double freqHz, double sampleRate) const noexcept
{
    return makeCursor (framePos01, freqHz, sampleRate).read (phase01);
}

} // namespace zw
