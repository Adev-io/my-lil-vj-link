// test.cpp: checks the analyser on synthetic audio and the protocol against the golden vectors in docs/PROTOCOL.md.
// Author: Ari Klopfer / A_Development · MIT
#include "Analyzer.h"
#include <cmath>
#include <cstdio>
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
double kick (double t, double start) { const double u = t - start; return u >= 0 && u < 0.08 ? 0.9 * std::exp (-u * 40) * std::sin (2 * pi * 60 * u) : 0.0; }
double kicks (double t) { double s = 0; for (int k = 0; k < 8; ++k) s += kick (t, 0.25 + 0.5 * k); return s; }
double flam (double t) { return kick (t, 0.25) + kick (t, 0.35); }

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

    std::printf (failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
