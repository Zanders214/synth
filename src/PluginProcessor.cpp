#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

//==============================================================================
ZandersWaveAudioProcessor::ZandersWaveAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", zw::createParameterLayout())
{
}

ZandersWaveAudioProcessor::~ZandersWaveAudioProcessor() = default;

//==============================================================================
void ZandersWaveAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    masterGain.prepare (spec);
    masterGain.setRampDurationSeconds (0.02);
}

void ZandersWaveAudioProcessor::releaseResources() {}

bool ZandersWaveAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        || out == juce::AudioChannelSet::mono();
}

void ZandersWaveAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    // Synth: silent until the M2 voice engine renders into the buffer.
    buffer.clear();

    // Master gain: 0..1 -> -24..0 dB (linear gain), smoothed.
    const float norm   = apvts.getRawParameterValue ("masterOut")->load();
    const float gainDb = norm * 24.0f - 24.0f;
    masterGain.setGainLinear (juce::Decibels::decibelsToGain (gainDb, -24.0f));

    juce::dsp::AudioBlock<float> block (buffer);
    masterGain.process (juce::dsp::ProcessContextReplacing<float> (block));
}

//==============================================================================
juce::AudioProcessorEditor* ZandersWaveAudioProcessor::createEditor()
{
    return new ZandersWaveAudioProcessorEditor (*this);
}

//==============================================================================
void ZandersWaveAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void ZandersWaveAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZandersWaveAudioProcessor();
}
