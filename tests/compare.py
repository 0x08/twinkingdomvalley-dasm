#!/usr/bin/env python3
"""Plays scripted games in both the original (c/runtkv, the real game code in
the 6502 emulator) and the reimplementation (c/playtkv), and checks they
print the same thing.

    python3 tests/compare.py [-v] [--browser] [script ...]

With no scripts, runs every tests/scripts/*.txt. Output is compared word by
word: line breaks and runs of spaces are ignored, as the original breaks
lines to fit 40 columns and playtkv doesn't (runtkv runs with -w so the
screen edge doesn't split words). Differences listed in KNOWN are allowed. Pictures, which both draw by running the game's code, must be identical.
Exits 1 if any script differs.

With --browser, the original is compared instead with the browser version
in docs/play, run in headless Chromium (set CHROMIUM to its command), with
pictures identified by a hash of their pixels.

A script is one input line per line. Header lines starting with # set up
the game image first:

    # start NN       north from the starting road leads to location NN
    # object NN=LL   object NN starts at location LL
    # code NN=CC     object NN starts with code CC (eg a filled jug)
    # npc NN=LL      character NN starts at location LL
    # anything else is a comment
"""
import difflib, html, os, re, subprocess, sys, tempfile, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
C = os.path.join(ROOT, 'c')
IMAGE = os.path.join(ROOT, 'machinecode', 'VALLEYR')

def patched_image(header, path):
    d = bytearray(open(IMAGE, 'rb').read())
    def g(a): return d[a - 0x400]
    def s(a, v): d[a - 0x400] = v
    for line in header:
        f = line[1:].split()
        if not f:
            continue
        if f[0] == 'start':  # Redirect the road's north exit
            a = g(0x2A01) | (g(0x2B01) << 8)
            p = a + 2 + (g(a + 1) & 0x1F)
            for _ in range(g(a + 1) >> 5):
                if g(p) & 0x3F == 0x20:
                    s(p + 1, int(f[1], 16))
                p += 3 if g(p) & 0x80 else 2
        elif f[0] in ('object', 'code', 'npc'):
            for kv in f[1:]:
                k, v = (int(x, 16) for x in kv.split('='))
                base = {'object': 0x25C0, 'code': 0x2580, 'npc': 0x29B4}[f[0]]
                s(base + k, v)
    open(path, 'wb').write(d)

PICTURE = re.compile('(?:(?:\x1b\\[38;2;[0-9;]+m\x1b\\[48;2;[0-9;]+m\u2580)+\x1b\\[0m\n?)+')

def words(text):
    # Each picture becomes one word identifying exactly what was printed
    text = PICTURE.sub(lambda m: ' [picture:%08x] ' % (zlib.crc32(m.group(0).encode()) & 0xFFFFFFFF), text)
    text = re.sub('\x1b\\[[0-9;]*m', '', text)  # Teletext colours
    # The prompt is followed by input, which playtkv doesn't echo and
    # runtkv's terminal would, so treat "?" as a word of its own
    # The original wraps lines itself, sometimes putting a full stop or
    # comma at the start of the next line, so join those to the word before
    text = re.sub(r'\s+([.,])(?=\s|$)', r'\1', text)
    return text.replace('?', ' ? ').split()

# Where playtkv deliberately differs: (original words, playtkv words)
KNOWN = [
    (['w'], ['worthy']),  # The keeper's word is corrupt in the game data ($07D3)
]

def run(program, script_input, image):
    env = dict(os.environ, TKV_IMAGE=image)
    env.pop('TKV_PICTURE_WIDTH', None)
    args = [os.path.join(C, program)] + (['-w'] if program == 'runtkv' else [])
    r = subprocess.run(args, input=script_input.encode(), cwd=C,
                       env=env, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=600)
    return r.stdout.decode('utf-8', 'replace')

def run_browser(script_input, image, tmp):
    script = os.path.join(tmp, 'script.txt')
    open(script, 'w').write(script_input)  # Lines as runtkv reads them
    page = 'file://%s/docs/play/index.html?test=%s&image=%s' % (ROOT, script, image)
    r = subprocess.run([os.environ.get('CHROMIUM', 'chromium'), '--headless=new', '--disable-gpu',
                        '--no-sandbox', '--allow-file-access-from-files',
                        '--user-data-dir=' + os.path.join(tmp, 'profile'),
                        '--virtual-time-budget=100000000', '--dump-dom', page],
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=1200)
    dom = r.stdout.decode('utf-8', 'replace')
    m = re.search(r'<pre id="transcript">(.*?)</pre>', dom, re.S)
    if '<title>done</title>' not in dom or m is None:
        return '[the browser version did not finish]'
    return html.unescape(m.group(1))

def compare(path, verbose, browser=False):
    lines = open(path).read().split('\n')
    header = [l for l in lines if l.startswith('#')]
    commands = '\n'.join(l for l in lines if not l.startswith('#'))
    with tempfile.TemporaryDirectory() as tmp:
        image = os.path.join(tmp, 'VALLEYR')
        patched_image(header, image)
        if browser:
            os.environ['TKV_PICTURE_HASH'] = '1'
            original = words(run('runtkv', commands, image))
            del os.environ['TKV_PICTURE_HASH']
            ours = words(run_browser(commands, image, tmp))
        else:
            original = words(run('runtkv', commands, image))
            ours = words(run('playtkv', commands, image))
    sm = difflib.SequenceMatcher(None, original, ours, autojunk=False)
    diffs = [op for op in sm.get_opcodes() if op[0] != 'equal'
             and (original[op[1]:op[2]], ours[op[3]:op[4]]) not in KNOWN]
    if not diffs:
        print('ok      %s (%d words)' % (os.path.basename(path), len(original)))
        return True
    print('DIFFERS %s' % os.path.basename(path))
    shown = 0
    for tag, i1, i2, j1, j2 in diffs:
        before = ' '.join(original[max(0, i1 - 8):i1])
        print('  ...%s' % before)
        print('  original: %s' % ' '.join(original[i1:i2]))
        print('  %s %s' % ('browser: ' if browser else 'playtkv: ', ' '.join(ours[j1:j2])))
        shown += 1
        if shown >= (20 if verbose else 3):
            break
    return False

def main():
    args = sys.argv[1:]
    verbose = '-v' in args
    browser = '--browser' in args
    args = [a for a in args if a not in ('-v', '--browser')]
    scripts = args or sorted(
        os.path.join(ROOT, 'tests', 'scripts', f)
        for f in os.listdir(os.path.join(ROOT, 'tests', 'scripts')) if f.endswith('.txt'))
    failed = [s for s in scripts if not compare(s, verbose, browser)]
    print('%d of %d scripts match' % (len(scripts) - len(failed), len(scripts)))
    sys.exit(1 if failed else 0)

if __name__ == '__main__':
    main()
