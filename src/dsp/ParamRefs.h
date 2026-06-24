#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"
#include "ModMatrix.h"
#include <atomic>

namespace zw
{

//==============================================================================
// Cached std::atomic<float>* handles to APVTS parameters, so the audio thread
// reads parameter values without per-sample string lookups. Populated once in
// the processor constructor. (Expanded as later milestones read more params.)
//==============================================================================
struct OscRefs
{
    std::atomic<float> *enable {}, *wtpos {}, *warp {}, *unison {}, *detune {},
                       *level {}, *pan {}, *octave {}, *coarse {}, *fine {},
                       *phase {}, *phaserand {}, *uniwidth {};
};

struct ParamRefs
{
    OscRefs a, b;

    std::atomic<float> *subEnable {}, *subWave {}, *subOctave {}, *subSat {}, *subLevel {};
    std::atomic<float> *noiseEnable {}, *noiseType {}, *noiseColor {}, *noiseLevel {};
    std::atomic<float> *filterEnable {}, *filterType {}, *cutoff {}, *reso {}, *drive {},
                       *fmix {}, *routeA {}, *routeB {}, *routeS {}, *routeN {};
    std::atomic<float> *env1A {}, *env1D {}, *env1S {}, *env1R {};
    std::atomic<float> *masterOut {}, *bendRange {}, *glideTime {};

    // ---- Modulation sources -------------------------------------------------
    std::atomic<float> *env2A {}, *env2D {}, *env2S {}, *env2R {};
    std::atomic<float> *env3A {}, *env3D {}, *env3S {}, *env3R {};
    std::atomic<float> *macro[4] {};

    struct LfoP
    {
        std::atomic<float> *shape {}, *sync {}, *ratehz {}, *ratediv {},
                           *depth {}, *rise {}, *phase {}, *mode {};
    };
    LfoP lfo[4];

    // ---- Modulation destinations (base normalised value + range) -----------
    juce::RangedAudioParameter* dest[kNumModDests] {};

    void prepare (juce::AudioProcessorValueTreeState& s);

    static bool on (const std::atomic<float>* p) noexcept { return p != nullptr && p->load() >= 0.5f; }
};

} // namespace zw
