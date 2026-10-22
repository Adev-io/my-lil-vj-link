// test.cpp: checks the analyser on synthetic audio and the protocol against the golden vectors in docs/PROTOCOL.md.
// Author: Ari Klopfer / A_Development · MIT
#include "Analyzer.h"
#include "Protocol.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <vector>

namespace
{
const double sr = 48000.0;
const double pi = 3.14159265358979323846;
int failures = 0;

void check (bool ok, const char* what, double got = 0.0)
{
    std::printf ("%s  %s  (%g)\n", ok ? "ok  " : "FAIL", what, got);
    if (! ok) ++failures;
}

// Feeds `seconds` of signal; returns the last frame and counts beats.
vjlink::Frame run (vjlink::Analyzer& a, double seconds, double (*sig) (double), int* beats = nullptr)
{
    vjlink::Frame last;
    const int n = (int) (seconds * sr);
    for (int i = 0; i < n; ++i)
        if (a.push ((float) sig (i / sr)))
        {
            last = a.frame();
            if (beats && last.beat) ++*beats;
        }
    return last;
}

double silence (double) { return 0.0; }
double low (double t) { return 0.5 * std::sin (2 * pi * 60 * t); }
double treble (double t) { return 0.5 * std::sin (2 * pi * 3000 * t); }
double full (double t) { return std::sin (2 * pi * 60 * t); }
double kick (double t, double start) { const double u = t - start; return u >= 0 && u < 0.08 ? 0.9 * std::exp (-u * 40) * std::sin (2 * pi * 60 * u) : 0.0; }
double kicks (double t) { double s = 0; for (int k = 0; k < 8; ++k) s += kick (t, 0.25 + 0.5 * k); return s; }
double flam (double t) { return kick (t, 0.25) + kick (t, 0.35); }

bool sameBytes (const std::vector<vjlink::Msg>& v, std::initializer_list<int> bytes)
{
    if (v.size() * 3 != bytes.size()) return false;
    size_t i = 0;
    for (int b : bytes)
    {
        const vjlink::Msg& m = v[i / 3];
        const int got = i % 3 == 0 ? m.status : i % 3 == 1 ? m.data1 : m.data2;
        if (got != b) return false;
        ++i;
    }
    return true;
}

} // namespace

