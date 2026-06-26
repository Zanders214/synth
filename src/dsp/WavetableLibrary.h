#pragma once

#include <JuceHeader.h>
#include "Wavetable.h"
#include <vector>
#include <memory>
#include <atomic>

namespace zw
{

//==============================================================================
// Owns the factory set of band-limited wavetables plus one user-import slot.
//
// All tables are pre-built on the message thread (in the constructor) or when a
// .wav is imported; the audio thread only ever reads getByIndex(), which is a
// pointer fetch (vector element or an atomic load) — no allocation, file I/O or
// locks. OSC A and OSC B each pick a table independently via the "wtselect"
// choice parameter, whose option order matches choices::wavetable().
//
// The user slot is published through an atomic pointer; previously-imported
// tables are retained for the library's lifetime so a pointer a voice already
// cached can never dangle when a new .wav replaces the slot.
//==============================================================================
class WavetableLibrary
{
public:
    WavetableLibrary();

    // Total selectable slots: every factory table plus the single user slot.
    // Equals choices::wavetable().size().
    int size() const noexcept { return (int) factory.size() + 1; }

    // Display name for a slot (factory name, or "User Import" for the last slot).
    juce::String getName (int index) const;

    // Table for a slot. Never returns nullptr: the user slot falls back to the
    // first factory table until a .wav has been imported. Audio-thread safe.
    const Wavetable* getByIndex (int index) const noexcept;

    // Index of the user-import slot (the last selectable slot).
    int getUserIndex() const noexcept { return (int) factory.size(); }

    // Read a .wav, slice it into kFrameSize single-cycle frames and band-limit it
    // into a Wavetable that replaces the user slot. Message thread only (does file
    // I/O and allocation). Returns false if the file can't be read.
    bool loadFromWav (const juce::File& file);

private:
    void buildFactory();
    void addFactory (const juce::String& name,
                     const std::function<float (int frame, int numFrames, float phase01)>& fn,
                     int numFrames);

    std::vector<std::unique_ptr<Wavetable>> factory;   // stable addresses (vector of ptrs)
    juce::StringArray                       names;     // factory names (without user slot)

    // User slot: built on the message thread, published atomically. Old imports
    // are kept alive for the session so audio-thread pointers never dangle.
    std::vector<std::unique_ptr<Wavetable>> ownedUser;
    std::atomic<const Wavetable*>           userTable { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavetableLibrary)
};

} // namespace zw
