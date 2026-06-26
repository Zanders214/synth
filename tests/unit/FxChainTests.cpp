// Unit tests for zw::FxChain (the post-voice-mix 10-slot serial effects rack)
// and, transitively, the individual effects in src/dsp/fx/Effects.h.
//
// Strategy: build a real APVTS via DummyProcessor, point FxChain at it with
// prepareParams(), prepare() against a ProcessSpec, then push a stereo test
// signal through an AudioBlock and assert on invariants:
//   * with every slot disabled the chain is a transparent passthrough,
//   * with a single slot enabled (and parameters that guarantee work) the
//     output stays finite but is measurably different from the dry signal,
//   * reset() and sample-rate / block-size changes keep the chain finite.
#include "dsp/fx/FxChain.h"
#include "TestHelpers.h"

#include <cmath>
#include <functional>
#include <vector>

namespace
{
using namespace zwtest;

//==============================================================================
// Fill a stereo buffer with a deterministic, broadband-ish test signal: a mix
// of a low and a high sine plus a couple of impulses, slightly different per
// channel so stereo effects have something to act on.
void fillTestSignal (juce::AudioBuffer<float>& buf, double sr, float amp = 0.6f)
{
    const int ns = buf.getNumSamples();
    for (int c = 0; c < buf.getNumChannels(); ++c)
    {
        auto* x = buf.getWritePointer (c);
        const float fLow  = 220.0f + 30.0f * (float) c;
        const float fHigh = 4000.0f + 500.0f * (float) c;
        for (int i = 0; i < ns; ++i)
        {
            const float t = (float) i / (float) sr;
            x[i] = amp * (0.6f * std::sin (juce::MathConstants<float>::twoPi * fLow  * t)
                        + 0.4f * std::sin (juce::MathConstants<float>::twoPi * fHigh * t));
        }
        x[0]      += amp;          // leading impulse to wake delay/reverb tails
        x[ns / 2] -= amp * 0.5f;
    }
}

// True if every sample in the buffer is finite.
bool allFinite (const juce::AudioBuffer<float>& buf)
{
    for (int c = 0; c < buf.getNumChannels(); ++c)
    {
        const auto* x = buf.getReadPointer (c);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            if (! std::isfinite (x[i]))
                return false;
    }
    return true;
}

// True if no sample exceeds the given absolute bound (sane-range guard).
bool allBelow (const juce::AudioBuffer<float>& buf, float bound)
{
    for (int c = 0; c < buf.getNumChannels(); ++c)
    {
        const auto* x = buf.getReadPointer (c);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            if (std::abs (x[i]) > bound)
                return false;
    }
    return true;
}

// Sum of absolute per-sample differences between two equally-shaped buffers.
double diff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double d = 0.0;
    for (int c = 0; c < a.getNumChannels(); ++c)
    {
        const auto* pa = a.getReadPointer (c);
        const auto* pb = b.getReadPointer (c);
        for (int i = 0; i < a.getNumSamples(); ++i)
            d += std::abs ((double) pa[i] - (double) pb[i]);
    }
    return d;
}

// Run the test signal through the chain in place; returns the processed buffer.
juce::AudioBuffer<float> runChain (zw::FxChain& chain, double sr, int ns)
{
    juce::AudioBuffer<float> buf (2, ns);
    fillTestSignal (buf, sr);
    juce::dsp::AudioBlock<float> block (buf);
    chain.process (block);
    return buf;
}

struct FxChainTests : juce::UnitTest
{
    FxChainTests() : juce::UnitTest ("FxChain", "DSP") {}

    // Enable exactly one slot, setting all the named slot params (besides enable)
    // to a normalised value; everything else stays at default (disabled).
    void enableOnly (juce::AudioProcessorValueTreeState& s, const char* slot)
    {
        setParam (s, zw::id::fx (slot, "enable"), 1.0f);
    }

    // FX slot 'enable' params default to ON, so transparency tests must force
    // every slot off explicitly before enabling only what they want.
    void disableAll (juce::AudioProcessorValueTreeState& s)
    {
        for (auto* slot : { "hyper","distort","flanger","phaser","chorus",
                            "delay","reverb","comp","eq","filter" })
            setParam (s, zw::id::fx (slot, "enable"), 0.0f);
    }

