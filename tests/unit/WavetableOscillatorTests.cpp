// Unit tests for zw::WavetableOscillator's warp modes. Each "Warp Mode" choice
// (Sync, Bend, PWM, Asym, Remap, Quantize) must drive real DSP in the read
// path: for every mode we render at warp=0 and warp=1 and assert the output is
// (a) finite, (b) measurably changed by warp, and (c) distinct from every other
// mode -- proving each mode does something audibly different.
#include <juce_core/juce_core.h>

#include "dsp/Wavetable.h"
#include "dsp/WavetableOscillator.h"

#include <cmath>
#include <vector>

namespace
{
struct WarpModeTests : juce::UnitTest
{
    WarpModeTests() : juce::UnitTest ("WavetableOscillator Warp Modes", "DSP") {}

    // Render the left channel for a single warp mode + amount into a buffer.
    // noteOn() resets phase first so renders are directly comparable.
    static std::vector<float> render (zw::Wavetable& wt, int mode, float warp, int n)
    {
        zw::WavetableOscillator osc;
        osc.prepare (48000.0);
        osc.setWavetable (&wt);
        osc.noteOn (false, 0.0f);   // deterministic start phase (no randomisation)

        zw::WavetableOscillator::UpdateParams p {};
        p.framePos01  = 0.85f;      // rich, saw-leaning frame so warp has material
        p.warp01      = warp;
        p.unison      = 1;          // single voice -> warp shape is not averaged out
        p.detuneCents = 0.0f;
        p.level01     = 1.0f;
        p.pan         = 0.0f;
        p.width01     = 0.0f;
        p.baseFreqHz  = 110.0;
        p.warpMode    = mode;
        osc.update (p);

        std::vector<float> out ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            float l = 0.0f, r = 0.0f;
            osc.renderAdd (l, r);
            out[(size_t) i] = l;
        }
        return out;
    }

    static double rmsDiff (const std::vector<float>& a, const std::vector<float>& b)
    {
        double acc = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
        {
            const double d = (double) a[i] - (double) b[i];
            acc += d * d;
        }
        return std::sqrt (acc / (double) a.size());
    }

    void runTest() override
    {
        zw::Wavetable wt;
        wt.generateBasicShapes (64);

        const int         N        = 4096;
        const int         modes[]  = { 1, 2, 3, 4, 5, 6 };
        const char* const names[]  = { "Sync", "Bend", "PWM", "Asym", "Remap", "Quantize" };
        constexpr int     kNumModes = 6;
        constexpr double  kEps      = 1.0e-3;   // RMS difference floor

        std::vector<std::vector<float>> warpedAtOne;   // warp=1 render per mode

        beginTest ("each warp mode is finite and changes the sound vs warp=0");
        for (int k = 0; k < kNumModes; ++k)
        {
            const auto dry = render (wt, modes[k], 0.0f, N);   // identity at warp=0
            const auto wet = render (wt, modes[k], 1.0f, N);

            bool finite = true;
            for (float v : wet)
                if (! std::isfinite (v)) finite = false;
            expect (finite, juce::String (names[k]) + " produced non-finite output");

            // warp=0 must be the unwarped wave (all modes are the identity there),
            // so any mode that ignores warp would fail this.
            expectGreaterThan (rmsDiff (dry, wet), kEps,
                               juce::String (names[k]) + " warp=1 should differ from warp=0");

            warpedAtOne.push_back (wet);
        }

        beginTest ("warp modes are mutually distinct at warp=1");
        for (int i = 0; i < kNumModes; ++i)
            for (int j = i + 1; j < kNumModes; ++j)
                expectGreaterThan (rmsDiff (warpedAtOne[(size_t) i], warpedAtOne[(size_t) j]), kEps,
                                   juce::String (names[i]) + " and " + names[j]
                                       + " should sound different");

        beginTest ("Off mode (0) ignores warp entirely");
        {
            const auto a = render (wt, 0, 0.0f, N);
            const auto b = render (wt, 0, 1.0f, N);
            expectWithinAbsoluteError (rmsDiff (a, b), 0.0, 1.0e-9,
                                       "warp must have no effect when mode is Off");
        }
    }
};

static WarpModeTests warpModeTests;
}
