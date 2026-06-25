#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/ZWLookAndFeel.h"

namespace zw { class ZWPanel; }

//==============================================================================
// The plugin editor: hosts the fixed 1320x900 ZWPanel and scales it with an
// AffineTransform so the window stays proportional at any size.
//==============================================================================
class ZandersWaveAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ZandersWaveAudioProcessorEditor (ZandersWaveAudioProcessor&);
    ~ZandersWaveAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kDesignW = 1320;
    static constexpr int kDesignH = 900;

    zw::ZWLookAndFeel lnf;
    std::unique_ptr<zw::ZWPanel> panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZandersWaveAudioProcessorEditor)
};
