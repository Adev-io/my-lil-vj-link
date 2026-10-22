// PluginProcessor.cpp: see PluginProcessor.h.
// Author: Ari Klopfer / A_Development · AGPL-3.0-or-later (links JUCE, see LICENSE)
#include "PluginProcessor.h"
#include "Editor.h"

static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "sens", 1 }, "Beat threshold",
                                                        juce::NormalisableRange<float> (0.25f, 2.f), 0.5f));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "trim", 1 }, "Input trim",
                                                        juce::NormalisableRange<float> (-40.f, 20.f), -20.f,
                                                        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    return l;
}

VjLinkProcessor::VjLinkProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "VjLink", makeLayout())
{
    analyzer.prepare (44100.0);
    scratch.reserve (64);
    pending.reserve (4096);
    openPort (defaultPortName());
}

VjLinkProcessor::~VjLinkProcessor() { openPort ({}); }

void VjLinkProcessor::prepareToPlay (double sampleRate, int)
{
    analyzer.prepare (sampleRate);
    resend = true;
}

bool VjLinkProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && l.getMainInputChannelSet() == out;
}

void VjLinkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (resend.exchange (false)) sender.reset();
    analyzer.setSensitivity (params.getRawParameterValue ("sens")->load());
    analyzer.setTrimDb (params.getRawParameterValue ("trim")->load());
    const int chans = juce::jmin (2, buffer.getNumChannels());
    if (chans == 0) return;
    const float* l = buffer.getReadPointer (0);
    const float* r = buffer.getReadPointer (chans - 1);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        if (! analyzer.push (0.5f * (l[i] + r[i]))) continue;
        const vjlink::Frame& f = analyzer.frame();
        const float levels[5] = { f.bass, f.mid, f.high, f.volume, f.pulse };
        for (size_t k = 0; k < 5; ++k) toVj[k] = levels[k];
        for (size_t b = 0; b < 16; ++b) spectrum[b] = f.spectrum[b] / 255.f;
        scratch.clear();
        sender.encodeToVj (f, scratch);
        // ponytail: a full FIFO (sender thread stalled) drops the tail of a frame; the 1 s resend repairs the VJ's view
        const auto w = fifo.write ((int) scratch.size());
        for (int k = 0; k < w.blockSize1; ++k) queue[(size_t) (w.startIndex1 + k)] = scratch[(size_t) k];
        for (int k = 0; k < w.blockSize2; ++k) queue[(size_t) (w.startIndex2 + k)] = scratch[(size_t) (w.blockSize1 + k)];
    }
}

void VjLinkProcessor::drainEncoded (std::vector<vjlink::Msg>& out)
{
    const auto r = fifo.read (fifo.getNumReady());
    for (int k = 0; k < r.blockSize1; ++k) out.push_back (queue[(size_t) (r.startIndex1 + k)]);
    for (int k = 0; k < r.blockSize2; ++k) out.push_back (queue[(size_t) (r.startIndex2 + k)]);
}

void VjLinkProcessor::hiResTimerCallback()   // its own thread, every 16 ms: MIDI I/O never runs on the audio thread
{
    pending.clear();
    drainEncoded (pending);
    const juce::ScopedLock sl (portLock);
    if (! midiOut) return;
    for (const auto& m : pending) midiOut->sendMessageNow (juce::MidiMessage (m.status, m.data1, m.data2));
}

void VjLinkProcessor::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (m.getRawDataSize() < 3) return;
    const juce::uint8* d = m.getRawData();
    if ((d[0] & 0x0F) != vjlink::kFromVj) return;   // our own channel 1, echoed back by loopMIDI
    receiver.handle (d[0], d[1], d[2]);
    for (int cc = 1; cc <= vjlink::kLevels; ++cc) fromVj[(size_t) cc] = receiver.values[cc];
    lastFromVjMs = juce::Time::getMillisecondCounterHiRes();
}

bool VjLinkProcessor::openPort (const juce::String& name)
{
    stopTimer();   // waits for a running callback, which takes portLock
    const juce::ScopedLock sl (portLock);
    if (midiIn) midiIn->stop();
    midiIn.reset();
    midiOut.reset();
    openName = {};
    if (name.isEmpty()) return true;
    for (const auto& d : juce::MidiOutput::getAvailableDevices())
        if (d.name == name) midiOut = juce::MidiOutput::openDevice (d.identifier);
    if (! midiOut) return false;
    for (const auto& d : juce::MidiInput::getAvailableDevices())
        if (d.name == name) midiIn = juce::MidiInput::openDevice (d.identifier, this);
    if (midiIn) midiIn->start();
    openName = name;
    resend = true;   // a new listener needs every value
    startTimer (16);
    return true;
}

juce::String VjLinkProcessor::portName() const
{
    const juce::ScopedLock sl (portLock);
    return openName;
}

juce::String VjLinkProcessor::defaultPortName()
{
    const auto outs = juce::MidiOutput::getAvailableDevices();
    for (const auto& d : outs) if (d.name.containsIgnoreCase ("lilvj")) return d.name;
    for (const auto& d : outs) if (d.name.containsIgnoreCase ("loopMIDI")) return d.name;
    return {};
}

void VjLinkProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = params.copyState();
    state.setProperty ("port", portName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void VjLinkProcessor::setStateInformation (const void* data, int size)
{
    const auto xml = getXmlFromBinary (data, size);
    if (! xml) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    const juce::String port = state.getProperty ("port").toString();
    state.removeProperty ("port", nullptr);
    params.replaceState (state);
    openPort (port);
}

juce::AudioProcessorEditor* VjLinkProcessor::createEditor() { return new VjLinkEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VjLinkProcessor(); }
