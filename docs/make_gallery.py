#!/usr/bin/env python3
"""Builds docs/index.html and docs/pictures/*.png: every picture in Twin
Kingdom Valley, drawn by the game's own code via `c/readtkv picture`.

Run from the repository root after building c/readtkv:
    python3 docs/make_gallery.py
"""
import html, os, struct, subprocess, sys, tempfile, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGE = os.path.join(ROOT, 'machinecode', 'VALLEYR')
READTKV = os.path.join(ROOT, 'c', 'readtkv')
OUT = os.path.join(ROOT, 'docs')

D = open(IMAGE, 'rb').read()
def g(a): return D[a - 0x400]

# Names of places and objects use the word table at $3600, found via the
# offsets at $3400; the last letter of each word has bit 7 set
def word_at(a):
    s = ''
    while True:
        b = g(a); s += chr(b & 0x7F); a += 1
        if b & 0x80: return s.lower()
def word(c):
    return word_at(0x3500 + 5 * c + g(0x3400 + c) + 128)

INCODE = {6: 'in a sloping maze', 2: 'on the canyon floor', 7: 'in an arid desert',
          3: 'wandering through high mountains', 4: 'wandering through the woods'}
def location_name(l):
    a = g(0x2A00 + l) | (g(0x2B00 + l) << 8)
    n = g(a + 1) & 0x1F
    if n == 0:  # Described in code, see $2122
        return INCODE.get(g(a) & 7, 'wandering through dense forest')
    words, caps = [], False
    for b in (g(a + 2 + i) for i in range(n)):
        if b == 0xFF: caps = True; continue
        w = word(b); words.append(w[0].upper() + w[1:] if caps else w); caps = False
    return ' '.join(words)

def object_name(code):
    return ' '.join(word(g(t + code)) for t in (0x2750, 0x277A, 0x27A4, 0x27CE) if g(t + code))

def png_rows(path):
    d = open(path, 'rb').read(); i = 8; idat = b''; w = h = 0
    while i < len(d):
        n = struct.unpack('>I', d[i:i + 4])[0]; t = d[i + 4:i + 8]; body = d[i + 8:i + 8 + n]
        if t == b'IHDR': w, h = struct.unpack('>II', body[:8])
        if t == b'IDAT': idat += body
        i += 12 + n
    raw = zlib.decompress(idat); stride = 1 + 3 * w
    return w, h, [raw[y * stride + 1:(y + 1) * stride] for y in range(h)]

def write_png(path, w, h, rows):
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
    raw = b''.join(b'\0' + r for r in rows)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

