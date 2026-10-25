#!/usr/bin/env python3
"""panel.py: writes res/VjLink.svg and src/layout.hpp from one layout table.

Labels become paths (DejaVu Sans from the Rack install, or the font named by RACK_FONT), so the SVG has no <text>,
as the VCV panel guide asks. The widget reads the same table through layout.hpp, so art and components cannot drift.
Run from anywhere: py -3.11 tools/panel.py   (needs fontTools).
Author: Ari Klopfer / A_Development. MIT
"""
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

H = 128.5
W = 5.08 * 20
INK, INK2 = '#16161a', '#202027'
LIME, MAGENTA, WHITE, DIM = '#c8ff3a', '#ff3d9a', '#f2f2f4', '#8d8d96'
FONT = os.environ.get('RACK_FONT', r'C:\Program Files\VCV\Rack2Free\res\fonts\DejaVuSans.ttf')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# kinds: knob Rogan1PS, light MediumLight, in (lime ring), out (magenta pad)
RADIUS = {'knob': 6.7, 'light': 1.5, 'in': 4.0, 'out': 4.0}
PAD_R = 4.8
LABEL_SIZE = {'knob': 2.6, 'light': 2.4, 'in': 2.4, 'out': 2.3}

font = TTFont(FONT)
glyphs, cmap, upm = font.getGlyphSet(), font.getBestCmap(), font['head'].unitsPerEm


def text(s, x, y, size, fill, anchor='middle', stroke=0.0):
    """One <path> for the string: baseline at y, em size in mm, optional stroke for a bolder face."""
    k = size / upm
    width = sum(glyphs[cmap[ord(c)]].width for c in s) * k
    pen_x = x - width / 2 if anchor == 'middle' else x - width if anchor == 'end' else x
    d = []
    for c in s:
        g = glyphs[cmap[ord(c)]]
        pen = SVGPathPen(glyphs)
        g.draw(TransformPen(pen, (k, 0, 0, -k, pen_x, y)))
        d.append(pen.getCommands())
        pen_x += g.width * k
    bold = ' stroke="%s" stroke-width="%g" stroke-linejoin="round"' % (fill, stroke) if stroke else ''
    return '<path fill="%s"%s d="%s"/>' % (fill, bold, ' '.join(d))


# Rectangles the widget fills with displays: x, y, w, h in mm.
FACE = (W - 14.46, 4.3, 3.1)   # x, y, r of the mascot in the title band (clear of the right screw)
RECTS = {
    'METER': (4, 11, W - 8, 29),
    'OUT_MIDI': (4, 46, 45.3, 17),
    'IN_MIDI': (W - 4 - 45.3, 46, 45.3, 17),
}

C = (10.5, 26.6, 42.7, 58.8, 74.9, 91.0)   # output columns
ROWS = (86.5, 100, 113.5)
OUT_LABELS = (
    ('BASS', 'MID', 'HIGH', 'VOL', 'PULSE', 'BEAT'),
    ('MOVE', 'MOT X', 'MOT Y', 'HAND X', 'HAND Y', 'OPEN'),
    ('LFO 1', 'LFO 2', 'LFO 3', 'LFO 4', 'CLIP', 'DECK'),
)
OUT_IDS = (
    ('BASS', 'MID', 'HIGH', 'VOL', 'PULSE', 'BEAT'),
    ('MOVE', 'MOTX', 'MOTY', 'HANDX', 'HANDY', 'OPEN'),
    ('LFO1', 'LFO2', 'LFO3', 'LFO4', 'CLIP', 'DECK'),
)
LINK = [
    ('IN_L_INPUT', 'in', 10, 70, 'IN L'), ('IN_R_INPUT', 'in', 22, 70, 'IN R'),
    ('TRIM_PARAM', 'knob', 37, 70, 'TRIM'),
    ('SENS_PARAM', 'knob', 53, 70, 'BEAT THR'),
    ('BEAT_LIGHT', 'light', 70, 70, 'BEAT'), ('LINK_LIGHT', 'light', 86, 70, 'LINK'),
] + [(OUT_IDS[r][c] + '_OUTPUT', 'out', C[c], ROWS[r], OUT_LABELS[r][c]) for r in range(3) for c in range(6)]


