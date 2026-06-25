// Unit tests for zw::MultimodeFilter (stereo TPT state-variable multimode filter).
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>           // MultimodeFilter.h -> FastMath.h uses juce::dsp
#include "dsp/MultimodeFilter.h"

#include <cmath>
#include <vector>

namespace
{
// Compute RMS of running a single-frequency sine through the filter on channel 0.
// Skips an initial warm-up so the measurement reflects the steady-state response.
static float filteredSineRms (zw::MultimodeFilter& filt, double sr, double freqHz, int numSamples)
{
    const double twoPiFOverSr = 2.0 * juce::MathConstants<double>::pi * freqHz / sr;
    const int warmup = juce::jmin (numSamples / 2, 4096);
    double sumSq = 0.0;
    int counted = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        const float x = (float) std::sin (twoPiFOverSr * (double) i);
        const float y = filt.processSample (0, x);
        if (i >= warmup)
        {
            sumSq += (double) y * (double) y;
            ++counted;
        }
    }
    return counted > 0 ? (float) std::sqrt (sumSq / (double) counted) : 0.0f;
}

struct MultimodeFilterTests : juce::UnitTest
{
    MultimodeFilterTests() : juce::UnitTest ("MultimodeFilter", "DSP") {}

    void runTest() override
    {
        const double sr = 48000.0;

        beginTest ("prepare/reset leave the filter producing finite output from silence");
        {
            zw::MultimodeFilter filt;
            filt.prepare (sr);
            filt.setParams (zw::MultimodeFilter::LP12, 1000.0f, 0.2f, 0.0f);
            for (int i = 0; i < 256; ++i)
            {
                const float y = filt.processSample (0, 0.0f);
                expectEquals (y, 0.0f); // silence in, silence out (no drive, zero state)
            }
            filt.reset();
            const float y = filt.processSample (0, 0.0f);
            expectEquals (y, 0.0f);
        }

        beginTest ("every filter Type produces finite, bounded output for a broadband impulse train");
        {
            const int types[] = {
                zw::MultimodeFilter::LP24, zw::MultimodeFilter::LP12,
                zw::MultimodeFilter::HP24, zw::MultimodeFilter::HP12,
                zw::MultimodeFilter::BP12, zw::MultimodeFilter::Notch
            };

            for (int t : types)
            {
                zw::MultimodeFilter filt;
                filt.prepare (sr);
                filt.setParams (t, 2000.0f, 0.5f, 0.0f);

                float peak = 0.0f;
                for (int i = 0; i < 8000; ++i)
                {
                    // Mix of frequencies so every band sees energy.
                    const double p = (double) i;
                    const float x = 0.4f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * p / sr)
                                  + 0.4f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 8000.0 * p / sr);
                    const float y = filt.processSample (0, x);
                    expect (std::isfinite (y), "filter output must be finite");
                    peak = juce::jmax (peak, std::abs (y));
                }
                // The SVF is stable: a bounded input must yield a bounded output.
                expectLessThan (peak, 10.0f);
            }
        }

        beginTest ("low cutoff attenuates a high-frequency tone more than a high cutoff (LP12)");
        {
            const double hiTone = 10000.0;

            zw::MultimodeFilter lowCut;
            lowCut.prepare (sr);
            lowCut.setParams (zw::MultimodeFilter::LP12, 300.0f, 0.1f, 0.0f);
            const float rmsLowCut = filteredSineRms (lowCut, sr, hiTone, 16000);

            zw::MultimodeFilter highCut;
            highCut.prepare (sr);
            highCut.setParams (zw::MultimodeFilter::LP12, 18000.0f, 0.1f, 0.0f);
            const float rmsHighCut = filteredSineRms (highCut, sr, hiTone, 16000);

            expect (std::isfinite (rmsLowCut) && std::isfinite (rmsHighCut));
            expectGreaterThan (rmsHighCut, rmsLowCut * 2.0f); // low cutoff kills the highs
        }

        beginTest ("LP24 rolls off a high tone more steeply than LP12 at the same cutoff");
        {
            const double hiTone = 6000.0;
            const float cutoff = 800.0f;

            zw::MultimodeFilter lp12;
            lp12.prepare (sr);
            lp12.setParams (zw::MultimodeFilter::LP12, cutoff, 0.0f, 0.0f);
            const float rms12 = filteredSineRms (lp12, sr, hiTone, 16000);

            zw::MultimodeFilter lp24;
            lp24.prepare (sr);
            lp24.setParams (zw::MultimodeFilter::LP24, cutoff, 0.0f, 0.0f);
            const float rms24 = filteredSineRms (lp24, sr, hiTone, 16000);

            // 24 dB/oct (cascaded cores) attenuates more above the corner than 12 dB/oct.
            expectLessThan (rms24, rms12);
        }

        beginTest ("high-pass passes highs and attenuates a low tone (HP12 / HP24)");
        {
            const double lowTone = 80.0;
            const float cutoff = 2000.0f;

            zw::MultimodeFilter hp12;
            hp12.prepare (sr);
            hp12.setParams (zw::MultimodeFilter::HP12, cutoff, 0.0f, 0.0f);
            const float lowRms12  = filteredSineRms (hp12, sr, lowTone, 16000);
            hp12.reset();
            const float highRms12 = filteredSineRms (hp12, sr, 9000.0, 16000);
            expectGreaterThan (highRms12, lowRms12);

            zw::MultimodeFilter hp24;
            hp24.prepare (sr);
            hp24.setParams (zw::MultimodeFilter::HP24, cutoff, 0.0f, 0.0f);
            const float lowRms24  = filteredSineRms (hp24, sr, lowTone, 16000);
            hp24.reset();
            const float highRms24 = filteredSineRms (hp24, sr, 9000.0, 16000);
            expectGreaterThan (highRms24, lowRms24);
            // 24 dB/oct cuts the sub-cutoff tone harder than 12 dB/oct.
            expectLessThan (lowRms24, lowRms12);
        }

        beginTest ("band-pass peaks near its centre and rejects far-away tones (BP12)");
        {
            const float centre = 1000.0f;

            zw::MultimodeFilter bp;
            bp.prepare (sr);
            bp.setParams (zw::MultimodeFilter::BP12, centre, 0.4f, 0.0f);
            const float atCentre = filteredSineRms (bp, sr, centre, 16000);
            bp.reset();
            const float belowBand = filteredSineRms (bp, sr, 50.0, 16000);
            bp.reset();
            const float aboveBand = filteredSineRms (bp, sr, 16000.0, 16000);

            expect (std::isfinite (atCentre));
            expectGreaterThan (atCentre, belowBand);
            expectGreaterThan (atCentre, aboveBand);
        }

        beginTest ("notch rejects a tone at its centre relative to a far-away tone");
        {
            const float centre = 1000.0f;

            zw::MultimodeFilter notch;
            notch.prepare (sr);
            notch.setParams (zw::MultimodeFilter::Notch, centre, 0.5f, 0.0f);
            const float atCentre = filteredSineRms (notch, sr, centre, 16000);
            notch.reset();
            const float farLow = filteredSineRms (notch, sr, 50.0, 16000);

            expect (std::isfinite (atCentre) && std::isfinite (farLow));
            // A notch attenuates its centre frequency; a far-away tone passes.
            expectLessThan (atCentre, farLow);
        }

        beginTest ("higher resonance boosts response near the cutoff (LP12)");
        {
            const float cutoff = 1000.0f;

            zw::MultimodeFilter lowQ;
            lowQ.prepare (sr);
            lowQ.setParams (zw::MultimodeFilter::LP12, cutoff, 0.0f, 0.0f);
            const float rmsLowQ = filteredSineRms (lowQ, sr, cutoff, 16000);

            zw::MultimodeFilter highQ;
            highQ.prepare (sr);
            highQ.setParams (zw::MultimodeFilter::LP12, cutoff, 0.9f, 0.0f);
            const float rmsHighQ = filteredSineRms (highQ, sr, cutoff, 16000);

            expect (std::isfinite (rmsHighQ));
            expectGreaterThan (rmsHighQ, rmsLowQ); // resonant peak amplifies at fc
        }

        beginTest ("drive engages the saturator and stays finite/bounded");
        {
            zw::MultimodeFilter noDrive;
            noDrive.prepare (sr);
            noDrive.setParams (zw::MultimodeFilter::LP12, 4000.0f, 0.1f, 0.0f);

            zw::MultimodeFilter maxDrive;
            maxDrive.prepare (sr);
            maxDrive.setParams (zw::MultimodeFilter::LP12, 4000.0f, 0.1f, 1.0f);

            // A loud signal: no-drive passes it ~linearly, drive saturates -> compressed.
            bool sawDifference = false;
            for (int i = 0; i < 4000; ++i)
            {
                const float x = 0.95f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 200.0 * (double) i / sr);
                const float yClean = noDrive.processSample (0, x);
                const float yDriven = maxDrive.processSample (0, x);
                expect (std::isfinite (yClean) && std::isfinite (yDriven));
                expect (std::abs (yDriven) < 8.0f, "driven output must stay bounded");
                if (std::abs (yDriven - yClean) > 1.0e-4f)
                    sawDifference = true;
            }
            expect (sawDifference, "drive should change the output versus no drive");
        }

        beginTest ("cutoff is clamped to the legal range without producing NaNs");
        {
            zw::MultimodeFilter filt;
            filt.prepare (sr);

            // Below the 20 Hz floor and above the sr*0.49 ceiling, plus extreme resonance.
            filt.setParams (zw::MultimodeFilter::LP24, -100.0f, 1.0f, 1.0f);
            for (int i = 0; i < 2000; ++i)
            {
                const float y = filt.processSample (0, (i % 64 == 0) ? 1.0f : 0.0f);
                expect (std::isfinite (y));
            }

            filt.reset();
            filt.setParams (zw::MultimodeFilter::HP24, 1.0e6f, 1.0f, 1.0f);
            for (int i = 0; i < 2000; ++i)
            {
                const float y = filt.processSample (0, (i % 64 == 0) ? 1.0f : 0.0f);
                expect (std::isfinite (y));
            }
        }

        beginTest ("the two channels keep independent state (ic1/ic2)");
        {
            zw::MultimodeFilter filt;
            filt.prepare (sr);
            filt.setParams (zw::MultimodeFilter::LP12, 1000.0f, 0.3f, 0.0f);

            // Drive only channel 0 with an impulse; channel 1 sees silence.
            float ch0Energy = 0.0f;
            float ch1Energy = 0.0f;
            for (int i = 0; i < 1024; ++i)
            {
                const float x0 = (i == 0) ? 1.0f : 0.0f;
                const float y0 = filt.processSample (0, x0);
                const float y1 = filt.processSample (1, 0.0f);
                ch0Energy += std::abs (y0);
                ch1Energy += std::abs (y1);
                expect (std::isfinite (y0) && std::isfinite (y1));
            }
            expectGreaterThan (ch0Energy, 0.0f);  // channel 0 rang from the impulse
            expectEquals (ch1Energy, 0.0f);       // channel 1 stayed silent / independent
        }

        beginTest ("reset clears ringing so a subsequent impulse response repeats");
        {
            zw::MultimodeFilter filt;
            filt.prepare (sr);
            filt.setParams (zw::MultimodeFilter::BP12, 1500.0f, 0.6f, 0.0f);

            std::vector<float> first (512), second (512);
            for (int i = 0; i < 512; ++i)
                first[(size_t) i] = filt.processSample (0, (i == 0) ? 1.0f : 0.0f);

            filt.reset();
            for (int i = 0; i < 512; ++i)
                second[(size_t) i] = filt.processSample (0, (i == 0) ? 1.0f : 0.0f);

            for (int i = 0; i < 512; ++i)
                expectWithinAbsoluteError (second[(size_t) i], first[(size_t) i], 1.0e-6f);
        }

        beginTest ("a sample-rate change is honoured by re-prepare + setParams");
        {
            zw::MultimodeFilter filt;
            filt.prepare (44100.0);
            filt.setParams (zw::MultimodeFilter::LP12, 1000.0f, 0.2f, 0.0f);
            for (int i = 0; i < 256; ++i)
                expect (std::isfinite (filt.processSample (0, 0.5f)));

            filt.prepare (96000.0);   // different sample rate, state reset inside prepare
            filt.setParams (zw::MultimodeFilter::LP12, 1000.0f, 0.2f, 0.0f);
            const float yAfterPrepare = filt.processSample (0, 0.0f);
            expectEquals (yAfterPrepare, 0.0f); // prepare() reset the state
            for (int i = 0; i < 256; ++i)
                expect (std::isfinite (filt.processSample (0, 0.5f)));
        }
    }
};

static MultimodeFilterTests multimodeFilterTests;
}
