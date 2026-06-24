#pragma once

#include <JuceHeader.h>
#include "Wavetable.h"
#include "WavetableOscillator.h"
#include "BasicSources.h"
#include "MultimodeFilter.h"
#include "Envelope.h"
#include "ParamRefs.h"

namespace zw
{

//==============================================================================
struct ZWSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

//==============================================================================
// One polyphonic voice: OSC A + OSC B + SUB + NOISE -> per-source filter routing
// -> stereo multimode filter -> amp VCA (ENV1). Reads live parameters via ParamRefs.
//==============================================================================
class ZWVoice : public juce::SynthesiserVoice
{
public:
    ZWVoice (const ParamRefs& refs, const Wavetable& wt);

    bool canPlaySound (juce::SynthesiserSound* s) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}
    void setCurrentPlaybackSampleRate (double newRate) override;
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    void updateBlockParams();
    static float semis (float octave, float coarse, float fineCents) noexcept
    {
        return octave * 12.0f + coarse + fineCents * 0.01f;
    }

    const ParamRefs& p;
    const Wavetable& table;

    WavetableOscillator oscA, oscB;
    SubOscillator       sub;
    NoiseOscillator     noise;
    MultimodeFilter     filter;
    Envelope            ampEnv;

    double noteFreq = 440.0;
    float  velocity = 1.0f;
    float  pitchBendSemis = 0.0f;
    int    midiNote = 60;

    bool   aOn = true, bOn = true, subOn = true, noiseOn = false;
    bool   filterOn = true, rA = true, rB = true, rS = false, rN = false;
    float  filterMix = 1.0f;
};

} // namespace zw
