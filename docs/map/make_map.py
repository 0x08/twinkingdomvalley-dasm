#!/usr/bin/env python3
"""Draws the map of Twin Kingdom Valley from the game's own data
(tkvdata.json, made by tkvdata.py) and the layout in layout.py, as
map.html, and prints it to map.pdf with headless Chromium.

    python3 tkvdata.py && python3 make_map.py
"""
import html, json, math, os, shutil, subprocess, sys

import layout

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = json.load(open(os.path.join(HERE, 'tkvdata.json')))
LOC = {l['id']: l for l in DATA['locations']}

U, V = 150, 104          # Pixels per column and row
BW, BH = 124, 70         # Box size
PAD = 70                 # Around each panel
GAP = 50                 # Between panels

VEC = {'N': (0, -1), 'S': (0, 1), 'E': (1, 0), 'W': (-1, 0)}
WORDS = {'N': 'N', 'S': 'S', 'E': 'E', 'W': 'W', 'U': 'up', 'D': 'down'}


def esc(s):
    return html.escape(s, quote=True)


def dirlabel(d):
    flat = ''.join(c for c in d if c in 'NSEW')
    vert = [WORDS[c] for c in d if c in 'UD']
    if flat and vert:
        return flat + '+' + vert[0]
    return flat or vert[0]


def dirvec(d):
    x = sum(VEC[c][0] for c in d if c in VEC)
    y = sum(VEC[c][1] for c in d if c in VEC)
    if x == 0 and y == 0:
        x, y = (0.7, -0.7) if 'U' in d else (-0.7, 0.7)
    n = math.hypot(x, y)
    return x / n, y / n


def short(i):
    if i in layout.NAMES:
        return layout.NAMES[i]
    n = LOC[i]['name'].split(' , ')[0].replace(', paths lead', '')
    for p in ('wandering through ', 'wandering up ', 'inside ', 'locked up in ', 'close by ',
              'in ', 'on ', 'at ', 'by ', 'to '):
        if n.startswith(p):
            n = n[len(p):]
            break
    for a in ('a ', 'an ', 'the '):
        if n.startswith(a):
            n = n[len(a):]
            break
    return n[0].upper() + n[1:]


# What's where at the start: objects lying around, and characters with what they carry
CARRIED = 0xC8
things = {}
carry = {}
for o in DATA['objects']:
    label = o['name'].split(' ', 1)[1] if o['name'].split(' ', 1)[0] in ('a', 'an', 'the', 'some') else o['name']
    if o['value']:
        label += ' ★'
    if o['at'] < 0xB1:
        things.setdefault(o['at'], []).append(label)
    elif o['at'] > CARRIED:
        carry.setdefault(o['at'] - CARRIED, []).append(label)
people = {}
for c in DATA['characters'][1:]:
    name = c['name'].split(' ', 1)[1] if c['name'].split(' ', 1)[0] in ('a', 'an', 'the') else c['name']
    if c['id'] in carry:
        name += ' (' + ', '.join(carry[c['id']]) + ')'
    people.setdefault(c['at'], []).append(name)


def tally(items):
    out, seen = [], {}
    for it in items:
        seen[it] = seen.get(it, 0) + 1
    for it, n in seen.items():
        plural = {'elf': 'elves', 'knife': 'knives'}.get(it, it + 's')
        out.append(it if n == 1 else '%d %s' % (n, plural) if not it.endswith(')') else '%d × %s' % (n, it))
    return out


# Panels: where each location's box goes in page pixels
panel_of = {}
for key, pos in layout.LAYOUT.items():
    for i in pos:
        panel_of[i] = key

def bounds(key):
    pos = layout.LAYOUT[key].values()
    caps = layout.CAPTIONS.get(key, [])
    xs = [p[0] for p in pos] + [c[0] for c in caps]
    ys = [p[1] for p in pos] + [c[1] for c in caps]
    return min(xs), min(ys), max(xs), max(ys)

# Page arrangement: the valley on the left; underground and castle on the right
geo = {}
def place(key, ox, oy):
    x0, y0, x1, y1 = bounds(key)
    w = (x1 - x0) * U + BW + 2 * PAD
    h = (y1 - y0) * V + BH + 2 * PAD + 40
    geo[key] = dict(ox=ox, oy=oy, x0=x0, y0=y0, w=w, h=h)
    return w, h