    void runTest() override
    {
        const double sr = 48000.0;
        const int    bs = 512;

        // ---------------------------------------------------------------------
        beginTest ("all slots disabled => transparent passthrough");
        {
            DummyProcessor proc;
            disableAll (proc.apvts);
            zw::FxChain chain;
            chain.prepareParams (proc.apvts);
            juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, (juce::uint32) 2 };
            chain.prepare (spec);

            juce::AudioBuffer<float> dry (2, bs);
            fillTestSignal (dry, sr);
            juce::AudioBuffer<float> wet;
            wet.makeCopyOf (dry);

            juce::dsp::AudioBlock<float> block (wet);
            chain.process (block);   // every slot off -> nothing should run

            expect (allFinite (wet), "passthrough must be finite");
            expectWithinAbsoluteError (diff (dry, wet), 0.0, 1.0e-6,
                                       "disabled chain must not alter the signal");
        }

        // ---------------------------------------------------------------------
        beginTest ("prepare + process with all 10 slots enabled stays finite");
        {
            DummyProcessor proc;
            auto& s = proc.apvts;
            // Turn every slot on at its default parameters and push the rack.
            for (auto* slot : { "hyper","distort","flanger","phaser","chorus",
                                "delay","reverb","comp","eq","filter" })
                setParam (s, zw::id::fx (slot, "enable"), 1.0f);
            // Give the EQ some gain so its branch is non-trivial.
            setParam (s, zw::id::fx ("eq", "low"),  1.0f);
            setParam (s, zw::id::fx ("eq", "high"), 0.0f);

            zw::FxChain chain;
            chain.prepareParams (s);
            juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, (juce::uint32) 2 };
            chain.prepare (spec);

