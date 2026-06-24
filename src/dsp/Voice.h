#pragma once

#include <JuceHeader.h>
#include "Wavetable.h"
#include "WavetableOscillator.h"
#include "BasicSources.h"
#include "MultimodeFilter.h"
#include "Envelope.h"
#include "Lfo.h"
#include "ModMatrix.h"
#include "ParamRefs.h"
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
    ZWVoice (const ParamRefs& refs, const Wavetable& wt,
             const ModMatrix& matrix, const std::atomic<double>& bpm,
             std::atomic<double>& lastNoteFreq);

    bool canPlaySound (juce::SynthesiserSound* s) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}
    void setCurrentPlaybackSampleRate (double newRate) override;
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    void updateBlockParams (int numSamples);
    float lfoFreqHz (int i) const;
    static float semis (float octave, float coarse, float fineCents) noexcept
    {
        return octave * 12.0f + coarse + fineCents * 0.01f;
    }
    static float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

    const ParamRefs& p;
    const Wavetable& table;
    const ModMatrix& matrix;
    const std::atomic<double>& bpmRef;
    std::atomic<double>& lastNoteFreqRef;

    WavetableOscillator oscA, oscB;
    SubOscillator       sub;
    NoiseOscillator     noise;
    MultimodeFilter     filter;
    Envelope            ampEnv, env2, env3;
    Lfo                 lfo[4];

    double noteFreq = 440.0;     // gliding (sounding) frequency
    double targetFreq = 440.0;   // destination pitch
    float  velocity = 1.0f, rawVelocity = 1.0f, noteNorm = 0.0f;
    float  pitchBendSemis = 0.0f;
    int    midiNote = 60;

    bool   aOn = true, bOn = true, subOn = true, noiseOn = false;
    bool   filterOn = true, rA = true, rB = true, rS = false, rN = false;
    float  filterMix = 1.0f;
};

} // namespace zw
