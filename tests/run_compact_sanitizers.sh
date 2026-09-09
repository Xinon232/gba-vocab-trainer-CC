#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD=$(mktemp -d /tmp/gbavocab-compact-XXXXXX)
trap 'rm -rf "$BUILD"' EXIT
FLAGS=(-std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all -fno-pie -no-pie -Iinclude)
gcc -std=c11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-pie -Wno-discarded-qualifiers -Ireferences/gbawriter/src -Ireferences/gbawriter/src/fonts -c src/entry_font.c -o "$BUILD/font.o"
python3 tests/setup_renderer_mocks.py "$BUILD/mocks"
for test in compact_body compact_bounds native_hide_reuse; do
 g++ -I"$BUILD/mocks" "${FLAGS[@]}" src/writer_core.cpp src/vocab.cpp src/vocab_file_io.cpp src/state.cpp src/flashcard_font.cpp tests/host_compact_font.cpp "$BUILD/font.o" "tests/test_$test.cpp" -o "$BUILD/$test"
 "$BUILD/$test"
done
g++ -I"$BUILD/mocks" "${FLAGS[@]}" src/writer_core.cpp src/vocab.cpp src/vocab_file_io.cpp src/state.cpp src/render.cpp src/flashcard_font.cpp tests/host_compact_font.cpp "$BUILD/font.o" tests/test_renderer_regressions.cpp -o "$BUILD/renderer"
"$BUILD/renderer"
printf 'PASS compact provider/production framebuffer/deferred allocator ASan+UBSan\n'
