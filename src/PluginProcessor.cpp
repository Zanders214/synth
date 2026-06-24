#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

//==============================================================================
ZandersWaveAudioProcessor::ZandersWaveAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", zw::createParameterLayout())
{
    paramRefs.prepare (apvts);
    wavetable.generateBasicShapes (64);

    synth.addSound (new zw::ZWSound());
    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new zw::ZWVoice (paramRefs, wavetable, modMatrix, currentBpm));
}

ZandersWaveAudioProcessor::~ZandersWaveAudioProcessor() = default;

//==============================================================================
void ZandersWaveAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

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
                                              juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    buffer.clear();

    // Track host tempo for tempo-synced LFOs (and later arp/FX).
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                currentBpm.store (*bpm);

    // Voice engine renders all active voices into the buffer.
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    // Master gain: 0..1 -> -24..0 dB (linear gain), smoothed.
    const float norm   = apvts.getRawParameterValue (zw::id::masterOut)->load();
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
    // Bundle the APVTS state and the mod matrix under one wrapper tree.
    juce::ValueTree wrapper ("ZANDERSWAVE");
    wrapper.appendChild (apvts.copyState(), nullptr);
    wrapper.appendChild (modMatrix.toValueTree(), nullptr);

    if (auto xml = wrapper.createXml())
        copyXmlToBinary (*xml, destData);
}

void ZandersWaveAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto wrapper = juce::ValueTree::fromXml (*xml);

    if (wrapper.hasType ("ZANDERSWAVE"))
    {
        auto params = wrapper.getChildWithName (apvts.state.getType());
        if (params.isValid())
            apvts.replaceState (params);

        modMatrix.fromValueTree (wrapper.getChildWithName ("MODMATRIX"));
    }
    else if (wrapper.hasType (apvts.state.getType()))
    {
        // Backwards-compat: older state without the wrapper.
        apvts.replaceState (wrapper);
    }
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZandersWaveAudioProcessor();
}
