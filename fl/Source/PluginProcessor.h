// PluginProcessor.h: VJ Link as an insert effect. Audio passes through untouched; its analysis goes to My Lil VJ over
// a loopMIDI port (channel 1), and what the VJ sends back (channel 2) drives the meters. Protocol: docs/PROTOCOL.md.
// Author: Ari Klopfer / A_Development · AGPL-3.0-or-later (links JUCE, see LICENSE)
#pragma once
#include "Analyzer.h"
#include "Protocol.h"
#include <array>
#include <atomic>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

class VjLinkProcessor : public juce::AudioProcessor, private juce::HighResolutionTimer, private juce::MidiInputCallback
{
public:
    VjLinkProcessor();
    ~VjLinkProcessor() override;

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "My lil VJ Link"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Opens the loopMIDI port by name, both directions. Empty name closes it. False if no output has that name.
    bool openPort (const juce::String& name);
    juce::String portName() const;
    static juce::String defaultPortName();   // first output named *lilvj*, else *loopMIDI*, else empty

    juce::AudioProcessorValueTreeState params;

    // Meter data: written on the audio and MIDI threads, read by the editor.
    std::array<std::atomic<float>, 5> toVj {};        // bass, mid, high, volume, pulse
    std::array<std::atomic<float>, 16> spectrum {};   // 0..1 per band
    std::array<std::atomic<float>, 16> fromVj {};     // by CC number 1..15
    std::atomic<double> lastFromVjMs { -1.0e9 };

    // Test hook (and the sender thread's input): moves the encoded, unsent messages into out.
    void drainEncoded (std::vector<vjlink::Msg>& out);

private:
    void hiResTimerCallback() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    vjlink::Analyzer analyzer;
    vjlink::Sender sender;
    vjlink::Receiver receiver;
    std::vector<vjlink::Msg> scratch, pending;
    std::atomic<bool> resend { false };
    juce::AbstractFifo fifo { 4096 };
    std::array<vjlink::Msg, 4096> queue {};
    juce::CriticalSection portLock;
    std::unique_ptr<juce::MidiOutput> midiOut;
    std::unique_ptr<juce::MidiInput> midiIn;
    juce::String openName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VjLinkProcessor)
};
