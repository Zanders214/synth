#include "ParamRefs.h"

namespace zw
{

static void prepOsc (OscRefs& r, juce::AudioProcessorValueTreeState& s, char ab)
{
    r.enable    = s.getRawParameterValue (id::osc (ab, "enable"));
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

void ParamRefs::prepare (juce::AudioProcessorValueTreeState& s)
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
}

} // namespace zw