int main()
{
    { // silence reads as nothing
        vjlink::Analyzer a; a.prepare (sr);
        int beats = 0;
        const vjlink::Frame f = run (a, 1.0, silence, &beats);
        check (f.bass == 0.f && f.mid == 0.f && f.high == 0.f && f.volume == 0.f, "silence: all bands zero", f.volume);
        check (beats == 0, "silence: no beats", beats);
    }
    { // a 60 Hz tone is bass
        vjlink::Analyzer a; a.prepare (sr);
        const vjlink::Frame f = run (a, 1.0, low);
        check (f.bass > 0.9f, "60 Hz: bass high", f.bass);
        check (f.mid < 0.5f, "60 Hz: mid low", f.mid);
        check (f.high < 0.5f, "60 Hz: high low", f.high);
    }
    { // a 3 kHz tone is high, and lands in spectrum band 13 (bins 113..159; 3 kHz = bin 128)
        vjlink::Analyzer a; a.prepare (sr);
        const vjlink::Frame f = run (a, 1.0, treble);
        check (f.high > 0.9f, "3 kHz: high high", f.high);
        check (f.bass < 0.5f, "3 kHz: bass low", f.bass);
        check (f.spectrum[13] == 255, "3 kHz: spectrum band 13 full", f.spectrum[13]);
        check (f.spectrum[0] < 64, "3 kHz: spectrum band 0 quiet", f.spectrum[0]);
    }
    { // 8 kicks at 120 BPM give 8 beats; a beat frame reads pulse 1 and the next 0.92
        vjlink::Analyzer a; a.prepare (sr);
        int beats = 0;
        bool sawOne = false, decayOk = false, prevBeat = false;
        for (int i = 0; i < (int) (4.25 * sr); ++i)
            if (a.push ((float) kicks (i / sr)))
            {
                const vjlink::Frame& f = a.frame();
                if (f.beat) { ++beats; sawOne = sawOne || f.pulse == 1.f; }
                if (prevBeat && ! f.beat) decayOk = std::fabs (f.pulse - 0.92f) < 1e-6f;
                prevBeat = f.beat;
            }
        check (beats == 8, "kick train: 8 beats", beats);
        check (sawOne, "beat frame: pulse = 1");
        check (decayOk, "frame after a beat: pulse = 0.92");
    }
    { // two kicks 100 ms apart: the 250 ms lockout keeps one
        vjlink::Analyzer a; a.prepare (sr);
        int beats = 0;
        run (a, 0.5, flam, &beats);
        check (beats == 1, "flam inside lockout: 1 beat", beats);
    }
    { // frames come at 60 per second
        vjlink::Analyzer a; a.prepare (sr);
        int frames = 0;
        for (int i = 0; i < (int) sr; ++i) frames += a.push (0.f) ? 1 : 0;
        check (frames == 60, "60 frames per second", frames);
    }
    { // ring order: an impulse 800 samples old sits mid-window (oldest sample first), not on the window's zero edge
        vjlink::Analyzer a; a.prepare (sr);
        bool ready = a.push (1.f);
        for (int i = 1; i < 800; ++i) ready = a.push (0.f);
        check (ready, "frame lands on sample 800 at 48 kHz");
        check (a.fftBytes()[10] > 90, "impulse windowed mid-window, oldest sample first", a.fftBytes()[10]);
    }
    { // input trim: a full-scale tone pins the VJ's -30 dB ceiling; -20 dB brings it back under; 0 dB is the identity
        auto peak = [] (const vjlink::Analyzer& a) -> int {
            int p = 0;
            for (int i = 0; i < vjlink::kBins; ++i) p = std::max (p, (int) a.fftBytes()[i]);
            return p;
        };
        vjlink::Analyzer a; a.prepare (sr);
        run (a, 0.5, full);
        check (peak (a) == 255, "trim 0 dB: a full-scale tone pins the ceiling", peak (a));
        a.setTrimDb (-20.f);
        run (a, 0.5, full);
        const int trimmed = peak (a);
        check (trimmed > 200 && trimmed < 255, "trim -20 dB: the peak drops under the ceiling", trimmed);
        a.setTrimDb (0.f);
        run (a, 0.5, full);
        check (peak (a) == 255, "trim back to 0 dB: the identity again", peak (a));
    }

    // ---- protocol golden vectors (docs/PROTOCOL.md)
    {
        vjlink::Sender s; std::vector<vjlink::Msg> v;
        s.level (vjlink::kToVj, vjlink::CC_BASS, 1.f, v);
        check (sameBytes (v, { 0xB0, 0x01, 0x7F, 0xB0, 0x21, 0x7F }), "level ch1 CC1 = 1.0");
        v.clear(); s.level (vjlink::kToVj, vjlink::CC_BASS, 0.5f, v);
        check (sameBytes (v, { 0xB0, 0x01, 0x40, 0xB0, 0x21, 0x00 }), "level ch1 CC1 = 0.5");
        v.clear(); s.level (vjlink::kFromVj, vjlink::CC_LFO1, 0.25f, v);
        check (sameBytes (v, { 0xB1, 0x0C, 0x20, 0xB1, 0x2C, 0x00 }), "level ch2 CC12 = 0.25");
        v.clear(); s.band (13, 255, v);
        check (sameBytes (v, { 0xB0, 0x73, 0x7F }), "spectrum band 13 = 255");
        v.clear(); s.level (vjlink::kToVj, vjlink::CC_BASS, 0.5f, v);
        check (v.empty(), "unchanged level sends nothing", (double) v.size());
        v.clear(); s.band (13, 254, v);
        check (v.empty(), "a band change below 7-bit resolution sends nothing", (double) v.size());
    }
    { // a whole frame: 5 levels (10 msgs) + 16 bands the first time, nothing the second, everything again at frame 60
        vjlink::Sender s; vjlink::Frame f; f.bass = 0.3f; f.spectrum[2] = 200;
        std::vector<vjlink::Msg> v;
        s.encodeToVj (f, v);
        check (v.size() == 26, "first frame sends everything", (double) v.size());
        size_t later = 0;
        for (int i = 2; i <= 59; ++i) { v.clear(); s.encodeToVj (f, v); later += v.size(); }
        check (later == 0, "frames 2..59 send nothing new", (double) later);
        v.clear(); s.encodeToVj (f, v);
        check (v.size() == 26, "frame 60 resends everything", (double) v.size());
    }
    {
        vjlink::Receiver r;
        r.handle (0xB1, 0x06, 0x40);
        check (std::fabs (r.values[vjlink::CC_MOTION_E] - 8192 / 16383.f) < 1e-6f, "MSB alone: coarse value", r.values[6]);
        r.handle (0xB1, 0x26, 0x00);
        check (std::fabs (r.values[vjlink::CC_MOTION_E] - 8192 / 16383.f) < 1e-6f, "decode B1 06 40, B1 26 00", r.values[6]);
        r.handle (0xB1, 0x26, 0x7F);
        check (std::fabs (r.values[vjlink::CC_MOTION_E] - (64 * 128 + 127) / 16383.f) < 1e-6f, "LSB refines the MSB", r.values[6]);
        r.handle (0xB1, 0x70, 0x7F);
        check (std::fabs (r.values[vjlink::CC_MOTION_E] - (64 * 128 + 127) / 16383.f) < 1e-6f, "spectrum CC on the receive side is ignored", r.values[6]);
        r.handle (0xB1, 0x06, 0x7F); r.handle (0xB1, 0x26, 0x7F);
        check (r.values[vjlink::CC_MOTION_E] == 1.f, "14-bit full scale = 1.0", r.values[6]);
        r.handle (0xB0, 0x06, 0x00);
        check (r.values[vjlink::CC_MOTION_E] == 1.f, "the other channel is ignored", r.values[6]);
        check (r.handle (0x91, vjlink::NOTE_CLIP, 127) == vjlink::NOTE_CLIP, "note-on ch2 returns the note");
        check (r.handle (0x91, vjlink::NOTE_CLIP, 0) == -1, "note-on velocity 0 is a note-off");
        check (r.handle (0x90, vjlink::NOTE_CLIP, 127) == -1, "note-on ch1 ignored");
    }

    std::printf (failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
