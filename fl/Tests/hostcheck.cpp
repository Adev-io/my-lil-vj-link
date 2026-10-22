// hostcheck.cpp: loads the built "My lil VJ Link.vst3" the way a DAW does and plays it kicks over a bass line in real
// time, so the plugin's own sender thread talks on the real loopMIDI port. Record the port meanwhile to see what
// arrives. Not a ctest: it needs the port. Usage: MyLilVJLinkHostCheck "<path to My lil VJ Link.vst3>" [seconds]
// Author: Ari Klopfer / A_Development · AGPL-3.0-or-later (links JUCE, see LICENSE)
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: MyLilVJLinkHostCheck <My lil VJ Link.vst3> [seconds]\n"); return 2; }
    juce::ScopedJuceInitialiser_GUI gui;
    const double sr = 48000.0, seconds = argc > 2 ? std::atof (argv[2]) : 5.0, pi = juce::MathConstants<double>::pi;
    const int block = 512;

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile (found, juce::String::fromUTF8 (argv[1]));
    if (found.isEmpty()) { std::printf ("FAIL  no VST3 plugin in %s\n", argv[1]); return 1; }
    juce::String error;
    std::unique_ptr<juce::AudioPluginInstance> plugin = format.createInstanceFromDescription (*found[0], sr, block, error);
    if (! plugin) { std::printf ("FAIL  could not load: %s\n", error.toRawUTF8()); return 1; }
    std::printf ("ok    loaded %s %s\n", found[0]->name.toRawUTF8(), found[0]->version.toRawUTF8());
    for (auto* p : plugin->getParameters())
        std::printf ("      %s = %s\n", p->getName (64).toRawUTF8(), p->getCurrentValueAsText().toRawUTF8());

    plugin->setPlayConfigDetails (2, 2, sr, block);
    plugin->prepareToPlay (sr, block);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    const double start = juce::Time::getMillisecondCounterHiRes();
    const int blocks = (int) (seconds * sr / block);
    long t = 0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < block; ++i, ++t)
        {
            const double s = (double) t / sr, u = std::fmod (s, 0.5);   // a kick every 0.5 s over a 110 Hz bass
            const double kick = u < 0.08 ? 0.9 * std::exp (-u * 40.0) * std::sin (2.0 * pi * 60.0 * u) : 0.0;
            const float x = (float) (kick + 0.2 * std::sin (2.0 * pi * 110.0 * s));
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
        }
        plugin->processBlock (buf, midi);
        const double due = start + (b + 1) * 1000.0 * block / sr;   // real time, so the 16 ms sender keeps pace
        while (juce::Time::getMillisecondCounterHiRes() < due) juce::Thread::sleep (1);
    }
    plugin->releaseResources();
    std::printf ("ok    played %.1f s\n", seconds);
    return 0;
}