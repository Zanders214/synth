// Unit tests for the global voicing behaviours added by zw::ZWSynthesiser:
//   * Polyphony cap   (global_polyphony) — at most N voices sound at once.
//   * Mono   (global_monomode = 1) — a single voice, last-note priority, retriggered.
//   * Legato (global_monomode = 2) — a single voice, retargeted WITHOUT retriggering:
//       the base voice keeps its original note-on (changeNote only glides the pitch).
//
// Built on the same rig style as VoiceTests.cpp (DummyProcessor + ParamRefs + a synth
// populated with ZWSound/ZWVoices), but driving a zw::ZWSynthesiser so the mono/poly
// overrides are exercised.

#include <juce_core/juce_core.h>          // for juce::UnitTest
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "dsp/WavetableLibrary.h"
#include "dsp/ParamRefs.h"
#include "dsp/ModMatrix.h"
#include "dsp/Voice.h"
#include "dsp/ZWSynthesiser.h"
#include "TestHelpers.h"

#include <atomic>
#include <cmath>

namespace
{
// A voice rig built around zw::ZWSynthesiser (not a plain juce::Synthesiser), with the
// param refs wired so monoMode/polyphony are read live. Rebuilt per case.
struct ModeRig
{
    zwtest::DummyProcessor proc;
    zw::ParamRefs          refs;
    zw::WavetableLibrary   library;
    zw::ModMatrix          matrix;
    std::atomic<double>    bpm { 120.0 };
    std::atomic<double>    lastNoteFreq { 440.0 };
    zw::ZWSynthesiser      synth;

    explicit ModeRig (double sampleRate = 48000.0, int numVoices = 8)
    {
        refs.prepare (proc.apvts);
        synth.addSound (new zw::ZWSound());
        for (int i = 0; i < numVoices; ++i)
            synth.addVoice (new zw::ZWVoice (refs, library, matrix, bpm, lastNoteFreq));
        synth.setParamRefs (refs);
        synth.setNoteStealingEnabled (true);
        synth.setCurrentPlaybackSampleRate (sampleRate);
    }

    // Set an int/choice parameter by its raw (denormalised) value.
    void setRaw (const juce::String& id, float rawValue)
    {
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (rawValue));
    }

    void render (int blocks, int bs)
    {
        juce::AudioBuffer<float> buffer (2, bs);
        juce::MidiBuffer empty;
        for (int b = 0; b < blocks; ++b) { buffer.clear(); synth.renderNextBlock (buffer, empty, 0, bs); }
    }

    int activeVoices() const
    {
        int n = 0;
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (synth.getVoice (i)->isVoiceActive()) ++n;
        return n;
    }

    int playingNote (int voiceIndex) const { return synth.getVoice (voiceIndex)->getCurrentlyPlayingNote(); }
};

struct VoiceModeTests : juce::UnitTest
{
    VoiceModeTests() : juce::UnitTest ("VoiceModes", "DSP") {}

    void runTest() override
    {
        const double sr = 48000.0;
        const int    bs = 256;

        beginTest ("polyphony cap limits the number of sounding voices");
        {
            ModeRig rig (sr, 8);
            rig.setRaw (zw::id::glidePoly, 4.0f);          // cap at 4 (mode stays Poly)
            for (const int note : { 60, 62, 64, 65, 67, 69, 71, 72 })
                rig.synth.noteOn (1, note, 0.9f);
            rig.render (1, bs);
            expect (rig.activeVoices() <= 4, "no more than the polyphony cap may sound");
            expect (rig.activeVoices() >= 1, "the capped voices should still be sounding");
        }

        beginTest ("a higher polyphony cap allows more simultaneous voices");
        {
            ModeRig rig (sr, 8);
            rig.setRaw (zw::id::glidePoly, 8.0f);
            for (const int note : { 60, 62, 64, 65, 67, 69 })
                rig.synth.noteOn (1, note, 0.9f);
            rig.render (1, bs);
            expectEquals (rig.activeVoices(), 6, "all six notes sound when under the cap");
        }

        beginTest ("Mono mode: one voice, last-note priority, retriggered");
        {
            ModeRig rig (sr, 8);
            rig.setRaw (zw::id::glideMono, 1.0f);          // Mono
            rig.synth.noteOn (1, 60, 1.0f);
            rig.render (1, bs);
            rig.synth.noteOn (1, 64, 1.0f);                // second note while 60 is held
            rig.render (1, bs);
            expectEquals (rig.activeVoices(), 1, "mono mode plays a single voice");
            expectEquals (rig.playingNote (0), 64, "newest note has priority and the voice retriggered to it");

            rig.synth.noteOff (1, 64, 0.0f, true);         // release the top note
            rig.render (1, bs);
            expectEquals (rig.activeVoices(), 1, "still one voice after releasing the top note");
            expectEquals (rig.playingNote (0), 60, "mono falls back to the still-held note");
        }

        beginTest ("Legato mode: pitch retargets WITHOUT retriggering the voice");
        {
            ModeRig rig (sr, 8);
            rig.setRaw (zw::id::glideMono, 2.0f);          // Legato
            rig.synth.noteOn (1, 60, 1.0f);
            rig.render (2, bs);
            const int started = rig.playingNote (0);
            rig.synth.noteOn (1, 64, 1.0f);                // legato slur up to 64
            rig.render (2, bs);
            expectEquals (rig.activeVoices(), 1, "legato plays a single voice");
            expectEquals (started, 60, "the first note started the voice");
            expectEquals (rig.playingNote (0), 60,
                          "legato does NOT retrigger: the voice keeps its original note-on");
        }

        beginTest ("Mono mode renders finite, audible audio across a slur");
        {
            ModeRig rig (sr, 4);
            rig.setRaw (zw::id::glideMono, 1.0f);
            rig.synth.noteOn (1, 55, 1.0f);
            rig.synth.noteOn (1, 60, 1.0f);
            rig.synth.noteOn (1, 64, 1.0f);

            juce::AudioBuffer<float> buffer (2, bs);
            juce::MidiBuffer empty;
            bool   finite = true;
            double sumSq  = 0.0;
            long   count  = 0;
            for (int b = 0; b < 8; ++b)
            {
                buffer.clear();
                rig.synth.renderNextBlock (buffer, empty, 0, bs);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < bs; ++i)
                    {
                        const float s = buffer.getSample (ch, i);
                        if (! std::isfinite (s)) finite = false;
                        sumSq += (double) s * (double) s;
                        ++count;
                    }
            }
            expect (finite, "mono output must be finite");
            expectGreaterThan (std::sqrt (sumSq / (double) juce::jmax ((long) 1, count)), 1.0e-4,
                               "mono note must be audible");
        }
    }
};

static VoiceModeTests voiceModeTests;
}
