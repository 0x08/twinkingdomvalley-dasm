# twinkingdomvalley-dasm

A disassembly of **Twin Kingdom Valley**, Trevor Hall's 1983 text adventure
with pictures for the BBC Micro, and a reimplementation of it in C that
uses the game's own data and plays the same, word for word.

Play it in your browser: <https://0x08.github.io/twinkingdomvalley-dasm/play/>  
Gallery of every picture: <https://0x08.github.io/twinkingdomvalley-dasm/>

## The tools

Build them with `make -C c playtkv runtkv readtkv` (any C compiler, no
libraries). They read the game from `machinecode/VALLEYR`; run them from
`c/`, or set `TKV_IMAGE` to the file.

- **`playtkv`** – the game reimplemented in C, from the disassembly: the
  parser, every command, the characters' movement and fighting, the special
  events, OPTION, `*SAVE`/`*LOAD`, and the pictures, printed in the terminal
  with 24-bit colour.

      ./playtkv [-d] [-o 1-6] [location]

  `-o` chooses the display option instead of being asked (5 and 6 give no
  pictures); `-d` shows the player's location, the lamp oil and every
  character's move; `location` (hex) prints that location's description.
  `TKV_PICTURE_WIDTH` sets the width of pictures in columns.

- **`runtkv`** – the original game itself, running in a 6502 emulator
  with just enough of the BBC's operating system for text, input and
  pictures. `runtkv -w` doesn't wrap lines at 40 characters. `*SAVE` and
  `*LOAD` do nothing.

- **`readtkv`** – looks things up in the game data, eg
  `readtkv location '$00'`, `readtkv object '$01'`, `readtkv npc '$09'`,
  `readtkv words '$2A'`, or draws a picture: `readtkv picture 2e road.png`.

Pictures are drawn by running the game's own drawing code at `$3C00` in the
emulator (`c/m6502.c`), which only asks the operating system to change
screen mode and palette and draw lines; `c/pictures.c` does that and the
game does its own fills in screen memory.

## In the browser

`docs/play/` runs the original game in the browser: `tkv.js` is the same
6502 emulator and operating system stand-in in JavaScript, with MODE 7
teletext text and MODE 5 pictures drawn live on a canvas, and
`valleyr.bin` a copy of `machinecode/VALLEYR`. `*SAVE` and `*LOAD` use
the browser's storage. It needs no build: serve or open `docs/`.

## Tests

    make -C c test

plays every script in `tests/scripts` in both `runtkv` (the original) and
`playtkv`, and checks they print exactly the same words and pictures, then
runs them all again with AddressSanitizer and UndefinedBehaviorSanitizer.
If Chromium is installed it also checks the browser version against the
original the same way (`tests/compare.py --browser`).
Scripts can start the player somewhere else or move objects first; see
`tests/compare.py`. The one deliberate difference is a word that is corrupt
in the game data (`$07D3`): playtkv says "worthy" where the game says "w".

Both programs start the game the same way, so they agree with each other.
A real BBC may differ slightly in two ways nothing in these files can tell:
what was in memory when the game started, which seeds the random numbers
(`$1E`, `$1F`) and a counter used for the ending (`$28`), all taken as 0
here; and how deep the stack was, taken as `$FD`, which affects how far the
picture code's flood fill goes.

## The disassembly

- `tkv_disk/` – the original disk: `!BOOT` runs `TWIN` (BASIC), which shows
  `TITLE` and runs `LVAL`, which loads `VALLEY_` and moves pages of it
  into place before starting the game at `$1400`.
- `make_valleyr.sh` – does what `LVAL` does, making `machinecode/VALLEYR`,
  the game as it is in memory from `$0400` to `$5DFF`.
- `machinecode/`, `command/`, `disassembly/` – slices of that image, the
  BeebDis command files and the disassembly they produce.
- `tkv_notes.txt`, `tkv_routines.txt`, `tkv_memmap.txt`,
  `0C00_structure.txt` – notes on the code and data.
- `docs/` – the picture gallery; `docs/make_gallery.py` rebuilds it.
