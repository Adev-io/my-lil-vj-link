// Analyzer.h: My Lil VJ's audio analysis, re-implemented from its documented behaviour (design spec §3).
// Push mono samples (full scale ±1); every 1/60 s a Frame holds the numbers the VJ's own analyser would produce.
// Author: Ari Klopfer / A_Development · MIT
#pragma once
#include "Fft.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vjlink
{
static const int kFftSize = 2048;
static const int kBins = 256;       // the VJ only reads the first 256 bins
static const int kSpecBands = 16;

// 17 edges over the VJ's 512-bin FFT row; band k covers bins [edge k, edge k+1).
inline int specEdge (int k)
{
    static const int e[kSpecBands + 1] = { 0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 57, 80, 113, 160, 256, 512 };
    return e[k];
}

struct Frame
{
    float bass = 0.f, mid = 0.f, high = 0.f, volume = 0.f, pulse = 0.f;   // 0..1, the VJ's __vjAudio fields
    bool beat = false;                                                     // a beat fired on this frame
    uint8_t spectrum[kSpecBands] = {};                                     // peak byte per band
};

class Analyzer
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        hop = std::max (1, (int) std::lround (sr / 60.0));
        untilHop = hop;
        ring.assign (kFftSize, 0.f);
        pos = 0;
        buf.assign (kFftSize, std::complex<double> (0.0, 0.0));
        win.resize (kFftSize);
        for (int i = 0; i < kFftSize; ++i)
        {
            const double a = 2.0 * 3.14159265358979323846 * i / kFftSize;
            win[i] = 0.42 - 0.5 * std::cos (a) + 0.08 * std::cos (2.0 * a);
        }
        smooth.assign (kFftSize / 2, 0.0);
        std::fill (bytes, bytes + kFftSize / 2, (uint8_t) 0);
        prev.assign (kBins, 0.f);
        fluxRing.assign (60, 0.f);
        fluxIdx = 0;
        maxBass = maxMid = maxHigh = maxVol = 0.1f;
        pulseV = 0.f;
        sinceBeat = 1e9;
        bassMax = std::max (1, std::min (kBins - 2, (int) std::floor (250.0 * kFftSize / sr)));
        midMax = std::max (bassMax + 1, std::min (kBins - 1, (int) std::floor (2000.0 * kFftSize / sr)));
        out = Frame();
    }

    // Beat threshold, as the VJ's sensitivity: higher means fewer beats.
    void setSensitivity (float s) { sens = std::min (2.f, std::max (0.25f, s)); }

    // Input trim in dB, applied before the analysis (0 dB = as is). A mixed song at Rack's ±5 V or an FL master near
    // 0 dBFS pins most bins at the VJ's -30 dB ceiling and flattens the bands; about -20 dB brings the motion back.
    void setTrimDb (float db)
    {
        if (db == trimDb) return;
        trimDb = db;
        gain = std::pow (10.f, db / 20.f);
    }

    // One mono sample in. True when a new frame is ready in frame().
    bool push (float x)
    {
        ring[(size_t) pos] = x * gain;
        pos = (pos + 1) % kFftSize;
        sinceBeat += 1.0;
        if (--untilHop > 0) return false;
        untilHop = hop;
        analyse();
        return true;
    }

    const Frame& frame() const { return out; }
    const uint8_t* fftBytes() const { return bytes; }   // the 1024 analyser bytes, for tests
    double sampleRate() const { return sr; }

private:
    void analyse()
    {
        for (int i = 0; i < kFftSize; ++i)   // oldest sample first
            buf[(size_t) i] = std::complex<double> (ring[(size_t) ((pos + i) % kFftSize)] * win[(size_t) i], 0.0);
        fft (buf);
        for (int k = 0; k < kFftSize / 2; ++k)
        {
            smooth[(size_t) k] = 0.3 * smooth[(size_t) k] + 0.7 * std::abs (buf[(size_t) k]) / kFftSize;
            const double db = smooth[(size_t) k] > 0.0 ? 20.0 * std::log10 (smooth[(size_t) k]) : -1000.0;
            bytes[k] = (uint8_t) std::max (0.0, std::min (255.0, std::floor ((db + 100.0) * 255.0 / 70.0)));
        }

        int pB = 0, pM = 0, pH = 0;
        double sV = 0.0;
        for (int i = 0; i <= bassMax; ++i) pB = std::max (pB, (int) bytes[i]);
        for (int i = bassMax + 1; i <= midMax; ++i) pM = std::max (pM, (int) bytes[i]);
        for (int i = midMax + 1; i < kBins; ++i) pH = std::max (pH, (int) bytes[i]);
        for (int i = 0; i < kBins; ++i) sV += bytes[i];
        const float bR = pB / 255.f, mR = pM / 255.f, hR = pH / 255.f, vR = (float) (sV / kBins / 255.0);
        maxBass = std::max (std::max (maxBass * 0.9995f, bR), 0.1f);
        maxMid = std::max (std::max (maxMid * 0.9995f, mR), 0.1f);
        maxHigh = std::max (std::max (maxHigh * 0.9995f, hR), 0.1f);
        maxVol = std::max (std::max (maxVol * 0.9995f, vR), 0.1f);
        out.bass = std::min (1.f, bR / maxBass);
        out.mid = std::min (1.f, mR / maxMid);
        out.high = std::min (1.f, hR / maxHigh);
        out.volume = std::min (1.f, vR / maxVol);

        float flux = 0.f;
        for (int i = 0; i < kBins; ++i)
        {
            const float f = bytes[i] / 255.f, d = f - prev[(size_t) i];
            if (d > 0.f) flux += d;
            prev[(size_t) i] = f;
        }
        fluxRing[(size_t) fluxIdx] = flux;
        fluxIdx = (fluxIdx + 1) % (int) fluxRing.size();
        float mean = 0.f;
        for (float v : fluxRing) mean += v;
        mean /= (float) fluxRing.size();
        pulseV *= 0.92f;   // decay before the trigger, so a beat frame reads exactly 1
        out.beat = false;
        if (flux > 0.25f && flux > mean * (1.f + sens) && sinceBeat > 0.25 * sr)
        {
            pulseV = 1.f;
            sinceBeat = 0.0;
            out.beat = true;
        }
        out.pulse = pulseV;

        for (int b = 0; b < kSpecBands; ++b)
        {
            int pk = 0;
            for (int i = specEdge (b); i < specEdge (b + 1); ++i) pk = std::max (pk, (int) bytes[i]);
            out.spectrum[b] = (uint8_t) pk;
        }
    }

    double sr = 0.0, sinceBeat = 1e9;
    int hop = 1, untilHop = 1, pos = 0, fluxIdx = 0, bassMax = 1, midMax = 2;
    float sens = 0.5f, maxBass = 0.1f, maxMid = 0.1f, maxHigh = 0.1f, maxVol = 0.1f, pulseV = 0.f, trimDb = 0.f, gain = 1.f;
    std::vector<float> ring, prev, fluxRing;
    std::vector<double> win, smooth;
    std::vector<std::complex<double>> buf;
    uint8_t bytes[kFftSize / 2] = {};
    Frame out;
};
} // namespace vjlink
