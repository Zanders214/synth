// Unit tests for zw::SubOscillator and zw::NoiseOscillator (BasicSources).
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/BasicSources.h"

#include <cmath>

namespace
{
struct BasicSourcesTests : juce::UnitTest
{
    BasicSourcesTests() : juce::UnitTest ("BasicSources", "DSP") {}

    void runTest() override
    {
        const double sr = 48000.0;

        //======================================================================
        // SubOscillator
        //======================================================================
        beginTest ("sub: zero level renders pure silence regardless of wave");
        {
            zw::SubOscillator sub;
            sub.prepare (sr);
            sub.noteOn();
            for (int wave = zw::SubOscillator::Sine; wave <= zw::SubOscillator::Square; ++wave)
            {
                sub.update (wave, 0, 0.0f, 0.0f /*level*/, 110.0);
                for (int i = 0; i < 64; ++i)
                    expectEquals (sub.render(), 0.0f);
            }
        }

        beginTest ("sub: every wave enum renders finite output within sane range");
        {
            zw::SubOscillator sub;
            sub.prepare (sr);
            for (int wave = zw::SubOscillator::Sine; wave <= zw::SubOscillator::Square; ++wave)
            {
                sub.noteOn();
                sub.update (wave, 0, 0.0f, 1.0f, 110.0);
                float peak = 0.0f;
                for (int i = 0; i < (int) sr; ++i)
                {
                    const float s = sub.render();
                    expect (std::isfinite (s), "sub output must be finite");
                    // No saturation, unity gain: naive shapes stay within [-1, 1]
                    // (plus a tiny margin for the sine approximation).
                    expect (s <= 1.001f && s >= -1.001f, "sub output out of range");
                    peak = juce::jmax (peak, std::abs (s));
                }
                expect (peak > 0.5f, "an audible wave should produce real amplitude");
            }
        }

        beginTest ("sub: square wave only ever emits the two rail values");
        {
            zw::SubOscillator sub;
            sub.prepare (sr);
            sub.noteOn();
            sub.update (zw::SubOscillator::Square, 0, 0.0f /*no sat*/, 1.0f, 220.0);
            bool sawPos = false, sawNeg = false;
            for (int i = 0; i < 4096; ++i)
            {
                const float s = sub.render();
                // gain == 1, no saturation -> output is exactly +/-1.
                expect (std::abs (std::abs (s) - 1.0f) < 1.0e-5f, "square must be +/-1");
                if (s > 0.0f) sawPos = true; else sawNeg = true;
            }
            expect (sawPos && sawNeg, "square should swing both ways");
        }

        beginTest ("sub: level parameter scales amplitude proportionally");
        {
            // Render the same deterministic waveform at two levels and compare peaks.
            auto peakAtLevel = [sr] (float level) -> float
            {
                zw::SubOscillator sub;
                sub.prepare (sr);
                sub.noteOn();
                sub.update (zw::SubOscillator::Square, 0, 0.0f, level, 200.0);
                float pk = 0.0f;
                for (int i = 0; i < 2048; ++i)
                    pk = juce::jmax (pk, std::abs (sub.render()));
                return pk;
            };

            const float full = peakAtLevel (1.0f);
            const float half = peakAtLevel (0.5f);
            expectWithinAbsoluteError (full, 1.0f, 1.0e-5f);
            expectWithinAbsoluteError (half, 0.5f, 1.0e-5f);
            expectGreaterThan (full, half);
        }

        beginTest ("sub: octave offset raises pitch (more zero crossings per block)");
        {
            auto crossings = [sr] (int octave) -> int
            {
                zw::SubOscillator sub;
                sub.prepare (sr);
                sub.noteOn();
                sub.update (zw::SubOscillator::Sine, octave, 0.0f, 1.0f, 100.0);
                int xc = 0;
                float prev = sub.render();
                for (int i = 0; i < 8192; ++i)
                {
                    const float s = sub.render();
                    if ((s >= 0.0f) != (prev >= 0.0f)) ++xc;
                    prev = s;
                }
                return xc;
            };

            const int low  = crossings (0);
            const int high = crossings (2); // two octaves up -> ~4x frequency
            expectGreaterThan (high, low);
        }

        beginTest ("sub: saturation keeps output bounded and finite at extremes");
        {
            zw::SubOscillator sub;
            sub.prepare (sr);
            sub.noteOn();
            sub.update (zw::SubOscillator::Sine, 0, 1.0f /*max saturate*/, 1.0f, 130.0);
            float peak = 0.0f;
            for (int i = 0; i < (int) sr; ++i)
            {
                const float s = sub.render();
                expect (std::isfinite (s), "saturated sub must stay finite");
                // satComp normalises tanh so the peak stays close to +/-1.
                expect (s <= 1.05f && s >= -1.05f, "saturated sub out of range");
                peak = juce::jmax (peak, std::abs (s));
            }
            expect (peak > 0.5f, "saturated sine should still be audible");
        }

        beginTest ("sub: noteOn restarts phase from zero (deterministic restart)");
        {
            zw::SubOscillator sub;
            sub.prepare (sr);

            // Capture the first few samples of a fresh note.
            sub.noteOn();
            sub.update (zw::SubOscillator::Tri, 0, 0.0f, 1.0f, 170.0);
            float first[8];
            for (auto& v : first) v = sub.render();

            // Run on, then noteOn() again and update with identical params.
            for (int i = 0; i < 500; ++i) sub.render();
            sub.noteOn();
            sub.update (zw::SubOscillator::Tri, 0, 0.0f, 1.0f, 170.0);
            for (int i = 0; i < 8; ++i)
                expectWithinAbsoluteError (sub.render(), first[i], 1.0e-5f);
        }

        beginTest ("sub: sample-rate change keeps audio finite");
        {
            zw::SubOscillator sub;
            sub.prepare (44100.0);
            sub.noteOn();
            sub.update (zw::SubOscillator::Sine, 0, 0.5f, 0.8f, 100.0);
            for (int i = 0; i < 256; ++i) expect (std::isfinite (sub.render()));

            sub.prepare (96000.0);                 // change SR mid-life
            sub.update (zw::SubOscillator::Sine, 0, 0.5f, 0.8f, 100.0);
            for (int i = 0; i < 256; ++i) expect (std::isfinite (sub.render()));
        }

        //======================================================================
        // NoiseOscillator
        //======================================================================
        beginTest ("noise: zero level renders pure silence for every type");
        {
            zw::NoiseOscillator noise;
            noise.prepare (sr);
            for (int t = zw::NoiseOscillator::White; t <= zw::NoiseOscillator::Vinyl; ++t)
            {
                noise.update (t, 0.5f, 0.0f /*level*/);
                for (int i = 0; i < 64; ++i)
                    expectEquals (noise.render(), 0.0f);
            }
        }

        beginTest ("noise: every type stays finite and bounded by the gain ceiling");
        {
            zw::NoiseOscillator noise;
            noise.prepare (sr);
            const float level = 0.75f;
            for (int t = zw::NoiseOscillator::White; t <= zw::NoiseOscillator::Vinyl; ++t)
            {
                noise.reset();
                noise.update (t, 0.5f, level);
                float peak = 0.0f;
                for (int i = 0; i < (int) sr; ++i)
                {
                    const float s = noise.render();
                    expect (std::isfinite (s), "noise output must be finite");
                    // render() clamps the bed to [-1,1] then multiplies by gain.
                    expect (std::abs (s) <= level + 1.0e-5f, "noise exceeds gain ceiling");
                    peak = juce::jmax (peak, std::abs (s));
                }
                expect (peak > 0.0f, "noise of non-zero level should produce signal");
            }
        }

        beginTest ("noise: color extremes (dark LP vs bright HP) both stay finite");
        {
            zw::NoiseOscillator noise;
            noise.prepare (sr);
            for (float color : { 0.0f, 0.49f, 0.5f, 1.0f })
            {
                noise.reset();
                noise.update (zw::NoiseOscillator::White, color, 0.9f);
                for (int i = 0; i < 4096; ++i)
                {
                    const float s = noise.render();
                    expect (std::isfinite (s), "colored noise must be finite");
                    expect (std::abs (s) <= 0.9f + 1.0e-5f, "colored noise out of range");
                }
            }
        }

        beginTest ("noise: level parameter scales the amplitude envelope");
        {
            // Compare RMS energy at full vs quarter level for white noise.
            auto rms = [sr] (float level) -> double
            {
                zw::NoiseOscillator noise;
                noise.prepare (sr);
                noise.update (zw::NoiseOscillator::White, 0.6f, level);
                double acc = 0.0;
                const int n = 20000;
                for (int i = 0; i < n; ++i)
                {
                    const double s = (double) noise.render();
                    acc += s * s;
                }
                return std::sqrt (acc / (double) n);
            };

            const double full    = rms (1.0f);
            const double quarter = rms (0.25f);
            expectGreaterThan (full, 0.0);
            expectGreaterThan (quarter, 0.0);
            // Amplitude scales linearly, so RMS should too (loose tolerance for RNG).
            expect (full > quarter * 2.0, "louder level must raise RMS energy");
        }

        beginTest ("noise: reset() clears internal state and keeps output well-behaved");
        {
            // Drive the vinyl path hard so lp, pinkState and crackle accumulate,
            // then assert reset() leaves the generator in a clean, stable state.
            // (We can't compare two instances sample-for-sample: each owns its own
            // juce::Random, which is seeded non-deterministically. So we assert the
            // observable invariants of a cleared filter bank instead.)
            zw::NoiseOscillator noise;
            noise.prepare (sr);
            noise.update (zw::NoiseOscillator::Vinyl, 0.95f, 0.8f);
            for (int i = 0; i < 8000; ++i)
                expect (std::isfinite (noise.render()));

            noise.reset();
            // Immediately after reset the one-pole filters start from zero, so the
            // stream must remain finite and bounded by the gain ceiling, and it must
            // still be able to produce real signal.
            float peak = 0.0f;
            for (int i = 0; i < 4096; ++i)
            {
                const float s = noise.render();
                expect (std::isfinite (s), "post-reset noise must be finite");
                expect (std::abs (s) <= 0.8f + 1.0e-5f, "post-reset noise out of range");
                peak = juce::jmax (peak, std::abs (s));
            }
            expect (peak > 0.0f, "post-reset noise should still generate signal");

            // reset() must be safe to call repeatedly (e.g. from prepare()).
            noise.reset();
            noise.reset();
            expect (std::isfinite (noise.render()));
        }

        beginTest ("noise: prepare() implies reset (clean start after prepare)");
        {
            zw::NoiseOscillator noise;
            noise.prepare (sr);
            noise.update (zw::NoiseOscillator::Pink, 0.5f, 0.7f);
            for (int i = 0; i < 3000; ++i)
            {
                const float s = noise.render();
                expect (std::isfinite (s));
            }
            // prepare() again should restore a finite, bounded stream.
            noise.prepare (sr);
            noise.update (zw::NoiseOscillator::Pink, 0.5f, 0.7f);
            for (int i = 0; i < 64; ++i)
            {
                const float s = noise.render();
                expect (std::isfinite (s));
                expect (std::abs (s) <= 0.7f + 1.0e-5f);
            }
        }
    }
};

static BasicSourcesTests basicSourcesTests;
}
