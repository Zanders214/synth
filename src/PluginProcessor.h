#pragma once

#include <JuceHeader.h>
#include "dsp/Wavetable.h"
#include "dsp/ParamRefs.h"
#include "dsp/Voice.h"
#include "dsp/fx/FxChain.h"

//==============================================================================
// ZandersWave — Serum 2-class wavetable synthesizer.
// M0 scaffold: a valid, loadable synth that outputs silence and exposes a single
// Master Out parameter. The voice engine (M2) and full parameter tree (M1) layer
// on top of this skeleton without changing the AudioProcessor contract.
//==============================================================================
class ZandersWaveAudioProcessor : public juce::AudioProcessor
{
public:
    ZandersWaveAudioProcessor();
    ~ZandersWaveAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==========================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr int kNumVoices = 16;

    zw::Wavetable      wavetable;
    zw::ParamRefs      paramRefs;
    zw::ModMatrix      modMatrix;
    std::atomic<double> currentBpm { 120.0 };
    juce::Synthesiser  synth;
    zw::FxChain        fxChain;
    juce::dsp::Gain<float> masterGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZandersWaveAudioProcessor)
};
