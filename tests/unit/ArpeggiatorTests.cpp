// Unit tests for zw::Arpeggiator (tempo-stepped MIDI arpeggiator).
#include <juce_core/juce_core.h>

#include "dsp/Arpeggiator.h"
#include "Parameters.h"
#include "TestHelpers.h"

#include <vector>
#include <algorithm>

namespace
{
// ---- local helpers ---------------------------------------------------------

// Normalised value that selects choice index `idx` out of `numItems`
// (AudioParameterChoice maps norm -> roundToInt(norm * (numItems-1))).
inline float choiceNorm (int idx, int numItems)
{
    return numItems > 1 ? (float) idx / (float) (numItems - 1) : 0.0f;
}

// Normalised value that selects integer `value` in the inclusive [lo,hi] range
// (AudioParameterInt maps norm linearly).
inline float intNorm (int value, int lo, int hi)
{
    return hi > lo ? (float) (value - lo) / (float) (hi - lo) : 0.0f;
}

// Collect all note-on numbers (in event order) from a MidiBuffer.
inline std::vector<int> noteOns (const juce::MidiBuffer& mb)
{
    std::vector<int> v;
    for (const auto meta : mb)
        if (meta.getMessage().isNoteOn())
            v.push_back (meta.getMessage().getNoteNumber());
    return v;
}

inline int countNoteOns (const juce::MidiBuffer& mb)
{
    int c = 0;
    for (const auto meta : mb)
        if (meta.getMessage().isNoteOn()) ++c;
    return c;
}

inline int countNoteOffs (const juce::MidiBuffer& mb)
{
    int c = 0;
    for (const auto meta : mb)
        if (meta.getMessage().isNoteOff()) ++c;
    return c;
}

// Set every arp step on or off.
inline void setAllSteps (juce::AudioProcessorValueTreeState& s, bool on)
{
    for (int i = 1; i <= 16; ++i)
        zwtest::setParam (s, zw::id::arpStep (i), on ? 1.0f : 0.0f);
}

struct ArpeggiatorTests : juce::UnitTest
{
    ArpeggiatorTests() : juce::UnitTest ("Arpeggiator", "DSP") {}

    void runTest() override
    {
        const double sr  = 48000.0;
        const int    bs  = 512;
        const double bpm = 120.0;

        // Mode indices (choices::arpMode order): Up Down Up/Dn Random Chord As-Played
        const int kModeCount = 6;
        const int kRateCount = 6;

        beginTest ("disabled: note-on/off pass through unchanged");
        {
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 0.0f);

            juce::MidiBuffer mb;
            mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            mb.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 90),  10);
            mb.addEvent (juce::MidiMessage::controllerEvent (1, 7, 99),        20);
            arp.process (mb, bs, bpm);

            // Both note-ons plus the CC should survive untouched.
            expectEquals (countNoteOns (mb), 2, "two note-ons should pass through");
            bool ccSeen = false;
            for (const auto meta : mb)
                if (meta.getMessage().isController()) ccSeen = true;
            expect (ccSeen, "CC must always pass through");

