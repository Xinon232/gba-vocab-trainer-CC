#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD=$(mktemp -d /tmp/gbavocab-host-XXXXXX)
trap 'rm -rf "$BUILD"' EXIT
ulimit -c 0
CORRECTNESS_ONLY=false
case "${1:-}" in
  "") ;;
  --correctness-only) CORRECTNESS_ONLY=true ;;
  *) printf 'Usage: %s [--correctness-only]\n' "$0" >&2; exit 2 ;;
esac
CXX=${CXX:-g++}
COMMON=(-std=c++17 -O2 -Wall -Wextra -Iinclude)
for name in test_vocab test_vocab_10k test_vocab_grouped test_vocab_file_io_perf test_state test_feedback_hold test_integration test_save_status test_empty_boxes test_scan_limits test_scan_visit test_long_display test_embedded_control test_switch test_page_input test_save_navigation test_text_layout test_body_pixels; do
  if $CORRECTNESS_ONLY && [[ "$name" == test_vocab_file_io_perf ]]; then continue; fi
  "$CXX" "${COMMON[@]}" src/vocab.cpp src/vocab_file_io.cpp src/state.cpp "tests/$name.cpp" -o "$BUILD/$name"
  "$BUILD/$name"
done
"$CXX" "${COMMON[@]}" -DVOCAB_HOST_FATFS src/vocab.cpp tests/host_fatfs.cpp tests/test_io_windows.cpp -o "$BUILD/io_windows"
for mode in capacity generated maximum short-scanner short-identity; do
  "$BUILD/io_windows" "$mode"
done
"$CXX" "${COMMON[@]}" -DVOCAB_HOST_FATFS src/vocab.cpp src/vocab_file_io.cpp tests/host_fatfs.cpp tests/test_fatfs_transaction.cpp -o "$BUILD/fatfs"
"$BUILD/fatfs"
"$CXX" "${COMMON[@]}" -DVOCAB_HOST_FATFS src/vocab.cpp src/vocab_file_io.cpp src/state.cpp tests/host_fatfs.cpp tests/test_fatfs_faults.cpp -o "$BUILD/fatfs_faults"
for mode in identity-boundaries load-truncated-stream output-tail-corruption alias-backup-crash alias-temp-crash alias-backup-error alias-temp-error alias-backup-temp recovery-open recovery-close stat-owned create-collision recovery-read recovery-stat cleanup-stat append edit precommit journal-short temp-short temp-crash rollback-park rollback-restore rename-restore reopen-ok reopen-fail; do
  "$BUILD/fatfs_faults" "$mode"
done
"$CXX" "${COMMON[@]}" src/vocab.cpp src/vocab_file_io.cpp src/state.cpp tests/test_ui_regressions.cpp -o "$BUILD/ui_regressions"
"$BUILD/ui_regressions" boxes
"$BUILD/ui_regressions" gesture
python3 tests/setup_renderer_mocks.py "$BUILD/renderer_mocks"
"$CXX" -I"$BUILD/renderer_mocks" "${COMMON[@]}" src/vocab.cpp src/vocab_file_io.cpp src/state.cpp src/render.cpp tests/test_renderer_regressions.cpp -o "$BUILD/renderer_regressions"
"$BUILD/renderer_regressions"
python3 tests/test_no_sav_persistence.py
python3 tests/test_ui_palette.py
python3 tests/test_ui_io_contract.py
printf '\nPASS all host suites (production FatFS adapter included)\n'
