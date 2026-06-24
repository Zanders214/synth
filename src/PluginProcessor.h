#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "dsp/Wavetable.h"
#include "dsp/ParamRefs.h"
#include "dsp/Voice.h"
#include "dsp/Arpeggiator.h"
#include "dsp/fx/FxChain.h"
#include "PresetManager.h"

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
    int getNumPrograms() override { return juce::jmax (1, presets.getNumFactory()); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override { currentProgram = index; presets.applyFactory (index); }
    const juce::String getProgramName (int index) override { return presets.factoryName (index); }
    void changeProgramName (int, const juce::String&) override {}

    zw::PresetManager& getPresetManager() { return presets; }

    // ---- Lightweight audio->UI taps (read on the message thread) ----
    static constexpr int kScopeSize = 1024;
    const float* getScopeRing() const noexcept { return scopeRing.data(); }
    int   getScopeWritePos() const noexcept { return scopeWritePos.load(); }
    float getOutputPeak() const noexcept { return outputPeak.load(); }
    int   getActiveVoiceCount() const noexcept { return activeVoices.load(); }

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;   // drives the on-screen keyboard

private:
    static constexpr int kNumVoices = 16;

    zw::Wavetable      wavetable;
    zw::ParamRefs      paramRefs;
    zw::ModMatrix      modMatrix;
    zw::PresetManager  presets { *this, apvts, modMatrix };
    int                currentProgram = 0;
    zw::Arpeggiator    arp;
    std::atomic<double> currentBpm { 120.0 };
    std::atomic<double> lastNoteFreq { 440.0 };
    juce::Synthesiser  synth;
    zw::FxChain        fxChain;
    juce::dsp::Gain<float> masterGain;

    // audio->UI taps
    std::array<float, kScopeSize> scopeRing {};
    std::atomic<int>   scopeWritePos { 0 };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<int>   activeVoices { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZandersWaveAudioProcessor)
};