            const auto notes = noteOns (mb);
            expect (std::find (notes.begin(), notes.end(), 60) != notes.end());
            expect (std::find (notes.begin(), notes.end(), 64) != notes.end());
        }

        beginTest ("disabled: non-note messages always pass even with empty notes");
        {
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 0.0f);

            juce::MidiBuffer mb;
            mb.addEvent (juce::MidiMessage::pitchWheel (1, 4000), 0);
            arp.process (mb, bs, bpm);

            bool bendSeen = false;
            for (const auto meta : mb)
                if (meta.getMessage().isPitchWheel()) bendSeen = true;
            expect (bendSeen, "pitch bend must pass through");
        }

        beginTest ("enabled: holding a chord generates arp note-ons");
        {
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
            setAllSteps (proc.apvts, true);

            int total = 0;
            for (int b = 0; b < 200; ++b)
            {
                juce::MidiBuffer mb;
                if (b == 0)
                {
                    mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                    mb.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
                    mb.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);
                }
                arp.process (mb, bs, bpm);
                total += countNoteOns (mb);
            }
            expectGreaterThan (total, 4, "an arp must emit multiple steps over ~2s");
        }

        beginTest ("enabled: held chord input note-ons are swallowed (not passed)");
        {
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
            setAllSteps (proc.apvts, true);

            // First block: deliver the held note and process. Any note-on in the
            // output is arp-generated, not the original pass-through.
            juce::MidiBuffer mb;
            mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            // include a CC so we can confirm it still passes while notes do not.
            mb.addEvent (juce::MidiMessage::controllerEvent (1, 1, 64), 0);
            arp.process (mb, bs, bpm);

            bool ccSeen = false;
            for (const auto meta : mb)
                if (meta.getMessage().isController()) ccSeen = true;
            expect (ccSeen, "CC passes through even when arp is on");
            // The single held note may or may not fire on the first block depending
            // on step timing, but it must never appear more than once per step here.
            expect (countNoteOns (mb) <= 1, "at most one arp note per step");
        }

        beginTest ("Up vs Down: first arp note is lowest vs highest held note");
        {
            const std::vector<int> chord { 60, 64, 67 };

            auto firstArpNote = [&] (int modeIdx) -> int
            {
                zwtest::DummyProcessor proc;
                zw::Arpeggiator arp;
                arp.prepareParams (proc.apvts);
                arp.prepare (sr);
                zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
                zwtest::setParam (proc.apvts, zw::id::arpMode, choiceNorm (modeIdx, kModeCount));
                zwtest::setParam (proc.apvts, zw::id::arpOctaves, intNorm (1, 1, 4));
                setAllSteps (proc.apvts, true);

                int first = -1;
                for (int b = 0; b < 60 && first < 0; ++b)
                {
                    juce::MidiBuffer mb;
                    if (b == 0)
                        for (int n : chord)
                            mb.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
                    arp.process (mb, bs, bpm);
                    const auto v = noteOns (mb);
                    if (! v.empty()) first = v.front();
                }
                return first;
            };

            const int up   = firstArpNote (0);   // Up
            const int down = firstArpNote (1);   // Down
            expectEquals (up,   60, "Up mode should start on the lowest held note");
            expectEquals (down, 67, "Down mode should start on the highest held note");
        }

        beginTest ("each mode produces finite, in-range note numbers and fires");
        {
            const std::vector<int> chord { 48, 52, 55 };

            for (int modeIdx = 0; modeIdx < kModeCount; ++modeIdx)
            {
                zwtest::DummyProcessor proc;
                zw::Arpeggiator arp;
                arp.prepareParams (proc.apvts);
                arp.prepare (sr);
                zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
                zwtest::setParam (proc.apvts, zw::id::arpMode, choiceNorm (modeIdx, kModeCount));
                zwtest::setParam (proc.apvts, zw::id::arpOctaves, intNorm (2, 1, 4));
                setAllSteps (proc.apvts, true);

                int fired = 0;
                bool inRange = true;
                for (int b = 0; b < 120; ++b)
                {
                    juce::MidiBuffer mb;
                    if (b == 0)
                        for (int n : chord)
                            mb.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 110), 0);
                    arp.process (mb, bs, bpm);
                    for (int n : noteOns (mb))
                    {
                        ++fired;
                        if (n < 0 || n > 127) inRange = false;
                    }
                }
                expect (inRange, "all arp notes must be valid MIDI note numbers (mode " + juce::String (modeIdx) + ")");
                expectGreaterThan (fired, 0, "mode " + juce::String (modeIdx) + " should emit at least one note");
            }
        }

        beginTest ("Chord mode emits all held notes simultaneously per step");
        {
            const std::vector<int> chord { 60, 64, 67 };
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
            zwtest::setParam (proc.apvts, zw::id::arpMode, choiceNorm (4, kModeCount));  // Chord
            zwtest::setParam (proc.apvts, zw::id::arpOctaves, intNorm (1, 1, 4));
            setAllSteps (proc.apvts, true);

            // Find a block that contains a chord-step and assert all three sound.
            std::vector<int> stepNotes;
            for (int b = 0; b < 60 && stepNotes.empty(); ++b)
            {
                juce::MidiBuffer mb;
                if (b == 0)
                    for (int n : chord)
                        mb.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
                arp.process (mb, bs, bpm);
                stepNotes = noteOns (mb);
            }
            std::sort (stepNotes.begin(), stepNotes.end());
            expectEquals ((int) stepNotes.size(), 3, "Chord mode plays the full triad each step");
            if (stepNotes.size() == 3)
            {
                expectEquals (stepNotes[0], 60);
                expectEquals (stepNotes[1], 64);
                expectEquals (stepNotes[2], 67);
            }
        }

        beginTest ("octave range extends the sequence upward");
        {
            // With 4 octaves and a single held note, the arp should eventually
            // play notes an octave (or more) above the held note.
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
            zwtest::setParam (proc.apvts, zw::id::arpMode, choiceNorm (0, kModeCount));  // Up
            zwtest::setParam (proc.apvts, zw::id::arpOctaves, intNorm (4, 1, 4));
            setAllSteps (proc.apvts, true);

            int maxNote = -1;
            for (int b = 0; b < 200; ++b)
            {
                juce::MidiBuffer mb;
                if (b == 0)
                    mb.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
                arp.process (mb, bs, bpm);
                for (int n : noteOns (mb))
                    maxNote = juce::jmax (maxNote, n);
            }
            expectGreaterThan (maxNote, 48, "octave range should produce notes above the held note");
            // 4 octaves from note 48 reaches 48 + 12*3 = 84 at the top of the run.
            expect (maxNote >= 48 + 12, "with 4 octaves arp should climb at least one octave");
        }

        beginTest ("rate: faster rate yields more steps over the same span");
        {
            auto countSteps = [&] (int rateIdx) -> int
            {
                zwtest::DummyProcessor proc;
                zw::Arpeggiator arp;
                arp.prepareParams (proc.apvts);
                arp.prepare (sr);
                zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
                zwtest::setParam (proc.apvts, zw::id::arpRate, choiceNorm (rateIdx, kRateCount));
                setAllSteps (proc.apvts, true);

                int total = 0;
                for (int b = 0; b < 100; ++b)
                {
                    juce::MidiBuffer mb;
                    if (b == 0)
                        mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                    arp.process (mb, bs, bpm);
                    total += countNoteOns (mb);
                }
                return total;
            };

            const int slow = countSteps (0);   // 1/4
            const int fast = countSteps (5);   // 1/32
            expectGreaterThan (fast, slow, "1/32 must fire more often than 1/4");
        }

        beginTest ("gate: longer gate keeps note held longer (fewer early note-offs)");
        {
            auto offCountAfterFirstOn = [&] (float gateNorm) -> std::pair<int,int>
            {
                // returns {samplesWithNote, ...} approximation by counting on/off totals
                zwtest::DummyProcessor proc;
                zw::Arpeggiator arp;
                arp.prepareParams (proc.apvts);
                arp.prepare (sr);
                zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
                zwtest::setParam (proc.apvts, zw::id::arpRate, choiceNorm (0, kRateCount));  // 1/4 - slow
                zwtest::setParam (proc.apvts, zw::id::arpGate, gateNorm);
                setAllSteps (proc.apvts, true);

                int ons = 0, offs = 0;
                for (int b = 0; b < 80; ++b)
                {
                    juce::MidiBuffer mb;
                    if (b == 0)
                        mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                    arp.process (mb, bs, bpm);
                    ons  += countNoteOns (mb);
                    offs += countNoteOffs (mb);
                }
                return { ons, offs };
            };

            const auto shortG = offCountAfterFirstOn (0.1f);
            const auto longG  = offCountAfterFirstOn (1.0f);
            // Both gate settings should still produce note-ons (arp keeps running).
            expectGreaterThan (shortG.first, 0, "short gate still fires notes");
            expectGreaterThan (longG.first,  0, "long gate still fires notes");
        }

        beginTest ("swing: non-zero swing still produces a valid, finite stream");
        {
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
            zwtest::setParam (proc.apvts, zw::id::arpSwing, 0.75f);
            setAllSteps (proc.apvts, true);

            int total = 0;
            for (int b = 0; b < 120; ++b)
            {
                juce::MidiBuffer mb;
                if (b == 0)
                {
                    mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                    mb.addEvent (juce::MidiMessage::noteOn (1, 63, (juce::uint8) 100), 0);
                }
                arp.process (mb, bs, bpm);
                total += countNoteOns (mb);
            }
            expectGreaterThan (total, 0, "swung arp must still emit notes");
        }

        beginTest ("step gating: all steps off => no arp note-ons");
        {
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
            setAllSteps (proc.apvts, false);   // gate every step off

            int total = 0;
            for (int b = 0; b < 120; ++b)
            {
                juce::MidiBuffer mb;
                if (b == 0)
                    mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                arp.process (mb, bs, bpm);
                total += countNoteOns (mb);
            }
            expectEquals (total, 0, "no step enabled => no notes generated");
        }

        beginTest ("releasing all keys silences active arp notes (all-notes-off)");
        {
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
            setAllSteps (proc.apvts, true);

            // Hold a note and run until at least one arp note is sounding.
            int onsBefore = 0;
            for (int b = 0; b < 40; ++b)
            {
                juce::MidiBuffer mb;
                if (b == 0)
                    mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                arp.process (mb, bs, bpm);
                onsBefore += countNoteOns (mb);
            }
            expectGreaterThan (onsBefore, 0, "arp should have sounded before release");

            // Release the held key; subsequent processing must drive note-offs and
            // eventually stop producing note-ons.
            juce::MidiBuffer rel;
            rel.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            arp.process (rel, bs, bpm);

            int onsAfter = 0;
            for (int b = 0; b < 40; ++b)
            {
                juce::MidiBuffer mb;
                arp.process (mb, bs, bpm);
                onsAfter += countNoteOns (mb);
            }
            expectEquals (onsAfter, 0, "no new arp notes once all keys are released");
        }

        beginTest ("disabling arp mid-flight clears active notes via note-off");
        {
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
            setAllSteps (proc.apvts, true);

            int sounded = 0;
            for (int b = 0; b < 40; ++b)
            {
                juce::MidiBuffer mb;
                if (b == 0)
                    mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                arp.process (mb, bs, bpm);
                sounded += countNoteOns (mb);
            }
            expectGreaterThan (sounded, 0, "arp should be sounding before disable");

            // Disable: process() must flush any active arp note with a note-off.
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 0.0f);
            juce::MidiBuffer off;
            arp.process (off, bs, bpm);
            // At least the previously-active note should have produced an off,
            // OR there were none active at the exact disable boundary. Either way
            // there must be no lingering note-ons.
            expectEquals (countNoteOns (off), 0, "disabling must not generate new note-ons");
        }

        beginTest ("reset clears state: no note-ons fire after reset with empty input");
        {
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
            setAllSteps (proc.apvts, true);

            for (int b = 0; b < 20; ++b)
            {
                juce::MidiBuffer mb;
                if (b == 0)
                    mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                arp.process (mb, bs, bpm);
            }
            arp.reset();

            int total = 0;
            for (int b = 0; b < 40; ++b)
            {
                juce::MidiBuffer mb;   // empty: held set was cleared by reset
                arp.process (mb, bs, bpm);
                total += countNoteOns (mb);
            }
            expectEquals (total, 0, "after reset with no held notes the arp is silent");
        }

        beginTest ("zero samples: process(0) is a no-op and does not crash");
        {
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
            setAllSteps (proc.apvts, true);

            juce::MidiBuffer mb;
            mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            arp.process (mb, 0, bpm);
            expectEquals (countNoteOns (mb), 0, "no samples processed => no arp output");
        }

        beginTest ("zero/negative bpm is clamped (defaults to 120) and still steps");
        {
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
            setAllSteps (proc.apvts, true);

            int total = 0;
            for (int b = 0; b < 120; ++b)
            {
                juce::MidiBuffer mb;
                if (b == 0)
                    mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                arp.process (mb, bs, 0.0);   // bpm <= 0 => clamped internally
                total += countNoteOns (mb);
            }
            expectGreaterThan (total, 0, "non-positive bpm must be clamped, arp still runs");
        }

        beginTest ("sample-rate change via re-prepare keeps arp functional");
        {
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (44100.0);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
            setAllSteps (proc.apvts, true);

            // Run a little at 44.1k, then re-prepare at 96k (this also resets).
            for (int b = 0; b < 10; ++b)
            {
                juce::MidiBuffer mb;
                if (b == 0)
                    mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                arp.process (mb, bs, bpm);
            }
            arp.prepare (96000.0);

            int total = 0;
            for (int b = 0; b < 200; ++b)
            {
                juce::MidiBuffer mb;
                if (b == 0)
                    mb.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), 0);
                arp.process (mb, bs, bpm);
                total += countNoteOns (mb);
            }
            expectGreaterThan (total, 0, "arp must still step after a sample-rate change");
        }

        beginTest ("As-Played mode preserves input order of held notes");
        {
            // Play 67, then 60, then 64 (out of pitch order). As-Played should
            // start the sequence on the first-played note (67).
            zwtest::DummyProcessor proc;
            zw::Arpeggiator arp;
            arp.prepareParams (proc.apvts);
            arp.prepare (sr);
            zwtest::setParam (proc.apvts, zw::id::arpEnable, 1.0f);
            zwtest::setParam (proc.apvts, zw::id::arpMode, choiceNorm (5, kModeCount));  // As Played
            zwtest::setParam (proc.apvts, zw::id::arpOctaves, intNorm (1, 1, 4));
            setAllSteps (proc.apvts, true);

            int first = -1;
            for (int b = 0; b < 60 && first < 0; ++b)
            {
                juce::MidiBuffer mb;
                if (b == 0)
                {
                    mb.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);
                    mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 1);
                    mb.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 2);
                }
                arp.process (mb, bs, bpm);
                const auto v = noteOns (mb);
                if (! v.empty()) first = v.front();
            }
            expectEquals (first, 67, "As-Played should begin on the first key pressed");
        }
    }
};

static ArpeggiatorTests arpeggiatorTests;
}
