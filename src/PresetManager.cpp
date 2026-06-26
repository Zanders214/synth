#include "PresetManager.h"
#include "Parameters.h"

namespace zw
{

PresetManager::PresetManager (juce::AudioProcessor& proc,
                              juce::AudioProcessorValueTreeState& s, ModMatrix& mm)
    : processor (proc), apvts (s), modMatrix (mm)
{
    buildFactory();
}

//==============================================================================
juce::ValueTree PresetManager::captureState() const
{
    juce::ValueTree wrapper (kWrapperType);
    wrapper.appendChild (apvts.copyState(), nullptr);
    wrapper.appendChild (modMatrix.toValueTree(), nullptr);
    return wrapper;
}

void PresetManager::applyState (const juce::ValueTree& wrapper)
{
    if (wrapper.hasType (kWrapperType))
    {
        if (auto params = wrapper.getChildWithName (apvts.state.getType()); params.isValid())
            apvts.replaceState (params);
        modMatrix.fromValueTree (wrapper.getChildWithName ("MODMATRIX"));
    }
    else if (wrapper.hasType (apvts.state.getType()))
    {
        apvts.replaceState (wrapper);   // legacy: bare APVTS state
    }
}

//==============================================================================
void PresetManager::resetToDefaults()
{
    for (auto* p : processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
}

void PresetManager::setActual (const juce::String& id, float actual)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (actual));
}

juce::String PresetManager::factoryName (int index) const
{
    return juce::isPositiveAndBelow (index, (int) factory.size()) ? factory[(size_t) index].name : juce::String();
}

void PresetManager::applyFactory (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) factory.size()))
        return;

    resetToDefaults();
    const auto& preset = factory[(size_t) index];
    for (const auto& ov : preset.overrides)
        setActual (ov.id, ov.actual);
    if (preset.clearMod)
        modMatrix.clear();
    else
        modMatrix.seedDefaults();
}

void PresetManager::buildFactory()
{
    using namespace zw;
    factory.clear();

    // 1. Init — a single basic saw, nothing else: one voice, no second osc,
    //    no sub/noise, no filter, no FX, no modulation (Serum/Vital-style init).
    factory.push_back ({ "Init", {
        { id::osc ('A', "wtpos"),   1.0f },   // full saw
        { id::osc ('A', "warp"),    0.0f },
        { id::osc ('A', "unison"),  1.0f },   // single voice, no detune
        { id::osc ('A', "detune"),  0.0f },
        { id::osc ('A', "level"),   0.7f },
        { id::osc ('B', "enable"),  0.0f },   // no second oscillator
        { id::subEnable,    0.0f },           // no sub
        { id::noiseEnable,  0.0f },           // no noise
        { id::filterEnable, 0.0f },           // no filter
        { id::env (1, "attack"),  0.0f },     // instant attack
        { id::env (1, "sustain"), 1.0f },     // full sustain
        { id::env (1, "release"), 0.1f },     // short release
        { id::fx ("hyper",   "enable"), 0.0f },   // all FX off (these default ON)
        { id::fx ("distort", "enable"), 0.0f },
        { id::fx ("chorus",  "enable"), 0.0f },
        { id::fx ("delay",   "enable"), 0.0f },
        { id::fx ("reverb",  "enable"), 0.0f },
        { id::fx ("eq",      "enable"), 0.0f } }, true });   // clearMod = true

    // 2. Hyper Lead — wide detuned bright lead.
    factory.push_back ({ "Hyper Lead", {
        { id::osc ('A', "unison"), 8.0f }, { id::osc ('A', "detune"), 35.0f },
        { id::osc ('B', "level"), 0.5f },  { id::osc ('B', "detune"), 20.0f },
        { id::filterCutoff, 8500.0f },     { id::filterReso, 0.30f },
        { id::env (1, "attack"), 0.005f }, { id::env (1, "release"), 0.25f } } });

    // 3. Glass Pad — slow, lush, reverberant.
    factory.push_back ({ "Glass Pad", {
        { id::osc ('A', "wtpos"), 0.60f }, { id::osc ('B', "level"), 0.45f },
        { id::env (1, "attack"), 1.4f },   { id::env (1, "release"), 2.2f },
        { id::env (1, "sustain"), 0.85f }, { id::filterCutoff, 4200.0f },
        { id::fx ("reverb", "mix"), 0.55f }, { id::fx ("chorus", "mix"), 0.5f } } });

    // 4. Sub Bass — sub-heavy, dark, tight.
    factory.push_back ({ "Sub Bass", {
        { id::osc ('A', "octave"), -1.0f }, { id::osc ('A', "unison"), 1.0f },
        { id::osc ('A', "level"), 0.6f },   { id::osc ('B', "level"), 0.0f },
        { id::subLevel, 0.9f },             { id::filterCutoff, 650.0f },
        { id::filterReso, 0.18f },          { id::env (1, "release"), 0.25f } } });

    // 5. Detuned Brass — driven, mid-forward.
    factory.push_back ({ "Detuned Brass", {
        { id::osc ('A', "detune"), 25.0f }, { id::osc ('B', "level"), 0.6f },
        { id::filterCutoff, 3200.0f },      { id::filterDrive, 0.30f },
        { id::env (1, "attack"), 0.04f },   { id::fx ("distort", "mix"), 0.4f } } });

    // 6. Vapor Keys — chorus/delay/reverb wash.
    factory.push_back ({ "Vapor Keys", {
        { id::osc ('A', "wtpos"), 0.40f },  { id::env (1, "release"), 1.3f },
        { id::fx ("chorus", "mix"), 0.6f }, { id::fx ("delay", "mix"), 0.4f },
        { id::fx ("reverb", "mix"), 0.4f } } });
}

//==============================================================================
juce::File PresetManager::getUserDir() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("ZandersWave").getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

bool PresetManager::saveUserPreset (const juce::String& name) const
{
    const auto safe = juce::File::createLegalFileName (name);
    if (safe.isEmpty())
        return false;

    auto file = getUserDir().getChildFile (safe + kFileExt);
    if (auto xml = captureState().createXml())
        return xml->writeTo (file);
    return false;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    for (const auto& f : getUserDir().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kFileExt))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool PresetManager::loadUserPreset (const juce::String& name)
{
    auto file = getUserDir().getChildFile (name + kFileExt);
    if (! file.existsAsFile())
        return false;

    if (auto xml = juce::XmlDocument::parse (file))
    {
        applyState (juce::ValueTree::fromXml (*xml));
        return true;
    }
    return false;
}

juce::StringArray PresetManager::getAllProgramNames() const
{
    juce::StringArray names;
    for (int i = 0; i < getNumFactory(); ++i)
        names.add (factoryName (i));
    names.addArray (getUserPresetNames());
    return names;
}

} // namespace zw
