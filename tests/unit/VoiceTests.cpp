// Integration-level unit tests for zw::ZWVoice (the polyphonic synth voice).
// Built the same way as tests/render_smoke.cpp: a DummyProcessor supplies the
// APVTS, ParamRefs caches handles, a Wavetable + ModMatrix back the voice, and a
// juce::Synthesiser drives note on/off and block rendering. These tests assert
// audible-but-finite output for held notes, silence after release, sample-rate
// handling, and that toggling source/filter enables and mod routes affects audio.

#include <juce_core/juce_core.h>          // for juce::UnitTest
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "dsp/Wavetable.h"
#include "dsp/WavetableLibrary.h"
#include "dsp/ParamRefs.h"
#include "dsp/ModMatrix.h"
#include "dsp/Voice.h"
#include "TestHelpers.h"

#include <atomic>
#include <cmath>

namespace
{
// A self-contained voice rig: owns the processor, refs, table, matrix, atomics
// and a Synthesiser populated with one ZWSound + N ZWVoices. Rebuilt per case so
// every test starts from the factory parameter/route defaults.
struct VoiceRig
{
    zwtest::DummyProcessor proc;
    zw::ParamRefs          refs;
    zw::WavetableLibrary   library;
    zw::ModMatrix          matrix;
    std::atomic<double>    bpm { 120.0 };
    std::atomic<double>    lastNoteFreq { 440.0 };
    juce::Synthesiser      synth;

    explicit VoiceRig (double sampleRate = 48000.0, int numVoices = 8)
    {
        refs.prepare (proc.apvts);

        synth.addSound (new zw::ZWSound());
        for (int i = 0; i < numVoices; ++i)
            synth.addVoice (new zw::ZWVoice (refs, library, matrix, bpm, lastNoteFreq));
        synth.setCurrentPlaybackSampleRate (sampleRate);
    }

    void setBool (const juce::String& id, bool on)
    {
        zwtest::setParam (proc.apvts, id, on ? 1.0f : 0.0f);
    }
};

// Measured stats over a rendered span.
struct Stats { double rms = 0.0; double peak = 0.0; bool finite = true; };

// Render `blocks` blocks of `bs` samples (clearing each block) and accumulate
// RMS/peak/finiteness over the whole span.
Stats renderSpan (juce::Synthesiser& synth, int blocks, int bs)
{
    juce::AudioBuffer<float> buffer (2, bs);
    juce::MidiBuffer empty;

    double sumSq = 0.0;
    double peak  = 0.0;
    bool   finite = true;
    long   count = 0;

    for (int b = 0; b < blocks; ++b)
    {
        buffer.clear();
        synth.renderNextBlock (buffer, empty, 0, bs);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < bs; ++i)
            {
                const float s = buffer.getSample (ch, i);
                if (! std::isfinite (s)) finite = false;
                sumSq += (double) s * (double) s;
                peak   = juce::jmax (peak, (double) std::abs (s));
                ++count;
            }
    }

    Stats st;
    st.rms    = std::sqrt (sumSq / (double) juce::jmax ((long) 1, count));
    st.peak   = peak;
    st.finite = finite;
    return st;
}

struct VoiceIntegrationTests : juce::UnitTest
{
    VoiceIntegrationTests() : juce::UnitTest ("VoiceIntegration", "DSP") {}

