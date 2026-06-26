#include "Wavetable.h"

namespace zw
{

void Wavetable::allocateMips()
{
    constexpr int N       = kFrameSize;   // 2048
    const int maxHarmonic = N / 2;        // 1024

    // Mip levels: full band, then halve harmonic count each level down to 1.
    mips.clear();
    for (int h = maxHarmonic; h >= 1; h /= 2)
    {
        Mip m;
        m.maxHarmonics = h;
        m.data.assign ((size_t) numFrames * N, 0.0f);
        mips.push_back (std::move (m));
    }
}

void Wavetable::generateBasicShapes (int frames)
{
    numFrames = juce::jmax (2, frames);

    constexpr int N        = kFrameSize;          // 2048
    constexpr int fftOrder = 11;                  // 2^11 = 2048
    const int maxHarmonic  = N / 2;               // 1024

    allocateMips();

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

void Wavetable::buildFromFrames (const std::vector<std::vector<float>>& frames)
{
    numFrames = juce::jmax (2, (int) frames.size());

    constexpr int N        = kFrameSize;          // 2048
    constexpr int fftOrder = 11;                  // 2^11 = 2048

    allocateMips();

    juce::dsp::FFT fft (fftOrder);
    std::vector<float> spectrum ((size_t) N * 2, 0.0f);  // interleaved complex bins
    std::vector<float> harmonics ((size_t) N * 2, 0.0f); // cached forward-transform bins
    std::vector<float> cycle ((size_t) N, 0.0f);

    for (int f = 0; f < numFrames; ++f)
    {
        // Resample this frame's source data into one kFrameSize cycle. Frames are
        // single cycles, so linear interpolation across the wrap is sufficient.
        const auto& srcVec = frames[(size_t) juce::jmin (f, (int) frames.size() - 1)];
        const int   srcLen = juce::jmax (1, (int) srcVec.size());
        const float* src   = srcVec.data();
        for (int i = 0; i < N; ++i)
        {
            if (srcLen == N)
            {
                cycle[(size_t) i] = src[(size_t) i];
            }
            else
            {
                const float pos = (float) i / (float) N * (float) srcLen;
                const int   a   = (int) pos % srcLen;
                const int   b   = (a + 1) % srcLen;
                const float fr  = pos - std::floor (pos);
                cycle[(size_t) i] = src[(size_t) a] + (src[(size_t) b] - src[(size_t) a]) * fr;
            }
        }

        // Forward FFT once to obtain this frame's harmonic spectrum, then band-limit
        // per mip by zeroing bins above each mip's harmonic ceiling.
        std::fill (harmonics.begin(), harmonics.end(), 0.0f);
        for (int i = 0; i < N; ++i)
            harmonics[(size_t) i] = cycle[(size_t) i];   // real input, imag = 0
        fft.performRealOnlyForwardTransform (harmonics.data());

        // Mip 0 (full band) first to get this frame's peak for normalisation.
        auto renderMip = [&] (int maxHarmonics, float* dst, float norm)
        {
            std::fill (spectrum.begin(), spectrum.end(), 0.0f);
            // Keep DC + harmonics 1..maxHarmonics (complex bin = [re, im] pair).
            const int keep = juce::jmin (maxHarmonics, N / 2);
            for (int n = 0; n <= keep; ++n)
            {
                spectrum[(size_t) (2 * n)]     = harmonics[(size_t) (2 * n)];
                spectrum[(size_t) (2 * n) + 1] = harmonics[(size_t) (2 * n) + 1];
            }
            fft.performRealOnlyInverseTransform (spectrum.data());
            for (int i = 0; i < N; ++i)
                dst[i] = spectrum[(size_t) i] * norm;
        };

        // Full-band render to measure the peak.
        renderMip (mips[0].maxHarmonics, mips[0].data.data() + (size_t) f * N, 1.0f);
        float peak = 1.0e-9f;
        {
            const float* row0 = mips[0].data.data() + (size_t) f * N;
            for (int i = 0; i < N; ++i)
                peak = juce::jmax (peak, std::abs (row0[i]));
        }
        const float norm = 1.0f / peak;

        // Re-render every mip (including 0) with the shared per-frame normalisation.
        for (size_t mi = 0; mi < mips.size(); ++mi)
            renderMip (mips[mi].maxHarmonics, mips[mi].data.data() + (size_t) f * N, norm);
    }
}

void Wavetable::buildFromGenerator (int frames,
                                    const std::function<float (int, int, float)>& fn)
{
    const int nf = juce::jmax (2, frames);
    std::vector<std::vector<float>> data ((size_t) nf, std::vector<float> ((size_t) kFrameSize, 0.0f));
    for (int f = 0; f < nf; ++f)
        for (int i = 0; i < kFrameSize; ++i)
            data[(size_t) f][(size_t) i] = fn (f, nf, (float) i / (float) kFrameSize);
    buildFromFrames (data);
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
