// Editor.h: port picker, input trim, beat threshold, link LED and the same meters as the Rack panel.
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
    juce::Label portLabel, trimLabel, sensLabel;
    juce::ComboBox port;
    juce::TextButton rescan { "Rescan" };
    juce::Slider trim, sens;
    juce::AudioProcessorValueTreeState::SliderAttachment trimAttach, sensAttach;
    juce::Rectangle<int> meterArea;
};
