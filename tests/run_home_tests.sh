#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD=$(mktemp -d "$PWD/tests/.home-tests-XXXXXX")
trap 'rm -rf "$BUILD"' EXIT
CXX=${CXX:-g++}
FLAGS=(-std=c++17 -O1 -g -Wall -Wextra -DHOME_SCREEN_HOST_TEST -Iinclude -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie)
"$CXX" "${FLAGS[@]}" tests/test_home_screen.cpp src/home_screen.cpp -o "$BUILD/state"
"$BUILD/state"
gcc -std=c11 -Wno-discarded-qualifiers -Ireferences/gbawriter/src -Ireferences/gbawriter/src/fonts -c src/entry_font.c -o "$BUILD/font.o"
"$CXX" "${FLAGS[@]}" -Ireferences/gbawriter/src/fonts tests/test_home_text.cpp src/home_screen.cpp "$BUILD/font.o" -o "$BUILD/text"
"$BUILD/text" references/gbawriter/res/fonts.pack references/gbawriter/res/reader-symbols.pack
python3 tests/test_home_wiring.py
