// smoke.cpp: drives the real processor with audio (no host, no MIDI port) and checks what it would send.
// Author: Ari Klopfer / A_Development · AGPL-3.0-or-later (links JUCE, see LICENSE)
#include "PluginProcessor.h"
#include <cmath>
#include <cstdio>

namespace
{
int failures = 0;
void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}
bool has (const std::vector<vjlink::Msg>& v, int s, int d1, int d2)
{
    for (const auto& m : v)
        if (m.status == s && m.data1 == d1 && m.data2 == d2) return true;
    return false;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    VjLinkProcessor p;
    p.openPort ({});   // no port: messages stay queued for drainEncoded
    p.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    std::vector<vjlink::Msg> sent;
    bool passthrough = true;
    long t = 0;
    for (int block = 0; block < 94; ++block)   // about 1 s
    {
        for (int i = 0; i < 512; ++i, ++t)
        {
            const float x = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * (double) t / 48000.0);
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
        }
        juce::AudioBuffer<float> copy (buf);
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i) passthrough = passthrough && buf.getSample (c, i) == copy.getSample (c, i);
        p.drainEncoded (sent);
    }
    check (passthrough, "audio passes through untouched");
    check (has (sent, 0xB0, 1, 127) && has (sent, 0xB0, 33, 127), "a loud 60 Hz tone sends bass = 1.0 on channel 1");
    check (p.toVj[0].load() > 0.9f, "bass meter reads the tone");
    bool spec = false;
    for (const auto& m : sent) spec = spec || (m.status == 0xB0 && m.data1 >= 102 && m.data1 < 118);
    check (spec, "spectrum bands are sent");

    auto* sens = p.params.getParameter ("sens");
    sens->setValueNotifyingHost (sens->convertTo0to1 (1.5f));
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    VjLinkProcessor q;
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    check (std::abs (q.params.getRawParameterValue ("sens")->load() - 1.5f) < 1e-3f, "state keeps the beat threshold");
    check (q.portName().isEmpty(), "state keeps the (closed) port");

    std::printf (failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
