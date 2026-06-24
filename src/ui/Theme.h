#pragma once

#include <JuceHeader.h>

//==============================================================================
// Central "Neon Plugins / Zanders" design tokens (exact values from the design
// handoff) plus the spectrum/source-colour helpers used by every control and
// display. Nothing in the UI should hardcode a hex value — pull it from here.
//==============================================================================
namespace zw::theme
{
    // ---- Spectrum (the brand; meters/rings/curves, 0->100% = left->right) ----
    inline const juce::Colour cyan   { 0xff34d8ff };
    inline const juce::Colour violet { 0xff8b7bff };
    inline const juce::Colour pink   { 0xffff5fa8 };
    inline const juce::Colour amber  { 0xffffc24b };

    // ---- Accent / signal ----
    inline const juce::Colour accent  { 0xff5e93ff };
    inline const juce::Colour danger0 { 0xffff5a5a };
    inline const juce::Colour danger1 { 0xffe23b3b };

    // ---- Surfaces ----
    inline const juce::Colour pageBg   { 0xff06070c };
    inline const juce::Colour panelTop { 0xff1a2030 };
    inline const juce::Colour panelBot { 0xff0a0b12 };
    inline const juce::Colour well     { 0xff070a0e };
    inline const juce::Colour knobHi   { 0xff222731 };
    inline const juce::Colour knobLo   { 0xff0b0d12 };

    // ---- Text ladder ----
    inline const juce::Colour t1     { 0xffe8ecf3 };
    inline const juce::Colour t2     { 0xffcfd4dc };
    inline const juce::Colour t3     { 0xff9aa3b3 };
    inline const juce::Colour tMuted { 0xff8a93a3 };
    inline const juce::Colour tLabel { 0xff7e8794 };
    inline const juce::Colour tFaint { 0xff5d6473 };

    inline juce::Colour wa (float a) { return juce::Colours::white.withAlpha (a); }   // white-alpha layer

    enum Radii { rPanel = 16, rWell = 12, rBtn = 12, rChip = 8, rPill = 20 };

    // Piecewise spectrum ramp (port of the prototype's spectrum()).
    inline juce::Colour spectrum (float x)
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        if (x < 0.4f) return cyan.interpolatedWith (violet, x / 0.4f);
        if (x < 0.7f) return violet.interpolatedWith (pink, (x - 0.4f) / 0.3f);
        return pink.interpolatedWith (amber, (x - 0.7f) / 0.3f);
    }

    // Modulation-source colour (ENV cyan, LFO pink, MACRO accent, VEL amber, else violet).
    inline juce::Colour srcColour (const juce::String& s)
    {
        if (s.startsWith ("ENV"))   return cyan;
        if (s.startsWith ("LFO"))   return pink;
        if (s.startsWith ("MACRO")) return accent;
        if (s == "VEL")             return amber;
        return violet;
    }

    inline juce::ColourGradient panelGradient (juce::Rectangle<float> r)
    {
        // radial-ish: bright top-centre fading to near-black.
        juce::ColourGradient g (panelTop, r.getCentreX(), r.getY() - r.getHeight() * 0.1f,
                                panelBot, r.getX(), r.getBottom(), true);
        return g;
    }
}
