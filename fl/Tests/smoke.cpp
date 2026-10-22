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

// Feeds `seconds` of a 60 Hz sine at amplitude `amp` and returns the loudest spectrum band (0..1) afterwards.
float loudestBand (VjLinkProcessor& p, float amp, double seconds)
{
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    std::vector<vjlink::Msg> sink;
    static long t = 0;
    for (int block = 0; block < (int) (seconds * 48000.0 / 512.0); ++block)
    {
        for (int i = 0; i < 512; ++i, ++t)
        {
            const float x = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * (double) t / 48000.0);
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
        }
        p.processBlock (buf, midi);
        sink.clear();
        p.drainEncoded (sink);
    }
    float m = 0.f;
    for (const auto& b : p.spectrum) m = juce::jmax (m, b.load());
    return m;
}

void setParam (VjLinkProcessor& p, const char* id, float v)
{
    auto* prm = p.params.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}} // namespace

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

    {
        VjLinkProcessor r;
        r.openPort ({});
        r.prepareToPlay (48000.0, 512);
        check (std::abs (r.params.getRawParameterValue ("trim")->load() + 20.f) < 1e-3f, "input trim defaults to -20 dB");
        setParam (r, "trim", 0.f);
        check (loudestBand (r, 1.f, 0.5) == 1.f, "trim 0 dB: a full-scale tone fills the spectrum");
        setParam (r, "trim", -20.f);
        const float trimmed = loudestBand (r, 1.f, 0.5);
        check (trimmed < 1.f && trimmed > 0.75f, "trim -20 dB: the same tone stays under full");
    }
    setParam (p, "sens", 1.5f);
    setParam (p, "trim", -12.f);
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    VjLinkProcessor q;
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    check (std::abs (q.params.getRawParameterValue ("sens")->load() - 1.5f) < 1e-3f, "state keeps the beat threshold");
    check (std::abs (q.params.getRawParameterValue ("trim")->load() + 12.f) < 1e-3f, "state keeps the input trim");
    check (q.portName().isEmpty(), "state keeps the (closed) port");
    std::printf (failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
