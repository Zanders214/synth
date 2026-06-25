#include "Arpeggiator.h"
#include <algorithm>
#include <array>

namespace zw
{

void Arpeggiator::prepareParams (const juce::AudioProcessorValueTreeState& s)
{
    pEnable = s.getRawParameterValue (id::arpEnable);
    pRate   = s.getRawParameterValue (id::arpRate);
    pMode   = s.getRawParameterValue (id::arpMode);
    pOct    = s.getRawParameterValue (id::arpOctaves);
    pGate   = s.getRawParameterValue (id::arpGate);
    pSwing  = s.getRawParameterValue (id::arpSwing);
    for (int i = 0; i < 16; ++i)
        pStep[i] = s.getRawParameterValue (id::arpStep (i + 1));
}

void Arpeggiator::reset() noexcept
{
    held.clear();
    activeNotes.clear();
    seqIndex = 0;
    stepIndex = -1;
    samplesToNextStep = 0.0;
    gateSamplesLeft = 0.0;
    stepIsEven = true;
}

int Arpeggiator::buildSequence (std::vector<int>& seq)
{
    seq.clear();
    if (held.empty()) return 0;

    const auto mode = (int) val (pMode);                 // 0 Up,1 Down,2 Up/Dn,3 Random,4 Chord,5 As Played
    const int octs = juce::jlimit (1, 4, (int) val (pOct));

    baseScratch.clear();
    for (const auto& h : held) baseScratch.push_back (h.note);

    if (mode == 5)                  { /* As Played: keep order */ }
    else if (mode == 1)             { std::sort (baseScratch.begin(), baseScratch.end(), std::greater<int>()); }
    else                            { std::sort (baseScratch.begin(), baseScratch.end()); }   // Up / Up-Dn / Random / Chord

    // Extend across octave range, building directly into seq.
    for (int o = 0; o < octs; ++o)
        for (int n : baseScratch)
            seq.push_back (n + 12 * o);

    if (mode == 2 && seq.size() > 1)   // Up/Dn: append the descending middle
    {
        const auto sz = (int) seq.size();
        for (int i = sz - 2; i >= 1; --i)
            seq.push_back (seq[(size_t) i]);
    }

    return (int) seq.size();
}

void Arpeggiator::allNotesOff (juce::MidiBuffer& out, int sample)
{
    for (int n : activeNotes)
        out.addEvent (juce::MidiMessage::noteOff (1, n), sample);
    activeNotes.clear();
}

// Fires one arp step at sampleOffset: advances the step index, selects & emits
// the note(s), and arms the gate. Uses swungLenCurrent (set by the caller) for
// the gate length. Mutations preserve the exact order of the inline original.
void Arpeggiator::emitStep (juce::MidiBuffer& out, int sampleOffset)
{
    const double swungLen = swungLenCurrent;
    const float  gate     = juce::jlimit (0.05f, 1.0f, val (pGate));

    stepIndex = (stepIndex + 1) % 16;

    if (held.empty()) { if (! activeNotes.empty()) allNotesOff (out, sampleOffset); return; }
    if (! on (pStep[stepIndex])) return;        // step gated off

    allNotesOff (out, sampleOffset);

    const int count = buildSequence (seqScratch);
    if (count == 0) return;

    const auto mode = (int) val (pMode);
    const juce::uint8 vel = held.back().vel;

    if (mode == 4)   // Chord: all held notes
    {
        for (const auto& h : held)
        {
            out.addEvent (juce::MidiMessage::noteOn (1, h.note, h.vel), sampleOffset);
            activeNotes.push_back (h.note);
        }
    }
    else
    {
        if (mode == 3) seqIndex = juce::Random::getSystemRandom().nextInt (count);
        else           seqIndex = seqIndex % count;

        const int note = juce::jlimit (0, 127, seqScratch[(size_t) seqIndex]);
        out.addEvent (juce::MidiMessage::noteOn (1, note, vel), sampleOffset);
        activeNotes.push_back (note);
        if (mode != 3) seqIndex = (seqIndex + 1) % count;
    }

    // Gate must end strictly before the next step so a new note isn't cut.
    gateSamplesLeft = juce::jmax (1.0, juce::jmin (swungLen - 1.0, swungLen * gate));
}

// Updates the held-note set from the incoming stream and gathers the
// pass-through buffer (when the arp is disabled, notes pass straight through;
// CC/pitch-bend always pass). Factored out of process() to keep its cognitive
// complexity in check.
void Arpeggiator::absorbMidi (const juce::MidiBuffer& midi, juce::MidiBuffer& passthrough, bool enabled)
{
    passthrough.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int  t = meta.samplePosition;
        if (m.isNoteOn())
        {
            const int note = m.getNoteNumber();
            held.erase (std::remove_if (held.begin(), held.end(),
                        [note] (const Held& h) { return h.note == note; }), held.end());
            held.push_back ({ note, (juce::uint8) m.getVelocity() });
            if (! enabled) passthrough.addEvent (m, t);
        }
        else if (m.isNoteOff())
        {
            const int note = m.getNoteNumber();
            held.erase (std::remove_if (held.begin(), held.end(),
                        [note] (const Held& h) { return h.note == note; }), held.end());
            if (! enabled) passthrough.addEvent (m, t);
        }
        else
        {
            passthrough.addEvent (m, t);   // CC, pitch bend, etc. always pass
        }
    }
}

void Arpeggiator::process (juce::MidiBuffer& midi, int numSamples, double bpm)
{
    const bool enabled = on (pEnable);

    juce::MidiBuffer& passthrough = passthroughBuf;   // reused; no allocation
    absorbMidi (midi, passthrough, enabled);

    if (! enabled)
    {
        if (! activeNotes.empty()) allNotesOff (passthrough, 0);
        midi.clear();
        midi.addEvents (passthrough, 0, numSamples, 0);
        return;
    }

    juce::MidiBuffer& out = outBuf;   // reused; no allocation
    out.clear();
    out.addEvents (passthrough, 0, numSamples, 0);

    if (bpm <= 0.0) bpm = 120.0;
    static constexpr std::array<double, 6> beatsPerStep { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    const int rate = juce::jlimit (0, 5, (int) val (pRate));
    const double stepDur = (60.0 / bpm) * beatsPerStep[rate] * sampleRate;
    const float  swing   = val (pSwing);

    for (int i = 0; i < numSamples; ++i)
    {
        if (gateSamplesLeft > 0.0)
        {
            gateSamplesLeft -= 1.0;
            if (gateSamplesLeft <= 0.0)
                allNotesOff (out, i);
        }

        samplesToNextStep -= 1.0;
        if (samplesToNextStep <= 0.0)
        {
            // Swing pushes the off-beats later.
            const double swungLen = stepDur * (stepIsEven ? (1.0 + swing * 0.5) : (1.0 - swing * 0.5));
            samplesToNextStep += swungLen;
            stepIsEven = ! stepIsEven;
            swungLenCurrent = swungLen;

            emitStep (out, i);
        }
    }

    midi.clear();
    midi.addEvents (out, 0, numSamples, 0);
}

} // namespace zw
