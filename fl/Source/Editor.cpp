// Editor.cpp: see Editor.h.
// Author: Ari Klopfer / A_Development · AGPL-3.0-or-later (links JUCE, see LICENSE)
#include "Editor.h"

namespace
{
const juce::Colour ink (0xff16161a), lime (0xffc8ff3a), dimLime (0xff6e8c20), magenta (0xffff3d9a), dim (0xff8d8d96);
}

VjLinkEditor::VjLinkEditor (VjLinkProcessor& p)
    : AudioProcessorEditor (p), proc (p), trimAttach (p.params, "trim", trim), sensAttach (p.params, "sens", sens)
{
    portLabel.setText ("loopMIDI port", juce::dontSendNotification);
    sensLabel.setText ("Beat threshold", juce::dontSendNotification);
    trimLabel.setText ("Input trim", juce::dontSendNotification);
    for (juce::Slider* s : { &trim, &sens })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    }
    port.setTextWhenNothingSelected ("no port: make one named lilvj in loopMIDI, then Rescan");
    port.onChange = [this] {
        const auto name = port.getText();
        if (! proc.openPort (name)) port.setText ("could not open " + name, juce::dontSendNotification);
    };
    rescan.onClick = [this] { refreshPorts(); };
    for (juce::Component* c : { (juce::Component*) &portLabel, (juce::Component*) &port, (juce::Component*) &rescan,
                                (juce::Component*) &trimLabel, (juce::Component*) &trim,
                                (juce::Component*) &sensLabel, (juce::Component*) &sens })
        addAndMakeVisible (c);
    refreshPorts();
    setSize (460, 290);
    startTimerHz (30);
}

void VjLinkEditor::refreshPorts()
{
    port.clear (juce::dontSendNotification);
    int id = 1;
    for (const auto& d : juce::MidiOutput::getAvailableDevices()) port.addItem (d.name, id++);
    const auto cur = proc.portName();
    for (int i = 0; i < port.getNumItems(); ++i)
        if (port.getItemText (i) == cur) port.setSelectedItemIndex (i, juce::dontSendNotification);
}

void VjLinkEditor::paint (juce::Graphics& g)
{
    g.fillAll (ink);
    g.setColour (lime);
    g.fillRect (0, 0, getWidth(), 28);
    g.setColour (ink);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText ("MY LIL VJ LINK", 10, 0, 300, 28, juce::Justification::centredLeft);
    const bool alive = juce::Time::getMillisecondCounterHiRes() - proc.lastFromVjMs.load() < 500.0;
    g.setColour (alive ? magenta : dim);
    g.fillEllipse ((float) getWidth() - 24.f, 8.f, 12.f, 12.f);

    auto bars = [&g] (juce::Rectangle<float> r, int count, auto value, auto colour) {
        const float w = r.getWidth() / (float) count;
        for (int i = 0; i < count; ++i)
        {
            const float v = juce::jlimit (0.f, 1.f, value (i));
            g.setColour (colour (i));
            g.fillRect (r.getX() + (float) i * w + 1.f, r.getBottom() - v * r.getHeight(), w - 2.f, v * r.getHeight());
        }
    };
    auto area = meterArea.toFloat();
    const auto top = area.removeFromTop (area.getHeight() / 2.f).reduced (0.f, 4.f);
    bars (top, 21, [this] (int i) { return i < 5 ? proc.toVj[(size_t) i].load() : proc.spectrum[(size_t) (i - 5)].load(); },
          [] (int i) { return i < 5 ? lime : dimLime; });
    bars (area.reduced (0.f, 4.f), vjlink::kLevels, [this] (int i) { return proc.fromVj[(size_t) (i + 1)].load(); },
          [] (int) { return magenta; });
    g.setColour (dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("TO VJ  /  FROM VJ", meterArea.getX(), meterArea.getY() - 14, 200, 12, juce::Justification::left);
}

void VjLinkEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    r.removeFromTop (28);
    auto row = r.removeFromTop (24);
    portLabel.setBounds (row.removeFromLeft (100));
    rescan.setBounds (row.removeFromRight (70));
    port.setBounds (row.reduced (4, 0));
    r.removeFromTop (6);
    row = r.removeFromTop (24);
    trimLabel.setBounds (row.removeFromLeft (100));
    trim.setBounds (row);
    r.removeFromTop (6);
    row = r.removeFromTop (24);
    sensLabel.setBounds (row.removeFromLeft (100));
    sens.setBounds (row);
    r.removeFromTop (18);
    meterArea = r;
}
