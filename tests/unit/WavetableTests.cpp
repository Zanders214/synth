// Unit tests for zw::Wavetable (band-limited mipped wavetable) and
// zw::WavetableOscillator (unison/detune/pan/width additive renderer).
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/Wavetable.h"
#include "dsp/WavetableOscillator.h"

#include <cmath>

namespace
{
// True if v is finite and within [-lim, lim].
static bool inRange (float v, float lim) noexcept
{
    return std::isfinite (v) && v >= -lim && v <= lim;
}

struct WavetableTests : juce::UnitTest
{
    WavetableTests() : juce::UnitTest ("Wavetable", "DSP") {}

    void runTest() override
    {
        const double sr = 48000.0;

        beginTest ("default wavetable is empty before generation");
        {
            zw::Wavetable wt;
            expect (wt.isEmpty(), "fresh table should report empty");
            expectEquals (wt.getNumFrames(), 0);

            // makeCursor on an empty table yields a null cursor that reads 0.
            auto c = wt.makeCursor (0.5f, 440.0, sr);
            expectEquals (c.read (0.25f), 0.0f);
            // getSample routes through makeCursor; also 0 on empty table.
            expectEquals (wt.getSample (0.5f, 0.5f, 440.0, sr), 0.0f);
        }

        beginTest ("generateBasicShapes sets requested frame count");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes (64);
            expect (! wt.isEmpty());
            expectEquals (wt.getNumFrames(), 64);
        }

        beginTest ("generateBasicShapes clamps frame count to a minimum of 2");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes (1);   // jmax (2, frames) => 2
            expectEquals (wt.getNumFrames(), 2);

            zw::Wavetable wt0;
            wt0.generateBasicShapes (0);
            expectEquals (wt0.getNumFrames(), 2);

            zw::Wavetable wtNeg;
            wtNeg.generateBasicShapes (-5);
            expectEquals (wtNeg.getNumFrames(), 2);
        }

