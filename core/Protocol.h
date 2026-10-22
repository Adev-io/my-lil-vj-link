// Protocol.h: the VJ Link MIDI map (docs/PROTOCOL.md). One loopMIDI port; channel 1 carries DAW -> VJ,
// channel 2 carries VJ -> DAW. Levels are 14-bit CC (MSB on CC n, LSB on CC n+32).
// Author: Ari Klopfer / A_Development · MIT
#pragma once
#include "Analyzer.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace vjlink
{
static const uint8_t kToVj = 0;     // channel 1 (status low nibble)
static const uint8_t kFromVj = 1;   // channel 2

enum Cc
{
    CC_BASS = 1, CC_MID, CC_HIGH, CC_VOLUME, CC_PULSE,                          // both directions
    CC_MOTION_E, CC_MOTION_X, CC_MOTION_Y, CC_HAND_X, CC_HAND_Y, CC_HAND_OPEN,   // VJ -> DAW
    CC_LFO1, CC_LFO2, CC_LFO3, CC_LFO4                                           // VJ -> DAW, bipolar sent as (v+1)/2
};
static const int kLevels = 15;      // CC 1..15
static const int kSpecCc = 102;     // CC 102..117: spectrum band 0..15, 7-bit (DAW -> VJ)
enum Note { NOTE_BEAT = 36, NOTE_CLIP = 37, NOTE_DECK = 38 };   // VJ -> DAW gates

struct Msg { uint8_t status, data1, data2; };

// Encodes levels, sending only what changed; a reset (or every 60th frame) resends everything.
class Sender
{
public:
    Sender() { reset(); }
    void reset()
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i) sent[c][i] = -1;
    }

    void level (uint8_t ch, int cc, float v, std::vector<Msg>& out)
    {
        const int q = (int) std::lround (std::min (1.f, std::max (0.f, v)) * 16383.f);
        int& last = sent[ch & 1][cc];
        if (q == last) return;
        last = q;
        out.push_back (Msg { (uint8_t) (0xB0 | ch), (uint8_t) cc, (uint8_t) (q >> 7) });
        out.push_back (Msg { (uint8_t) (0xB0 | ch), (uint8_t) (cc + 32), (uint8_t) (q & 127) });
    }

    void band (int b, uint8_t byte, std::vector<Msg>& out)
    {
        const int v = byte >> 1;
        int& last = sent[kToVj][kSpecCc + b];
        if (v == last) return;
        last = v;
        out.push_back (Msg { (uint8_t) (0xB0 | kToVj), (uint8_t) (kSpecCc + b), (uint8_t) v });
    }

    // One analysis frame, DAW -> VJ.
    void encodeToVj (const Frame& f, std::vector<Msg>& out)
    {
        if (++frames >= 60) { frames = 0; reset(); }   // once a second at 60 frames/s, so a late listener catches up
        level (kToVj, CC_BASS, f.bass, out);
        level (kToVj, CC_MID, f.mid, out);
        level (kToVj, CC_HIGH, f.high, out);
        level (kToVj, CC_VOLUME, f.volume, out);
        level (kToVj, CC_PULSE, f.pulse, out);
        for (int b = 0; b < kSpecBands; ++b) band (b, f.spectrum[b], out);
    }

private:
    int sent[2][128];
    long frames = 0;
};

// Decodes one channel. values[cc] is 0..1 for CC 1..15 (values[0] unused).
class Receiver
{
public:
    uint8_t channel = kFromVj;
    float values[kLevels + 1] = {};

    // Returns the note number of a note-on on this channel, else -1.
    int handle (uint8_t status, uint8_t d1, uint8_t d2)
    {
        if ((status & 0x0F) != channel) return -1;
        const uint8_t kind = status & 0xF0;
        if (kind == 0x90 && d2 > 0) return d1;
        if (kind != 0xB0) return -1;
        if (d1 >= 1 && d1 <= kLevels)
        {
            msb[d1] = d2;
            values[d1] = d2 * 128 / 16383.f;   // a 7-bit sender still works
        }
        else if (d1 >= 33 && d1 <= 32 + kLevels)
            values[d1 - 32] = (msb[d1 - 32] * 128 + d2) / 16383.f;
        return -1;
    }

private:
    uint8_t msb[kLevels + 1] = {};
};
} // namespace vjlink
