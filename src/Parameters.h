#pragma once

#include <JuceHeader.h>

//==============================================================================
// Central definition of every plugin parameter (the APVTS layout) plus the
// string-ID scheme shared between parameter creation and the DSP/UI that read
// them. IDs are stable strings so DAW automation and saved state stay valid.
//==============================================================================
namespace zw
{

namespace id
{
    // ---- Oscillators A & B (ab = 'A' or 'B') -------------------------------
    inline juce::String osc (char ab, juce::StringRef p) { return "osc" + juce::String::charToString ((juce::juce_wchar) ab) + "_" + p; }
    // ---- Envelopes 1..3 ----------------------------------------------------
    inline juce::String env (int n, juce::StringRef p)   { return "env" + juce::String (n) + "_" + p; }
    // ---- LFOs 1..4 ---------------------------------------------------------
    inline juce::String lfo (int n, juce::StringRef p)   { return "lfo" + juce::String (n) + "_" + p; }
    // ---- Macros 1..4 -------------------------------------------------------
    inline juce::String macro (int n)                    { return "macro" + juce::String (n); }
    // ---- FX slot params ----------------------------------------------------
    inline juce::String fx (juce::StringRef slot, juce::StringRef p) { return "fx_" + slot + "_" + p; }
    // ---- Arp step n (1..16) ------------------------------------------------
    inline juce::String arpStep (int n)                  { return "arp_step" + juce::String (n); }

    // ---- Singletons --------------------------------------------------------
    static constexpr const char* masterOut    = "masterOut";

    static constexpr const char* subEnable    = "sub_enable";
    static constexpr const char* subWave      = "sub_wave";
    static constexpr const char* subOctave    = "sub_octave";
    static constexpr const char* subSaturate  = "sub_saturate";
    static constexpr const char* subLevel     = "sub_level";

    static constexpr const char* noiseEnable  = "noise_enable";
    static constexpr const char* noiseType    = "noise_type";
    static constexpr const char* noiseColor   = "noise_color";
    static constexpr const char* noiseLevel   = "noise_level";

    static constexpr const char* filterEnable = "filter_enable";
    static constexpr const char* filterType   = "filter_type";
    static constexpr const char* filterCutoff = "filter_cutoff";
    static constexpr const char* filterReso   = "filter_reso";
    static constexpr const char* filterDrive  = "filter_drive";
    static constexpr const char* filterMix    = "filter_mix";
    static constexpr const char* filterRouteA = "filter_routeA";
    static constexpr const char* filterRouteB = "filter_routeB";
    static constexpr const char* filterRouteS = "filter_routeS";
    static constexpr const char* filterRouteN = "filter_routeN";

    static constexpr const char* arpEnable    = "arp_enable";
    static constexpr const char* arpRate      = "arp_rate";
    static constexpr const char* arpMode      = "arp_mode";
    static constexpr const char* arpOctaves   = "arp_octaves";
    static constexpr const char* arpGate      = "arp_gate";
    static constexpr const char* arpSwing     = "arp_swing";

    static constexpr const char* glidePoly    = "global_polyphony";
    static constexpr const char* glideMono    = "global_monomode";
    static constexpr const char* glideTime    = "global_glide";
    static constexpr const char* bendRange    = "global_bendrange";
    static constexpr const char* mpeEnable    = "global_mpe";
}

// Choice option lists (shared so DSP/UI index the same order the params use).
namespace choices
{
    inline juce::StringArray subWave()     { return { "Sine", "Tri", "Square" }; }
    inline juce::StringArray noiseType()   { return { "White", "Pink", "Vinyl" }; }
    inline juce::StringArray filterType()  { return { "LP24", "LP12", "HP24", "HP12", "BP12", "Notch" }; }
    inline juce::StringArray lfoShape()    { return { "Sine", "Triangle", "Saw", "Ramp", "Square", "Random" }; }
    inline juce::StringArray lfoMode()     { return { "Trigger", "Envelope", "Free" }; }
    inline juce::StringArray syncDiv()     { return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }; }
    inline juce::StringArray arpRate()     { return { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }; }
    inline juce::StringArray arpMode()     { return { "Up", "Down", "Up/Dn", "Random", "Chord", "As Played" }; }
    inline juce::StringArray monoMode()    { return { "Poly", "Mono", "Legato" }; }
    inline juce::StringArray distortMode() { return { "Tube", "Diode", "Fold" }; }
    inline juce::StringArray fxFilterType(){ return { "LP", "HP", "BP" }; }
}

// Maps a normalised filter-cutoff position to Hz and back: Hz = 20 * 1000^x.
juce::NormalisableRange<float> makeLogRange (float lo, float hi);

// Builds the full parameter layout for the APVTS.
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace zw
