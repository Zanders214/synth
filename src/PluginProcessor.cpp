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

    // Collect on-screen-keyboard notes on the message thread (see processBlock).
    keyboardState.addListener (this);

    // Boot into the clean Init patch (program 0) so a fresh instance opens on a
    // basic single-oscillator sound, like Serum/Vital open on their init patch.
    presets.applyFactory (0);
}

ZandersWaveAudioProcessor::~ZandersWaveAudioProcessor()
{
    keyboardState.removeListener (this);
}

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

void ZandersWaveAudioProcessor::releaseResources() { /* no-op: nothing to release */ }

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
    if (const auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                currentBpm.store (*bpm);

    // Merge on-screen keyboard input (collected on the message thread) without
    // locking: drain the SPSC FIFO into the MIDI stream at the block start.
    {
        const auto rd = kbFifo.read (kbFifo.getNumReady());
        auto emit = [this, &midi] (int start, int size)
        {
            for (int i = 0; i < size; ++i)
            {
                const auto& e = kbEvents[(size_t) (start + i)];
                midi.addEvent (e.noteOn ? juce::MidiMessage::noteOn  (e.channel, e.note, e.velocity)
                                        : juce::MidiMessage::noteOff (e.channel, e.note, e.velocity), 0);
            }
        };
        emit (rd.startIndex1, rd.blockSize1);
        emit (rd.startIndex2, rd.blockSize2);
    }

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
// On-screen-keyboard listener callbacks: fire on the message thread when the
// user plays the MidiKeyboardComponent. They only enqueue into the lock-free
// FIFO that processBlock drains; no audio-thread locking is involved.
void ZandersWaveAudioProcessor::handleNoteOn (juce::MidiKeyboardState*, int ch, int note, float vel)
{
    pushKeyEvent ({ (juce::uint8) ch, (juce::uint8) note,
                    (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (vel * 127.0f)), true });
}

void ZandersWaveAudioProcessor::handleNoteOff (juce::MidiKeyboardState*, int ch, int note, float vel)
{
    pushKeyEvent ({ (juce::uint8) ch, (juce::uint8) note,
                    (juce::uint8) juce::jlimit (0, 127, juce::roundToInt (vel * 127.0f)), false });
}

void ZandersWaveAudioProcessor::pushKeyEvent (const KeyEvent& e) noexcept
{
    const auto wr = kbFifo.write (1);
    if (wr.blockSize1 > 0)      kbEvents[(size_t) wr.startIndex1] = e;
    else if (wr.blockSize2 > 0) kbEvents[(size_t) wr.startIndex2] = e;
    // else: FIFO full -> drop the event (not reachable for hand-played input).
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