        beginTest ("default-argument generateBasicShapes produces 64 frames");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes();   // default numFrames = 64
            expectEquals (wt.getNumFrames(), 64);
        }

        beginTest ("frame 0 (sine) samples are finite and within unit range");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes (16);

            // Low fundamental keeps full-band mip 0; frame 0 is a normalised sine.
            float peak = 0.0f;
            for (int i = 0; i < 2048; ++i)
            {
                const float ph = (float) i / 2048.0f;
                const float s  = wt.getSample (0.0f, ph, 55.0, sr);
                expect (inRange (s, 1.05f), "sine sample out of expected range");
                peak = juce::jmax (peak, std::abs (s));
            }
            // Per-frame peak normalisation => sine frame peaks at ~1.0.
            expectWithinAbsoluteError (peak, 1.0f, 0.05f);
        }

        beginTest ("interpolation across the wrap boundary stays finite/in-range");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes (32);

            // Phases >= 1 and < 0 must wrap (read() uses phase - floor(phase)).
            const float testPhases[] = { -0.75f, -0.25f, 0.0f, 0.999f, 1.0f, 1.5f, 2.25f };
            for (float ph : testPhases)
            {
                const float s = wt.getSample (0.5f, ph, 110.0, sr);
                expect (inRange (s, 2.0f), "wrapped-phase sample not finite/in-range");
            }

            // framePos01 is clamped to [0,1]; out-of-range values must not crash
            // and must still read a valid frame.
            expect (inRange (wt.getSample (-1.0f, 0.3f, 110.0, sr), 2.0f));
            expect (inRange (wt.getSample ( 2.0f, 0.3f, 110.0, sr), 2.0f));
        }

        beginTest ("mip selection is monotonic in frequency (higher freq -> fewer harmonics)");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes (8);

            // makeCursor picks the mip via mipForFreq; we observe the chosen mip
            // indirectly through r0's address: higher mips live in different
            // backing vectors, but within one mip r0 advances by frame. To make
            // the comparison clean we hold framePos at 0 so r0 == mip base.
            // Instead of inspecting addresses, assert behavioural monotonicity:
            // the number of distinct mips actually used is non-decreasing with freq.
            const double freqs[] = { 20.0, 110.0, 440.0, 1760.0, 7040.0, 18000.0, 30000.0 };

            // Round-trip a representative sample at each freq: must be finite.
            for (double f : freqs)
            {
                auto c = wt.makeCursor (0.0f, f, sr);
                expect (c.r0 != nullptr, "cursor base must be valid after generation");
                expect (std::isfinite (c.read (0.123f)), "cursor read must be finite");
            }

            // Very high notes should not read MORE high-frequency content than low
            // notes: compare peak of the highest harmonics by sampling densely.
            // We use a proxy: a very high freq selects the last (smoothest) mip,
            // whose frame 1 (saw end) has smaller sample-to-sample deltas than the
            // full-band mip. Measure max |delta| over a cycle for low vs high freq.
            auto maxDelta = [&] (double freq)
            {
                auto c = wt.makeCursor (1.0f, freq, sr);  // saw-end frame
                float prev = c.read (0.0f);
                float md   = 0.0f;
                for (int i = 1; i <= 2048; ++i)
                {
                    const float cur = c.read ((float) i / 2048.0f);
                    md   = juce::jmax (md, std::abs (cur - prev));
                    prev = cur;
                }
                return md;
            };

            const float deltaLow  = maxDelta (40.0);     // full band
            const float deltaHigh = maxDelta (30000.0);  // fewest harmonics
            expect (std::isfinite (deltaLow) && std::isfinite (deltaHigh));
            // Fewer harmonics => smoother => smaller maximum slope.
            expectLessThan (deltaHigh, deltaLow);
        }

        beginTest ("getSample matches makeCursor + read (consistency)");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes (24);

            const float  fp  = 0.37f;
            const double frq = 220.0;
            const float  ph  = 0.61f;

            auto  c    = wt.makeCursor (fp, frq, sr);
            const float viaCursor = c.read (ph);
            const float viaSample = wt.getSample (fp, ph, frq, sr);
            expectWithinAbsoluteError (viaSample, viaCursor, 1.0e-6f);
        }

        beginTest ("sample-rate changes still yield valid cursors");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes (16);
            const double rates[] = { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 };
            for (double rate : rates)
            {
                auto c = wt.makeCursor (0.5f, 440.0, rate);
                expect (c.r0 != nullptr);
                expect (std::isfinite (c.read (0.5f)));
            }
        }

        beginTest ("Cursor default-constructed reads zero");
        {
            zw::Wavetable::Cursor c;   // r0 == nullptr
            expectEquals (c.read (0.0f), 0.0f);
            expectEquals (c.read (0.9f), 0.0f);
        }

        //======================================================================
        // WavetableOscillator
        //======================================================================
        beginTest ("oscillator with no wavetable renders silence");
        {
            zw::WavetableOscillator osc;
            osc.prepare (sr);
            // table is null -> renderAdd is a no-op.
            float l = 1.0f, r = -1.0f;   // pre-seed; renderAdd must add nothing
            zw::WavetableOscillator::UpdateParams p {};
            p.framePos01  = 0.5f;
            p.warp01      = 0.0f;
            p.unison      = 1;
            p.detuneCents = 0.0f;
            p.level01     = 1.0f;
            p.pan         = 0.0f;
            p.width01     = 0.0f;
            p.baseFreqHz  = 440.0;
            osc.update (p);
            osc.renderAdd (l, r);
            expectEquals (l, 1.0f);
            expectEquals (r, -1.0f);
        }

        beginTest ("mono oscillator renders finite, non-trivial output");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes (32);

            zw::WavetableOscillator osc;
            osc.prepare (sr);
            osc.setWavetable (&wt);
            osc.noteOn (false, 0.0f);

            zw::WavetableOscillator::UpdateParams p {};
            p.framePos01  = 1.0f;     // saw end -> rich content
            p.warp01      = 0.0f;
            p.unison      = 1;
            p.detuneCents = 0.0f;
            p.level01     = 1.0f;
            p.pan         = 0.0f;     // centred -> L == R
            p.width01     = 0.0f;
            p.baseFreqHz  = 220.0;
            osc.update (p);

            float peak = 0.0f;
            bool  channelsEqual = true;
            for (int i = 0; i < 2048; ++i)
            {
                float l = 0.0f, r = 0.0f;
                osc.renderAdd (l, r);
                expect (std::isfinite (l) && std::isfinite (r), "osc output not finite");
                if (std::abs (l - r) > 1.0e-4f) channelsEqual = false;
                peak = juce::jmax (peak, std::abs (l));
            }
            expect (peak > 0.0f, "oscillator should produce non-zero output");
            expect (channelsEqual, "centred pan should give equal L/R");
        }

        beginTest ("zero level produces silence (gain<=0 early-out)");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes (16);

            zw::WavetableOscillator osc;
            osc.prepare (sr);
            osc.setWavetable (&wt);
            osc.noteOn (false, 0.0f);

            zw::WavetableOscillator::UpdateParams p {};
            p.framePos01  = 0.5f;
            p.unison      = 1;
            p.level01     = 0.0f;     // gain == 0 -> renderAdd early-outs
            p.baseFreqHz  = 440.0;
            osc.update (p);

            float l = 0.0f, r = 0.0f;
            for (int i = 0; i < 64; ++i) osc.renderAdd (l, r);
            expectEquals (l, 0.0f);
            expectEquals (r, 0.0f);
        }

        beginTest ("unison count is clamped to [1, kMaxUnison] and stays finite");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes (16);

            zw::WavetableOscillator osc;
            osc.prepare (sr);
            osc.setWavetable (&wt);
            osc.noteOn (true, 0.0f);   // randomise phases path

            const int unisonRequests[] = { -3, 0, 1, 7, 16, 64 };
            for (int u : unisonRequests)
            {
                zw::WavetableOscillator::UpdateParams p {};
                p.framePos01  = 0.5f;
                p.warp01      = 0.2f;
                p.unison      = u;
                p.detuneCents = 25.0f;   // exercise detune spread
                p.level01     = 0.8f;
                p.pan         = 0.0f;
                p.width01     = 1.0f;    // full stereo width spread
                p.baseFreqHz  = 330.0;
                osc.update (p);

                // renderAdd() is additive into the caller's mix accumulators, so
                // reset them each sample and check the per-sample contribution.
                for (int i = 0; i < 256; ++i)
                {
                    float l = 0.0f, r = 0.0f;
                    osc.renderAdd (l, r);
                    expect (std::isfinite (l) && std::isfinite (r),
                            "unison render produced non-finite sample");
                    // Equal-power gain (level/sqrt(count)) keeps each sample bounded.
                    expect (inRange (l, 8.0f) && inRange (r, 8.0f),
                            "unison output magnitude unexpectedly large");
                }
            }
        }

        beginTest ("pan extremes steer energy to one channel");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes (16);

            zw::WavetableOscillator osc;
            osc.prepare (sr);
            osc.setWavetable (&wt);

            auto channelEnergy = [&] (float pan, float& eL, float& eR)
            {
                osc.noteOn (false, 0.0f);
                zw::WavetableOscillator::UpdateParams p {};
                p.framePos01  = 1.0f;
                p.warp01      = 0.0f;
                p.unison      = 1;
                p.detuneCents = 0.0f;
                p.level01     = 1.0f;
                p.pan         = pan;
                p.width01     = 0.0f;
                p.baseFreqHz  = 220.0;
                osc.update (p);

                eL = 0.0f; eR = 0.0f;
                for (int i = 0; i < 1024; ++i)
                {
                    float l = 0.0f, r = 0.0f;
                    osc.renderAdd (l, r);
                    eL += std::abs (l);
                    eR += std::abs (r);
                }
            };

            float leftL = 0.0f, leftR = 0.0f, rightL = 0.0f, rightR = 0.0f;
            channelEnergy (-1.0f, leftL, leftR);   // hard left -> gainR == sin(0) == 0
            channelEnergy ( 1.0f, rightL, rightR); // hard right -> gainL == cos(halfPi) == 0

            expectGreaterThan (leftL, leftR);   // hard left favours L
            expectGreaterThan (rightR, rightL); // hard right favours R
            expectWithinAbsoluteError (leftR, 0.0f, 1.0e-3f);
            expectWithinAbsoluteError (rightL, 0.0f, 1.0e-3f);
        }

        beginTest ("warp phase-bend keeps output finite and bounded");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes (16);

            zw::WavetableOscillator osc;
            osc.prepare (sr);
            osc.setWavetable (&wt);

            const float warps[] = { 0.0f, 0.0005f, 0.25f, 0.5f, 1.0f };
            for (float w : warps)
            {
                osc.noteOn (false, 0.0f);
                zw::WavetableOscillator::UpdateParams p {};
                p.framePos01  = 0.5f;
                p.warp01      = w;
                p.unison      = 2;
                p.detuneCents = 10.0f;
                p.level01     = 0.9f;
                p.pan         = 0.0f;
                p.width01     = 0.5f;
                p.baseFreqHz  = 440.0;
                osc.update (p);

                for (int i = 0; i < 512; ++i)
                {
                    float l = 0.0f, r = 0.0f;
                    osc.renderAdd (l, r);
                    expect (inRange (l, 8.0f) && inRange (r, 8.0f),
                            "warped output not finite/in-range");
                }
            }
        }

        beginTest ("sampleRate change via prepare alters phase increment / pitch");
        {
            zw::Wavetable wt;
            wt.generateBasicShapes (16);

            zw::WavetableOscillator osc;
            osc.setWavetable (&wt);

            // Same note at two sample rates: higher sr -> smaller per-sample
            // phase increment -> fewer zero-crossings over a fixed sample count.
            auto crossings = [&] (double rate)
            {
                osc.prepare (rate);
                osc.noteOn (false, 0.0f);
                zw::WavetableOscillator::UpdateParams p {};
                p.framePos01  = 1.0f;
                p.warp01      = 0.0f;
                p.unison      = 1;
                p.detuneCents = 0.0f;
                p.level01     = 1.0f;
                p.pan         = 0.0f;
                p.width01     = 0.0f;
                p.baseFreqHz  = 440.0;
                osc.update (p);

                int   zc   = 0;
                float prev = 0.0f;
                for (int i = 0; i < 4096; ++i)
                {
                    float l = 0.0f, r = 0.0f;
                    osc.renderAdd (l, r);
                    if (i > 0 && ((prev < 0.0f) != (l < 0.0f))) ++zc;
                    prev = l;
                }
                return zc;
            };

            const int zcLow  = crossings (44100.0);
            const int zcHigh = crossings (192000.0);
            // Higher sample rate covers fewer cycles in the same number of samples.
            expectGreaterThan (zcLow, zcHigh);
        }
    }
};

static WavetableTests wavetableTests;
}
