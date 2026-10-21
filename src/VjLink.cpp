// VjLink.cpp: VJ Link, 20 HP. Sends this patch's audio analysis to My Lil VJ over a loopMIDI port (channel 1) and
// turns what the VJ sends back (channel 2) into CV and gates. Protocol: docs/PROTOCOL.md.
// Author: Ari Klopfer / A_Development · MIT
#include "Analyzer.h"
#include "Protocol.h"
#include "layout.hpp"
#include "plugin.hpp"

struct VjLink : Module
{
    enum ParamId { SENS_PARAM, PARAMS_LEN };
    enum InputId { IN_L_INPUT, IN_R_INPUT, INPUTS_LEN };
    enum OutputId
    {
        BASS_OUTPUT, MID_OUTPUT, HIGH_OUTPUT, VOL_OUTPUT, PULSE_OUTPUT, BEAT_OUTPUT,
        MOVE_OUTPUT, MOTX_OUTPUT, MOTY_OUTPUT, HANDX_OUTPUT, HANDY_OUTPUT, OPEN_OUTPUT,
        LFO1_OUTPUT, LFO2_OUTPUT, LFO3_OUTPUT, LFO4_OUTPUT, CLIP_OUTPUT, DECK_OUTPUT, OUTPUTS_LEN
    };
    enum LightId { BEAT_LIGHT, LINK_LIGHT, LIGHTS_LEN };

    midi::Output midiOut;
    midi::InputQueue midiIn;
    vjlink::Analyzer analyzer;
    vjlink::Sender sender;
    vjlink::Receiver receiver;
    std::vector<vjlink::Msg> msgs;
    dsp::PulseGenerator gates[3];   // beat, clip, deck, as sent by the VJ
    float preparedSr = 0.f;
    float sinceVj = 1e9f;           // seconds since the last channel 2 message
    vjlink::Frame shown;            // the meter reads this from the UI thread; a torn read only misdraws one frame

    VjLink()
    {
        config (PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam (SENS_PARAM, 0.25f, 2.f, 0.5f, "Beat threshold (higher = fewer beats)");
        configInput (IN_L_INPUT, "Audio left (or mono)");
        configInput (IN_R_INPUT, "Audio right (normalled to left)");
        const char* names[OUTPUTS_LEN] = {
            "VJ bass (0-10 V)", "VJ mid (0-10 V)", "VJ high (0-10 V)", "VJ volume (0-10 V)", "VJ beat envelope (0-10 V)", "VJ beat (gate)",
            "Camera motion energy (0-10 V)", "Camera motion X (0-10 V)", "Camera motion Y (0-10 V)",
            "Hand X (0-10 V)", "Hand Y (0-10 V)", "Hand openness (0-10 V)",
            "VJ LFO 1 (±5 V)", "VJ LFO 2 (±5 V)", "VJ LFO 3 (±5 V)", "VJ LFO 4 (±5 V)", "Clip change (gate)", "Deck switch (gate)" };
        for (int i = 0; i < OUTPUTS_LEN; ++i) configOutput (i, names[i]);
        msgs.reserve (64);
        fixChannels();
    }

    // The protocol fixes the channels, so the displays hide their channel rows: send on 1, listen on all.
    void fixChannels() { midiOut.setChannel (0); midiIn.setChannel (-1); }

    void send (const vjlink::Msg& m)
    {
        midi::Message out;   // 3 bytes by default
        out.bytes[0] = m.status; out.bytes[1] = m.data1; out.bytes[2] = m.data2;
        midiOut.sendMessage (out);
    }

    void process (const ProcessArgs& args) override
    {
        if (args.sampleRate != preparedSr) { preparedSr = args.sampleRate; analyzer.prepare (args.sampleRate); }
        analyzer.setSensitivity (params[SENS_PARAM].getValue());

        const float l = inputs[IN_L_INPUT].getVoltage();
        const float x = inputs[IN_R_INPUT].isConnected() ? 0.5f * (l + inputs[IN_R_INPUT].getVoltage()) : l;
        if (analyzer.push (x / 5.f))   // Rack audio is ±5 V
        {
            shown = analyzer.frame();
            msgs.clear();
            sender.encodeToVj (shown, msgs);
            for (const vjlink::Msg& m : msgs) send (m);
        }

        midi::Message in;
        while (midiIn.tryPop (&in, args.frame))
        {
            if (in.getSize() < 3) continue;
            if ((in.bytes[0] & 0x0F) == vjlink::kFromVj) sinceVj = 0.f;
            const int note = receiver.handle (in.bytes[0], in.bytes[1], in.bytes[2]);
            if (note >= vjlink::NOTE_BEAT && note <= vjlink::NOTE_DECK) gates[note - vjlink::NOTE_BEAT].trigger (1e-2f);
        }
        sinceVj += args.sampleTime;

        static const int levelOut[vjlink::kLevels] = {
            BASS_OUTPUT, MID_OUTPUT, HIGH_OUTPUT, VOL_OUTPUT, PULSE_OUTPUT, MOVE_OUTPUT, MOTX_OUTPUT, MOTY_OUTPUT,
            HANDX_OUTPUT, HANDY_OUTPUT, OPEN_OUTPUT, LFO1_OUTPUT, LFO2_OUTPUT, LFO3_OUTPUT, LFO4_OUTPUT };
        for (int cc = 1; cc <= vjlink::kLevels; ++cc)
        {
            const float v = receiver.values[cc];
            outputs[levelOut[cc - 1]].setVoltage (cc >= vjlink::CC_LFO1 ? 10.f * v - 5.f : 10.f * v);
        }
        static const int gateOut[3] = { BEAT_OUTPUT, CLIP_OUTPUT, DECK_OUTPUT };
        for (int i = 0; i < 3; ++i) outputs[gateOut[i]].setVoltage (gates[i].process (args.sampleTime) ? 10.f : 0.f);

        lights[BEAT_LIGHT].setBrightness (shown.pulse);
        lights[LINK_LIGHT].setBrightness (sinceVj < 0.5f ? 1.f : 0.f);
    }

    void onReset() override
    {
        midiOut.reset(); midiIn.reset(); fixChannels();
        sender.reset(); preparedSr = 0.f;
    }

    json_t* dataToJson() override
    {
        json_t* root = json_object();
        json_object_set_new (root, "midiOut", midiOut.toJson());
        json_object_set_new (root, "midiIn", midiIn.toJson());
        return root;
    }

    void dataFromJson (json_t* root) override
    {
        if (json_t* j = json_object_get (root, "midiOut")) midiOut.fromJson (j);
        if (json_t* j = json_object_get (root, "midiIn")) midiIn.fromJson (j);
        fixChannels();
        sender.reset();
    }
};

// Top half: what goes to the VJ (5 levels, then the 16 spectrum bands). Bottom half: the 15 levels coming back.
struct LinkMeter : LedDisplay
{
    VjLink* module = nullptr;

