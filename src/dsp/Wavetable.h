#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>

namespace zw
{

//==============================================================================
// A band-limited wavetable: N single-cycle frames, each pre-rendered at several
// mip levels (decreasing harmonic content). Playback picks the mip whose top
// harmonic stays below Nyquist for the note's fundamental, which is what keeps
// high notes alias-free. Frames are interpolated (WT Position) and samples are
// linearly interpolated within a frame.
//==============================================================================
class Wavetable
{
public:
    static constexpr int kFrameSize = 2048;

    Wavetable() = default;

    // Procedural placeholder library: a sine -> saw morph across frames. Real
    // .zwt libraries replace this in M6.
    void generateBasicShapes (int numFrames = 64);

    int getNumFrames() const noexcept { return numFrames; }
    bool isEmpty()     const noexcept { return numFrames == 0; }

    // framePos01: 0..1 across frames. phase01: 0..1 within a cycle.
    float getSample (float framePos01, float phase01, double freqHz, double sampleRate) const noexcept;

    // Per-block reader: the mip selection and frame interpolation depend only on
    // (framePos, freq), which are constant within an audio block. makeCursor() does
    // that work once; Cursor::read() then samples per output-sample cheaply (no mip
    // search, no frame-index math) — the hot oscillator inner loop.
    struct Cursor
    {
        const float* r0 = nullptr;   // chosen mip, lower interpolated frame
        const float* r1 = nullptr;   // upper interpolated frame
        float        ff = 0.0f;      // frame blend 0..1

        float read (float phase01) const noexcept
        {
            if (r0 == nullptr) return 0.0f;
            const float ph  = phase01 - std::floor (phase01);
            const float pos = ph * (float) kFrameSize;
            const auto  i0  = (int) pos;
            const int   i1  = (i0 + 1) & (kFrameSize - 1);
            const float pf  = pos - (float) i0;
            const float s0  = r0[i0] + (r0[i1] - r0[i0]) * pf;
            const float s1  = r1[i0] + (r1[i1] - r1[i0]) * pf;
            return s0 + (s1 - s0) * ff;
        }
    };

    Cursor makeCursor (float framePos01, double freqHz, double sampleRate) const noexcept;

private:
    struct Mip { int maxHarmonics = 0; std::vector<float> data; }; // numFrames * kFrameSize, frame-major
    std::vector<Mip> mips;
    int numFrames = 0;

    int mipForFreq (double freqHz, double sampleRate) const noexcept;
};

} // namespace zw
