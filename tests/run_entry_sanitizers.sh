#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD=$(mktemp -d /tmp/gbavocab-entry-asan-XXXXXX)
trap 'rm -rf "$BUILD"' EXIT
FLAGS=(-std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all -fno-pie -no-pie -Iinclude)
for test in editor columns; do
 g++ "${FLAGS[@]}" src/writer_core.cpp src/writer_layout.cpp src/entry_editor.cpp src/vocab.cpp "tests/test_entry_$test.cpp" -o "$BUILD/$test"
 "$BUILD/$test"
done
for test in storage storage_failures; do
 g++ "${FLAGS[@]}" -DVOCAB_HOST_FATFS src/writer_core.cpp src/vocab.cpp src/vocab_file_io.cpp tests/host_fatfs.cpp "tests/test_entry_$test.cpp" -o "$BUILD/$test"
 "$BUILD/$test"
done
gcc -std=c11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-pie -Wno-discarded-qualifiers -Ireferences/gbawriter/src -Ireferences/gbawriter/src/fonts -c src/entry_font.c -o "$BUILD/font.o"
g++ "${FLAGS[@]}" -Ireferences/gbawriter/src/fonts src/writer_core.cpp src/writer_layout.cpp src/entry_editor.cpp src/entry_render.cpp src/vocab.cpp tests/test_entry_render.cpp "$BUILD/font.o" -o "$BUILD/render"
"$BUILD/render" references/gbawriter/res/fonts.pack references/gbawriter/res/reader-symbols.pack
printf 'PASS Entry editor ASan/UBSan suites\n'
