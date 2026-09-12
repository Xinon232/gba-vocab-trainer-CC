#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD=$(mktemp -d /tmp/gbavocab-listmeta-XXXXXX)
trap 'rm -rf "$BUILD"' EXIT
ulimit -c 0
CXX=${CXX:-g++}
FLAGS=(-std=c++17 -O2 -Wall -Wextra -Iinclude -DVOCAB_HOST_FATFS)
if [[ "${1:-}" == --sanitize ]]; then FLAGS+=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie); fi
for name in test_list_metadata test_list_metadata_faults; do
 "$CXX" "${FLAGS[@]}" src/writer_core.cpp src/vocab.cpp src/vocab_file_io.cpp tests/host_fatfs.cpp "tests/$name.cpp" -o "$BUILD/$name"
done
"$BUILD/test_list_metadata"
for mode in endings no-sd malformed isolation corrupt meta-write meta-sync meta-close meta-read meta-rename meta-reopen txt-write alias; do "$BUILD/test_list_metadata_faults" "$mode"; done