    void runTest() override
    {
        const double sr = 48000.0;
        const int    bs = 512;

        beginTest ("held note renders non-silent finite audio, then silence after release");
        {
            VoiceRig rig (sr, 8);
            rig.synth.noteOn (1, 60, 0.8f);

            const Stats held = renderSpan (rig.synth, 12, bs);   // ~128 ms held
            expect (held.finite, "held-note output must be finite");
            expectGreaterThan (held.rms, 1.0e-4, "held note must be audible");
            expectLessThan (held.peak, 4.0, "held note must not blow up");

            rig.synth.noteOff (1, 60, 0.0f, true);
            // Let any release tail fully die away (env1 release is short by default).
            renderSpan (rig.synth, 200, bs);
            const Stats after = renderSpan (rig.synth, 20, bs);
            expect (after.finite, "post-release output must be finite");
            expectLessThan (after.rms, 1.0e-5, "voice must fall silent after release");
        }

        beginTest ("hard note-off (no tail) silences immediately");
        {
            VoiceRig rig (sr, 4);
            rig.synth.noteOn (1, 64, 1.0f);
            renderSpan (rig.synth, 4, bs);
            rig.synth.noteOff (1, 64, 0.0f, false);   // allowTailOff = false
            const Stats after = renderSpan (rig.synth, 4, bs);
            expect (after.finite);
            expectLessThan (after.rms, 1.0e-5, "hard note-off should produce silence");
        }

        beginTest ("setCurrentPlaybackSampleRate at two rates both produce audio");
        {
            for (const double rate : { 44100.0, 96000.0 })
            {
                VoiceRig rig (rate, 4);
                rig.synth.noteOn (1, 57, 0.9f);
                const Stats s = renderSpan (rig.synth, 8, bs);
                expect (s.finite, "output finite at alternate sample rate");
                expectGreaterThan (s.rms, 1.0e-4, "audible at alternate sample rate");
            }
        }

        beginTest ("zero-sample render block is a finite no-op");
        {
            VoiceRig rig (sr, 4);
            rig.synth.noteOn (1, 60, 0.8f);
            juce::AudioBuffer<float> buffer (2, bs);
            juce::MidiBuffer empty;
            buffer.clear();
            rig.synth.renderNextBlock (buffer, empty, 0, 0);   // numSamples == 0
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < bs; ++i)
                    expectEquals (buffer.getSample (ch, i), 0.0f);
        }

        beginTest ("disabling every source yields silence even with a held note");
        {
            VoiceRig rig (sr, 4);
            rig.setBool (zw::id::osc ('A', "enable"), false);
            rig.setBool (zw::id::osc ('B', "enable"), false);
            rig.setBool (zw::id::subEnable,   false);
            rig.setBool (zw::id::noiseEnable, false);
            rig.synth.noteOn (1, 60, 1.0f);
            const Stats s = renderSpan (rig.synth, 8, bs);
            expect (s.finite);
            expectLessThan (s.rms, 1.0e-5, "no enabled sources means no sound");
        }

        beginTest ("noise-only source (osc/sub off, noise on) is audible");
        {
            VoiceRig rig (sr, 4);
            rig.setBool (zw::id::osc ('A', "enable"), false);
            rig.setBool (zw::id::osc ('B', "enable"), false);
            rig.setBool (zw::id::subEnable,   false);
            rig.setBool (zw::id::noiseEnable, true);
            // Noise level is a mod destination; push its base up so it is audible.
            zwtest::setParam (rig.proc.apvts, zw::id::noiseLevel, 1.0f);
            rig.synth.noteOn (1, 60, 1.0f);
            const Stats s = renderSpan (rig.synth, 8, bs);
            expect (s.finite, "noise output finite");
            expectGreaterThan (s.rms, 1.0e-4, "enabled noise should produce sound");
        }

        beginTest ("filter on/off both render finite audio");
        {
            VoiceRig on (sr, 4);
            on.setBool (zw::id::filterEnable, true);
            on.synth.noteOn (1, 48, 1.0f);
            const Stats sOn = renderSpan (on.synth, 8, bs);

            VoiceRig off (sr, 4);
            off.setBool (zw::id::filterEnable, false);
            off.synth.noteOn (1, 48, 1.0f);
            const Stats sOff = renderSpan (off.synth, 8, bs);

            expect (sOn.finite && sOff.finite, "both filter states finite");
            expectGreaterThan (sOn.rms, 1.0e-4, "audible with filter on");
            expectGreaterThan (sOff.rms, 1.0e-4, "audible with filter off");
        }