            // Run several blocks so modulated / feedback effects build up state.
            juce::AudioBuffer<float> buf (2, bs);
            for (int b = 0; b < 8; ++b)
            {
                fillTestSignal (buf, sr);
                juce::dsp::AudioBlock<float> block (buf);
                chain.process (block);
                expect (allFinite (buf), "full chain output must be finite");
                expect (allBelow (buf, 16.0f), "full chain output must stay bounded");
            }
        }

        // ---------------------------------------------------------------------
        beginTest ("each slot: disabled is transparent, enabled changes signal");
        {
            // For every slot, configure parameters that guarantee the effect
            // does audible work, then compare enabled output against the dry
            // signal and against the disabled output.
            struct Case { const char* slot; std::function<void (juce::AudioProcessorValueTreeState&)> cfg; };

            std::vector<Case> cases;
            cases.push_back ({ "hyper", [] (juce::AudioProcessorValueTreeState& s)
            {
                setParam (s, zw::id::fx ("hyper", "detune"), 0.8f);
                setParam (s, zw::id::fx ("hyper", "voices"), 1.0f);   // 8 voices
                setParam (s, zw::id::fx ("hyper", "width"),  0.9f);
                setParam (s, zw::id::fx ("hyper", "mix"),    1.0f);
            }});
            cases.push_back ({ "flanger", [] (juce::AudioProcessorValueTreeState& s)
            {
                setParam (s, zw::id::fx ("flanger", "rate"),     0.3f);
                setParam (s, zw::id::fx ("flanger", "depth"),    0.8f);
                setParam (s, zw::id::fx ("flanger", "feedback"), 0.5f);
                setParam (s, zw::id::fx ("flanger", "mix"),      1.0f);
            }});
            cases.push_back ({ "phaser", [] (juce::AudioProcessorValueTreeState& s)
            {
                setParam (s, zw::id::fx ("phaser", "rate"),  0.3f);
                setParam (s, zw::id::fx ("phaser", "depth"), 0.9f);
                setParam (s, zw::id::fx ("phaser", "mix"),   1.0f);
            }});
            cases.push_back ({ "chorus", [] (juce::AudioProcessorValueTreeState& s)
            {
                setParam (s, zw::id::fx ("chorus", "rate"),  0.4f);
                setParam (s, zw::id::fx ("chorus", "depth"), 0.9f);
                setParam (s, zw::id::fx ("chorus", "mix"),   1.0f);
            }});
            cases.push_back ({ "delay", [] (juce::AudioProcessorValueTreeState& s)
            {
                setParam (s, zw::id::fx ("delay", "time"),     0.3f);
                setParam (s, zw::id::fx ("delay", "feedback"), 0.5f);
                setParam (s, zw::id::fx ("delay", "width"),    0.7f);
                setParam (s, zw::id::fx ("delay", "mix"),      1.0f);
            }});
            cases.push_back ({ "reverb", [] (juce::AudioProcessorValueTreeState& s)
            {
                setParam (s, zw::id::fx ("reverb", "size"),  0.8f);
                setParam (s, zw::id::fx ("reverb", "decay"), 0.7f);
                setParam (s, zw::id::fx ("reverb", "damp"),  0.3f);
                setParam (s, zw::id::fx ("reverb", "mix"),   1.0f);
            }});
            cases.push_back ({ "comp", [] (juce::AudioProcessorValueTreeState& s)
            {
                setParam (s, zw::id::fx ("comp", "threshold"), 0.1f);  // very low
                setParam (s, zw::id::fx ("comp", "ratio"),     1.0f);  // 20:1
                setParam (s, zw::id::fx ("comp", "attack"),    0.0f);  // fast
                setParam (s, zw::id::fx ("comp", "makeup"),    0.6f);  // boost
            }});
            cases.push_back ({ "eq", [] (juce::AudioProcessorValueTreeState& s)
            {
                setParam (s, zw::id::fx ("eq", "low"),   1.0f);  // +15 dB
                setParam (s, zw::id::fx ("eq", "lomid"), 0.0f);  // -15 dB
                setParam (s, zw::id::fx ("eq", "himid"), 1.0f);  // +15 dB
                setParam (s, zw::id::fx ("eq", "high"),  0.0f);  // -15 dB
            }});
            cases.push_back ({ "filter", [] (juce::AudioProcessorValueTreeState& s)
            {
                setParam (s, zw::id::fx ("filter", "cutoff"), 0.25f); // low LP cutoff
                setParam (s, zw::id::fx ("filter", "reso"),   0.4f);
                setParam (s, zw::id::fx ("filter", "type"),   0.0f);  // LP
                setParam (s, zw::id::fx ("filter", "mix"),    1.0f);
            }});

            for (const auto& cse : cases)
            {
                // Disabled reference: build the chain, configure params but leave
                // the slot off, and confirm it is a no-op.
                DummyProcessor off;
                disableAll (off.apvts);
                cse.cfg (off.apvts);                // params set, every slot off
                zw::FxChain chainOff;
                chainOff.prepareParams (off.apvts);
                juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, (juce::uint32) 2 };
                chainOff.prepare (spec);

                juce::AudioBuffer<float> dry (2, bs);
                fillTestSignal (dry, sr);
                juce::AudioBuffer<float> bufOff;
                bufOff.makeCopyOf (dry);
                juce::dsp::AudioBlock<float> blockOff (bufOff);
                chainOff.process (blockOff);
                expectWithinAbsoluteError (diff (dry, bufOff), 0.0, 1.0e-6,
                    juce::String (cse.slot) + " disabled must be transparent");

                // Enabled: same params + enable, then process.
                DummyProcessor on;
                disableAll (on.apvts);
                cse.cfg (on.apvts);
                enableOnly (on.apvts, cse.slot);
                zw::FxChain chainOn;
                chainOn.prepareParams (on.apvts);
                chainOn.prepare (spec);

                juce::AudioBuffer<float> bufOn;
                bufOn.makeCopyOf (dry);
                // A couple of passes so tails / modulation accrue.
                for (int b = 0; b < 3; ++b)
                {
                    juce::dsp::AudioBlock<float> blockOn (bufOn);
                    chainOn.process (blockOn);
                }
                expect (allFinite (bufOn), juce::String (cse.slot) + " output must be finite");
                expectGreaterThan (diff (dry, bufOn), 1.0e-3,
                    juce::String (cse.slot) + " enabled must change the signal");
            }
        }

        // ---------------------------------------------------------------------
        // Phaser "stages" (Int 2-12) and Chorus "voices" (Int 1-4) used to be
        // cached but never applied (the old juce::dsp wrappers had nowhere to
        // put the count), so their sliders were dead. These cases pin that the
        // count now reaches the DSP: the same dry signal through the same slot
        // at two different counts must produce measurably different output, and
        // the default count must still alter the signal.
        beginTest ("phaser stages / chorus voices are wired and change the output");
        {
            juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, (juce::uint32) 2 };

            // Build the slot at a given count param and return its output after a
            // few passes (so modulation / feedback state accrues), starting from
            // the same dry signal each time.
            auto render = [&] (const char* slot, const char* countId, float countNorm)
            {
                DummyProcessor proc;
                auto& s = proc.apvts;
                disableAll (s);
                setParam (s, zw::id::fx (slot, "enable"), 1.0f);
                setParam (s, zw::id::fx (slot, "rate"),   0.4f);
                setParam (s, zw::id::fx (slot, "depth"),  0.9f);
                setParam (s, zw::id::fx (slot, "mix"),    1.0f);
                setParam (s, zw::id::fx (slot, countId),  countNorm);

                zw::FxChain chain;
                chain.prepareParams (s);
                chain.prepare (spec);

                juce::AudioBuffer<float> buf (2, bs);
                fillTestSignal (buf, sr);
                for (int b = 0; b < 3; ++b)
                {
                    juce::dsp::AudioBlock<float> block (buf);
                    chain.process (block);
                }
                return buf;
            };

            juce::AudioBuffer<float> dry (2, bs);
            fillTestSignal (dry, sr);

            // Phaser: stages norm 0.0 -> 2 stages, 1.0 -> 12 stages (default 6).
            auto phLo  = render ("phaser", "stages", 0.0f);   // 2 stages
            auto phHi  = render ("phaser", "stages", 1.0f);   // 12 stages
            auto phDef = render ("phaser", "stages", 0.4f);   // 6 stages (default)
            expect (allFinite (phLo) && allFinite (phHi) && allFinite (phDef),
                    "phaser output must be finite at every stage count");
            expectGreaterThan (diff (phLo, phHi), 1.0e-3,
                    "changing phaser stages must change the output");
            expectGreaterThan (diff (dry, phDef), 1.0e-3,
                    "phaser at default stages must still alter the signal");

            // Chorus: voices norm 0.0 -> 1 voice, 1.0 -> 4 voices (default 2).
            auto chLo  = render ("chorus", "voices", 0.0f);   // 1 voice
            auto chHi  = render ("chorus", "voices", 1.0f);   // 4 voices
            auto chDef = render ("chorus", "voices", 1.0f / 3.0f);  // 2 voices (default)
            expect (allFinite (chLo) && allFinite (chHi) && allFinite (chDef),
                    "chorus output must be finite at every voice count");
            expectGreaterThan (diff (chLo, chHi), 1.0e-3,
                    "changing chorus voices must change the output");
            expectGreaterThan (diff (dry, chDef), 1.0e-3,
                    "chorus at default voices must still alter the signal");
        }

        // ---------------------------------------------------------------------
        beginTest ("distortion exercises all three shaping modes");
        {
            juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, (juce::uint32) 2 };
            const float modeNorm[3] = { 0.0f, 0.5f, 1.0f };   // Tube / Diode / Fold
            for (int m = 0; m < 3; ++m)
            {
                DummyProcessor proc;
                auto& s = proc.apvts;
                disableAll (s);
                setParam (s, zw::id::fx ("distort", "enable"), 1.0f);
                setParam (s, zw::id::fx ("distort", "drive"),  0.9f);
                setParam (s, zw::id::fx ("distort", "tone"),   0.8f);   // != default 0.5 -> recompute shelf
                setParam (s, zw::id::fx ("distort", "mix"),    1.0f);
                setParam (s, zw::id::fx ("distort", "out"),    0.8f);
                setParam (s, zw::id::fx ("distort", "mode"),   modeNorm[m]);

                zw::FxChain chain;
                chain.prepareParams (s);
                chain.prepare (spec);

                juce::AudioBuffer<float> dry (2, bs);
                fillTestSignal (dry, sr);
                juce::AudioBuffer<float> wet;
                wet.makeCopyOf (dry);
                juce::dsp::AudioBlock<float> block (wet);
                chain.process (block);

                expect (allFinite (wet), "distort mode output must be finite");
                // Solo aggressive distortion (drive 0.9, fold) can peak hard on the
                // impulse transient; this is a runaway/NaN guard, not a level check.
                expect (allBelow (wet, 64.0f), "distort output must not run away");
                expectGreaterThan (diff (dry, wet), 1.0e-3,
                    "distort mode " + juce::String (m) + " must change the signal");
            }
        }

        // ---------------------------------------------------------------------
        beginTest ("FX filter exercises LP / HP / BP types");
        {
            juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, (juce::uint32) 2 };
            const float typeNorm[3] = { 0.0f, 0.5f, 1.0f };   // LP / HP / BP
            for (int t = 0; t < 3; ++t)
            {
                DummyProcessor proc;
                auto& s = proc.apvts;
                disableAll (s);
                setParam (s, zw::id::fx ("filter", "enable"), 1.0f);
                setParam (s, zw::id::fx ("filter", "cutoff"), 0.5f);
                setParam (s, zw::id::fx ("filter", "reso"),   0.5f);
                setParam (s, zw::id::fx ("filter", "type"),   typeNorm[t]);
                setParam (s, zw::id::fx ("filter", "mix"),    1.0f);

                zw::FxChain chain;
                chain.prepareParams (s);
                chain.prepare (spec);

                juce::AudioBuffer<float> dry (2, bs);
                fillTestSignal (dry, sr);
                juce::AudioBuffer<float> wet;
                wet.makeCopyOf (dry);
                juce::dsp::AudioBlock<float> block (wet);
                chain.process (block);

                expect (allFinite (wet), "filter type output must be finite");
                expectGreaterThan (diff (dry, wet), 1.0e-3,
                    "filter type " + juce::String (t) + " must change the signal");
            }
        }

        // ---------------------------------------------------------------------
        beginTest ("reset clears internal state (tails do not persist)");
        {
            DummyProcessor proc;
            auto& s = proc.apvts;
            disableAll (s);
            // Delay + reverb both keep tails we can flush with reset().
            setParam (s, zw::id::fx ("delay", "enable"),   1.0f);
            setParam (s, zw::id::fx ("delay", "time"),     0.4f);
            setParam (s, zw::id::fx ("delay", "feedback"), 0.6f);
            setParam (s, zw::id::fx ("delay", "mix"),      1.0f);
            setParam (s, zw::id::fx ("reverb", "enable"),  1.0f);
            setParam (s, zw::id::fx ("reverb", "mix"),     1.0f);

            zw::FxChain chain;
            chain.prepareParams (s);
            juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, (juce::uint32) 2 };
            chain.prepare (spec);

            // Pump signal to build up delay/reverb tails.
            (void) runChain (chain, sr, bs);
            (void) runChain (chain, sr, bs);

            chain.reset();

            // After reset, feeding silence should yield (near) silence: no tail.
            juce::AudioBuffer<float> silence (2, bs);
            silence.clear();
            juce::dsp::AudioBlock<float> block (silence);
            chain.process (block);

            expect (allFinite (silence), "post-reset output must be finite");
            double energy = 0.0;
            for (int c = 0; c < silence.getNumChannels(); ++c)
            {
                const auto* x = silence.getReadPointer (c);
                for (int i = 0; i < silence.getNumSamples(); ++i)
                    energy += std::abs ((double) x[i]);
            }
            expectLessThan (energy, 1.0e-3, "reset should flush delay/reverb tails");
        }

        // ---------------------------------------------------------------------
        beginTest ("re-prepare at new sample rate / block size stays finite");
        {
            DummyProcessor proc;
            auto& s = proc.apvts;
            for (auto* slot : { "hyper","distort","flanger","delay","filter" })
                setParam (s, zw::id::fx (slot, "enable"), 1.0f);

            zw::FxChain chain;
            chain.prepareParams (s);

            // First spec.
            juce::dsp::ProcessSpec specA { 44100.0, (juce::uint32) 256, (juce::uint32) 2 };
            chain.prepare (specA);
            (void) runChain (chain, 44100.0, 256);

            // Re-prepare with a different rate and a larger block.
            juce::dsp::ProcessSpec specB { 96000.0, (juce::uint32) 1024, (juce::uint32) 2 };
            chain.prepare (specB);
            auto out = runChain (chain, 96000.0, 1024);
            expect (allFinite (out), "output after re-prepare must be finite");
            expect (allBelow (out, 16.0f), "output after re-prepare must stay bounded");
        }

        // ---------------------------------------------------------------------
        beginTest ("zero-length block is a no-op and does not crash");
        {
            DummyProcessor proc;
            auto& s = proc.apvts;
            for (auto* slot : { "hyper","distort","delay","reverb","filter" })
                setParam (s, zw::id::fx (slot, "enable"), 1.0f);

            zw::FxChain chain;
            chain.prepareParams (s);
            juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, (juce::uint32) 2 };
            chain.prepare (spec);

            juce::AudioBuffer<float> buf (2, bs);
            buf.clear();
            // A subBlock of length 0 still has channels but no samples.
            juce::dsp::AudioBlock<float> full (buf);
            auto empty = full.getSubBlock (0, 0);
            chain.process (empty);
            expect (empty.getNumSamples() == 0, "empty block stays empty");
            expect (allFinite (buf), "zero-length processing leaves buffer finite");
        }
    }
};

static FxChainTests fxChainTests;
}
