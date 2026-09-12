#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD=$(mktemp -d)
trap 'rm -rf "$BUILD"' EXIT
ulimit -c 0
CXX=${CXX:-g++}
FLAGS=(-std=c++17 -O2 -Wall -Wextra -Iinclude)
python3 tests/test_dictionary_builder.py
python3 tests/make_dictionary_fixture.py "$BUILD/dictionary.bin"
"$CXX" "${FLAGS[@]}" tests/test_dictionary.cpp -o "$BUILD/index"
"$BUILD/index" "$BUILD/dictionary.bin"
"$CXX" "${FLAGS[@]}" src/writer_core.cpp src/vocab.cpp tests/test_pair_metadata.cpp -o "$BUILD/pair"
"$BUILD/pair"
"$CXX" "${FLAGS[@]}" -DVOCAB_HOST_FATFS src/writer_core.cpp src/vocab.cpp src/vocab_file_io.cpp tests/host_fatfs.cpp tests/test_pair_storage.cpp -o "$BUILD/storage"
"$BUILD/storage"
"$CXX" "${FLAGS[@]}" src/writer_core.cpp src/writer_layout.cpp src/vocab.cpp src/entry_editor.cpp tests/test_dictionary_input.cpp -o "$BUILD/input"
"$CXX" "${FLAGS[@]}" -DVOCAB_HOST_FATFS src/writer_core.cpp src/dictionary_additions.cpp tests/host_fatfs.cpp tests/test_dictionary_additions.cpp -o "$BUILD/additions"
"$BUILD/additions"
"$CXX" "${FLAGS[@]}" -DVOCAB_HOST_FATFS src/writer_core.cpp src/dictionary_additions.cpp tests/host_fatfs.cpp tests/test_dictionary_search.cpp -o "$BUILD/search"
"$BUILD/search" "$BUILD/dictionary.bin"
"$BUILD/input"
"$CXX" "${FLAGS[@]}" -DHOME_SCREEN_HOST_TEST src/home_screen.cpp tests/test_dictionary_home.cpp -o "$BUILD/home"
"$BUILD/home"
python3 tests/test_dictionary_wiring.py
python3 tests/test_dictionary_add_wiring.py
python3 tests/test_dictionary_docs.py
printf 'PASS all dictionary host suites\n'
