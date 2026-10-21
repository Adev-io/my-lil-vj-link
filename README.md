# My lil VJ Link

Two-way audio-reactive bridge between your DAW and the **My Lil VJ** visuals app, over one virtual MIDI port.

- **DAW → VJ:** the plugin analyses your track the same way My Lil VJ analyses its own input (bass, mid, high,
  volume, beat, 16-band spectrum) and sends it over MIDI. In My Lil VJ, pick **Link (DAW)** as the audio source,
  and every audio-reactive LFO, binding and shader follows your mix.
- **VJ → DAW:** My Lil VJ sends back its own levels, camera motion, hand tracking, four VJ LFOs, and beat, clip
  change and deck switch events. In VCV Rack they come out as CV and gates; in FL Studio you map them with
  Link to controller.

Two plugins, one protocol ([docs/PROTOCOL.md](docs/PROTOCOL.md)):

| | Where | Licence |
|---|---|---|
| VCV Rack 2 module **VJ Link** | repo root | MIT |
| VST3 **My lil VJ Link** (FL Studio and other VST3 hosts, Windows) | [`fl/`](fl) | AGPL-3.0-or-later (it links JUCE) |

## Setup (Windows)

1. Install [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) and create one port named `lilvj`.
2. My Lil VJ (Studio): Audio Reactivity → **Link (DAW)**. The LINK row reads "DAW connected" while audio plays.
3. Your DAW:
   - **VCV Rack:** add VJ Link. Set TO VJ and FROM VJ to `lilvj`. Patch audio into IN L (and IN R).
   - **FL Studio:** put My lil VJ Link on the mixer track you want the visuals to follow (the master is fine). It
     picks the `lilvj` port by itself. For VJ → FL: Options → MIDI settings, enable `lilvj` as an input, then
     right-click any knob → Link to controller and move the VJ source you want (channel 2).

## VCV Rack module

| Jack | What |
|---|---|
| IN L, IN R | audio to analyse (R normalled to L) |
| BASS, MID, HIGH, VOL, PULSE | the VJ's own levels, 0–10 V |
| MOVE, MOT X, MOT Y | camera motion energy and position, 0–10 V |
| HAND X, HAND Y, OPEN | hand position and openness, 0–10 V |
| LFO 1–4 | the VJ's four LFOs, ±5 V |
| BEAT, CLIP, DECK | 10 ms gates: VJ beat, clip change, deck switch |

BEAT THR sets the beat threshold for the audio you send (higher = fewer beats). The BEAT light flashes on your beats;
LINK is lit while the VJ is talking back. The display shows what goes out (lime) and what comes back (magenta).

## Build

Rack: MSYS2 MINGW64 with the Rack SDK, `export RACK_DIR=/path/to/Rack-SDK`, then `make test` and `make install`.
FL plugin: see [`fl/INSTALL.txt`](fl/INSTALL.txt); `fl\build.bat` builds it and runs the smoke test.

By Ari Klopfer / A_Development.
