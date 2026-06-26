#pragma once

#include <JuceHeader.h>
#include "Wavetable.h"
#include "WavetableLibrary.h"
#include "WavetableOscillator.h"
#include "BasicSources.h"
#include "MultimodeFilter.h"
#include "Envelope.h"
#include "Lfo.h"
#include "ModMatrix.h"
#include "ParamRefs.h"
#include "RtSafety.h"
#include <array>
#include <atomic>

namespace zw
{

//==============================================================================
struct ZWSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

//==============================================================================
// One polyphonic voice. Sources -> per-source filter routing -> stereo filter
// -> amp VCA (ENV1). ENV2/3, LFO1-4, macros, velocity and note feed the mod
// matrix, which is applied (per block) on top of each destination's base value.
//==============================================================================
class ZWVoice : public juce::SynthesiserVoice
{
public:
    ZWVoice (const ParamRefs& refs, const WavetableLibrary& library,
             const ModMatrix& matrix, const std::atomic<double>& bpm,
             std::atomic<double>& lastNoteFreq);

    bool canPlaySound (juce::SynthesiserSound* s) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override { /* No MIDI CC handling: modulation is driven by the mod matrix. */ }
    void setCurrentPlaybackSampleRate (double newRate) override;
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) ZW_RT_NONBLOCKING override;
    using juce::SynthesiserVoice::renderNextBlock;

private:
    void updateBlockParams (int numSamples);
    float lfoFreqHz (int i) const;
    static float semis (float octave, float coarse, float fineCents) noexcept
    {
        return octave * 12.0f + coarse + fineCents * 0.01f;
    }
    static float clamp01 (float x) noexcept
    {
        const float upper = x > 1.0f ? 1.0f : x;
        return x < 0.0f ? 0.0f : upper;
    }

    const ParamRefs& p;
    const WavetableLibrary& library;   // OSC A/B each pick a table from here (per block)
    const ModMatrix& matrix;
    const std::atomic<double>& bpmRef;
    std::atomic<double>& lastNoteFreqRef;

    WavetableOscillator oscA;
    WavetableOscillator oscB;
    SubOscillator       sub;
    NoiseOscillator     noise;
    MultimodeFilter     filter;
    Envelope            ampEnv;
    Envelope            env2;
    Envelope            env3;
    std::array<Lfo, 4>  lfo;

    double noteFreq = 440.0;     // gliding (sounding) frequency
    double targetFreq = 440.0;   // destination pitch
    float  velocity = 1.0f;
    float  rawVelocity = 1.0f;
    float  noteNorm = 0.0f;
    float  pitchBendSemis = 0.0f;
    int    midiNote = 60;

    bool   aOn = true;
    bool   bOn = true;
    bool   subOn = true;
    bool   noiseOn = false;
    bool   filterOn = true;
    bool   rA = true;
    bool   rB = true;
    bool   rS = false;
    bool   rN = false;
    float  filterMix = 1.0f;
};

} // namespace zw
