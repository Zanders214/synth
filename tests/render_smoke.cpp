// Headless sound check for the M2 voice engine: build the APVTS + Synthesiser,
// send a note, render ~1 s, and assert the output is non-silent and finite.
// Exercises the DSP path directly (no plugin wrapper) so it runs anywhere.

#include <juce_audio_utils/juce_audio_utils.h>

#include "Parameters.h"
#include "dsp/Wavetable.h"
#include "dsp/ParamRefs.h"
#include "dsp/ModMatrix.h"
#include "dsp/Voice.h"
#include "dsp/Arpeggiator.h"

#include <atomic>
#include <cstdio>
#include <cmath>

namespace
{
// Minimal AudioProcessor just so an APVTS can be constructed off the real layout.
struct DummyProcessor : public juce::AudioProcessor
{
    juce::AudioProcessorValueTreeState apvts;
    DummyProcessor() : apvts (*this, nullptr, "PARAMETERS", zw::createParameterLayout()) {}

    const juce::String getName() const override            { return "smoke"; }
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
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    DummyProcessor proc;
    zw::ParamRefs refs;
    refs.prepare (proc.apvts);

    zw::Wavetable wt;
    wt.generateBasicShapes (64);

    zw::ModMatrix matrix;                 // default routes
    std::atomic<double> bpm { 120.0 };
    std::atomic<double> lastNoteFreq { 440.0 };

    const double sr = 48000.0;
    const int    bs = 512;

    juce::Synthesiser synth;
    synth.addSound (new zw::ZWSound());
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new zw::ZWVoice (refs, wt, matrix, bpm, lastNoteFreq));
    synth.setCurrentPlaybackSampleRate (sr);

    juce::AudioBuffer<float> buffer (2, bs);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

    double sumSq = 0.0, peak = 0.0;
    long   samples = 0;
    const int blocks = (int) (sr / bs);   // ~1 second

    for (int b = 0; b < blocks; ++b)
    {
        buffer.clear();
        synth.renderNextBlock (buffer, midi, 0, bs);
        midi.clear();
        if (b == blocks / 2)              // release halfway through
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < bs; ++i)
            {
                const float s = buffer.getSample (ch, i);
                sumSq += (double) s * s;
                peak   = juce::jmax (peak, (double) std::abs (s));
                ++samples;
            }
    }

    const double rms = std::sqrt (sumSq / (double) juce::jmax ((long) 1, samples));
    std::printf ("ZandersWave render smoke: RMS=%.6f peak=%.6f\n", rms, peak);
    const bool soundOk = std::isfinite (rms) && std::isfinite (peak) && rms > 1.0e-4 && peak <= 4.0;

    // ---- Arpeggiator check: enable arp, hold a chord, count generated notes ----
    if (auto* pe = proc.apvts.getParameter (zw::id::arpEnable))
        pe->setValueNotifyingHost (1.0f);

    zw::Arpeggiator arp;
    arp.prepareParams (proc.apvts);
    arp.prepare (sr);

    int arpNoteOns = 0;
    for (int b = 0; b < 200; ++b)
    {
        juce::MidiBuffer mb;
        if (b == 0)   // hold a C major triad
        {
            mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            mb.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
            mb.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);
        }
        arp.process (mb, bs, 120.0);
        for (const auto meta : mb)
            if (meta.getMessage().isNoteOn())
                ++arpNoteOns;
    }
    std::printf ("Arpeggiator: %d note-ons over ~2s\n", arpNoteOns);
    const bool arpOk = arpNoteOns >= 4;

    const bool ok = soundOk && arpOk;
    std::printf ("%s\n", ok ? "PASS: synth produced sound + arp stepped"
                            : "FAIL: silent output or arp not stepping");
    return ok ? 0 : 1;
}
