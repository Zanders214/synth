#include "PluginEditor.h"

//==============================================================================
ZandersWaveAudioProcessorEditor::ZandersWaveAudioProcessorEditor (ZandersWaveAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), generic (p)
{
    addAndMakeVisible (generic);
    setResizable (true, true);
    setResizeLimits (420, 300, 1600, 1200);
    setSize (560, 420);
}

ZandersWaveAudioProcessorEditor::~ZandersWaveAudioProcessorEditor() = default;

//==============================================================================
void ZandersWaveAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff06070c));   // page background from the design tokens
}

void ZandersWaveAudioProcessorEditor::resized()
{
    generic.setBounds (getLocalBounds());
}
