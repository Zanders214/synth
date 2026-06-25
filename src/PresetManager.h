#pragma once

#include <JuceHeader.h>
#include "dsp/ModMatrix.h"
#include <vector>

namespace zw
{

//==============================================================================
// Owns the plugin's state (APVTS + mod matrix) serialization, a factory preset
// bank (exposed to the host via the program interface), and user preset save/
// load to disk. The wrapper tree format is shared with the processor's
// get/setStateInformation so DAW session recall and presets use the same data.
//==============================================================================
class PresetManager
{
public:
    PresetManager (juce::AudioProcessor& proc, juce::AudioProcessorValueTreeState& apvts, ModMatrix& mm);

    // ---- Whole-plugin state ----
    juce::ValueTree captureState() const;
    void applyState (const juce::ValueTree& wrapper);

    // ---- Factory bank (host programs) ----
    int          getNumFactory() const           { return (int) factory.size(); }
    juce::String factoryName (int index) const;
    void         applyFactory (int index);

    // ---- User presets (disk) ----
    juce::File       getUserDir() const;
    bool             saveUserPreset (const juce::String& name) const;
    juce::StringArray getUserPresetNames() const;
    bool             loadUserPreset (const juce::String& name);

    static constexpr const char* kWrapperType = "ZANDERSWAVE";
    static constexpr const char* kFileExt = ".zwpreset";

private:
    struct Override { juce::String id; float actual; };
    struct Factory  { juce::String name; std::vector<Override> overrides; };

    void buildFactory();
    void resetToDefaults();
    void setActual (const juce::String& id, float actual);

    juce::AudioProcessor&                 processor;
    juce::AudioProcessorValueTreeState&   apvts;
    ModMatrix&                            modMatrix;
    std::vector<Factory>                  factory;
};

} // namespace zw
