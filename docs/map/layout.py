"""Where each location goes on the map: panel and (column, row), north up.

Follows the Electron User maps of 1985 (maps 1-5) as closely as the game's
own exits allow. Columns and rows are in box spacings; halves are fine."""

PANELS = [
    ('valley', 'The valley', 'South of the canyon, the canyon and the desert around the castle'),
    ('under', 'Underground', 'Caves, the sloping maze, the twisting maze and the passages beneath'),
    ('castle', 'The castle', 'Towers, ground floor, cellars and dungeons'),
]

VALLEY = {
    # North: the west mountains, the desert and the castle
    0x8A: (2, -8.4), 0x30: (8, -8.6),
    0x2F: (5, -7), 0x2E: (8, -7),
    0x83: (0, -5), 0x82: (1, -5), 0x80: (2, -5), 0x2C: (5, -5), 0x2B: (6.5, -5),
    0x32: (8, -5.15), 0x2A: (9.5, -5),
    0x84: (0, -4), 0x81: (1, -4), 0x2D: (2, -4), 0x27: (5, -4), 0x29: (8, -4),
    0x28: (8, -3),
    0x26: (7, -2), 0x25: (8, -2), 0x24: (10, -2),
    0x23: (3, -1), 0x22: (6, -1), 0x21: (8, -1), 0x20: (10.5, -1),
    # The canyon, and the crystal bridge across the fissure
    0x89: (0, 0), 0x88: (1, 0), 0x87: (2, 0), 0x97: (3, 0), 0x1F: (4, 0), 0x1E: (5, 0),
    0x1D: (6, 0), 0x1C: (7, 0), 0x1B: (8, 0), 0x1A: (9, 0),
    # South of the canyon
    0x85: (0, 1), 0x10: (1.5, 1), 0x19: (3, 1), 0x18: (6, 1),
    0xAC: (10, 1), 0x31: (12, 0.4), 0xAD: (11, 1.7), 0xAB: (10, 2.3),
    0x17: (6, 3), 0x16: (7, 3), 0x15: (8, 3), 0x14: (9, 3.3),
    0x0E: (3, 4), 0x0A: (6, 4), 0x13: (8, 4), 0x12: (9, 4.4),
    0x86: (0, 5), 0x0F: (2, 5), 0x0B: (3, 5), 0x0D: (8, 5), 0x0C: (9, 5.5),
    0x07: (3, 6), 0x06: (4, 6.3), 0x09: (8, 6), 0x08: (9, 6.6), 0x11: (11, 6), 0xA0: (12, 6),
    0x05: (8, 7.2), 0x04: (9.3, 7.6), 0x03: (10.4, 7.2),
    0x00: (8, 8.2), 0x9F: (12, 8), 0xA1: (13, 8),
    0x02: (7, 9.2), 0x01: (8, 9.2), 0x98: (9.3, 9.2), 0x99: (10.4, 9.2), 0x9B: (11.5, 9.2), 0x9D: (12.6, 9.2),
    0xA4: (8, 10.2), 0x9A: (9.3, 10.2), 0x9C: (10.4, 10.2), 0x9E: (11.5, 10.2),
    0x37: (8.5, 1.65), 0x33: (12.4, -2.6), 0xAF: (12.4, -4.2),
    0xA6: (6, 11.2), 0xA7: (8, 11.2), 0xA5: (10.4, 11.2), 0xA3: (11.5, 11.2), 0xA2: (13, 11.2),
}

UNDER = {
    # The sloping maze, under the west mountains
    0x8E: (2, 0), 0x8F: (3, 0), 0x95: (4.2, 0),
    0x91: (0, 1), 0x90: (1, 1), 0x92: (2, 1),
    0x94: (0, 2), 0x93: (1, 2), 0xAE: (3.1, 3.0), 0x96: (4.2, 1.6),
    0x8C: (0, 3.3), 0x8D: (1.2, 3.3),
    0x8B: (1.2, 4.5),
    # The castle's deepest level, the princess and the dwarf's rooms
    0x4B: (8, 0), 0x4C: (8, 1), 0x4D: (7, 2), 0xAA: (8, 2), 0xA8: (9, 2),
    0x5F: (6, 3), 0x61: (7, 3), 0x5E: (5, 4), 0x60: (6, 4.4), 0x62: (7, 4.2),
    0x63: (7, 5.3),
    # The forest king, the caves under the trail, the clearing's passage
    0x6D: (5, 6.3), 0x35: (6, 6.3), 0x67: (8, 6.3), 0x68: (9, 6.3), 0x69: (10, 6.3),
    0x6E: (5, 7.4), 0x66: (6.5, 7.4), 0x6C: (9, 7.4), 0x6A: (10, 7.4),
    0xA9: (3.6, 7.1),
    0xB0: (4.4, 8.6), 0x6F: (5.6, 8.6), 0x64: (7, 8.6), 0x36: (9, 8.6), 0x6B: (10, 8.6),
    # The twisting maze
    0x72: (5, 10), 0x70: (6, 10), 0x73: (7, 10), 0x71: (8, 10),
    0x75: (5, 11), 0x76: (6, 11), 0x77: (7, 11), 0x74: (8, 11),
    0x79: (5, 12), 0x78: (6, 12), 0x7A: (7, 12), 0x7B: (8, 12),
    0x7C: (5, 13), 0x7E: (6, 13), 0x7D: (7, 13), 0x7F: (8, 13),
}

