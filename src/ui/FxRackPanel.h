#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "ZWLookAndFeel.h"
#include "../Parameters.h"
#include <array>
#include <vector>

namespace zw
{

//==============================================================================
// FX RACK lower-tab editor.
//
// The rack has 10 serial effect slots, each with an enable plus 3-5 real,
// DSP-wired parameters (see Parameters.cpp / FxChain.cpp). This panel exposes
// them as an effect picker + a per-effect parameter strip:
//
//     [ Hyper  Distort  Flanger  ...  Filter ]   <- radio picker (which strip)
//     [ ENABLE | knob  knob  knob  knob ... ]     <- strip for the picked effect
//
// Picking an effect swaps in its strip. Each strip is a channel-strip-style
// row: a tall enable toggle on the left, then that effect's controls as
// LabeledKnobs (and LabeledCombos for the two enumerated params). Every control
// is bound to its real APVTS parameter via an attachment, so moving a knob or
// flipping enable changes the audio exactly as the DSP reads it.
//
// IMPORTANT: this header reuses LabeledKnob, which is defined in
// PluginEditor.cpp. It must therefore be #included *after* that class
// definition (see the include site in PluginEditor.cpp); it is only used there.
//==============================================================================

//==============================================================================
// The combo-box analogue of LabeledKnob, used for the rack's enumerated params
// (distort mode, FX-filter type). Same label styling so a strip reads uniformly.
class LabeledCombo : public juce::Component
{
public:
    LabeledCombo (juce::AudioProcessorValueTreeState& s, const juce::String& id,
                  const juce::String& name, const juce::StringArray& items, ZWLookAndFeel& lf)
    {
        combo.addItemList (items, 1);
        combo.setJustificationType (juce::Justification::centred);
        combo.setLookAndFeel (&lf);
        addAndMakeVisible (combo);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (lf.displayFont (8.8f, true).withExtraKerningFactor (0.12f));
        label.setColour (juce::Label::textColourId, theme::tLabel);
        addAndMakeVisible (label);

        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, combo);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (13));
        combo.setBounds (r.withSizeKeepingCentre (juce::jmax (40, r.getWidth() - 6), 24));
    }

private:
    juce::ComboBox combo;
    juce::Label    label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

//==============================================================================
class FxRackPanel : public juce::Component
{
public:
    FxRackPanel (juce::AudioProcessorValueTreeState& apvtsIn, ZWLookAndFeel& lnfIn)
        : apvts (apvtsIn), lnf (lnfIn)
    {
        const auto specs = effectSpecs();
        for (int i = 0; i < (int) specs.size(); ++i)
        {
            const auto& fx = specs[(size_t) i];

            // Picker button (radio): selects which effect's strip is shown.
            auto* pick = pickers.add (std::make_unique<juce::TextButton> (fx.name));
            pick->setClickingTogglesState (true);
            pick->setRadioGroupId (4200);
            pick->setLookAndFeel (&lnf);
            pick->onClick = [this, i] { select (i); };
            addAndMakeVisible (pick);

            // The per-effect strip: enable toggle + that effect's param controls.
            auto* strip = strips.add (std::make_unique<Strip>());
            addChildComponent (strip);

            strip->enable.setButtonText (fx.name);
            strip->enable.setClickingTogglesState (true);
            strip->enable.setLookAndFeel (&lnf);
            strip->addAndMakeVisible (strip->enable);
            strip->enAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                               apvts, id::fx (fx.slot, "enable"), strip->enable);

            for (const auto& p : fx.params)
            {
                juce::Component* c = nullptr;
                if (p.choices.isEmpty())
                    c = new LabeledKnob (apvts, id::fx (fx.slot, p.suffix), p.label, lnf, true, fx.arc);
                else
                    c = new LabeledCombo (apvts, id::fx (fx.slot, p.suffix), p.label, p.choices, lnf);
                strip->controls.add (c);
                strip->addAndMakeVisible (c);
            }
        }

        pickers[0]->setToggleState (true, juce::dontSendNotification);
        select (0);
    }

    ~FxRackPanel() override
    {
        for (auto* p : pickers) p->setLookAndFeel (nullptr);
        for (auto* s : strips)  s->enable.setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (theme::well);
        g.fillRoundedRectangle (r, (float) theme::rWell);
        g.setColour (theme::wa (0.06f));
        g.drawRoundedRectangle (r, (float) theme::rWell, 1.0f);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (10, 8);

        auto pickerRow = b.removeFromTop (24);
        const int n = pickers.size();
        if (n > 0)
        {
            const int pw = pickerRow.getWidth() / n;
            for (auto* p : pickers) p->setBounds (pickerRow.removeFromLeft (pw).reduced (2, 0));
        }

        b.removeFromTop (8);
        for (auto* s : strips) s->setBounds (b);
    }

