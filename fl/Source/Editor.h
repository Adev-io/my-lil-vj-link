// Editor.h: port picker, beat threshold, link LED and the same meters as the Rack panel.
// Author: Ari Klopfer / A_Development · AGPL-3.0-or-later (links JUCE, see LICENSE)
#pragma once
#include "PluginProcessor.h"

class VjLinkEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VjLinkEditor (VjLinkProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }
    void refreshPorts();

    VjLinkProcessor& proc;
    juce::Label portLabel, sensLabel;
    juce::ComboBox port;
    juce::TextButton rescan { "Rescan" };
    juce::Slider sens;
    juce::AudioProcessorValueTreeState::SliderAttachment sensAttach;
    juce::Rectangle<int> meterArea;
};
