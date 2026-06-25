#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace zw
{

//==============================================================================
// LookAndFeel encoding the design language: bundled fonts, spectrum rotary
// knobs (with glow), ramp-filled sliders, and glowing accent pills/buttons.
//==============================================================================
class ZWLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ZWLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    juce::Font displayFont (float height, bool bold = false) const;
    juce::Font monoFont (float height) const;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;

    // Layered-stroke glow used across the UI.
    static void glowPath (juce::Graphics&, const juce::Path&, juce::Colour, float thickness, float glow);

private:
    juce::Typeface::Ptr display;
    juce::Typeface::Ptr mono;
};

} // namespace zw