def render(n, dest):
    """Draws picture n with readtkv and saves it 320x256: MODE 5 pixels doubled across."""
    with tempfile.TemporaryDirectory() as tmp:
        big = os.path.join(tmp, 'p.png')
        subprocess.run([READTKV, 'picture', '%02x' % n, big], cwd=os.path.join(ROOT, 'c'),
                       check=True, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
        w, h, rows = png_rows(big)  # 640x512: each MODE 5 pixel is 4x2
        small = [b''.join(rows[y][x * 3:x * 3 + 3] for x in range(0, w, 2)) for y in range(0, h, 2)]
        write_png(dest, w // 2, h // 2, small)

# Which picture shows where
locs = {}
for l in range(0xB1):
    a = g(0x2A00 + l) | (g(0x2B00 + l) << 8)
    if g(a) & 0x40: continue  # No picture here
    locs.setdefault(g(0x1F00 + l), []).append(l)

sections = []
sections.append(('Locations', 'The view on arriving somewhere, or with PICTURE.',
                 [(p, '; '.join('%s <span>$%02X</span>' % (html.escape(location_name(l)), l) for l in ls))
                  for p, ls in sorted(locs.items())]))

objects = []
for i in range(7):  # LOOK at a carried object, see $4263
    code, pic = g(0x42F2 + i), g(0x42F9 + i)
    if pic == 0xC3: continue
    objects.append((pic, html.escape(object_name(code))))
sections.append(('Objects', 'LOOK at one of these while carrying it.', objects))

def at(*ls):
    return '; '.join('%s <span>$%02X</span>' % (html.escape(location_name(l)), l) for l in ls)
closeups = [(0x55, at(0x00) + '; first of three ' + at(0x57)), (0x4E, 'second of three ' + at(0x57)),
            (0x51, 'third of three ' + at(0x57) + '; ' + at(0x38)), (0x54, at(0x56, 0x5B))]
sections.append(('Close-ups', 'LOOK PICTURE in a few places, see $420A.', closeups))

variants = []
for x, when in ((0, 'after the first wave'), (6, 'after the second wave')):
    for i in range(5):
        p, l = g(0x1380 + x + i), g(0x138C + i)
        if p not in locs:
            variants.append((p, '%s <span>$%02X</span>, %s of the rod' % (html.escape(location_name(l)), l, when)))
for p, t in ((0x0B, 'the drawbridge open'), (0x0C, 'the drawbridge closed')):
    if p not in locs:
        variants.append((p, t))
seen = set()
variants = [v for v in variants if not (v[0] in seen or seen.add(v[0]))]
if variants:
    sections.append(('Changes', 'Pictures that replace others when something happens.', variants))
sections.append(('The end', 'The secret of life is simple yet complex, as is the picture of it.',
                 [(0xC3, 'the secret of life, the last frame of its animation')]))

os.makedirs(os.path.join(OUT, 'pictures'), exist_ok=True)
done = set()
for _, _, items in sections:
    for p, _ in items:
        if p not in done:
            render(p, os.path.join(OUT, 'pictures', '%02x.png' % p)); done.add(p)
            print('picture %02x' % p, file=sys.stderr)

cards = []
for title, intro, items in sections:
    cards.append('<h2>%s</h2>\n<p class="intro">%s</p>\n<div class="grid">' % (title, html.escape(intro)))
    for p, caption in items:
        cards.append('<figure><img src="pictures/%02x.png" alt="Picture $%02X" loading="lazy" width="320" height="256">'
                     '<figcaption><b>$%02X</b> %s</figcaption></figure>' % (p, p, p, caption))
    cards.append('</div>')

page = '''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Twin Kingdom Valley pictures</title>
<style>
:root { --bg:#f4f1ea; --fg:#1c1b19; --muted:#625d55; --card:#fffdf8; --line:#ddd6c9; --accent:#9b2d20; }
@media (prefers-color-scheme: dark) {
  :root { --bg:#141311; --fg:#ece7de; --muted:#a39d92; --card:#1d1c19; --line:#34312c; --accent:#e0705f; }
}
* { box-sizing: border-box; }
body { margin:0; background:var(--bg); color:var(--fg);
       font:16px/1.55 system-ui, -apple-system, "Segoe UI", sans-serif; }
main { max-width:1180px; margin:0 auto; padding:40px 16px 64px; }
h1 { font-size:2rem; margin:0 0 8px; letter-spacing:-0.01em; }
h2 { font-size:1.3rem; margin:48px 0 4px; border-top:1px solid var(--line); padding-top:24px; }
p { max-width:72ch; }
.lede { color:var(--muted); margin-top:0; }
.intro { color:var(--muted); margin:0 0 16px; }
code { font-family: ui-monospace, Menlo, Consolas, monospace; font-size:0.92em; }
a { color:var(--accent); }
.grid { display:grid; grid-template-columns:repeat(auto-fill, minmax(260px, 1fr)); gap:16px; }
figure { margin:0; background:var(--card); border:1px solid var(--line); border-radius:6px; overflow:hidden; }
figure img { display:block; width:100%; height:auto; aspect-ratio:5/4; image-rendering:pixelated; background:#000; }
figcaption { padding:8px 10px 10px; font-size:0.9rem; }
figcaption b { font-family: ui-monospace, Menlo, Consolas, monospace; color:var(--accent); margin-right:4px; }
figcaption span { font-family: ui-monospace, Menlo, Consolas, monospace; color:var(--muted); font-size:0.85em; }
</style>
</head>
<body>
<main>
<h1>Twin Kingdom Valley pictures</h1>
<p class="lede">All {COUNT} pictures from Trevor Hall's 1983 BBC Micro adventure, drawn by the game's own code.</p>
<p>The game draws its pictures with a program at <code>$3C00</code> that only asks the BBC to change
screen mode and palette and to draw lines; it fills the shapes itself, straight into screen memory.
These were made by running that code in the 6502 emulator in <code>c/m6502.c</code>, with just enough
of the BBC's VDU drivers (<code>c/pictures.c</code>). <code>playtkv</code> shows them in the terminal
as you play, and <code>readtkv picture XX file.png</code> saves one. Numbers are as in the table at
<code>$1F00</code>; locations are given as names and numbers. Pixels are MODE 5's, twice as wide as tall.</p>
{CARDS}
</main>
</body>
</html>
'''.replace('{COUNT}', str(len(done))).replace('{CARDS}', '\n'.join(cards))
open(os.path.join(OUT, 'index.html'), 'w').write(page)
open(os.path.join(OUT, '.nojekyll'), 'w').close()
print('%d pictures' % len(done), file=sys.stderr)
