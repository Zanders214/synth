// Real-time safety check for the audio callback.
//
// Built with clang -fsanitize=realtime (see the ZW_RT_SANITIZE CMake option),
// which — together with the ZW_RT_NONBLOCKING ([[clang::nonblocking]]) attribute
// on the DSP hot paths (ZWVoice::renderNextBlock, FxChain::process) — aborts if
// any allocation, lock or syscall is reached on the audio thread. This driver
// instantiates the real plugin (like ui_snapshot.cpp), prepares it, and pumps
// processBlock with live MIDI so the voice and FX render paths actually execute.
// A clean run exits 0; an RT-safety violation aborts with a stack trace under
// RTSAN_OPTIONS=halt_on_error=1.

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

#include <cstdio>

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    ZandersWaveAudioProcessor proc;

    const double sr = 48000.0;
    const int    bs = 512;
    proc.prepareToPlay (sr, bs);

    juce::AudioBuffer<float> buffer (2, bs);
    const int blocks = (int) (sr / bs) * 2;   // ~2 seconds of callbacks

    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0)                                       // hold a chord
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);
        }
        else if (b == blocks / 2)                         // release halfway
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
        }

        buffer.clear();
        proc.processBlock (buffer, midi);                 // [[clang::nonblocking]]
    }

    std::printf ("PASS: %d processBlock calls with no real-time-safety violations\n", blocks);
    return 0;
}
