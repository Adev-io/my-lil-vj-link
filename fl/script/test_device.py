"""Self-check for device_MyLilVJLink.py with stand-in events (FL's own modules are not needed).
Run: py -3.11 -I fl/script/test_device.py
Author: Ari Klopfer / A_Development. AGPL-3.0-or-later
"""
import importlib.util
import os
import types

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('device', os.path.join(here, 'device_MyLilVJLink.py'))
device = importlib.util.module_from_spec(spec)
spec.loader.exec_module(device)


def event(chan):
    return types.SimpleNamespace(midiChan=chan, handled=False)


e = event(0)
device.OnMidiMsg(e)
assert e.handled, 'channel 1 (the plugin echo) must be dropped'
e = event(1)
device.OnMidiMsg(e)
assert not e.handled, 'channel 2 (from the VJ) must reach FL'
with open(os.path.join(here, 'device_MyLilVJLink.py'), encoding='utf-8') as f:
    assert f.readline().strip() == '# name=My lil VJ Link', 'FL lists the script by its first-line name'
print('ok')
