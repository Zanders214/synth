// Unit tests for zw::WavetableLibrary: the factory set has the expected count,
// the tables are spectrally distinct, every table reads finite samples across
// frame positions/frequencies, and a synthetic .wav round-trips into a usable
// Wavetable in the user-import slot. Built in the juce::UnitTest style used by
// the rest of tests/unit/.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "dsp/WavetableLibrary.h"

#include <cmath>
#include <vector>

namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;

// A multi-frame-position "signature" of one table: concatenated cycle samples at
// several frame positions, so two genuinely different tables differ somewhere
// even if they coincide at one position (e.g. a pure-sine first frame).
static std::vector<float> signatureOf (const zw::Wavetable& wt, double sr)
{
    const float framePositions[] = { 0.25f, 0.5f, 0.75f, 1.0f };
    constexpr int pts = 128;
    std::vector<float> sig;
    sig.reserve (sizeof (framePositions) / sizeof (float) * pts);
    for (float fp : framePositions)
        for (int i = 0; i < pts; ++i)
            sig.push_back (wt.getSample (fp, (float) i / (float) pts, 55.0, sr));
    return sig;
}

static double correlation (const std::vector<float>& a, const std::vector<float>& b)
{
    double sa = 0.0, sb = 0.0, sab = 0.0;
    const size_t n = juce::jmin (a.size(), b.size());
    for (size_t i = 0; i < n; ++i) { sa += a[i] * a[i]; sb += b[i] * b[i]; sab += a[i] * b[i]; }
    return (sa > 0.0 && sb > 0.0) ? sab / std::sqrt (sa * sb) : 0.0;
}

struct WavetableLibraryTests : juce::UnitTest
{
    WavetableLibraryTests() : juce::UnitTest ("WavetableLibrary", "DSP") {}

    void runTest() override
    {
        const double sr = 48000.0;

        beginTest ("factory set has the expected count (matches the choice list)");
        {
            zw::WavetableLibrary lib;
            // size() counts every factory table plus the single user-import slot,
            // and must line up with the wtselect parameter's option list.
            expectEquals (lib.size(), (int) zw::choices::wavetable().size());
            // At least 12 distinct factory tables (the user slot is the +1).
            expect (lib.size() - 1 >= 12, "expected at least 12 factory tables");
            // getByIndex is total over [0, size()) and never returns null.
            for (int i = 0; i < lib.size(); ++i)
            {
                expect (lib.getByIndex (i) != nullptr, "slot " + juce::String (i) + " null");
                expect (lib.getName (i).isNotEmpty());
            }
        }

        beginTest ("every table reads finite, in-range samples across frames/freqs");
        {
            zw::WavetableLibrary lib;
            const double freqs[] = { 27.5, 220.0, 880.0, 4000.0, 16000.0 };
            const float  fps[]   = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
            for (int idx = 0; idx < lib.size(); ++idx)
            {
                const auto* wt = lib.getByIndex (idx);
                expect (wt != nullptr && ! wt->isEmpty(), "table " + juce::String (idx) + " empty");
                for (double f : freqs)
                    for (float fp : fps)
                        for (int i = 0; i < 256; ++i)
                        {
                            const float s = wt->getSample (fp, (float) i / 256.0f, f, sr);
                            expect (std::isfinite (s), "non-finite sample in table " + juce::String (idx));
                            expect (s >= -2.0f && s <= 2.0f, "out-of-range sample in table " + juce::String (idx));
                        }
            }
        }

        beginTest ("factory tables are spectrally distinct (no duplicates)");
        {
            zw::WavetableLibrary lib;
            const int factoryCount = lib.size() - 1;   // exclude the empty user slot

            std::vector<std::vector<float>> sigs;
            for (int i = 0; i < factoryCount; ++i)
                sigs.push_back (signatureOf (*lib.getByIndex (i), sr));

            for (int i = 0; i < factoryCount; ++i)
                for (int j = i + 1; j < factoryCount; ++j)
                {
                    float maxDiff = 0.0f;
                    for (size_t k = 0; k < sigs[(size_t) i].size(); ++k)
                        maxDiff = juce::jmax (maxDiff, std::abs (sigs[(size_t) i][k] - sigs[(size_t) j][k]));
                    expect (maxDiff > 0.02f,
                            "tables " + lib.getName (i) + " and " + lib.getName (j) + " are too similar");
                }
        }

        beginTest ("user slot falls back to a factory table before any import");
        {
            zw::WavetableLibrary lib;
            const auto* user = lib.getByIndex (lib.getUserIndex());
            expect (user != nullptr && ! user->isEmpty(), "user slot must be usable pre-import");
        }

        beginTest ("a synthetic .wav round-trips into the user slot");
        {
            // Reference single cycle: fundamental + half-amplitude 2nd harmonic.
            constexpr int N = zw::Wavetable::kFrameSize;
            std::vector<float> ref ((size_t) N, 0.0f);
            for (int i = 0; i < N; ++i)
            {
                const float ph = (float) i / (float) N;
                ref[(size_t) i] = std::sin (kTwoPi * ph) + 0.5f * std::sin (kTwoPi * 2.0f * ph);
            }

            // Write four identical cycles to a temp .wav (mono, 48 kHz).
            auto file = juce::File::createTempFile (".wav");
            {
                juce::AudioBuffer<float> buf (1, 4 * N);
                auto* d = buf.getWritePointer (0);
                for (int i = 0; i < 4 * N; ++i)
                    d[i] = ref[(size_t) (i % N)];

                juce::WavAudioFormat fmt;
                if (auto os = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
                {
                    if (auto* w = fmt.createWriterFor (os.get(), sr, 1, 16, {}, 0))
                    {
                        os.release();   // writer now owns the stream
                        std::unique_ptr<juce::AudioFormatWriter> writer (w);
                        writer->writeFromAudioSampleBuffer (buf, 0, 4 * N);
                    }
                }
            }

            zw::WavetableLibrary lib;
            const bool ok = lib.loadFromWav (file);
            expect (ok, "loadFromWav should succeed on a valid file");

            const auto* user = lib.getByIndex (lib.getUserIndex());
            expect (user != nullptr && ! user->isEmpty(), "user slot empty after import");

            // The imported cycle should be finite, non-trivial, and correlate with
            // the reference waveform (band-limiting/normalisation aside).
            std::vector<float> got ((size_t) N, 0.0f);
            float peak = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                got[(size_t) i] = user->getSample (0.5f, (float) i / (float) N, 55.0, sr);
                expect (std::isfinite (got[(size_t) i]));
                peak = juce::jmax (peak, std::abs (got[(size_t) i]));
            }
            expect (peak > 0.1f, "imported wavetable should not be silent");
            expectGreaterThan (correlation (got, ref), 0.7, "imported cycle should resemble the source");

            file.deleteFile();
        }

        beginTest ("loadFromWav rejects a missing file");
        {
            zw::WavetableLibrary lib;
            expect (! lib.loadFromWav (juce::File ("/nonexistent/zw_does_not_exist.wav")));
        }
    }
};

static WavetableLibraryTests wavetableLibraryTests;
}
