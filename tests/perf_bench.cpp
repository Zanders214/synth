// Performance micro-benchmarks for the DSP graph, the "SonarCloud for
// performance" data source. Mirrors the render_smoke.cpp harness (APVTS via
// DummyProcessor, Wavetable, ModMatrix, Synthesiser + 16 voices) and times three
// tiers in isolation so a regression points at a subsystem:
//
//   1. Full graph  — voices + FX rack + master gain (mirrors PluginProcessor.cpp:72-82)
//   2. Voice render — 16 sounding voices, no FX
//   3. FX chain     — all 10 slots enabled, over a pre-filled stereo block
//
// Results are written as github-action-benchmark "customSmallerIsBetter" JSON
// (argv[1], default bench_result.json): one entry per tier in ns/block, plus a
// derived real-time DSP-load %. At 48 kHz / 512 the per-block budget is 10.67 ms;
// load < 100% means the tier is real-time-capable.
//
// NOTE: this measures the directly-compilable DSP graph. The wrapper-only
// audio->UI taps in PluginProcessor.cpp:84-104 (scope ring, peak, voice count)
// are cheap and stable and are intentionally out of scope here.

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <juce_audio_utils/juce_audio_utils.h>

#include "Parameters.h"
#include "dsp/Wavetable.h"
#include "dsp/WavetableLibrary.h"
#include "dsp/ParamRefs.h"
#include "dsp/ModMatrix.h"
#include "dsp/Voice.h"
#include "dsp/fx/FxChain.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
// Minimal AudioProcessor so an APVTS can be built off the real parameter layout.
struct DummyProcessor : public juce::AudioProcessor
{
    juce::AudioProcessorValueTreeState apvts;
    DummyProcessor() : apvts (*this, nullptr, "PARAMETERS", zw::createParameterLayout()) {}

    const juce::String getName() const override            { return "perf"; }
    void prepareToPlay (double, int) override              {}
    void releaseResources() override                       {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override           { return 0.0; }
    bool acceptsMidi() const override                      { return true; }
    bool producesMidi() const override                     { return false; }
    juce::AudioProcessorEditor* createEditor() override    { return nullptr; }
    bool hasEditor() const override                        { return false; }
    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override   {}
};

void setParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float norm)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (norm);
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    const double sr = 48000.0;
    const int    bs = 512;

    DummyProcessor proc;
    zw::ParamRefs refs;
    refs.prepare (proc.apvts);

    zw::WavetableLibrary library;          // factory tables (default selection = 0)

    zw::ModMatrix matrix;                  // default routes
    std::atomic<double> bpm { 120.0 };
    std::atomic<double> lastNoteFreq { 440.0 };

    juce::Synthesiser synth;
    synth.addSound (new zw::ZWSound());
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new zw::ZWVoice (refs, library, matrix, bpm, lastNoteFreq));
    synth.setCurrentPlaybackSampleRate (sr);

    // FX rack with every slot enabled (worst case).
    for (const char* slot : { "hyper", "distort", "flanger", "phaser", "chorus",
                              "delay", "reverb", "comp", "eq", "filter" })
        setParam (proc.apvts, zw::id::fx (slot, "enable"), 1.0f);

    zw::FxChain fxChain;
    fxChain.prepareParams (proc.apvts);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
    fxChain.prepare (spec);
    fxChain.reset();

    juce::dsp::Gain<float> masterGain;
    masterGain.prepare (spec);
    masterGain.setRampDurationSeconds (0.02);
    masterGain.setGainLinear (0.5f);

    // Hold a 4-note chord so all tiers run with voices actually sounding.
    juce::MidiBuffer chord;
    for (int note : { 48, 55, 60, 64 })
        chord.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
    {
        juce::AudioBuffer<float> warm (2, bs);
        warm.clear();
        synth.renderNextBlock (warm, chord, 0, bs);   // trigger note-ons once
    }
    juce::MidiBuffer empty;

    juce::AudioBuffer<float> buffer (2, bs);

    ankerl::nanobench::Bench bench;
    bench.title ("ZandersWave DSP @48k/512").unit ("block").warmup (20).minEpochIterations (200);

    // Tier 1 — full graph: voices + FX + master gain.
    bench.run ("full_graph", [&] {
        buffer.clear();
        synth.renderNextBlock (buffer, empty, 0, bs);
        juce::dsp::AudioBlock<float> block (buffer);
        fxChain.process (block);
        masterGain.process (juce::dsp::ProcessContextReplacing<float> (block));
        ankerl::nanobench::doNotOptimizeAway (buffer.getReadPointer (0)[0]);
    });
    const double fullNs = bench.results().back().median (ankerl::nanobench::Result::Measure::elapsed) * 1.0e9;

    // Tier 2 — voice render only.
    bench.run ("voice_render", [&] {
        buffer.clear();
        synth.renderNextBlock (buffer, empty, 0, bs);
        ankerl::nanobench::doNotOptimizeAway (buffer.getReadPointer (0)[0]);
    });
    const double voiceNs = bench.results().back().median (ankerl::nanobench::Result::Measure::elapsed) * 1.0e9;

    // Tier 3 — FX chain only, over a fixed pre-filled stereo block.
    juce::AudioBuffer<float> fxSource (2, bs);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < bs; ++i)
            fxSource.setSample (ch, i, 0.25f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / (float) sr));

    bench.run ("fx_chain", [&] {
        buffer.makeCopyOf (fxSource);
        juce::dsp::AudioBlock<float> block (buffer);
        fxChain.process (block);
        ankerl::nanobench::doNotOptimizeAway (buffer.getReadPointer (0)[0]);
    });
    const double fxNs = bench.results().back().median (ankerl::nanobench::Result::Measure::elapsed) * 1.0e9;

    // ---- Emit github-action-benchmark "customSmallerIsBetter" JSON ----
    const double budgetNs = (double) bs / sr * 1.0e9;        // 10.67 ms at 48k/512
    const double fullLoad = fullNs / budgetNs * 100.0;

    struct Entry { const char* name; const char* unit; double value; };
    const std::vector<Entry> entries {
        { "Full graph (16 voices + 10 FX)", "ns/block", fullNs  },
        { "Full graph DSP load @48k/512",   "%",        fullLoad },
        { "Voice render (16 voices)",       "ns/block", voiceNs },
        { "FX chain (10 slots)",            "ns/block", fxNs    },
    };

    juce::String json = "[\n";
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        json << "  { \"name\": \"" << e.name << "\", \"unit\": \"" << e.unit
             << "\", \"value\": " << juce::String (e.value, 3) << " }"
             << (i + 1 < entries.size() ? "," : "") << "\n";
    }
    json << "]\n";

    const juce::String outName = (argc > 1) ? juce::String (argv[1]) : juce::String ("bench_result.json");
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (outName);
    out.replaceWithText (json);

    std::printf ("full=%.0f ns/block (%.1f%% RT load)  voices=%.0f  fx=%.0f\nwrote %s\n",
                 fullNs, fullLoad, voiceNs, fxNs, out.getFullPathName().toRawUTF8());
    return 0;
}
