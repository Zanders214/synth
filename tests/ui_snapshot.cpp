// Headless UI snapshot: instantiate the processor + editor, lay it out, render
// to a PNG. Lets the UI layout be inspected without opening a window.

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    ZandersWaveAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (ed == nullptr) { std::fprintf (stderr, "no editor\n"); return 1; }

    const int w = 1320, h = 900;
    ed->setSize (w, h);
    ed->setBounds (0, 0, w, h);

    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), false, 1.0f);

    juce::File out = (argc > 1)
        ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
        : juce::File::getCurrentWorkingDirectory().getChildFile ("ui_snapshot.png");
    out.deleteFile();

    juce::PNGImageFormat png;
    if (auto os = std::unique_ptr<juce::FileOutputStream> (out.createOutputStream()))
    {
        png.writeImageToStream (img, *os);
        std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
        return 0;
    }
    std::fprintf (stderr, "could not write %s\n", out.getFullPathName().toRawUTF8());
    return 1;
}
