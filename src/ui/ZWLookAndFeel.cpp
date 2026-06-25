#include "ZWLookAndFeel.h"
#include "BinaryData.h"

namespace zw
{

ZWLookAndFeel::ZWLookAndFeel()
{
    display = juce::Typeface::createSystemTypefaceFor (BinaryData::SpaceGroteskVariable_ttf,
                                                       BinaryData::SpaceGroteskVariable_ttfSize);
    mono    = juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoVariable_ttf,
                                                       BinaryData::JetBrainsMonoVariable_ttfSize);

    setColour (juce::ResizableWindow::backgroundColourId, theme::pageBg);
    setColour (juce::Slider::textBoxTextColourId, theme::t1);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, theme::t2);
    setColour (juce::PopupMenu::backgroundColourId, theme::panelBot);
    setColour (juce::PopupMenu::textColourId, theme::t2);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent.withAlpha (0.25f));
    setColour (juce::ComboBox::backgroundColourId, theme::wa (0.04f));
    setColour (juce::ComboBox::textColourId, theme::t2);
    setColour (juce::ComboBox::outlineColourId, theme::wa (0.10f));
}

juce::Typeface::Ptr ZWLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    // Honour an explicit monospace request; otherwise the display face.
    if (f.getTypefaceName().containsIgnoreCase ("mono") && mono != nullptr)
        return mono;
    return display != nullptr ? display : LookAndFeel_V4::getTypefaceForFont (f);
}

juce::Font ZWLookAndFeel::displayFont (float h, bool bold) const
{
    juce::Font f (display); f.setHeight (h);
    return bold ? f.boldened() : f;
}

juce::Font ZWLookAndFeel::monoFont (float h) const
{
    juce::Font f (mono); f.setHeight (h); return f;
}

void ZWLookAndFeel::glowPath (juce::Graphics& g, const juce::Path& p, juce::Colour c, float thickness, float glow)
{
    // Draw the stroke a few times, fat->thin, fading, then the crisp core.
    for (int i = 3; i >= 1; --i)
    {
        const float t = thickness + (float) i * glow;
        g.setColour (c.withAlpha (0.10f * (float) i));
        g.strokePath (p, juce::PathStrokeType (t, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ZWLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                      float pos, float a0, float a1, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const auto r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float ang = a0 + pos * (a1 - a0);

    // Knob face (inset radial).
    juce::ColourGradient face (theme::knobHi, c.x - r * 0.3f, c.y - r * 0.4f,
                               theme::knobLo, c.x + r, c.y + r, true);
    g.setGradientFill (face);
    g.fillEllipse (bounds.reduced (r * 0.18f));

    // Background track.
    juce::Path track;
    track.addCentredArc (c.x, c.y, r * 0.86f, r * 0.86f, 0.0f, a0, a1, true);
    g.setColour (theme::wa (0.10f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc in the slider's accent colour (spectrum by value by default).
    auto arcColour = s.findColour (juce::Slider::rotarySliderFillColourId);
    if (arcColour == juce::Colour()) arcColour = theme::spectrum (pos);
    juce::Path val;
    val.addCentredArc (c.x, c.y, r * 0.86f, r * 0.86f, 0.0f, a0, ang, true);
    glowPath (g, val, arcColour, 3.0f, 2.0f);

    // White indicator tick.
    juce::Path tick;
    const float ti = r * 0.42f;
    const float to = r * 0.82f;
    tick.startNewSubPath (c.x + std::cos (ang - juce::MathConstants<float>::halfPi) * ti,
                          c.y + std::sin (ang - juce::MathConstants<float>::halfPi) * ti);
    tick.lineTo (c.x + std::cos (ang - juce::MathConstants<float>::halfPi) * to,
                 c.y + std::sin (ang - juce::MathConstants<float>::halfPi) * to);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.strokePath (tick, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ZWLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                      float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    auto track = juce::Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - 4.0f, (float) w, 8.0f);
    g.setColour (theme::well);
    g.fillRoundedRectangle (track, 4.0f);

    const float fillW = juce::jlimit ((float) x, (float) (x + w), pos) - (float) x;
    auto fill = track.withWidth (fillW);
    juce::ColourGradient grad (theme::cyan, fill.getX(), 0.0f, theme::amber, fill.getRight(), 0.0f, false);
    grad.addColour (0.5, theme::pink);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (fill, 4.0f);

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillEllipse (pos - 5.0f, (float) y + (float) h * 0.5f - 5.0f, 10.0f, 10.0f);
    juce::ignoreUnused (s);
}

void ZWLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                          bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (theme::accent.withAlpha (0.16f));
        g.fillRoundedRectangle (r, (float) theme::rChip);
        g.setColour (theme::accent.withAlpha (down ? 0.9f : 0.6f));
        g.drawRoundedRectangle (r, (float) theme::rChip, 1.2f);
    }
    else
    {
        g.setColour (theme::wa (highlighted ? 0.07f : 0.04f));
        g.fillRoundedRectangle (r, (float) theme::rChip);
        g.setColour (theme::wa (0.08f));
        g.drawRoundedRectangle (r, (float) theme::rChip, 1.0f);
    }
}

juce::Font ZWLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return displayFont (juce::jlimit (9.0f, 14.0f, (float) h * 0.42f), true)
               .withExtraKerningFactor (0.08f);
}

void ZWLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? theme::t1 : theme::t3);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawText (b.getButtonText().toUpperCase(), b.getLocalBounds(), juce::Justification::centred, false);
}

} // namespace zw