CASTLE = {
    # Upper levels
    0x4E: (2, 0),
    0x53: (2, 1), 0x4F: (0, 1.5), 0x51: (4, 1.5),
    0x56: (1, 2), 0x58: (2, 2), 0x57: (3, 2),
    0x52: (0, 3), 0x59: (1, 3), 0x3A: (2, 3), 0x5A: (3, 3), 0x55: (4, 3),
    0x5B: (1, 4), 0x5D: (2, 4), 0x5C: (3, 4),
    0x54: (2, 5), 0x50: (2, 6),
    # Ground level
    0x3C: (0, 7.3), 0x3B: (1, 7.3),
    0x38: (0, 8.3), 0x34: (1, 8.3), 0x39: (2, 8.3),
    # Below ground
    0x3D: (2.4, 9.4), 0x3E: (3.4, 9.4), 0x3F: (4.4, 9.4),
    0x40: (3.4, 10.4),
    0x41: (2.4, 11.4), 0x42: (3.4, 11.4), 0x43: (4.4, 11.4), 0x44: (5.4, 11.4),
    # Lower levels
    0x45: (6.4, 12.4),
    0x46: (3.4, 13.4), 0x47: (4.4, 13.4), 0x48: (5.4, 13.4),
    0x65: (4.4, 14.4), 0x49: (5.4, 14.4), 0x4A: (6.4, 14.4),
}

# Exits drawn as labelled stubs at both ends, not as a line: too far apart
STUBS = {(0x34, 0x3A), (0x30, 0x31), (0x11, 0x31)}

LAYOUT = {'valley': VALLEY, 'under': UNDER, 'castle': CASTLE}

# Places with no exits of their own, shown in a box of their own: the game
# moves the player on from them (see tkvdata.json 'special')
SPECIAL = []

# Level captions in the castle panel: (column, row, text)
CAPTIONS = {
    'castle': [(-0.5, -0.55, 'Upper levels'), (-0.5, 6.75, 'Ground level'),
               (3.6, 8.85, 'Below ground'), (2.4, 12.4, 'Lower levels')],
    'under': [(-0.5, -0.55, 'The sloping maze'), (4.5, -0.55, 'Beneath the castle'),
              (8.6, 5.65, 'Under the trail'), (4.5, 9.45, 'The twisting maze')],
    'valley': [(-0.5, -9.15, 'North of the canyon'), (-0.5, -0.5, 'The canyon'),
               (-0.5, 1.6, 'South of the canyon')],
}

# Short names for the boxes, where the game's own are long
NAMES = {
    0x07: 'Disused flint quarry', 0x1B: 'Large rock on the canyon floor',
    0x1C: 'Deep canyon', 0x1A: 'Crocodile pool', 0x21: 'North of the canyon',
    0x28: 'Desert around the castle', 0x2E: 'Back of the castle',
    0x19: 'South edge of the fissure', 0x23: 'Edge of the fissure',
    0x93: 'Small cave in the west mountains', 0xAE: 'River of gold',
    0x6D: 'Hall of the Forest King', 0x9D: 'The Sword Inn', 0x34: 'Entrance hall',
    0x89: 'Watersmeet', 0x8A: 'West mountains', 0x97: 'The Crystal Bridge',
    0x6F: 'Room full of bones', 0x32: 'The moat', 0xA9: 'Locked up in a dungeon',
    0xAF: 'The end of your life', 0x33: 'Diving down a dangerous fall',
    0x37: 'Diving down a large waterfall', 0x3A: 'Grand Staircase',
}
