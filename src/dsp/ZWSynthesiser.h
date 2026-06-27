#pragma once

#include <JuceHeader.h>
#include "Voice.h"
#include "ParamRefs.h"
#include <array>

namespace zw
{

//==============================================================================
// A juce::Synthesiser that adds the two voicing behaviours the base class lacks,
// driven by the global voicing parameters (read on the audio thread via ParamRefs):
//
//   * global_monomode — Poly (0) / Mono (1) / Legato (2). In Mono/Legato a single
//     voice plays with last-note priority. Legato retargets the pitch without
//     retriggering the amp envelope (ZWVoice::changeNote); Mono retriggers.
//   * global_polyphony — caps the number of simultaneously allocated voices to
//     1..N by bounding findFreeVoice() to the first N voices (steal the oldest
//     within that window).
//
// All overrides run on the audio thread (renderNextBlock -> handleMidiEvent), so
// the held-note bookkeeping is a fixed-size array — no allocation. The only lock
// taken is the base class's existing (recursive) lock, exactly as the stock
// Synthesiser::noteOn/noteOff already do.
//==============================================================================
class ZWSynthesiser : public juce::Synthesiser
{
public:
    // Give the synth the processor's cached parameter handles (monoMode/polyphony).
    void setParamRefs (const ParamRefs& refs) noexcept { p = &refs; }

    void noteOn (int midiChannel, int midiNoteNumber, float velocity) override
    {
        if (monoModeIndex() == 0)   // Poly: stock behaviour (bounded by findFreeVoice)
        {
            juce::Synthesiser::noteOn (midiChannel, midiNoteNumber, velocity);
            return;
        }

        const juce::ScopedLock sl (lock);
        pushHeld (midiNoteNumber, velocity);
        soundNote (midiChannel, midiNoteNumber, velocity);
    }

    void noteOff (int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override
    {
        if (monoModeIndex() == 0)
        {
            juce::Synthesiser::noteOff (midiChannel, midiNoteNumber, velocity, allowTailOff);
            return;
        }

        const juce::ScopedLock sl (lock);
        removeHeld (midiNoteNumber);
        if (heldCount > 0)
        {
            const Held top = held[(size_t) (heldCount - 1)];   // fall back to newest still-held
            if (top.note != soundingNote)
                soundNote (midiChannel, top.note, top.vel);
        }
        else if (auto* v = monoVoice())
        {
            stopVoice (v, velocity, allowTailOff);
            soundingNote = -1;
        }
    }

    void allNotesOff (int midiChannel, bool allowTailOff) override
    {
        heldCount    = 0;
        soundingNote = -1;
        juce::Synthesiser::allNotesOff (midiChannel, allowTailOff);
    }

protected:
    // Polyphony cap: only the first N voices are eligible; steal the oldest within N.
    juce::SynthesiserVoice* findFreeVoice (juce::SynthesiserSound* sound, int /*channel*/,
                                           int /*note*/, bool stealIfNoneAvailable) const override
    {
        const int n = activePolyphony();
        juce::SynthesiserVoice* oldest = nullptr;
        for (int i = 0; i < n; ++i)
        {
            auto* v = getVoice (i);
            if (! v->isVoiceActive() && v->canPlaySound (sound))
                return v;
            if (v->canPlaySound (sound) && (oldest == nullptr || v->wasStartedBefore (*oldest)))
                oldest = v;
        }
        return stealIfNoneAvailable ? oldest : nullptr;
    }

private:
    struct Held { int note; float vel; };

    int monoModeIndex() const noexcept
    {
        return (p != nullptr && p->monoMode != nullptr) ? juce::jlimit (0, 2, (int) p->monoMode->load()) : 0;
    }
    int activePolyphony() const noexcept
    {
        const int n = (p != nullptr && p->polyphony != nullptr) ? (int) p->polyphony->load() : getNumVoices();
        return juce::jlimit (1, juce::jmax (1, getNumVoices()), n);
    }

    juce::SynthesiserVoice* monoVoice() const noexcept { return getNumVoices() > 0 ? getVoice (0) : nullptr; }

    juce::SynthesiserSound* soundFor (int note, int channel) const
    {
        for (auto* s : sounds)
            if (s->appliesToNote (note) && s->appliesToChannel (channel))
                return s;
        return nullptr;
    }

    // Start (or, in Legato while already sounding, retarget) the single mono voice.
    void soundNote (int channel, int note, float velocity)
    {
        auto* v = monoVoice();
        if (v == nullptr) return;

        if (monoModeIndex() == 2 && v->isVoiceActive())      // Legato: glide, no retrigger
            static_cast<ZWVoice*> (v)->changeNote (note);
        else if (auto* s = soundFor (note, channel))         // Mono / first note: retrigger
            startVoice (v, s, channel, note, velocity);
        soundingNote = note;
    }

    void pushHeld (int note, float vel) noexcept
    {
        removeHeld (note);                                   // re-press moves the note to the top
        if (heldCount < (int) held.size())
            held[(size_t) heldCount++] = { note, vel };
    }
    void removeHeld (int note) noexcept
    {
        int w = 0;
        for (int r = 0; r < heldCount; ++r)
            if (held[(size_t) r].note != note)
                held[(size_t) w++] = held[(size_t) r];
        heldCount = w;
    }

    const ParamRefs* p = nullptr;
    std::array<Held, 128> held {};
    int heldCount    = 0;
    int soundingNote = -1;
};

} // namespace zw