MARGIN = 60
TITLE_H = 150
vw, vh = place('valley', MARGIN, MARGIN + TITLE_H)
uw, uh = place('under', MARGIN + vw + GAP, MARGIN + TITLE_H)
cw, ch = place('castle', MARGIN + vw + GAP + uw + GAP, MARGIN + TITLE_H)

def centre(i):
    g = geo[panel_of[i]]
    c, r = layout.LAYOUT[panel_of[i]][i]
    return (g['ox'] + PAD + BW / 2 + (c - g['x0']) * U,
            g['oy'] + PAD + 40 + BH / 2 + (r - g['y0']) * V)

boxes = {i: centre(i) for i in panel_of}


def clip(cx, cy, dx, dy, w=BW / 2, h=BH / 2):
    # Where a ray from a box's centre leaves the box
    tx = w / abs(dx) if dx else 1e9
    ty = h / abs(dy) if dy else 1e9
    t = min(tx, ty)
    return cx + dx * t, cy + dy * t


def hits(points, skip):
    for i, (bx, by) in boxes.items():
        if i in skip:
            continue
        for x, y in points:
            if abs(x - bx) < BW / 2 + 6 and abs(y - by) < BH / 2 + 6:
                return True
    return False


def route(a, b):
    """A path from box a to box b that misses the other boxes: straight if
    it can be, otherwise a curve. Returns (start, control, end, tangents)."""
    (ax, ay), (bx, by) = boxes[a], boxes[b]
    dx, dy = bx - ax, by - ay
    n = math.hypot(dx, dy)
    ux, uy = dx / n, dy / n
    px, py = -uy, ux
    for k in [0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6]:
        mx, my = (ax + bx) / 2 + px * k * 34, (ay + by) / 2 + py * k * 34
        # Leave and arrive along the tangent to the curve
        s = clip(ax, ay, mx - ax, my - ay)
        e = clip(bx, by, mx - bx, my - by)
        pts = [((1 - t) ** 2 * s[0] + 2 * (1 - t) * t * mx + t * t * e[0],
                (1 - t) ** 2 * s[1] + 2 * (1 - t) * t * my + t * t * e[1]) for t in [j / 24 for j in range(1, 24)]]
        if not hits(pts, (a, b)) or k == -6:
            return s, (mx, my), e
    return s, (mx, my), e


def angle_ok(d, s, c):
    # Does the line leave in the direction the game gives? Then it needs no label
    if any(ch in 'UD' for ch in d):
        return False
    vx, vy = dirvec(d)
    tx, ty = c[0] - s[0], c[1] - s[1]
    n = math.hypot(tx, ty) or 1
    return (vx * tx + vy * ty) / n > math.cos(math.radians(28))


svg = []
labels = []      # Drawn last, over the lines
def text(x, y, s, cls, anchor='middle'):
    labels.append('<text x="%.1f" y="%.1f" class="%s" text-anchor="%s">%s</text>' % (x, y, cls, anchor, esc(s)))

def end_label(d, s, c, t=None, side=1):
    tx, ty = c[0] - s[0], c[1] - s[1]
    n = math.hypot(tx, ty) or 1
    x, y = s[0] + tx / n * 15, s[1] + ty / n * 15
    if t is not None and math.hypot(t[0] - s[0], t[1] - s[1]) < 75:
        # A short line: both labels by its middle, one each side
        x = (s[0] + t[0]) / 2 - ty / n * 24 * side
        y = (s[1] + t[1]) / 2 + tx / n * 24 * side
    labels.append('<g class="dl"><rect x="%.1f" y="%.1f" width="%.1f" height="13" rx="3"/>'
                  '<text x="%.1f" y="%.1f" text-anchor="middle">%s</text></g>'
                  % (x - 3.4 * len(dirlabel(d)) - 4, y - 7, 6.8 * len(dirlabel(d)) + 8, x, y + 3.5, esc(dirlabel(d))))


def door_note(e):
    t = e.get('text', '')
    if e.get('lockable'):
        keys = [k for k in e['keys'] if k != 'master']
        t += ': ' + (' or '.join(keys) + ' key' if keys else 'master key')
        if not e['open']:
            t = 'locked ' + t
    return t


IMPASSABLE_NOTES = {
    (0x93, 0xAE): 'rock fall: opens when you bring the treasure chest',
}


