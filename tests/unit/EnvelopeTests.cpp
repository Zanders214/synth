// Unit tests for zw::Envelope (analog-style exponential ADSR).
#include <juce_core/juce_core.h>
#include "dsp/Envelope.h"

#include <cmath>

namespace
{
struct EnvelopeTests : juce::UnitTest
{
    EnvelopeTests() : juce::UnitTest ("Envelope", "DSP") {}

    void runTest() override
    {
        const double sr = 48000.0;

        beginTest ("idle envelope is inactive and zero");
        {
            zw::Envelope env;
            env.setSampleRate (sr);
            expect (! env.isActive());
            expectEquals (env.getValue(), 0.0f);
            expectEquals (env.getNextSample(), 0.0f);
        }

        beginTest ("attack ramps up to ~1.0 then decays toward sustain");
        {
            zw::Envelope env;
            env.setSampleRate (sr);
            env.setParameters (0.01f, 0.05f, 0.5f, 0.1f);
            env.noteOn();
            expect (env.isActive());

            // Hold for a full second: attack -> decay -> sustain should settle.
            float v = 0.0f;
            float peak = 0.0f;
            for (int i = 0; i < (int) sr; ++i)
            {
                v = env.getNextSample();
                peak = juce::jmax (peak, v);
                expect (std::isfinite (v));
            }
            expect (peak > 0.99f, "attack should reach (near) full scale");
            expectWithinAbsoluteError (v, 0.5f, 0.02f); // settled at sustain
        }

        beginTest ("release falls back to zero and goes idle");
        {
            zw::Envelope env;
            env.setSampleRate (sr);
            env.setParameters (0.005f, 0.02f, 0.6f, 0.05f);
            env.noteOn();
            for (int i = 0; i < (int) sr; ++i) env.getNextSample();   // reach sustain
            env.noteOff();
            float v = env.getValue();
            for (int i = 0; i < (int) sr && env.isActive(); ++i) v = env.getNextSample();
            expect (! env.isActive(), "envelope should return to idle after release");
            expectEquals (v, 0.0f);
        }

        beginTest ("reset clears stage and value");
        {
            zw::Envelope env;
            env.setSampleRate (sr);
            env.setParameters (0.01f, 0.05f, 0.7f, 0.1f);
            env.noteOn();
            for (int i = 0; i < 100; ++i) env.getNextSample();
            env.reset();
            expect (! env.isActive());
            expectEquals (env.getValue(), 0.0f);
        }

        beginTest ("zero-time attack jumps straight to sustain region");
        {
            zw::Envelope env;
            env.setSampleRate (sr);
            env.setParameters (0.0f, 0.0f, 0.8f, 0.1f);
            env.noteOn();
            float v = 0.0f;
            for (int i = 0; i < 16; ++i) v = env.getNextSample();
            expect (std::isfinite (v));
            expect (v <= 1.0f && v >= 0.0f);
        }
    }
};

static EnvelopeTests envelopeTests;
}