def mascot(cx, cy, r):
    """The My lil face: magenta disc, dot eyes, a flat mouth (the widget draws the live mouth over it)."""
    e = r * 0.37
    return ''.join([
        '<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (cx, cy, r, MAGENTA),
        '<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (cx - e, cy - r * 0.32, r * 0.145, INK),
        '<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (cx + e, cy - r * 0.32, r * 0.145, INK),
        '<path d="M %g %g h %g" fill="none" stroke="%s" stroke-width="%g" stroke-linecap="round"/>'
        % (cx - r * 0.58, cy + r * 0.45, r * 1.16, INK, r * 0.16),
    ])


def panel():
    out = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="%gmm" height="%gmm" viewBox="0 0 %g %g">' % (W, H, W, H),
        '<rect width="%g" height="%g" fill="%s"/>' % (W, H, INK),
        '<rect width="%g" height="8.6" fill="%s"/>' % (W, LIME),
        text('MY LIL VJ LINK', W / 2, 6.3, 4.3, INK, stroke=0.39),
        mascot(*FACE),
        '<rect x="3.5" y="80.5" width="%g" height="42" rx="2" fill="%s"/>' % (W - 7, INK2),   # what the VJ sends back
        text('TO VJ', RECTS['OUT_MIDI'][0] + RECTS['OUT_MIDI'][2] / 2, 44.6, 2.6, LIME, stroke=0.2),
        text('FROM VJ', RECTS['IN_MIDI'][0] + RECTS['IN_MIDI'][2] / 2, 44.6, 2.6, MAGENTA, stroke=0.2),
    ]
    rowmax = {}
    for _, kind, x, y, label in LINK:
        if kind not in ('out', 'light'):
            rowmax[y] = max(rowmax.get(y, 0), RADIUS[kind])
    for _, kind, x, y, label in LINK:
        if kind == 'in':
            out.append('<circle cx="%g" cy="%g" r="%g" fill="none" stroke="%s" stroke-width="0.6"/>' % (x, y, RADIUS[kind] + 1.3, LIME))
        elif kind == 'out':
            out.append('<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (x, y, PAD_R, MAGENTA))
        reach = PAD_R if kind == 'out' else rowmax.get(y, RADIUS[kind])
        colour = MAGENTA if kind == 'out' else WHITE
        out.append(text(label, x, y + reach + 2.8, LABEL_SIZE[kind], colour, stroke=0.18))
    out.append(text('A_DEVELOPMENT', W / 2, 125.6, 2.4, DIM))
    out.append('</svg>')
    return '\n'.join(out) + '\n'


def header():
    lines = ['// layout.hpp: generated by tools/panel.py, do not edit. Component centres and display rects in mm.',
             '#pragma once', 'namespace layout {', 'namespace vjlink {']
    lines += ['constexpr float %s[2] = {%.2ff, %.2ff};' % (i, x, y) for i, _, x, y, _ in LINK]
    lines += ['constexpr float %s[4] = {%.2ff, %.2ff, %.2ff, %.2ff};' % ((k,) + r) for k, r in RECTS.items()]
    lines += ['constexpr float FACE[3] = {%.2ff, %.2ff, %.2ff};' % FACE]
    lines += ['}', '}', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    os.makedirs(os.path.join(ROOT, 'res'), exist_ok=True)
    files = {os.path.join('res', 'VjLink.svg'): panel(), os.path.join('src', 'layout.hpp'): header()}
    for rel, body in files.items():
        with open(os.path.join(ROOT, rel), 'w', encoding='utf-8') as f:
            f.write(body)
    print('wrote', ', '.join(files), '(%d components)' % len(LINK))
