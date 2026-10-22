# name=My lil VJ Link
# url=https://github.com/Adev-io/my-lil-vj-link
"""FL Studio controller script for the lilvj loopMIDI port.

Options > MIDI settings > Input: enable lilvj and set its Controller type to "My lil VJ Link".
loopMIDI echoes every message to every listener, so FL also hears the plugin's own analysis on channel 1
(60 frames a second; CC 1 is bass, which FL treats as the mod wheel). This script drops channel 1, so Link to
controller and the focused instrument only see what My Lil VJ sends on channel 2.
Author: Ari Klopfer / A_Development. AGPL-3.0-or-later
"""

TO_VJ = 0  # channel 1, zero-based


def OnMidiMsg(event):
    if event.midiChan == TO_VJ:
        event.handled = True