    void drawLayer (const DrawArgs& args, int layer) override
    {
        LedDisplay::drawLayer (args, layer);
        if (layer != 1 || ! module) return;
        NVGcontext* vg = args.vg;
        const float pad = 2.f, w = box.size.x - 2 * pad, half = box.size.y / 2.f, hh = half - 2 * pad;
        auto bar = [&] (float x, float bw, float y0, float v, NVGcolor c) {
            v = clamp (v, 0.f, 1.f);
            nvgBeginPath (vg);
            nvgRect (vg, x, y0 + hh * (1.f - v), bw, hh * v);
            nvgFillColor (vg, c);
            nvgFill (vg);
        };
        const NVGcolor lime = nvgRGB (0xc8, 0xff, 0x3a), dimLime = nvgRGB (0x6e, 0x8c, 0x20), magenta = nvgRGB (0xff, 0x3d, 0x9a);
        const vjlink::Frame& f = module->shown;
        const float levels[5] = { f.bass, f.mid, f.high, f.volume, f.pulse };
        const float cell = w / 21.f;
        for (int i = 0; i < 5; ++i) bar (pad + i * cell, cell * 0.8f, pad, levels[i], lime);
        for (int b = 0; b < vjlink::kSpecBands; ++b) bar (pad + (5 + b) * cell, cell * 0.8f, pad, f.spectrum[b] / 255.f, dimLime);
        const float cell2 = w / (float) vjlink::kLevels;
        for (int i = 0; i < vjlink::kLevels; ++i) bar (pad + i * cell2, cell2 * 0.8f, half + pad, module->receiver.values[i + 1], magenta);
    }
};

struct VjLinkWidget : ModuleWidget
{
    VjLinkWidget (VjLink* module)
    {
        namespace L = layout::vjlink;
        auto at = [] (const float* mm) { return mm2px (Vec (mm[0], mm[1])); };
        auto place = [] (Widget* w, const float* r) { w->box.pos = mm2px (Vec (r[0], r[1])); w->box.size = mm2px (Vec (r[2], r[3])); };
        setModule (module);
        setPanel (createPanel (asset::plugin (pluginInstance, "res/VjLink.svg")));
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        LinkMeter* meter = createWidget<LinkMeter> (Vec());
        place (meter, L::METER);
        meter->module = module;
        addChild (meter);

        MidiDisplay* out = createWidget<MidiDisplay> (Vec());
        place (out, L::OUT_MIDI);
        out->setMidiPort (module ? &module->midiOut : nullptr);
        out->deviceSeparator->hide(); out->channelChoice->hide();
        addChild (out);
        MidiDisplay* in = createWidget<MidiDisplay> (Vec());
        place (in, L::IN_MIDI);
        in->setMidiPort (module ? &module->midiIn : nullptr);
        in->deviceSeparator->hide(); in->channelChoice->hide();
        addChild (in);

        addInput (createInputCentered<PJ301MPort> (at (L::IN_L_INPUT), module, VjLink::IN_L_INPUT));
        addInput (createInputCentered<PJ301MPort> (at (L::IN_R_INPUT), module, VjLink::IN_R_INPUT));
        addParam (createParamCentered<Rogan1PSWhite> (at (L::SENS_PARAM), module, VjLink::SENS_PARAM));
        addChild (createLightCentered<MediumLight<LimeLight>> (at (L::BEAT_LIGHT), module, VjLink::BEAT_LIGHT));
        addChild (createLightCentered<MediumLight<MagentaLight>> (at (L::LINK_LIGHT), module, VjLink::LINK_LIGHT));

        const float* outs[VjLink::OUTPUTS_LEN] = {
            L::BASS_OUTPUT, L::MID_OUTPUT, L::HIGH_OUTPUT, L::VOL_OUTPUT, L::PULSE_OUTPUT, L::BEAT_OUTPUT,
            L::MOVE_OUTPUT, L::MOTX_OUTPUT, L::MOTY_OUTPUT, L::HANDX_OUTPUT, L::HANDY_OUTPUT, L::OPEN_OUTPUT,
            L::LFO1_OUTPUT, L::LFO2_OUTPUT, L::LFO3_OUTPUT, L::LFO4_OUTPUT, L::CLIP_OUTPUT, L::DECK_OUTPUT };
        for (int i = 0; i < VjLink::OUTPUTS_LEN; ++i) addOutput (createOutputCentered<PJ301MPort> (at (outs[i]), module, i));
    }
};

Model* modelVjLink = createModel<VjLink, VjLinkWidget> ("VJLink");
