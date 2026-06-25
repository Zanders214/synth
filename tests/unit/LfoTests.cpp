// Unit tests for zw::Lfo (control-rate LFO: 6 shapes, 3 modes, depth, rise, phase).
#include <juce_core/juce_core.h>
#include "dsp/Lfo.h"

#include <cmath>

namespace
{
struct LfoTests : juce::UnitTest
{
    LfoTests() : juce::UnitTest ("Lfo", "DSP") {}

    void runTest() override
    {
        const double sr = 48000.0;

        beginTest ("shapeValue at phase 0 matches each waveform's formula");
        {
            // With freq 0 the phase never advances, so processBlock returns the
            // shape value at the start phase (0 by default) * depth (1) * rise (1).
            struct Case { int shape; float expected; const char* name; };
            const Case cases[] = {
                { zw::Lfo::Sine,     0.0f,  "sine(0)=0"     },
                { zw::Lfo::Triangle, 1.0f,  "tri(0)=1"      },
                { zw::Lfo::Saw,      1.0f,  "saw(0)=1"      },
                { zw::Lfo::Ramp,    -1.0f,  "ramp(0)=-1"    },
                { zw::Lfo::Square,   1.0f,  "square(0)=1"   },
            };

            for (const auto& c : cases)
            {
                zw::Lfo lfo;
                lfo.prepare (sr);
                lfo.setParams (c.shape, 0.0f, 1.0f, 0.0f, 0.0f, zw::Lfo::Trigger);
                lfo.noteOn();
                const float v = lfo.processBlock (32);
                expectWithinAbsoluteError (v, c.expected, 1.0e-5f, c.name);
            }
        }

        beginTest ("every shape stays within [-depth, depth] over a full cycle");
        {
            const int shapes[] = { zw::Lfo::Sine, zw::Lfo::Triangle, zw::Lfo::Saw,
                                   zw::Lfo::Ramp, zw::Lfo::Square, zw::Lfo::Random };
            const float depth = 0.75f;

            for (int shape : shapes)
            {
                zw::Lfo lfo;
                lfo.prepare (sr);
                lfo.setParams (shape, 2.0f, depth, 0.0f, 0.0f, zw::Lfo::Free);
                lfo.noteOn();

                float maxAbs = 0.0f;
                for (int i = 0; i < 4096; ++i)
                {
                    const float v = lfo.processBlock (16);
                    expect (std::isfinite (v), "output must be finite");
                    maxAbs = juce::jmax (maxAbs, std::abs (v));
                    expect (v <= depth + 1.0e-5f && v >= -depth - 1.0e-5f,
                            "output bounded by depth");
                }
                // Each shape should actually swing somewhere (not stuck at 0).
                expectGreaterThan (maxAbs, 0.0f);
            }
        }

        beginTest ("depth scales the output linearly");
        {
            // Compare the same shape/phase at two depths; full-depth should be
            // exactly twice the half-depth value (ignoring sign-zero cases).
            zw::Lfo a, b;
            a.prepare (sr);
            b.prepare (sr);
            // Ramp at phase 0.25 -> 2*0.25-1 = -0.5
            a.setParams (zw::Lfo::Ramp, 0.0f, 1.0f, 0.0f, 0.25f, zw::Lfo::Trigger);
            b.setParams (zw::Lfo::Ramp, 0.0f, 0.5f, 0.0f, 0.25f, zw::Lfo::Trigger);
            a.noteOn();
            b.noteOn();
            const float va = a.processBlock (8);
            const float vb = b.processBlock (8);
            expectWithinAbsoluteError (va, -0.5f, 1.0e-5f);
            expectWithinAbsoluteError (vb, -0.25f, 1.0e-5f);
            expectWithinAbsoluteError (va, vb * 2.0f, 1.0e-5f, "depth is linear");
        }

        beginTest ("zero depth silences output for all shapes");
        {
            const int shapes[] = { zw::Lfo::Sine, zw::Lfo::Triangle, zw::Lfo::Saw,
                                   zw::Lfo::Ramp, zw::Lfo::Square, zw::Lfo::Random };
            for (int shape : shapes)
            {
                zw::Lfo lfo;
                lfo.prepare (sr);
                lfo.setParams (shape, 3.0f, 0.0f, 0.0f, 0.0f, zw::Lfo::Free);
                lfo.noteOn();
                for (int i = 0; i < 256; ++i)
                    expectEquals (lfo.processBlock (32), 0.0f);
            }
        }

        beginTest ("start phase positions the waveform at noteOn");
        {
            // Square is 1 for phase < 0.5 and -1 otherwise. Use start phase to
            // land in each half with freq 0 so the phase does not move.
            zw::Lfo lo, hi;
            lo.prepare (sr);
            hi.prepare (sr);
            lo.setParams (zw::Lfo::Square, 0.0f, 1.0f, 0.0f, 0.25f, zw::Lfo::Trigger);
            hi.setParams (zw::Lfo::Square, 0.0f, 1.0f, 0.0f, 0.75f, zw::Lfo::Trigger);
            lo.noteOn();
            hi.noteOn();
            expectWithinAbsoluteError (lo.processBlock (8),  1.0f, 1.0e-5f);
            expectWithinAbsoluteError (hi.processBlock (8), -1.0f, 1.0e-5f);
        }

        beginTest ("Trigger mode resets phase on noteOn; Free mode does not");
        {
            // Trigger: advance phase, retrigger -> output returns to start-phase value.
            zw::Lfo trig;
            trig.prepare (sr);
            // Ramp, start phase 0 -> value -1 right after noteOn.
            trig.setParams (zw::Lfo::Ramp, 1.0f, 1.0f, 0.0f, 0.0f, zw::Lfo::Trigger);
            trig.noteOn();
            const float firstTrig = trig.processBlock (1);   // tiny advance
            for (int i = 0; i < 100; ++i) trig.processBlock (64); // move phase along
            trig.noteOn();                                   // should reset to phase 0
            const float afterRetrig = trig.processBlock (1);
            expectWithinAbsoluteError (afterRetrig, firstTrig, 1.0e-3f,
                                       "Trigger noteOn resets phase");

            // Free: noteOn must NOT reset phase. Advance, capture, noteOn, capture.
            zw::Lfo freeLfo;
            freeLfo.prepare (sr);
            freeLfo.setParams (zw::Lfo::Ramp, 5.0f, 1.0f, 0.0f, 0.0f, zw::Lfo::Free);
            freeLfo.noteOn();
            float before = 0.0f;
            for (int i = 0; i < 50; ++i) before = freeLfo.processBlock (64);
            freeLfo.noteOn();                                // phase preserved in Free
            const float after = freeLfo.processBlock (1);    // near-identical to before
            // Phase barely moved by 1 sample, so values should be very close, and
            // crucially NOT snapped back to the -1 start value.
            expect (after > -0.99f, "Free mode keeps running phase across noteOn");
            expectWithinAbsoluteError (after, before, 0.05f);
        }

        beginTest ("Envelope mode also resets phase on noteOn (mode != Free)");
        {
            zw::Lfo env;
            env.prepare (sr);
            env.setParams (zw::Lfo::Ramp, 1.0f, 1.0f, 0.0f, 0.0f, zw::Lfo::Envelope);
            env.noteOn();
            for (int i = 0; i < 80; ++i) env.processBlock (64);
            env.noteOn();
            const float v = env.processBlock (1);
            expectWithinAbsoluteError (v, -1.0f, 1.0e-2f,
                                       "Envelope mode resets phase to start");
        }

        beginTest ("free-run rate vs synced rate: higher freq advances faster");
        {
            // Two LFOs, same shape/phase, different frequency. After one block the
            // higher-frequency one has progressed further along the rising ramp.
            zw::Lfo slow, fast;
            slow.prepare (sr);
            fast.prepare (sr);
            slow.setParams (zw::Lfo::Ramp, 1.0f,  1.0f, 0.0f, 0.0f, zw::Lfo::Free);
            fast.setParams (zw::Lfo::Ramp, 10.0f, 1.0f, 0.0f, 0.0f, zw::Lfo::Free);
            slow.noteOn();
            fast.noteOn();
            const int block = 512;
            const float vs = slow.processBlock (block);
            const float vf = fast.processBlock (block);
            // Ramp is monotonically increasing in phase, and neither has wrapped
            // (10 Hz * 512 / 48000 ~= 0.107 < 1), so faster > slower.
            expectGreaterThan (vf, vs);
        }

        beginTest ("phase wraps and stays bounded across many blocks at high freq");
        {
            zw::Lfo lfo;
            lfo.prepare (sr);
            lfo.setParams (zw::Lfo::Sine, 7000.0f, 1.0f, 0.0f, 0.0f, zw::Lfo::Free);
            lfo.noteOn();
            for (int i = 0; i < 1000; ++i)
            {
                const float v = lfo.processBlock (37);  // forces multi-cycle wraps
                expect (std::isfinite (v));
                expect (v >= -1.0001f && v <= 1.0001f);
            }
        }

        beginTest ("rise/fade-in ramps gain from 0 up to full");
        {
            // riseTime = 0.5 s. Square (constant magnitude 1 at phase 0) lets us
            // observe the rise gain directly. freq 0 keeps phase fixed at start.
            zw::Lfo lfo;
            lfo.prepare (sr);
            const float riseSec = 0.5f;
            lfo.setParams (zw::Lfo::Square, 0.0f, 1.0f, riseSec, 0.0f, zw::Lfo::Trigger);
            lfo.noteOn();

            const int blockSize = 480;            // 0.01 s per block at 48 kHz
            const float first = lfo.processBlock (blockSize);
            expect (first < 1.0f, "rise gain starts below full");
            expectGreaterThan (first, 0.0f);

            float prev = first;
            bool monotonic = true;
            float last = first;
            // Run well past riseSec; gain should climb monotonically then clamp at 1.
            for (int i = 0; i < 200; ++i)
            {
                const float v = lfo.processBlock (blockSize);
                if (v < prev - 1.0e-4f) monotonic = false;
                prev = v;
                last = v;
            }
            expect (monotonic, "rise gain is non-decreasing");
            expectWithinAbsoluteError (last, 1.0f, 1.0e-4f, "rise saturates at full depth");
        }

        beginTest ("rise below threshold is treated as instant full gain");
        {
            // riseTime <= 0.0001 -> the rise branch is skipped, gain = 1 immediately.
            zw::Lfo lfo;
            lfo.prepare (sr);
            lfo.setParams (zw::Lfo::Square, 0.0f, 1.0f, 0.0f, 0.0f, zw::Lfo::Trigger);
            lfo.noteOn();
            expectWithinAbsoluteError (lfo.processBlock (16), 1.0f, 1.0e-6f);
        }

        beginTest ("processBlock(0) does not advance phase");
        {
            zw::Lfo lfo;
            lfo.prepare (sr);
            lfo.setParams (zw::Lfo::Ramp, 100.0f, 1.0f, 0.0f, 0.0f, zw::Lfo::Free);
            lfo.noteOn();
            const float a = lfo.processBlock (0);
            const float b = lfo.processBlock (0);
            expectEquals (a, b);
            expectWithinAbsoluteError (a, -1.0f, 1.0e-5f, "phase 0 ramp = -1");
        }

        beginTest ("sample-rate change affects phase advance per block");
        {
            // At a higher sample rate the same block advances the phase less, so
            // after one block the rising ramp value is lower.
            zw::Lfo a, b;
            a.prepare (44100.0);
            b.prepare (96000.0);
            a.setParams (zw::Lfo::Ramp, 50.0f, 1.0f, 0.0f, 0.0f, zw::Lfo::Free);
            b.setParams (zw::Lfo::Ramp, 50.0f, 1.0f, 0.0f, 0.0f, zw::Lfo::Free);
            a.noteOn();
            b.noteOn();
            const int block = 256;
            const float va = a.processBlock (block);  // lower sr -> more progress
            const float vb = b.processBlock (block);  // higher sr -> less progress
            expectGreaterThan (va, vb);
        }

        beginTest ("Random shape holds value within a cycle and changes across cycles");
        {
            zw::Lfo lfo;
            lfo.prepare (sr);
            // 2 Hz, small blocks: many blocks share a cycle (value held), then change.
            lfo.setParams (zw::Lfo::Random, 2.0f, 1.0f, 0.0f, 0.0f, zw::Lfo::Free);
            lfo.noteOn();

            float seen = lfo.processBlock (32);
            bool held = true;       // observed at least one repeated value mid-cycle
            bool changed = false;   // observed at least one change across a cycle
            float prev = seen;
            for (int i = 0; i < 4000; ++i)
            {
                const float v = lfo.processBlock (32);
                expect (v >= -1.0001f && v <= 1.0001f, "S&H value bounded");
                if (std::abs (v - prev) < 1.0e-7f) held = true;
                if (std::abs (v - prev) > 1.0e-6f) changed = true;
                prev = v;
            }
            expect (held, "S&H holds its value between cycles");
            expect (changed, "S&H updates on a new cycle");
        }
    }
};

static LfoTests lfoTests;
}
