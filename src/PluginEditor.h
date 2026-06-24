#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// M0 editor: hosts JUCE's generic parameter editor so the plugin is fully usable
// and testable before the custom Serum-style UI is built in M7.
//==============================================================================
class ZandersWaveAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ZandersWaveAudioProcessorEditor (ZandersWaveAudioProcessor&);
    ~ZandersWaveAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ZandersWaveAudioProcessor& processorRef;
    juce::GenericAudioProcessorEditor generic;   // replaced by the custom UI in M7

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZandersWaveAudioProcessorEditor)
};
