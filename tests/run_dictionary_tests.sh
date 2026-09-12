#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD=$(mktemp -d)
trap 'rm -rf "$BUILD"' EXIT
ulimit -c 0
CXX=${CXX:-g++}
FLAGS=(-std=c++17 -O2 -Wall -Wextra -Iinclude)
if [[ ${1:-} == --sanitize ]]; then FLAGS+=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie); fi
python3 tests/test_dictionary_builder.py
python3 tests/test_external_builder.py
python3 tests/test_builder_app.py
python3 -c "import sys;sys.path.insert(0,'builder');import dictionary_builder as b;from pathlib import Path;Path(sys.argv[1]).write_bytes(b.build_dict(dict(name='English German',front='en',back='de',entries=[(f'front {i:05}',f'back {40009-i:05}') for i in range(40010)])))" "$BUILD/large.dict"
"$CXX" "${FLAGS[@]}" src/writer_core.cpp tests/test_external_dictionary.cpp -o "$BUILD/index"
"$BUILD/index" "$BUILD/large.dict"
"$CXX" "${FLAGS[@]}" -DVOCAB_HOST_FATFS src/writer_core.cpp src/vocab.cpp src/vocab_file_io.cpp src/dictionary.cpp src/dictionary_additions.cpp tests/host_fatfs.cpp tests/test_external_storage.cpp -o "$BUILD/storage"
"$BUILD/storage" "$BUILD/large.dict"
python3 -c "import sys;sys.path.insert(0,'builder');import dictionary_builder as b;from pathlib import Path;d=b.read_dict(Path(sys.argv[1]).read_bytes());assert len(d['entries'])==40010 and len(d['additions'])==512;assert d['additions'][-1]==('added 511','neu 511');print('PASS C++ committed records read by PC producer/reader')" "$BUILD/large.dict.roundtrip.dict"
"$CXX" "${FLAGS[@]}" src/writer_core.cpp src/vocab.cpp tests/test_pair_metadata.cpp -o "$BUILD/pair"
"$BUILD/pair"
"$CXX" "${FLAGS[@]}" -DVOCAB_HOST_FATFS src/writer_core.cpp src/vocab.cpp src/vocab_file_io.cpp tests/host_fatfs.cpp tests/test_pair_storage.cpp -o "$BUILD/pairs"
"$BUILD/pairs"
"$CXX" "${FLAGS[@]}" -DVOCAB_HOST_FATFS src/writer_core.cpp src/vocab.cpp src/vocab_file_io.cpp tests/host_fatfs.cpp tests/test_metadata_review.cpp -o "$BUILD/review"
for mode in orphan temporary probe dirty blocked failure clean-add clean-edit clean-remove; do "$BUILD/review" "$mode" "$BUILD/$mode"; done
"$CXX" "${FLAGS[@]}" src/writer_core.cpp src/writer_layout.cpp src/vocab.cpp src/entry_editor.cpp tests/test_dictionary_input.cpp -o "$BUILD/input"
"$BUILD/input"
"$CXX" "${FLAGS[@]}" -DHOME_SCREEN_HOST_TEST src/home_screen.cpp tests/test_dictionary_home.cpp -o "$BUILD/home"
"$BUILD/home"
python3 tests/test_dictionary_wiring.py
python3 tests/test_dictionary_add_wiring.py
python3 tests/test_dictionary_docs.py
printf 'PASS all external dictionary host suites\n'
