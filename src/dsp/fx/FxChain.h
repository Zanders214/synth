#pragma once

#include <JuceHeader.h>
#include "Effects.h"
#include "../RtSafety.h"
#include <atomic>

namespace zw
{

//==============================================================================
// The global, post-voice-mix effects rack: 10 serial bypassable slots in the
// fixed README order. Caches its parameter atomics and processes the stereo
// master block in place, skipping disabled slots.
//==============================================================================
class FxChain
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void prepareParams (juce::AudioProcessorValueTreeState& apvts);
    void reset();
    void process (juce::dsp::AudioBlock<float>& block) ZW_RT_NONBLOCKING;

private:
    static bool on (const std::atomic<float>* p) noexcept { return p != nullptr && p->load() >= 0.5f; }
    static float val (const std::atomic<float>* p) noexcept { return p != nullptr ? p->load() : 0.0f; }

    fx::Hyper    hyper;
    fx::Distort  distort;
    fx::Flanger  flanger;
    fx::Phaser   phaser;
    fx::Chorus   chorus;
    fx::Delay    delay;
    fx::Reverb   reverb;
    fx::Comp     comp;
    fx::Eq       eq;
    fx::FilterFx filterFx;

    struct P
    {
        std::atomic<float>* enable {};
        std::atomic<float>* p1 {};
        std::atomic<float>* p2 {};
        std::atomic<float>* p3 {};
        std::atomic<float>* p4 {};
        std::atomic<float>* p5 {};
    };
    P hy;
    P di;
    P fl;
    P ph;
    P ch;
    P dl;
    P rv;
    P cp;
    P eqp;
    P fi;
};

} // namespace zw
