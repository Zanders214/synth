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
    fxChain.prepareParams (apvts);
    arp.prepareParams (apvts);
    wavetable.generateBasicShapes (64);

    synth.addSound (new zw::ZWSound());
    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new zw::ZWVoice (paramRefs, wavetable, modMatrix, currentBpm, lastNoteFreq));
}

ZandersWaveAudioProcessor::~ZandersWaveAudioProcessor() = default;

//==============================================================================
void ZandersWaveAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    arp.prepare (sampleRate);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    fxChain.prepare (spec);
    fxChain.reset();

    masterGain.prepare (spec);
    masterGain.setRampDurationSeconds (0.02);
}

void ZandersWaveAudioProcessor::releaseResources() {}

bool ZandersWaveAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo-only: the FX rack (oversampled distortion, ping-pong delay, widener)
    // assumes two channels.
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
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

    // Merge on-screen keyboard input into the MIDI stream.
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    // Arpeggiator rewrites the MIDI stream (pass-through when disabled).
    arp.process (midi, buffer.getNumSamples(), currentBpm.load());

    // Voice engine renders all active voices into the buffer.
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    // Global FX rack (serial), then master gain.
    juce::dsp::AudioBlock<float> block (buffer);
    fxChain.process (block);

    // Master gain: 0..1 -> -24..0 dB (linear gain), smoothed.
    const float norm   = apvts.getRawParameterValue (zw::id::masterOut)->load();
    const float gainDb = norm * 24.0f - 24.0f;
    masterGain.setGainLinear (juce::Decibels::decibelsToGain (gainDb, -24.0f));
    masterGain.process (juce::dsp::ProcessContextReplacing<float> (block));

    // ---- Audio->UI taps: scope ring + output peak + voice count ----
    const int   ns = buffer.getNumSamples();
    const auto* L  = buffer.getReadPointer (0);
    const auto* R  = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : L;
    int wp = scopeWritePos.load();
    float peak = 0.0f;
    for (int i = 0; i < ns; ++i)
    {
        const float m = 0.5f * (L[i] + R[i]);
        scopeRing[(size_t) (wp & (kScopeSize - 1))] = m;
        ++wp;
        peak = juce::jmax (peak, std::abs (m));
    }
    scopeWritePos.store (wp);
    outputPeak.store (juce::jmax (peak, outputPeak.load() * 0.85f));   // light decay

    int voices = 0;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (synth.getVoice (i)->isVoiceActive())
            ++voices;
    activeVoices.store (voices);
}

//==============================================================================
juce::AudioProcessorEditor* ZandersWaveAudioProcessor::createEditor()
{
    return new ZandersWaveAudioProcessorEditor (*this);
}

//==============================================================================
void ZandersWaveAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = presets.captureState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ZandersWaveAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        presets.applyState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZandersWaveAudioProcessor();
}
