#!/bin/sh
# Builds the tools, checks playtkv against the original game (runtkv) on
# every script in tests/scripts, then runs the scripts through builds with
# AddressSanitizer and UndefinedBehaviorSanitizer. Run from anywhere:
#     sh tests/run_tests.sh      or      make -C c test
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
make -C "$ROOT/c" playtkv runtkv readtkv >/dev/null

echo "== Comparing playtkv with the original"
python3 "$ROOT/tests/compare.py"

echo "== Running the scripts under the sanitizers"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
SRC="$ROOT/c"
FLAGS="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -w"
gcc $FLAGS -o "$TMP/playtkv" "$SRC/playtkv.c" "$SRC/libtkv.c" "$SRC/pictures.c" "$SRC/m6502.c" -lm
gcc $FLAGS -o "$TMP/runtkv" "$SRC/runtkv.c" "$SRC/libtkv.c" "$SRC/pictures.c" "$SRC/m6502.c" -lm
failed=0
for script in "$ROOT"/tests/scripts/*.txt; do
  grep -v '^#' "$script" > "$TMP/input"
  for program in playtkv runtkv; do
    # Scripts that patch the game image are run on the original image here:
    # this checks memory safety, not behaviour
    if ! (cd "$SRC" && "$TMP/$program" < "$TMP/input" > "$TMP/out" 2> "$TMP/err") \
       || grep -qE 'ERROR: AddressSanitizer|runtime error' "$TMP/err"; then
      echo "FAILED  $program $(basename "$script")"
      grep -E 'ERROR|runtime error' "$TMP/err" | head -3
      failed=1
    fi
  done
done
[ $failed = 0 ] && echo "ok      no sanitizer errors"
rm -f "$SRC/tkv.sav"
exit $failed
