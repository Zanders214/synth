#include "Parameters.h"

namespace zw
{

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using APF    = juce::AudioParameterFloat;
using API    = juce::AudioParameterInt;
using APB    = juce::AudioParameterBool;
using APC    = juce::AudioParameterChoice;

//==============================================================================
juce::NormalisableRange<float> makeLogRange (float lo, float hi)
{
    return juce::NormalisableRange<float> (lo, hi,
        [] (float s, float e, float t) { return s * std::pow (e / s, t); },
        [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); });
}

//==============================================================================
namespace
{
    // AudioParameterBool::getValue() returns the raw (un-snapped) normalised
    // value, so APVTS can leave a stale fractional value on state restore (it
    // skips the update when the boolean interpretation is unchanged). Snapping
    // getValue() to 0/1 makes bool params round-trip cleanly (pluginval @ 10).
    struct SnappingBool : public juce::AudioParameterBool
    {
        using juce::AudioParameterBool::AudioParameterBool;
        float getValue() const override { return get() ? 1.0f : 0.0f; }
    };

    // --- small typed helpers to keep the layout declarative -----------------
    void addF (Layout& l, const juce::String& id, const juce::String& name,
               juce::NormalisableRange<float> range, float def, const juce::String& unit = {})
    {
        l.add (std::make_unique<APF> (juce::ParameterID { id, 1 }, name, range, def,
                                      juce::AudioParameterFloatAttributes().withLabel (unit)));
    }

    void addPct (Layout& l, const juce::String& id, const juce::String& name, float def)
    {
        addF (l, id, name, juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), def);
    }

    void addChoice (Layout& l, const juce::String& id, const juce::String& name,
                    juce::StringArray opts, int def)
    {
        l.add (std::make_unique<APC> (juce::ParameterID { id, 1 }, name, std::move (opts), def));
    }

    void addBool (Layout& l, const juce::String& id, const juce::String& name, bool def)
    {
        l.add (std::make_unique<SnappingBool> (juce::ParameterID { id, 1 }, name, def));
    }

    void addInt (Layout& l, const juce::String& id, const juce::String& name, int lo, int hi, int def)
    {
        l.add (std::make_unique<API> (juce::ParameterID { id, 1 }, name, lo, hi, def));
    }

    juce::NormalisableRange<float> timeRange()  { return { 0.0f, 8.0f, 0.0f, 0.3f }; }  // exp-ish ADSR seconds
    juce::NormalisableRange<float> riseRange()  { return { 0.0f, 4.0f, 0.0f, 0.4f }; }
    juce::NormalisableRange<float> pan()        { return { -1.0f, 1.0f, 0.0001f }; }
    juce::NormalisableRange<float> deg()        { return { 0.0f, 360.0f, 0.1f }; }

    struct OscDefaults { bool en; float wt; float warp; int uni; float det; float lvl; float pan; int oct; };

    void addOsc (Layout& l, char ab, const OscDefaults& d)
    {
        const juce::String name = juce::String ("OSC ") + ab + " ";
        addBool   (l, id::osc (ab, "enable"),    name + "Enable",    d.en);
        addPct    (l, id::osc (ab, "wtpos"),     name + "WT Pos",    d.wt);
        addPct    (l, id::osc (ab, "warp"),      name + "Warp",      d.warp);
        addChoice (l, id::osc (ab, "warpmode"),  name + "Warp Mode",
                   { "Off", "Sync", "Bend", "PWM", "Asym", "Remap", "Quantize" }, 0);
        addInt    (l, id::osc (ab, "unison"),    name + "Unison",    1, 16, d.uni);
        addF      (l, id::osc (ab, "detune"),    name + "Detune",    { 0.0f, 100.0f, 0.01f }, d.det, "ct");
        addPct    (l, id::osc (ab, "uniblend"),  name + "Uni Blend", 0.5f);
        addPct    (l, id::osc (ab, "uniwidth"),  name + "Uni Width", 0.5f);
        addPct    (l, id::osc (ab, "level"),     name + "Level",     d.lvl);
        addF      (l, id::osc (ab, "pan"),       name + "Pan",       pan(), d.pan);
        addF      (l, id::osc (ab, "phase"),     name + "Phase",     deg(), 0.0f, "deg");
        addBool   (l, id::osc (ab, "phaserand"), name + "Phase Rand", false);
        addInt    (l, id::osc (ab, "octave"),    name + "Octave",    -3, 3, d.oct);
        addInt    (l, id::osc (ab, "coarse"),    name + "Coarse",    -12, 12, 0);
        addF      (l, id::osc (ab, "fine"),      name + "Fine",      { -100.0f, 100.0f, 0.01f }, 0.0f, "ct");
    }

    struct AdsrDefaults { float a; float d; float s; float r; };

    void addEnv (Layout& l, int n, const AdsrDefaults& d)
    {
        const juce::String name = "ENV" + juce::String (n) + " ";
        addF   (l, id::env (n, "attack"),  name + "Attack",  timeRange(), d.a, "s");
        addF   (l, id::env (n, "decay"),   name + "Decay",   timeRange(), d.d, "s");
        addPct (l, id::env (n, "sustain"), name + "Sustain", d.s);
        addF   (l, id::env (n, "release"), name + "Release", timeRange(), d.r, "s");
    }

    void addLfo (Layout& l, int n, float depthDef)
    {
        const juce::String name = "LFO" + juce::String (n) + " ";
        addChoice (l, id::lfo (n, "shape"),  name + "Shape", choices::lfoShape(), 0);
        addBool   (l, id::lfo (n, "sync"),   name + "Sync",  true);
        addF      (l, id::lfo (n, "ratehz"), name + "Rate",  makeLogRange (0.01f, 40.0f), 2.0f, "Hz");
        addChoice (l, id::lfo (n, "ratediv"),name + "Div",   choices::syncDiv(), 3);   // 1/8
        addPct    (l, id::lfo (n, "depth"),  name + "Depth", depthDef);
        addF      (l, id::lfo (n, "rise"),   name + "Rise",  riseRange(), 0.0f, "s");
        addF      (l, id::lfo (n, "phase"),  name + "Phase", deg(), 0.0f, "deg");
        addChoice (l, id::lfo (n, "mode"),   name + "Mode",  choices::lfoMode(), 0);
    }
}

