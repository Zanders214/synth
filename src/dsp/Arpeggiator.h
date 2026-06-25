#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"
#include <array>
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
    void prepare (double sr)
    {
        sampleRate = sr;
        passthroughBuf.ensureSize (8192);
        outBuf.ensureSize (8192);
        seqScratch.reserve (kMaxSeq);
        baseScratch.reserve (kMaxSeq);
        held.reserve (32);
        activeNotes.reserve (32);
        reset();
    }
    static constexpr int kMaxSeq = 16 * 4 * 2 + 8;   // 16 notes * 4 octaves * up/dn
    void prepareParams (const juce::AudioProcessorValueTreeState& apvts);
    void reset() noexcept;

    // Rewrites `midi` in place; bpm/playing come from the host transport.
    void process (juce::MidiBuffer& midi, int numSamples, double bpm);

private:
    struct Held { int note; juce::uint8 vel; };

    int  buildSequence (std::vector<int>& seqOut);   // returns count (uses scratch members)
    void allNotesOff (juce::MidiBuffer& out, int sample);
    void emitStep (juce::MidiBuffer& out, int sampleOffset);   // fires one arp step

    static bool on (const std::atomic<float>* p) noexcept { return p != nullptr && p->load() >= 0.5f; }
    static float val (const std::atomic<float>* p) noexcept { return p != nullptr ? p->load() : 0.0f; }

    double sampleRate = 44100.0;

    std::vector<Held> held;          // currently physically-held notes (play order)
    std::vector<int>  activeNotes;   // notes the arp is currently sounding

    // Pre-allocated scratch reused every block (no audio-thread allocation).
    juce::MidiBuffer  passthroughBuf;
    juce::MidiBuffer  outBuf;
    std::vector<int>  seqScratch;
    std::vector<int>  baseScratch;

    int    seqIndex = 0;
    int    stepIndex = -1;
    double samplesToNextStep = 0.0;
    double gateSamplesLeft = 0.0;
    double swungLenCurrent = 0.0;   // length of the current step (for emitStep gate calc)
    bool   stepIsEven = true;

    // cached params
    std::atomic<float> *pEnable {};
    std::atomic<float> *pRate {};
    std::atomic<float> *pMode {};
    std::atomic<float> *pOct {};
    std::atomic<float> *pGate {};
    std::atomic<float> *pSwing {};
    std::array<std::atomic<float>*, 16> pStep {};
};

} // namespace zw
