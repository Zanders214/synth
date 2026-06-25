// Shared helpers for the DSP unit-test suite (juce::UnitTest based).
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "Parameters.h"

namespace zwtest
{
// Minimal AudioProcessor so an APVTS can be built off the real parameter layout.
// Mirrors the helper used by tests/render_smoke.cpp.
struct DummyProcessor : public juce::AudioProcessor
{
    juce::AudioProcessorValueTreeState apvts;
    DummyProcessor() : apvts (*this, nullptr, "PARAMETERS", zw::createParameterLayout()) {}

    const juce::String getName() const override            { return "test"; }
    void prepareToPlay (double, int) override              {}
    void releaseResources() override                       {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override           { return 0.0; }
    bool acceptsMidi() const override                      { return true; }
    bool producesMidi() const override                     { return false; }
    juce::AudioProcessorEditor* createEditor() override    { return nullptr; }
    bool hasEditor() const override                        { return false; }
    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override   {}
};

// Convenience: set a normalised APVTS parameter by id (0..1), notifying host.
inline void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float norm)
{
    if (auto* p = s.getParameter (id))
        p->setValueNotifyingHost (norm);
}

// Convenience: read the raw (denormalised) value of a parameter by id.
inline float rawParam (juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    if (auto* a = s.getRawParameterValue (id))
        return a->load();
    return 0.0f;
}
} // namespace zwtest