//==============================================================================
Layout createParameterLayout()
{
    Layout l;

    // ---- Oscillators A & B -------------------------------------------------
    addOsc (l, 'A', { true,  0.30f, 0.42f, 4, 18.0f, 0.82f, 0.0f, 0 });
    addOsc (l, 'B', { true,  0.62f, 0.24f, 1, 30.0f, 0.00f, 0.0f, 0 });

    // ---- Sub oscillator ----------------------------------------------------
    addBool   (l, id::subEnable,   "Sub Enable",   true);
    addChoice (l, id::subWave,     "Sub Wave",     choices::subWave(), 0);
    addInt    (l, id::subOctave,   "Sub Octave",   -2, 2, -1);
    addPct    (l, id::subSaturate, "Sub Saturate", 0.22f);
    addPct    (l, id::subLevel,    "Sub Level",    0.46f);

    // ---- Noise oscillator --------------------------------------------------
    addBool   (l, id::noiseEnable, "Noise Enable", false);
    addChoice (l, id::noiseType,   "Noise Type",   choices::noiseType(), 0);
    addPct    (l, id::noiseColor,  "Noise Color",  0.40f);
    addPct    (l, id::noiseLevel,  "Noise Level",  0.00f);

    // ---- Filter ------------------------------------------------------------
    addBool   (l, id::filterEnable, "Filter Enable", true);
    addChoice (l, id::filterType,   "Filter Type",   choices::filterType(), 0);
    addF      (l, id::filterCutoff, "Cutoff", makeLogRange (20.0f, 20000.0f), 2193.0f, "Hz");
    addPct    (l, id::filterReso,   "Resonance", 0.26f);
    addPct    (l, id::filterDrive,  "Drive", 0.16f);
    addPct    (l, id::filterMix,    "Filter Mix", 1.0f);
    addBool   (l, id::filterRouteA, "Filter Route A", true);
    addBool   (l, id::filterRouteB, "Filter Route B", true);
    addBool   (l, id::filterRouteS, "Filter Route Sub", false);
    addBool   (l, id::filterRouteN, "Filter Route Noise", false);

    // ---- Envelopes (ENV1 = amp) -------------------------------------------
    addEnv (l, 1, { 0.02f, 0.43f, 0.66f, 0.66f });
    addEnv (l, 2, { 0.31f, 0.62f, 0.30f, 0.85f });
    addEnv (l, 3, { 0.003f, 0.28f, 1.00f, 0.50f });

    // ---- LFOs --------------------------------------------------------------
    addLfo (l, 1, 0.62f);
    addLfo (l, 2, 0.50f);
    addLfo (l, 3, 0.50f);
    addLfo (l, 4, 0.50f);

    // ---- Macros ------------------------------------------------------------
    const float macroDef[4] = { 0.50f, 0.20f, 0.00f, 0.74f };
    for (int i = 1; i <= 4; ++i)
        addPct (l, id::macro (i), "Macro " + juce::String (i), macroDef[i - 1]);

    // ---- Master ------------------------------------------------------------
    addPct (l, id::masterOut, "Master Out", 0.80f);

    // ---- Arpeggiator -------------------------------------------------------
    addBool   (l, id::arpEnable,  "Arp Enable",  false);
    addChoice (l, id::arpRate,    "Arp Rate",    choices::arpRate(), 3);   // 1/16
    addChoice (l, id::arpMode,    "Arp Mode",    choices::arpMode(), 0);   // Up
    addInt    (l, id::arpOctaves, "Arp Octaves", 1, 4, 1);
    addPct    (l, id::arpGate,    "Arp Gate",    0.5f);
    addPct    (l, id::arpSwing,   "Arp Swing",   0.0f);
    const int stepDef[16] = { 1,0,1,1,0,1,1,0,1,0,1,1,0,1,0,1 };
    for (int i = 1; i <= 16; ++i)
        addBool (l, id::arpStep (i), "Arp Step " + juce::String (i), stepDef[i - 1] != 0);

    // ---- FX rack (10 serial slots: enable + representative dials) ----------
    auto fxEnable = [&l] (const char* slot, const juce::String& nice, bool on)
    { addBool (l, id::fx (slot, "enable"), nice + " Enable", on); };

    fxEnable ("hyper", "Hyper", true);
    addPct (l, id::fx ("hyper", "detune"), "Hyper Detune", 0.30f);
    addInt (l, id::fx ("hyper", "voices"), "Hyper Voices", 1, 8, 4);
    addPct (l, id::fx ("hyper", "width"),  "Hyper Width",  0.50f);
    addPct (l, id::fx ("hyper", "mix"),    "Hyper Mix",    1.00f);

    fxEnable ("distort", "Distort", true);
    addPct    (l, id::fx ("distort", "drive"), "Distort Drive", 0.30f);
    addPct    (l, id::fx ("distort", "tone"),  "Distort Tone",  0.50f);
    addPct    (l, id::fx ("distort", "mix"),   "Distort Mix",   1.00f);
    addPct    (l, id::fx ("distort", "out"),   "Distort Out",   0.80f);
    addChoice (l, id::fx ("distort", "mode"),  "Distort Mode",  choices::distortMode(), 0);

    fxEnable ("flanger", "Flanger", false);
    addF   (l, id::fx ("flanger", "rate"),     "Flanger Rate",     makeLogRange (0.01f, 10.0f), 0.5f, "Hz");
    addPct (l, id::fx ("flanger", "depth"),    "Flanger Depth",    0.50f);
    addPct (l, id::fx ("flanger", "feedback"), "Flanger Feedback", 0.30f);
    addPct (l, id::fx ("flanger", "mix"),      "Flanger Mix",      0.50f);

    fxEnable ("phaser", "Phaser", false);
    addF   (l, id::fx ("phaser", "rate"),  "Phaser Rate",  makeLogRange (0.01f, 10.0f), 0.3f, "Hz");
    addPct (l, id::fx ("phaser", "depth"), "Phaser Depth", 0.50f);
    addInt (l, id::fx ("phaser", "stages"),"Phaser Stages", 2, 12, 6);
    addPct (l, id::fx ("phaser", "mix"),   "Phaser Mix",   0.50f);

    fxEnable ("chorus", "Chorus", true);
    addF   (l, id::fx ("chorus", "rate"),   "Chorus Rate",   makeLogRange (0.01f, 10.0f), 0.8f, "Hz");
    addPct (l, id::fx ("chorus", "depth"),  "Chorus Depth",  0.30f);
    addInt (l, id::fx ("chorus", "voices"), "Chorus Voices", 1, 4, 2);
    addPct (l, id::fx ("chorus", "mix"),    "Chorus Mix",    0.50f);

    fxEnable ("delay", "Delay", true);
    addF   (l, id::fx ("delay", "time"),     "Delay Time",     makeLogRange (1.0f, 2000.0f), 300.0f, "ms");
    addPct (l, id::fx ("delay", "feedback"), "Delay Feedback", 0.35f);
    addPct (l, id::fx ("delay", "width"),    "Delay Width",    0.50f);
    addPct (l, id::fx ("delay", "mix"),      "Delay Mix",      0.30f);

    fxEnable ("reverb", "Reverb", true);
    addPct (l, id::fx ("reverb", "size"),  "Reverb Size",  0.60f);
    addPct (l, id::fx ("reverb", "decay"), "Reverb Decay", 0.50f);
    addPct (l, id::fx ("reverb", "damp"),  "Reverb Damp",  0.50f);
    addPct (l, id::fx ("reverb", "mix"),   "Reverb Mix",   0.30f);

    fxEnable ("comp", "Comp", false);
    addF (l, id::fx ("comp", "threshold"), "Comp Threshold", { -60.0f, 0.0f, 0.1f }, -18.0f, "dB");
    addF (l, id::fx ("comp", "ratio"),     "Comp Ratio",     { 1.0f, 20.0f, 0.1f }, 4.0f, ":1");
    addF (l, id::fx ("comp", "attack"),    "Comp Attack",    makeLogRange (0.1f, 100.0f), 10.0f, "ms");
    addF (l, id::fx ("comp", "makeup"),    "Comp Makeup",    { 0.0f, 24.0f, 0.1f }, 0.0f, "dB");

    fxEnable ("eq", "EQ", true);
    addF (l, id::fx ("eq", "low"),   "EQ Low",    { -15.0f, 15.0f, 0.1f }, 0.0f, "dB");
    addF (l, id::fx ("eq", "lomid"), "EQ Lo-Mid", { -15.0f, 15.0f, 0.1f }, 0.0f, "dB");
    addF (l, id::fx ("eq", "himid"), "EQ Hi-Mid", { -15.0f, 15.0f, 0.1f }, 0.0f, "dB");
    addF (l, id::fx ("eq", "high"),  "EQ High",   { -15.0f, 15.0f, 0.1f }, 0.0f, "dB");

    fxEnable ("filter", "FX Filter", false);
    addF      (l, id::fx ("filter", "cutoff"), "FX Filter Cutoff", makeLogRange (20.0f, 20000.0f), 1000.0f, "Hz");
    addPct    (l, id::fx ("filter", "reso"),   "FX Filter Reso",   0.20f);
    addChoice (l, id::fx ("filter", "type"),   "FX Filter Type",   choices::fxFilterType(), 0);
    addPct    (l, id::fx ("filter", "mix"),    "FX Filter Mix",    1.00f);

    // ---- Global / voicing --------------------------------------------------
    addInt    (l, id::glidePoly, "Polyphony",   1, 32, 16);
    addChoice (l, id::glideMono, "Mono Mode",   choices::monoMode(), 0);
    addF      (l, id::glideTime, "Glide",       { 0.0f, 1.0f, 0.001f, 0.4f }, 0.0f, "s");
    addInt    (l, id::bendRange, "Bend Range",  0, 24, 2);
    addBool   (l, id::mpeEnable, "MPE",         false);

    return l;
}

} // namespace zw
