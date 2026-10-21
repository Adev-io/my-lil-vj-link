# VJ Link protocol

How the VJ Link plugins (VCV Rack module, FL Studio VST3) and My Lil VJ talk over one virtual MIDI port.
Any tool that speaks MIDI can join in: this page is the whole contract.

## Port and channels

- One virtual MIDI port, e.g. a loopMIDI port. Name it so it contains `lilvj` (case does not matter); the plugins and
  My Lil VJ pick such a port automatically.
- loopMIDI hands every message to every program that has the port open, including the sender, so each direction
  has its own channel and every side ignores its own channel when receiving.
  - **Channel 1:** DAW → VJ
  - **Channel 2:** VJ → DAW

## Levels: 14-bit control change

A level is a number from 0 to 1, sent as `q = round(v × 16383)`: first CC n with `q >> 7`, then CC n+32 with
`q & 127`. A receiver applies the first byte alone as `msb × 128 / 16383`, so a plain 7-bit controller on CC n
still works, and refines the value when CC n+32 arrives.

| CC (LSB CC) | Channel 1, DAW → VJ | Channel 2, VJ → DAW |
|---|---|---|
| 1 (33) | bass | VJ bass |
| 2 (34) | mid | VJ mid |
| 3 (35) | high | VJ high |
| 4 (36) | volume | VJ volume |
| 5 (37) | pulse (beat envelope) | VJ pulse |
| 6 (38) | | camera motion energy |
| 7 (39) | | camera motion x |
| 8 (40) | | camera motion y |
| 9 (41) | | hand x |
| 10 (42) | | hand y |
| 11 (43) | | hand openness |
| 12–15 (44–47) | | VJ LFO 1–4; the LFO runs −1..1 and is sent as (v + 1) / 2 |
| 102–117 | spectrum band 0–15, one 7-bit CC each (byte >> 1) | |

## Events: notes on channel 2

Note-on with velocity 127, note-off one frame later.

| Note | Event |
|---|---|
| 36 | beat |
| 37 | clip change |
| 38 | deck switch |

## Timing

- DAW side: 60 frames per second. VJ side: 30 frames per second.
- Only levels that changed are sent. Once a second (every 60th DAW frame, every 30th VJ frame) everything is sent
  again, so a program that opens the port late catches up within a second.

## What the DAW side measures

The plugins reproduce My Lil VJ's own analyser, so a band means the same thing whichever side measured it:

- mono input (left + right) / 2, full scale ±1; every `round(sampleRate / 60)` samples, the last 2048 samples with a
  Blackman window go through an FFT; magnitude `|X[k]| / 2048`, smoothed `0.3 × previous + 0.7 × new`, then
  `floor((20·log10(m) + 100) × 255 / 70)` clamped to a byte (−100 dB → 0, −30 dB → 255)
- bass, mid, high: the loudest byte in 0–250 Hz, 250–2000 Hz, 2000 Hz up to bin 255; volume: the mean of bins 0–255
- each divided by its own running maximum (decays × 0.9995 per frame, never below 0.1), capped at 1
- beat: spectral flux (sum of positive byte/255 changes over bins 0–255) above 0.25 and above the 60-frame mean
  × (1 + threshold), at least 0.25 s after the last beat; pulse jumps to 1 and decays × 0.92 per frame;
  threshold 0.25–2, default 0.5
- spectrum: the loudest byte in each of 16 bands over bins `0,1,2,3,5,7,10,14,20,28,40,57,80,113,160,256,512`

## Golden vectors

| Message | Bytes (hex) |
|---|---|
| channel 1, CC 1 = 1.0 | `B0 01 7F`, `B0 21 7F` |
| channel 1, CC 1 = 0.5 | `B0 01 40`, `B0 21 00` |
| channel 2, CC 12 = 0.25 | `B1 0C 20`, `B1 2C 00` |
| channel 1, spectrum band 13 = byte 255 | `B0 73 7F` |
| `B1 06 40` then `B1 26 00` | motion energy = 8192 / 16383 |