def style(e, back):
    """Line class and note for an exit (and the one coming back, if any)"""
    for x in (e, back):
        if x and x.get('secret'):
            return 'secret', (x.get('text') or 'secret door') + (': ' + door_note(x).split(': ')[1] if x.get('lockable') else '') + ', once you know'
    for x in (e, back):
        if x and x.get('lockable'):
            return 'door', door_note(x)
    if not e['open'] and not e.get('lockable'):
        return 'blocked', e.get('text', 'blocked')
    return 'path', None


drawn = set()
stub_count = {}

def stub(a, e, text_):
    """An exit to another panel, or somewhere the game moves you on from"""
    sx, sy = boxes[a]
    vx, vy = dirvec(e['dir'])
    s = clip(sx, sy, vx, vy)
    k = stub_count.get((a, e['dir']), 0)
    stub_count[(a, e['dir'])] = k + 1
    ex, ey = s[0] + vx * 26, s[1] + vy * 26
    cls = 'stub' + (' blocked' if not e['open'] and not e.get('lockable') else '')
    svg.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" class="%s" marker-end="url(#arrow)"/>' % (s[0], s[1], ex, ey, cls))
    anchor = 'start' if vx > 0.3 else 'end' if vx < -0.3 else 'middle'
    lx, ly = ex + vx * 6, ey + vy * 6 + (4 if abs(vy) < 0.3 else (10 if vy > 0 else -3))
    if anchor == 'middle':
        # Up or down: the words beside the arrow, clear of the boxes above and below
        anchor, lx, ly = 'start', ex + 8, (s[1] + ey) / 2 + (3 if len(text_) == 1 else -2)
    for n, line in enumerate(text_):
        text(lx, ly + n * 11, line, 'stubt', anchor)


falls = {int(k, 16): v for k, v in DATA['special'][0]['falls'].items()}
WAVE = {0x19, 0x23}

for a in sorted(panel_of):
    for e in LOC[a]['exits']:
        b = e['to']
        if b == a:
            # Lost in the mountains: a loop back to the same place
            x, y = boxes[a]
            vx, vy = dirvec(e['dir'])
            sx, sy = clip(x, y, vx, vy)
            svg.append('<path d="M %.1f %.1f c %.1f %.1f %.1f %.1f %.1f %.1f" class="path" fill="none" marker-end="url(#arrow)"/>'
                       % (sx - 16, sy, -10, -30, 42, -30, 32, 0))
            text(sx + 26, sy - 26, dirlabel(e['dir']) + ': round in circles', 'note', 'start')
            continue
        if b == 0x33:
            # The fall: drawn to where you land
            land = falls.get(a)
            if a in WAVE:
                # WAVE the rod here and this exit leads onto the crystal bridge instead
                key = tuple(sorted((a, 0x97)))
                drawn.add(key)
                s, c, t = route(a, 0x97)
                svg.append('<path d="M %.1f %.1f Q %.1f %.1f %.1f %.1f" class="wave"/>' % (s + c + t))
                end_label(e['dir'], s, c)
                mx, my = 0.25 * s[0] + 0.5 * c[0] + 0.25 * t[0], 0.25 * s[1] + 0.5 * c[1] + 0.25 * t[1]
            s, c, t = route(a, land)
            svg.append('<path d="M %.1f %.1f Q %.1f %.1f %.1f %.1f" class="fall" marker-end="url(#arrowr)"/>' % (s + c + t))
            end_label(e['dir'], s, c)
            continue
        if b == 0x37:
            s, c, t = route(a, b)
            svg.append('<path d="M %.1f %.1f Q %.1f %.1f %.1f %.1f" class="fall" marker-end="url(#arrowr)"/>' % (s + c + t))
            continue
        far = math.hypot(boxes[a][0] - boxes[b][0], boxes[a][1] - boxes[b][1]) > 1.7 * U if b in boxes else True
        blocked = not e['open'] and not e.get('lockable') and (a, b) not in IMPASSABLE_NOTES
        if (b not in panel_of or panel_of[b] != panel_of[a] or (blocked and far)
                or tuple(sorted((a, b))) in layout.STUBS):
            where = {'valley': 'the valley', 'under': 'underground', 'castle': 'the castle'}.get(panel_of.get(b), '')
            if panel_of.get(b) == panel_of[a]:
                where = ''
            st, note = style(e, None)
            lines = ['%s: %s (%02X)' % (dirlabel(e['dir']), short(b), b)]
            if where:
                lines[0] += ', ' + where
            if note:
                lines.append(note)
            stub(a, e, lines)
            continue
        key = tuple(sorted((a, b)))
        if key in drawn:
            continue
        drawn.add(key)
        back = next((x for x in LOC[b]['exits'] if x['to'] == a), None)
        if a == 0x97 or b == 0x97 and a not in WAVE:
            back = back  # The bridge's own exits are ordinary
        st, note = style(e, back)
        s, c, t = route(a, b)
        one_way = back is None
        path = '<path d="M %.1f %.1f Q %.1f %.1f %.1f %.1f" class="%s"%s/>' % (
            s + c + t + (st, ' marker-end="url(#arrow)"' if one_way else ''))
        svg.append(path)
        if not angle_ok(e['dir'], s, c):
            end_label(e['dir'], s, c, t, 1)
        if back and not angle_ok(back['dir'], t, c):
            end_label(back['dir'], t, c, s, 1)
        mx, my = 0.25 * s[0] + 0.5 * c[0] + 0.25 * t[0], 0.25 * s[1] + 0.5 * c[1] + 0.25 * t[1]
        if st == 'door':
            svg.append('<circle cx="%.1f" cy="%.1f" r="5.5" class="doorm"/>' % (mx, my))
        if note and st != 'path':
            note = IMPASSABLE_NOTES.get((a, b), note)
            if abs(t[0] - s[0]) < abs(t[1] - s[1]):
                text(mx - 10, my + 3, note, 'doort ' + st, 'end')
            elif st == 'blocked':
                text(mx, my + 16, note, 'doort ' + st)
            elif math.hypot(t[0] - s[0], t[1] - s[1]) < 60:
                # Between neighbours: above them, in the gap between rows
                text(mx, min(boxes[a][1], boxes[b][1]) - BH / 2 - 5, note, 'doort ' + st)
            else:
                text(mx, my - 9, note, 'doort ' + st)

# Moves the game makes, not in the exit tables
MOVES = [
    (0x37, 0x1A, 'you land in the pool'),
    (0x1A, 0x1B, 'you scramble out'),
    (0x6D, 0xA9, 'arrested, without a gift'),
    (0xA9, 0x35, 'freed, for a gift'),
    (0xA9, 0x6F, 'thrown out, for no gift'),
]
for a, b, why in MOVES:
    s, c, t = route(a, b)
    svg.append('<path d="M %.1f %.1f Q %.1f %.1f %.1f %.1f" class="fall" marker-end="url(#arrowr)"/>' % (s + c + t))
    mx, my = 0.25 * s[0] + 0.5 * c[0] + 0.25 * t[0], 0.25 * s[1] + 0.5 * c[1] + 0.25 * t[1]
    text(mx + 6, my - 4, why, 'movet', 'start')

# The boxes
for i, (x, y) in boxes.items():
    l = LOC[i]
    cls = ['box']
    if l['dark']:
        cls.append('dark')
    if i in (0x32, 0x37, 0x1A, 0xA9, 0x33, 0xAF):
        cls.append('special')
    svg.append('<g class="%s"><rect x="%.1f" y="%.1f" width="%d" height="%d" rx="5"/>' % (' '.join(cls), x - BW / 2, y - BH / 2, BW, BH))
    svg.append('<text x="%.1f" y="%.1f" class="id">%02X%s</text>' % (x - BW / 2 + 5, y - BH / 2 + 11, i, ' ◐' if l['dark'] else ''))
    # Name, wrapped
    words, lines, cur = short(i).split(), [], ''
    for w in words:
        if len(cur) + len(w) + 1 > 19 and cur:
            lines.append(cur)
            cur = w
        else:
            cur = (cur + ' ' + w).strip()
    lines.append(cur)
    items = tally(things.get(i, []))
    folk = tally(people.get(i, []))
    detail = []
    for group, kind in ((items, 'obj'), (folk, 'who')):
        if not group:
            continue
        cur = ''
        for w in ', '.join(group).split(' '):
            if len(cur) + len(w) + 1 > 26 and cur:
                detail.append((cur, kind))
                cur = w
            else:
                cur = (cur + ' ' + w).strip()
        detail.append((cur, kind))
    top = y - BH / 2 + 23 + (0 if detail else 8) + (6 if len(lines) == 1 and len(detail) < 3 else 0)
    if len(lines) + len(detail) > 4:
        top -= 4
    for n, line in enumerate(lines[:3]):
        svg.append('<text x="%.1f" y="%.1f" class="name">%s</text>' % (x, top + n * 13, esc(line)))
    dy = top + len(lines[:3]) * 13 - 2
    for n, (line, kind) in enumerate(detail[:5 - len(lines[:3])]):
        svg.append('<text x="%.1f" y="%.1f" class="%s">%s</text>' % (x, dy + n * 10, kind, esc(line)))
    svg.append('</g>')

# Panels and captions
frames = []
for key, title, sub in layout.PANELS:
    g = geo[key]
    frames.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="14" class="panel"/>' % (g['ox'], g['oy'], g['w'], g['h']))
    frames.append('<text x="%.1f" y="%.1f" class="ptitle">%s</text>' % (g['ox'] + 24, g['oy'] + 42, esc(title)))
    frames.append('<text x="%.1f" y="%.1f" class="psub">%s</text>' % (g['ox'] + 24, g['oy'] + 64, esc(sub)))
    for c, r, cap in layout.CAPTIONS.get(key, []):
        x = g['ox'] + PAD + BW / 2 + (c - g['x0']) * U - BW / 2
        y = g['oy'] + PAD + 40 + BH / 2 + (r - g['y0']) * V
        frames.append('<text x="%.1f" y="%.1f" class="cap">%s</text>' % (x, y, esc(cap)))

# The notes, under the castle and underground
nx = geo['under']['ox']
ny = MARGIN + TITLE_H + max(geo['under']['h'], geo['castle']['h']) + GAP
nw = geo['castle']['ox'] + geo['castle']['w'] - nx
W = geo['castle']['ox'] + geo['castle']['w'] + MARGIN
fall_list = ', '.join('%s (%02X) to %s (%02X)' % (short(a), a, short(b), b) for a, b in sorted(falls.items()))
notes_html = open(os.path.join(HERE, 'notes.html')).read() if os.path.exists(os.path.join(HERE, 'notes.html')) else ''
notes_html = notes_html.replace('{FALLS}', esc(fall_list))
H = max(MARGIN + TITLE_H + geo['valley']['h'], ny + 720) + MARGIN

page = '''<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<title>Twin Kingdom Valley map</title>
<style>
@page {{ size: {W}px {H}px; margin: 0; }}
html, body {{ margin: 0; background: #f6f1e4; }}
body {{ width: {W}px; height: {H}px; position: relative; font-family: "DejaVu Sans", system-ui, sans-serif; color: #2c2418; }}
svg {{ position: absolute; left: 0; top: 0; }}
.panel {{ fill: #fffdf6; stroke: #cbbd9c; stroke-width: 2; }}
.ptitle {{ font: 600 30px "DejaVu Serif", Georgia, serif; fill: #6b2a1a; }}
.psub {{ font: 15px "DejaVu Sans", sans-serif; fill: #7c705c; }}
.cap {{ font: italic 600 17px "DejaVu Serif", Georgia, serif; fill: #9b7a4a; }}
.box rect {{ fill: #fff8dc; stroke: #5a4630; stroke-width: 1.6; }}
.box.dark rect {{ fill: #e9e3d4; }}
.box.special rect {{ fill: #fde6dc; stroke: #a8442c; stroke-dasharray: 5 3; }}
.id {{ font: 600 9.5px "DejaVu Sans Mono", monospace; fill: #8a7a62; }}
.name {{ font: 600 11.5px "DejaVu Sans", sans-serif; text-anchor: middle; fill: #2c2418; }}
.obj {{ font: italic 9px "DejaVu Sans", sans-serif; text-anchor: middle; fill: #2f5d2a; }}
.who {{ font: 9px "DejaVu Sans", sans-serif; text-anchor: middle; fill: #8a2c1c; }}
path, line {{ fill: none; }}
.path {{ stroke: #6d5a40; stroke-width: 2.2; }}
.door {{ stroke: #6d5a40; stroke-width: 2.2; }}
.doorm {{ fill: #d4a017; stroke: #5a4630; stroke-width: 1.2; }}
.blocked {{ stroke: #a39a8a; stroke-width: 2; stroke-dasharray: 2 4; }}
.secret {{ stroke: #7b3fa0; stroke-width: 2.2; stroke-dasharray: 8 4; }}
.fall {{ stroke: #c0392b; stroke-width: 2; stroke-dasharray: 6 4; }}
.wave {{ stroke: #2a7fb8; stroke-width: 2.4; stroke-dasharray: 3 3; }}
.stub {{ stroke: #6d5a40; stroke-width: 2; }}
.stub.blocked {{ stroke: #a39a8a; }}
.stubt {{ font: 9.5px "DejaVu Sans", sans-serif; fill: #4a3b2a; paint-order: stroke; stroke: #fffdf6; stroke-width: 3px; }}
.doort {{ font: 9px "DejaVu Sans", sans-serif; text-anchor: middle; fill: #7a5a10; paint-order: stroke; stroke: #fffdf6; stroke-width: 3.5px; }}
.doort.blocked {{ fill: #6f675a; }}
.doort.secret {{ fill: #7b3fa0; }}
.movet {{ font: italic 9.5px "DejaVu Sans", sans-serif; fill: #b03a2a; paint-order: stroke; stroke: #fffdf6; stroke-width: 3px; }}
.note {{ font: italic 9.5px "DejaVu Sans", sans-serif; fill: #6d5a40; }}
.dl rect {{ fill: #fffdf6; stroke: #b8a684; stroke-width: 0.8; }}
.dl text {{ font: 600 9px "DejaVu Sans Mono", monospace; fill: #6b2a1a; }}
h1 {{ position: absolute; left: {MARGIN}px; top: {MARGIN}px; margin: 0; font: 600 54px "DejaVu Serif", Georgia, serif; color: #6b2a1a; }}
.lede {{ position: absolute; left: {MARGIN}px; top: {LEDE}px; margin: 0; font-size: 19px; color: #6d604c; max-width: 2400px; }}
.notes {{ position: absolute; left: {NX}px; top: {NY}px; width: {NW}px; columns: 3; column-gap: 40px; font-size: 14.5px; line-height: 1.45; }}
.notes h2 {{ font: 600 21px "DejaVu Serif", Georgia, serif; color: #6b2a1a; margin: 0 0 6px; break-after: avoid; }}
.notes h3 {{ font: 600 16px "DejaVu Serif", Georgia, serif; color: #6b2a1a; margin: 14px 0 4px; break-after: avoid; }}
.notes p, .notes li {{ margin: 0 0 6px; }}
.notes ul {{ margin: 0 0 8px; padding-left: 18px; }}
.notes .key svg {{ position: static; vertical-align: middle; margin-right: 6px; }}
.notes code {{ font-family: "DejaVu Sans Mono", monospace; font-size: 13px; }}
</style></head><body>
<h1>Twin Kingdom Valley</h1>
<p class="lede">Every one of the game's 177 places and the ways between them, read from the game itself (the
BBC Micro version of Trevor Hall's 1983 adventure); laid out after the Electron User maps of 1985.
Hex numbers are the game's location numbers. Objects (<span style="color:#2f5d2a;font-style:italic">green</span>) and
characters (<span style="color:#8a2c1c">red</span>) are where the game starts them; &#9733; marks treasure,
&#9680; places that are dark without a lit lamp.</p>
<svg width="{W}" height="{H}" viewBox="0 0 {W} {H}" xmlns="http://www.w3.org/2000/svg">
<defs>
<marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" style="fill:#6d5a40;stroke:none"/></marker>
<marker id="arrowr" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" style="fill:#c0392b;stroke:none"/></marker>
</defs>
{FRAMES}
{LINES}
{LABELS}
</svg>
<div class="notes">{NOTES}</div>
</body></html>
'''.format(W=int(W), H=int(H), MARGIN=MARGIN, LEDE=MARGIN + 70, NX=int(nx), NY=int(ny), NW=int(nw),
           FRAMES='\n'.join(frames), LINES='\n'.join(svg), LABELS='\n'.join(labels), NOTES=notes_html)

out = os.path.join(HERE, 'map.html')
open(out, 'w').write(page)
print('wrote', out, int(W), 'x', int(H))

chrome = next((shutil.which(c) for c in ('chromium', 'chromium-browser', 'google-chrome') if shutil.which(c)), None)
if chrome and '--no-pdf' not in sys.argv:
    pdf = os.path.join(HERE, 'map.pdf')
    subprocess.run([chrome, '--headless', '--disable-gpu', '--no-pdf-header-footer',
                    '--print-to-pdf=' + pdf, 'file://' + out], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print('wrote', pdf)
