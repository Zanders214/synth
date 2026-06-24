#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"
#include <vector>
#include <atomic>

namespace zw
{

//==============================================================================
// MIDI arpeggiator. Runs before voice allocation: when enabled it consumes held
// notes and emits its own note on/off stream stepped to the host tempo (16-step
// on/off pattern, rate incl. triplets, mode, octave range, gate, swing). When
// disabled it passes MIDI through unchanged. Non-note messages always pass.
//==============================================================================
class Arpeggiator
{
public:
    void prepare (double sr) noexcept { sampleRate = sr; reset(); }
    void prepareParams (juce::AudioProcessorValueTreeState& apvts);
    void reset() noexcept;

    // Rewrites `midi` in place; bpm/playing come from the host transport.
    void process (juce::MidiBuffer& midi, int numSamples, double bpm);

private:
    struct Held { int note; juce::uint8 vel; };

    int  buildSequence (std::vector<int>& seqOut) const;   // returns count
    void allNotesOff (juce::MidiBuffer& out, int sample);

    static bool on (const std::atomic<float>* p) noexcept { return p != nullptr && p->load() >= 0.5f; }
    static float val (const std::atomic<float>* p) noexcept { return p != nullptr ? p->load() : 0.0f; }

    double sampleRate = 44100.0;

    std::vector<Held> held;          // currently physically-held notes (play order)
    std::vector<int>  activeNotes;   // notes the arp is currently sounding
    int    seqIndex = 0;
    int    stepIndex = -1;
    double samplesToNextStep = 0.0;
    double gateSamplesLeft = 0.0;
    bool   stepIsEven = true;

    // cached params
    std::atomic<float> *pEnable {}, *pRate {}, *pMode {}, *pOct {}, *pGate {}, *pSwing {};
    std::atomic<float> *pStep[16] {};
};

} // namespace zw