        beginTest ("higher cutoff passes at least as much energy as a low cutoff");
        {
            // Route nothing modulating the cutoff so the base param dominates.
            VoiceRig lo (sr, 4);
            lo.matrix.clear();
            zwtest::setParam (lo.proc.apvts, zw::id::filterCutoff, 0.02f);  // very low
            lo.synth.noteOn (1, 48, 1.0f);
            const Stats sLo = renderSpan (lo.synth, 10, bs);

            VoiceRig hi (sr, 4);
            hi.matrix.clear();
            zwtest::setParam (hi.proc.apvts, zw::id::filterCutoff, 1.0f);   // wide open
            hi.synth.noteOn (1, 48, 1.0f);
            const Stats sHi = renderSpan (hi.synth, 10, bs);

            expect (sLo.finite && sHi.finite, "both cutoffs finite");
            // A near-closed LP24 removes most harmonics; wide open keeps them.
            expectGreaterThan (sHi.peak, sLo.peak * 0.99, "open cutoff passes >= energy of near-closed cutoff");
        }

        beginTest ("every filter type renders finite audio");
        {
            auto* ftParam = rigParameter (zw::id::filterType);
            const int numTypes = (ftParam != nullptr) ? ftParam->getNumSteps() : 6;
            for (int t = 0; t < juce::jmax (1, numTypes); ++t)
            {
                VoiceRig rig (sr, 4);
                if (auto* p = rig.proc.apvts.getParameter (zw::id::filterType))
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) t));
                rig.synth.noteOn (1, 60, 1.0f);
                const Stats s = renderSpan (rig.synth, 6, bs);
                expect (s.finite, "filter type produces finite audio");
                expectGreaterThan (s.rms, 1.0e-5, "filter type produces some output");
            }
        }

        beginTest ("clearing the mod matrix keeps a held note audible");
        {
            // The default matrix routes Env1 -> OscA level; clearing removes that.
            // OscA level base default is non-zero, so the note must still sound.
            VoiceRig rig (sr, 4);
            rig.matrix.clear();
            expectEquals (rig.matrix.size(), 0);
            rig.synth.noteOn (1, 60, 1.0f);
            const Stats s = renderSpan (rig.synth, 8, bs);
            expect (s.finite, "cleared-matrix output finite");
            expectGreaterThan (s.rms, 1.0e-4, "note audible with empty matrix");
        }

        beginTest ("adding an LFO->cutoff route changes the rendered output");
        {
            VoiceRig baseRig (sr, 4);
            baseRig.matrix.clear();
            zwtest::setParam (baseRig.proc.apvts, zw::id::filterCutoff, 0.4f);
            baseRig.synth.noteOn (1, 50, 1.0f);
            const auto baseTail = lastBlock (baseRig.synth, 16, bs);

            VoiceRig modRig (sr, 4);
            modRig.matrix.clear();
            // Strong, fast LFO sweep on the cutoff so the tail block differs.
            modRig.matrix.addRoute (zw::ModSource::Lfo1, zw::ModDest::Cutoff, 1.0f);
            zwtest::setParam (modRig.proc.apvts, zw::id::filterCutoff, 0.4f);
            zwtest::setParam (modRig.proc.apvts, zw::id::lfo (1, "depth"), 1.0f);
            modRig.synth.noteOn (1, 50, 1.0f);
            const auto modTail = lastBlock (modRig.synth, 16, bs);

            expect (baseTail.size() == modTail.size(), "tail buffers comparable");
            double diff = 0.0;
            for (int i = 0; i < baseTail.size(); ++i)
                diff += std::abs ((double) baseTail[i] - (double) modTail[i]);
            expectGreaterThan (diff, 1.0e-3, "modulating cutoff should change the audio");
        }

        beginTest ("pitch wheel bend keeps the voice audible and finite");
        {
            // With pitch bend up, the sounding pitch shifts; verify rendering
            // stays sane and the voice remains audible across a bend.
            VoiceRig rig (sr, 4);
            rig.synth.noteOn (1, 60, 1.0f);
            renderSpan (rig.synth, 2, bs);
            rig.synth.handlePitchWheel (1, 16383);   // full bend up
            const Stats bent = renderSpan (rig.synth, 8, bs);
            expect (bent.finite, "bent output finite");
            expectGreaterThan (bent.rms, 1.0e-4, "voice still audible while bent");
        }

        beginTest ("glide (portamento) renders finite audio across a held note");
        {
            VoiceRig rig (sr, 4);
            zwtest::setParam (rig.proc.apvts, zw::id::glideTime, 0.5f);  // long glide
            // Seed a previous-note frequency far from the new note.
            rig.lastNoteFreq.store (110.0);
            rig.synth.noteOn (1, 72, 1.0f);
            const Stats s = renderSpan (rig.synth, 16, bs);
            expect (s.finite, "glide output finite");
            expectGreaterThan (s.rms, 1.0e-4, "audible during glide");
        }

        beginTest ("tempo-synced LFO uses bpm and renders finite audio");
        {
            VoiceRig rig (sr, 4);
            rig.bpm.store (140.0);
            // lfo sync defaults to on; route LFO1 -> cutoff with depth.
            rig.matrix.clear();
            rig.matrix.addRoute (zw::ModSource::Lfo1, zw::ModDest::Cutoff, 0.8f);
            zwtest::setParam (rig.proc.apvts, zw::id::lfo (1, "depth"), 1.0f);
            rig.synth.noteOn (1, 60, 1.0f);
            const Stats s = renderSpan (rig.synth, 12, bs);
            expect (s.finite, "synced-LFO output finite");
            expectGreaterThan (s.rms, 1.0e-4, "audible with tempo-synced LFO");
        }

        beginTest ("routing sub/noise around the filter (dry path) stays finite and audible");
        {
            VoiceRig rig (sr, 4);
            rig.setBool (zw::id::filterRouteS, true);    // sub bypasses filter
            rig.setBool (zw::id::filterRouteN, true);    // noise bypasses filter
            rig.setBool (zw::id::noiseEnable, true);
            zwtest::setParam (rig.proc.apvts, zw::id::noiseLevel, 0.8f);
            rig.synth.noteOn (1, 36, 1.0f);
            const Stats s = renderSpan (rig.synth, 8, bs);
            expect (s.finite, "dry-routed output finite");
            expectGreaterThan (s.rms, 1.0e-4, "audible with sub/noise routed dry");
        }

        beginTest ("polyphony: a held chord is louder than a single note");
        {
            VoiceRig single (sr, 8);
            single.matrix.clear();
            single.synth.noteOn (1, 60, 1.0f);
            const Stats one = renderSpan (single.synth, 8, bs);

            VoiceRig chord (sr, 8);
            chord.matrix.clear();
            chord.synth.noteOn (1, 60, 1.0f);
            chord.synth.noteOn (1, 64, 1.0f);
            chord.synth.noteOn (1, 67, 1.0f);
            const Stats three = renderSpan (chord.synth, 8, bs);

            expect (one.finite && three.finite, "mono and chord both finite");
            expectGreaterThan (one.rms, 1.0e-4, "single note audible");
            expectGreaterThan (three.rms, one.rms * 1.05, "chord should be louder than one note");
        }
    }

    // Render `blocks` blocks and return a copy of the final block's L channel.
    static juce::Array<float> lastBlock (juce::Synthesiser& synth, int blocks, int bs)
    {
        juce::AudioBuffer<float> buffer (2, bs);
        juce::MidiBuffer empty;
        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            synth.renderNextBlock (buffer, empty, 0, bs);
        }
        juce::Array<float> out;
        out.ensureStorageAllocated (bs);
        const auto* l = buffer.getReadPointer (0);
        for (int i = 0; i < bs; ++i)
            out.add (l[i]);
        return out;
    }

    // Build a throwaway rig only to fetch a parameter pointer for metadata.
    juce::RangedAudioParameter* rigParameter (const juce::String& paramId)
    {
        static VoiceRig probe;
        return probe.proc.apvts.getParameter (paramId);
    }
};

static VoiceIntegrationTests voiceIntegrationTests;
}