private:
    //==========================================================================
    // One effect's strip: a tall enable toggle plus the effect's controls.
    struct Strip : public juce::Component
    {
        void resized() override
        {
            auto b = getLocalBounds().reduced (2);
            enable.setBounds (b.removeFromLeft (84).reduced (2, 6));
            b.removeFromLeft (10);
            const int n = controls.size();
            if (n == 0) return;
            const int cw = b.getWidth() / n;
            for (auto* c : controls) c->setBounds (b.removeFromLeft (cw).reduced (4, 2));
        }

        juce::TextButton enable;
        juce::OwnedArray<juce::Component> controls;   // LabeledKnob / LabeledCombo
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enAtt;
    };

    void select (int which)
    {
        for (int i = 0; i < strips.size(); ++i)
            strips[i]->setVisible (i == which);
        if (which >= 0 && which < pickers.size())
            pickers[which]->setToggleState (true, juce::dontSendNotification);
    }

    //==========================================================================
    // Static description of one parameter: a suffix for id::fx(slot, suffix),
    // a short label, and (for enumerated params only) its choice list. An empty
    // choice list means a knob; a non-empty list means a combo.
    struct PSpec { juce::String suffix; juce::String label; juce::StringArray choices; };
    struct FxSpec { juce::String slot; juce::String name; juce::Colour arc; std::vector<PSpec> params; };

    // The slots, names, and per-effect params — kept in the same order as the
    // DSP reads them in FxChain.cpp so the labels match what each knob drives.
    // (Phaser "stages" and Chorus "voices" are still exposed though currently
    // no-ops in the DSP, so the UI reflects the full APVTS parameter set.)
    static std::vector<FxSpec> effectSpecs()
    {
        return {
            { "hyper",   "Hyper",   theme::cyan,   { {"detune","DETUNE",{}}, {"voices","VOICES",{}}, {"width","WIDTH",{}}, {"mix","MIX",{}} } },
            { "distort", "Distort", theme::amber,  { {"drive","DRIVE",{}}, {"tone","TONE",{}}, {"mix","MIX",{}}, {"out","OUT",{}}, {"mode","MODE", choices::distortMode()} } },
            { "flanger", "Flanger", theme::violet, { {"rate","RATE",{}}, {"depth","DEPTH",{}}, {"feedback","FBK",{}}, {"mix","MIX",{}} } },
            { "phaser",  "Phaser",  theme::violet, { {"rate","RATE",{}}, {"depth","DEPTH",{}}, {"stages","STAGES",{}}, {"mix","MIX",{}} } },
            { "chorus",  "Chorus",  theme::pink,   { {"rate","RATE",{}}, {"depth","DEPTH",{}}, {"voices","VOICES",{}}, {"mix","MIX",{}} } },
            { "delay",   "Delay",   theme::accent, { {"time","TIME",{}}, {"feedback","FBK",{}}, {"width","WIDTH",{}}, {"mix","MIX",{}} } },
            { "reverb",  "Reverb",  theme::accent, { {"size","SIZE",{}}, {"decay","DECAY",{}}, {"damp","DAMP",{}}, {"mix","MIX",{}} } },
            { "comp",    "Comp",    theme::pink,   { {"threshold","THRESH",{}}, {"ratio","RATIO",{}}, {"attack","ATK",{}}, {"makeup","MAKEUP",{}} } },
            { "eq",      "EQ",      theme::cyan,   { {"low","LOW",{}}, {"lomid","LO-MID",{}}, {"himid","HI-MID",{}}, {"high","HIGH",{}} } },
            { "filter",  "Filter",  theme::accent, { {"cutoff","CUTOFF",{}}, {"reso","RESO",{}}, {"type","TYPE", choices::fxFilterType()}, {"mix","MIX",{}} } },
        };
    }

    juce::AudioProcessorValueTreeState& apvts;
    ZWLookAndFeel& lnf;

    juce::OwnedArray<juce::TextButton> pickers;
    juce::OwnedArray<Strip>            strips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxRackPanel)
};

} // namespace zw
