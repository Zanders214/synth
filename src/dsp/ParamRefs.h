#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"
#include "ModMatrix.h"
#include <atomic>
#include <array>

namespace zw
{

//==============================================================================
// Cached std::atomic<float>* handles to APVTS parameters, so the audio thread
// reads parameter values without per-sample string lookups. Populated once in
// the processor constructor. (Expanded as later milestones read more params.)
//==============================================================================
struct OscRefs
{
    std::atomic<float> *enable {};
    std::atomic<float> *wtselect {};
    std::atomic<float> *wtpos {};
    std::atomic<float> *warp {};
    std::atomic<float> *warpmode {};
    std::atomic<float> *unison {};
    std::atomic<float> *detune {};
    std::atomic<float> *level {};
    std::atomic<float> *pan {};
    std::atomic<float> *octave {};
    std::atomic<float> *coarse {};
    std::atomic<float> *fine {};
    std::atomic<float> *phase {};
    std::atomic<float> *phaserand {};
    std::atomic<float> *uniwidth {};
};

struct ParamRefs
{
    OscRefs a;
    OscRefs b;

    std::atomic<float> *subEnable {};
    std::atomic<float> *subWave {};
    std::atomic<float> *subOctave {};
    std::atomic<float> *subSat {};
    std::atomic<float> *subLevel {};
    std::atomic<float> *noiseEnable {};
    std::atomic<float> *noiseType {};
    std::atomic<float> *noiseColor {};
    std::atomic<float> *noiseLevel {};
    std::atomic<float> *filterEnable {};
    std::atomic<float> *filterType {};
    std::atomic<float> *cutoff {};
    std::atomic<float> *reso {};
    std::atomic<float> *drive {};
    std::atomic<float> *fmix {};
    std::atomic<float> *routeA {};
    std::atomic<float> *routeB {};
    std::atomic<float> *routeS {};
    std::atomic<float> *routeN {};
    std::atomic<float> *env1A {};
    std::atomic<float> *env1D {};
    std::atomic<float> *env1S {};
    std::atomic<float> *env1R {};
    std::atomic<float> *masterOut {};
    std::atomic<float> *bendRange {};
    std::atomic<float> *glideTime {};

    // ---- Modulation sources -------------------------------------------------
    std::atomic<float> *env2A {};
    std::atomic<float> *env2D {};
    std::atomic<float> *env2S {};
    std::atomic<float> *env2R {};
    std::atomic<float> *env3A {};
    std::atomic<float> *env3D {};
    std::atomic<float> *env3S {};
    std::atomic<float> *env3R {};
    std::array<std::atomic<float>*, 4> macro {};

    struct LfoP
    {
        std::atomic<float> *shape {};
        std::atomic<float> *sync {};
        std::atomic<float> *ratehz {};
        std::atomic<float> *ratediv {};
        std::atomic<float> *depth {};
        std::atomic<float> *rise {};
        std::atomic<float> *phase {};
        std::atomic<float> *mode {};
    };
    std::array<LfoP, 4> lfo {};

    // ---- Modulation destinations (base normalised value + range) -----------
    std::array<juce::RangedAudioParameter*, kNumModDests> dest {};

    void prepare (const juce::AudioProcessorValueTreeState& s);

    static bool on (const std::atomic<float>* p) noexcept { return p != nullptr && p->load() >= 0.5f; }
};

} // namespace zw
