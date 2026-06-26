#include "ParamRefs.h"

namespace zw
{

static void prepOsc (OscRefs& r, const juce::AudioProcessorValueTreeState& s, char ab)
{
    r.enable    = s.getRawParameterValue (id::osc (ab, "enable"));
    r.wtselect  = s.getRawParameterValue (id::osc (ab, "wtselect"));
    r.wtpos     = s.getRawParameterValue (id::osc (ab, "wtpos"));
    r.warp      = s.getRawParameterValue (id::osc (ab, "warp"));
    r.unison    = s.getRawParameterValue (id::osc (ab, "unison"));
    r.detune    = s.getRawParameterValue (id::osc (ab, "detune"));
    r.level     = s.getRawParameterValue (id::osc (ab, "level"));
    r.pan       = s.getRawParameterValue (id::osc (ab, "pan"));
    r.octave    = s.getRawParameterValue (id::osc (ab, "octave"));
    r.coarse    = s.getRawParameterValue (id::osc (ab, "coarse"));
    r.fine      = s.getRawParameterValue (id::osc (ab, "fine"));
    r.phase     = s.getRawParameterValue (id::osc (ab, "phase"));
    r.phaserand = s.getRawParameterValue (id::osc (ab, "phaserand"));
    r.uniwidth  = s.getRawParameterValue (id::osc (ab, "uniwidth"));
}

void ParamRefs::prepare (const juce::AudioProcessorValueTreeState& s)
{
    prepOsc (a, s, 'A');
    prepOsc (b, s, 'B');

    subEnable = s.getRawParameterValue (id::subEnable);
    subWave   = s.getRawParameterValue (id::subWave);
    subOctave = s.getRawParameterValue (id::subOctave);
    subSat    = s.getRawParameterValue (id::subSaturate);
    subLevel  = s.getRawParameterValue (id::subLevel);

    noiseEnable = s.getRawParameterValue (id::noiseEnable);
    noiseType   = s.getRawParameterValue (id::noiseType);
    noiseColor  = s.getRawParameterValue (id::noiseColor);
    noiseLevel  = s.getRawParameterValue (id::noiseLevel);

    filterEnable = s.getRawParameterValue (id::filterEnable);
    filterType   = s.getRawParameterValue (id::filterType);
    cutoff       = s.getRawParameterValue (id::filterCutoff);
    reso         = s.getRawParameterValue (id::filterReso);
    drive        = s.getRawParameterValue (id::filterDrive);
    fmix         = s.getRawParameterValue (id::filterMix);
    routeA       = s.getRawParameterValue (id::filterRouteA);
    routeB       = s.getRawParameterValue (id::filterRouteB);
    routeS       = s.getRawParameterValue (id::filterRouteS);
    routeN       = s.getRawParameterValue (id::filterRouteN);

    env1A = s.getRawParameterValue (id::env (1, "attack"));
    env1D = s.getRawParameterValue (id::env (1, "decay"));
    env1S = s.getRawParameterValue (id::env (1, "sustain"));
    env1R = s.getRawParameterValue (id::env (1, "release"));

    masterOut = s.getRawParameterValue (id::masterOut);
    bendRange = s.getRawParameterValue (id::bendRange);
    glideTime = s.getRawParameterValue (id::glideTime);

    // ---- Modulation sources ----
    env2A = s.getRawParameterValue (id::env (2, "attack"));
    env2D = s.getRawParameterValue (id::env (2, "decay"));
    env2S = s.getRawParameterValue (id::env (2, "sustain"));
    env2R = s.getRawParameterValue (id::env (2, "release"));
    env3A = s.getRawParameterValue (id::env (3, "attack"));
    env3D = s.getRawParameterValue (id::env (3, "decay"));
    env3S = s.getRawParameterValue (id::env (3, "sustain"));
    env3R = s.getRawParameterValue (id::env (3, "release"));

    for (int i = 0; i < 4; ++i)
        macro[i] = s.getRawParameterValue (id::macro (i + 1));

    for (int i = 0; i < 4; ++i)
    {
        const int n = i + 1;
        lfo[i].shape   = s.getRawParameterValue (id::lfo (n, "shape"));
        lfo[i].sync    = s.getRawParameterValue (id::lfo (n, "sync"));
        lfo[i].ratehz  = s.getRawParameterValue (id::lfo (n, "ratehz"));
        lfo[i].ratediv = s.getRawParameterValue (id::lfo (n, "ratediv"));
        lfo[i].depth   = s.getRawParameterValue (id::lfo (n, "depth"));
        lfo[i].rise    = s.getRawParameterValue (id::lfo (n, "rise"));
        lfo[i].phase   = s.getRawParameterValue (id::lfo (n, "phase"));
        lfo[i].mode    = s.getRawParameterValue (id::lfo (n, "mode"));
    }

    // ---- Modulation destinations (RangedAudioParameter for base + range) ----
    dest[(int) ModDest::OscAWt]     = s.getParameter (id::osc ('A', "wtpos"));
    dest[(int) ModDest::OscAWarp]   = s.getParameter (id::osc ('A', "warp"));
    dest[(int) ModDest::OscALevel]  = s.getParameter (id::osc ('A', "level"));
    dest[(int) ModDest::OscAPan]    = s.getParameter (id::osc ('A', "pan"));
    dest[(int) ModDest::OscADetune] = s.getParameter (id::osc ('A', "detune"));
    dest[(int) ModDest::OscBWt]     = s.getParameter (id::osc ('B', "wtpos"));
    dest[(int) ModDest::OscBWarp]   = s.getParameter (id::osc ('B', "warp"));
    dest[(int) ModDest::OscBLevel]  = s.getParameter (id::osc ('B', "level"));
    dest[(int) ModDest::OscBPan]    = s.getParameter (id::osc ('B', "pan"));
    dest[(int) ModDest::OscBDetune] = s.getParameter (id::osc ('B', "detune"));
    dest[(int) ModDest::SubLevel]   = s.getParameter (id::subLevel);
    dest[(int) ModDest::NoiseLevel] = s.getParameter (id::noiseLevel);
    dest[(int) ModDest::Cutoff]     = s.getParameter (id::filterCutoff);
    dest[(int) ModDest::Reso]       = s.getParameter (id::filterReso);
    dest[(int) ModDest::Drive]      = s.getParameter (id::filterDrive);
}

} // namespace zw
