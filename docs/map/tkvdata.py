#!/usr/bin/env python3
"""Reads the locations, exits, objects and characters from the game image
(machinecode/VALLEYR) into tkvdata.json, for the map.

Names come from c/readtkv, which decodes the game's words; everything else
is read from the image here. See tkv_notes.txt for the tables."""
import json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
IMAGE = os.path.join(ROOT, 'machinecode', 'VALLEYR')
READTKV = os.path.join(ROOT, 'c', 'readtkv')
BASE = 0x0400
NLOC = 0xB1
DIRS = [(0x20, 'N'), (0x10, 'S'), (0x08, 'E'), (0x04, 'W'), (0x02, 'U'), (0x01, 'D')]
KEYS = ['master', 'gold', 'silver', 'bronze', 'brass']   # Keys 2-6, see $1745

mem = open(IMAGE, 'rb').read()
def b(a): return mem[a - BASE]

def readtkv(*args):
    out = subprocess.run([READTKV, *args], cwd=os.path.join(ROOT, 'c'),
                         capture_output=True, text=True).stdout
    return out

def name(kind, i):
    m = re.search(r'^\|(.*)\|$', readtkv(kind, '$%02X' % i), re.M)
    return m.group(1) if m else ''

def dirname(d):
    return ''.join(n for bit, n in DIRS if d & bit)

def door_keys(third):
    # $173D+bits 0-2 is a mask of which keys open the door: bit 0 for the
    # master key (object 2) up to bit 4 for the brass key (object 6), $1745
    mask = b(0x173D + (third & 7))
    return [KEYS[k] for k in range(5) if mask & (1 << k)]

_words = {}
def word(code):
    if code not in _words:
        m = re.search(r' is (.*)$', readtkv('words', '$%02X' % code), re.M)
        _words[code] = m.group(1).strip() if m else ''
    return _words[code]

def door_text(d, third):
    # The words for a door, grate, fence or path: a material from $295F and
    # a kind from $2900 and $2930 ($2220 for doors, $2255 for the others)
    out = []
    if third & 7 and b(0x295F + (third & 7)):
        out.append(word(b(0x295F + (third & 7))))
    kind = (third >> 3) & 0x0F
    if third & 0x80:
        kind |= 0x10
    elif d & 0x40:
        kind |= 0x20
    for table in (0x2900, 0x2930):
        if b(table + kind):
            out.append(word(b(table + kind)))
    return ' '.join(out)

def read_exits(p, n):
    exits = []
    for _ in range(n):
        d, dest = b(p), b(p + 1)
        e = {'dir': dirname(d & 0x3F), 'to': dest, 'open': bool(d & 0x40)}
        if d & 0x80:
            third = b(p + 2)
            e['door'] = third
            e['text'] = door_text(d, third)
            if third & 0x80:
                e['lockable'] = True
                e['keys'] = door_keys(third)
            p += 3
        else:
            p += 2
        exits.append(e)
    return exits, p

# $1A33: the locations that gain a secret exit when the player drinks from
# the magic pool at Watersmeet ($1973)
SECRET = [b(0x1A33 + i) for i in range(7)]

locations = []
for y in range(NLOC):
    a = b(0x2A00 + y) | b(0x2B00 + y) << 8
    flags, counts = b(a), b(a + 1)
    nwords, nexits = counts & 0x1F, counts >> 5
    exits, p = read_exits(a + 2 + nwords, nexits)
    if y in SECRET:   # One more exit after the others, revealed by the magic pool
        more, _ = read_exits(p, 1)
        for e in more:
            e['secret'] = True
        exits += more
    locations.append({'id': y, 'name': name('location', y), 'flags': flags,
                      'type': flags & 7, 'dark': bool(flags & 0x80),
                      'picture': not flags & 0x40, 'exits': exits})

# Moves the game makes itself, not in the exit tables (see c/playtkv.c)
falls = {b(0x1283 + i): b(0x1289 + i) for i in range(6)}   # $0F14: where a fall lands
special = [
    {'from': 0x33, 'to': list(sorted(set(falls.values()))),
     'why': 'falling into the canyon lands below where you jumped', 'falls': {'%02X' % k: v for k, v in falls.items()}},
    {'from': 0x37, 'to': [0x1A], 'why': 'diving down the waterfall lands in the crocodile pool'},
    {'from': 0x1A, 'to': [0x1B], 'why': 'you scramble out of the pool'},
    {'from': 0x32, 'to': [], 'why': 'you scramble out of the moat back where you came from'},
    {'from': 0x6D, 'to': [0xA9], 'why': "without a gift, the forest king's guard arrest you"},
    {'from': 0xA9, 'to': [0x35, 0x6F], 'why': 'a good enough gift frees you to the cave; otherwise you are thrown into the room full of bones'},
]
# WAVE the rod at either end of the chasm ($19, $23): their first exits lead
# to the crystal bridge ($97) or the fall ($33), see the table at $1380
bridge = {'ends': [0x19, 0x23], 'to': [b(0x1385), b(0x1385 + 6)]}
# The drawbridge ($28 north) leads to $29, or to the moat ($32) once closed
nobj, nnpc = b(0x27FC), b(0x27FE)
# $2654 by object code: what it's worth; the forest king takes a gift worth
# $1E or more ($10A4)
objects = [{'id': i, 'name': name('object', i), 'at': b(0x25C0 + i),
            'code': b(0x2580 + i), 'value': b(0x2654 + b(0x2580 + i))} for i in range(nobj)]
characters = [{'id': i, 'name': name('npc', i), 'at': b(0x29B4 + i)} for i in range(nnpc)]

json.dump({'special': special, 'bridge': bridge, 'secret': SECRET, 'locations': locations, 'objects': objects, 'characters': characters},
          open(os.path.join(HERE, 'tkvdata.json'), 'w'), indent=1)
print(len(locations), 'locations,', len(objects), 'objects,', len(characters), 'characters')
