#pragma once

#include <JuceHeader.h>
#include <vector>

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

private:
    struct Mip { int maxHarmonics = 0; std::vector<float> data; }; // numFrames * kFrameSize, frame-major
    std::vector<Mip> mips;
    int numFrames = 0;

    int mipForFreq (double freqHz, double sampleRate) const noexcept;
};

} // namespace zw
